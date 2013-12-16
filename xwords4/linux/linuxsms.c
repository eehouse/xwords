/* -*- compile-command: "make -j MEMDEBUG=TRUE";-*- */ 
/* 
 * Copyright 2006-2009 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef XWFEATURE_SMS

#include <glib.h>
#include <glib/gstdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/inotify.h>

#include "linuxsms.h"
#include "strutils.h"

#define SMS_DIR "/tmp/xw_sms"
#define LOCK_FILE ".lock"

/* The idea here is to mimic an SMS-based transport using files.  We'll use a
 * directory in /tmp to hold "messages", in files with names based on phone
 * number and port.  Servers and clients will listen for changes in that
 * directory and "receive" messages when a change is noted to a file with
 * their phone number and on the port they listen to.
 *
 * Server's phone number is known to both server and client and passed on the
 * commandline.  Client's "phone number" is the PID of the client process.
 * When the client sends to the server the phone number is passed in and its
 * return (own) number is included in the "message".  
 *
 * If I'm the server, I create an empty file for each port I listen on.  (Only
 * one now....).  Clients will append to that.  Likewise, a client creates a
 * file that it will listen on.
 *
 * Data is encoded using the same mechanism so size constraints can be
 * checked.
 */

static void
makeQueuePath( const XP_UCHAR* phone, XP_U16 port,
               XP_UCHAR* path, XP_U16 pathlen )
{
    snprintf( path, pathlen, "%s/%s_%d", SMS_DIR, phone, port );
}

typedef struct _LinSMS2Data {
    int fd, wd;
    XP_UCHAR myQueue[256];
    XP_U16 port;
    FILE* lock;
    XP_U16 count;

    const gchar* myPhone;
    const SMSProcs* procs;
    void* procClosure;
} LinSMS2Data;

typedef enum { NONE, INVITE, DATA, DEATH, ACK, } SMS_CMD;
#define SMS_PROTO_VERSION 0


static LinSMS2Data* getStorage( LaunchParams* params );
static void writeHeader( XWStreamCtxt* stream, SMS_CMD cmd );


static void
lock_queue2( LinSMS2Data* storage )
{
    char lock[256];
    snprintf( lock, sizeof(lock), "%s/%s", storage->myQueue, LOCK_FILE );
    FILE* fp = fopen( lock, "w" );
    XP_ASSERT( NULL == storage->lock );
    storage->lock = fp;
}

static void
unlock_queue2( LinSMS2Data* storage )
{
    XP_ASSERT( NULL != storage->lock );
    fclose( storage->lock );
    storage->lock = NULL;
}


static XP_S16
send_sms( LinSMS2Data* storage, XWStreamCtxt* stream, 
          const XP_UCHAR* phone, XP_U16 port )
{
    const XP_U8* buf = stream_getPtr( stream );
    XP_U16 buflen = stream_getSize( stream );

    XP_S16 nSent = -1;
    XP_ASSERT( !!storage );
    char path[256];

    lock_queue2( storage );

#ifdef DEBUG
    gchar* str64 = g_base64_encode( buf, buflen );
#endif

    makeQueuePath( phone, port, path, sizeof(path) );
    XP_LOGF( "%s: writing to %s", __func__, path );
    g_mkdir_with_parents( path, 0777 ); /* just in case */
    int len = strlen( path );
    snprintf( &path[len], sizeof(path)-len, "/%d", ++storage->count );

    XP_UCHAR sms[buflen*2];     /* more like (buflen*4/3) */
    XP_U16 smslen = sizeof(sms);
    binToSms( sms, &smslen, buf, buflen );
    XP_ASSERT( smslen == strlen(sms) );

#ifdef DEBUG
    XP_ASSERT( !strcmp( str64, sms ) );
    g_free( str64 );

    XP_U8 testout[buflen];
    XP_U16 lenout = sizeof( testout );
    XP_ASSERT( smsToBin( testout, &lenout, sms, smslen ) );
    XP_ASSERT( lenout == buflen );
    XP_ASSERT( XP_MEMCMP( testout, buf, smslen ) );
#endif

    FILE* fp = fopen( path, "w" );
    XP_ASSERT( !!fp );
    (void)fprintf( fp, "from: %s\n", storage->myPhone );
    (void)fprintf( fp, "%s\n", sms );
    fclose( fp );
    sync();

    unlock_queue2( storage );

    nSent = buflen;

    return nSent;
} /* linux_sms_send */

static XP_S16
decodeAndDelete2( LinSMS2Data* storage, const gchar* name, 
                  XP_U8* buf, XP_U16 buflen, CommsAddrRec* addr )
{
    LOG_FUNC();
    XP_S16 nRead = -1;
    char path[256];
    snprintf( path, sizeof(path), "%s/%s", storage->myQueue, name );

    gchar* contents;
    gsize length;
#ifdef DEBUG
    gboolean success = 
#endif
        g_file_get_contents( path, &contents, &length, NULL );
    XP_ASSERT( success );
    unlink( path );

    if ( 0 == strncmp( "from: ", contents, 6 ) ) {
        char phone[32];
        gchar* eol = strstr( contents, "\n" );
        *eol = '\0';
        XP_STRNCPY( phone, &contents[6], sizeof(phone) );
        XP_ASSERT( !*eol );
        ++eol;         /* skip NULL */
        *strstr(eol, "\n" ) = '\0';

        XP_U16 inlen = strlen(eol);      /* skip \n */
        XP_U8 out[inlen];
        XP_U16 outlen = sizeof(out);
        XP_Bool valid = smsToBin( out, &outlen, eol, inlen );

        if ( valid && outlen <= buflen ) {
            XP_MEMCPY( buf, out, outlen );
            nRead = outlen;
            addr->conType = COMMS_CONN_SMS;
            XP_STRNCPY( addr->u.sms.phone, phone, sizeof(addr->u.sms.phone) );
            XP_LOGF( "%s: message came from phone: %s", __func__, phone );
            addr->u.sms.port = 1; /* for now */
        }
    }

    g_free( contents );

    LOG_RETURNF( "%d", nRead );
    return nRead;
} /* decodeAndDelete2 */

static void
dispatch_invite( LinSMS2Data* storage, XP_U16 XP_UNUSED(proto), 
                 XWStreamCtxt* stream, const CommsAddrRec* addr )
{
    XP_UCHAR gameName[256];
    XP_UCHAR dictName[256];

    XP_U32 gameID = stream_getU32( stream );
    XP_LOGF( "%s: got gameID %ld", __func__, gameID );
    stringFromStreamHere( stream, gameName, VSIZE(gameName) );
    XP_U32 dictLang = stream_getU32( stream );
    stringFromStreamHere( stream, dictName, VSIZE(dictName) );
    XP_U8 nMissing = stream_getU8( stream );
    XP_U8 nPlayers = stream_getU8( stream );

    (*storage->procs->inviteReceived)( storage->procClosure, gameName, gameID,
                                       dictLang, dictName, nPlayers, nMissing,
                                       addr );
}

static void
dispatch_data( LinSMS2Data* storage, XP_U16 XP_UNUSED(proto), 
               XWStreamCtxt* stream, const CommsAddrRec* addr )
{
    XP_USE( addr );
    XP_U32 gameID = stream_getU32( stream );
    const XP_U8* data = stream_getPtr( stream );
    XP_U16 len = stream_getSize( stream );
    
    (*storage->procs->msgReceived)( storage->procClosure, gameID, 
                                    data, len, addr );
}

static void
parseAndDispatch( LaunchParams* params, uint8_t* buf, int len, 
                  const CommsAddrRec* addr )
{
    LinSMS2Data* storage = getStorage( params );
    XWStreamCtxt* stream = mem_stream_make( MPPARM(params->mpool)
                                            params->vtMgr, 
                                            NULL, CHANNEL_NONE, NULL );
    stream_putBytes( stream, buf, len );

    XP_U8 proto = stream_getU8( stream );
    XP_ASSERT( SMS_PROTO_VERSION == proto );
    XP_U8 cmd = stream_getU8( stream );
    switch( cmd ) {
    case INVITE:
        dispatch_invite( storage, proto, stream, addr );
        break;
    case DATA:
        dispatch_data( storage, proto, stream, addr );
        break;
    case DEATH:
    case ACK:
        break;
    default:
        XP_ASSERT( 0 );
    }

    stream_destroy( stream );
}

static void 
sms2_receive( void* closure, int socket )
{
    LOG_FUNC();
    LaunchParams* params = (LaunchParams*)closure;
    XP_ASSERT( !!params->sms2Storage );
    LinSMS2Data* storage = getStorage( params );

    XP_S16 nRead = -1;

    XP_ASSERT( socket == storage->fd );

    lock_queue2( storage );

    /* read required or we'll just get the event again.  But we don't care
       about the result or the buffer contents. */
    XP_U8 buffer[sizeof(struct inotify_event) + 16];
    if ( 0 > read( socket, buffer, sizeof(buffer) ) ) {
    }
    char shortest[256] = { '\0' };
    GDir* dir = g_dir_open( storage->myQueue, 0, NULL );
    XP_LOGF( "%s: opening %s", __func__, storage->myQueue );
    for ( ; ; ) {
        const gchar* name = g_dir_read_name( dir );
        if ( NULL == name ) {
            break;
        } else if ( 0 == strcmp( name, LOCK_FILE ) ) {
            continue;
        }
        if ( !shortest[0] || 0 < strcmp( shortest, name ) ) {
            snprintf( shortest, sizeof(shortest), "%s", name );
        }
    }
    g_dir_close( dir );

    uint8_t buf[256];
    CommsAddrRec fromAddr = {0};
    if ( !!shortest[0] ) {
        nRead = decodeAndDelete2( storage, shortest, buf, sizeof(buf), &fromAddr );
    }

    unlock_queue2( storage );

    if ( 0 < nRead ) {
        parseAndDispatch( params, buf, nRead, &fromAddr );
    }
} /* sms2_receive */

void
linux_sms2_init( LaunchParams* params, const gchar* phone,
                 const SMSProcs* procs, void* procClosure )
{
    XP_ASSERT( !!phone );
    LinSMS2Data* storage = getStorage( params );
    XP_ASSERT( !!storage );
    storage->myPhone = phone;
    storage->procs = procs;
    storage->procClosure = procClosure;

    makeQueuePath( phone, params->connInfo.sms.port,
                   storage->myQueue, sizeof(storage->myQueue) );
    XP_LOGF( "%s: my queue: %s", __func__, storage->myQueue );
    storage->port = params->connInfo.sms.port;

    (void)g_mkdir_with_parents( storage->myQueue, 0777 );

    int fd = inotify_init();
    storage->fd = fd;
    storage->wd = inotify_add_watch( fd, storage->myQueue, IN_MODIFY );
    
    (*procs->socketChanged)( procClosure, fd, -1, sms2_receive, params );
} /* linux_sms2_init */

void
linux_sms2_invite( LaunchParams* params, const CurGameInfo* gi, 
                   const gchar* gameName, XP_U16 nMissing, const gchar* phone,
                   int port )
{
    LOG_FUNC();
    XWStreamCtxt* stream;
    stream = mem_stream_make( MPPARM(params->mpool) params->vtMgr,
                              NULL, CHANNEL_NONE, NULL );
    writeHeader( stream, INVITE );
    stream_putU32( stream, gi->gameID );
    stringToStream( stream, gameName );
    stream_putU32( stream, gi->dictLang );
    stringToStream( stream, gi->dictName );
    stream_putU8( stream, nMissing );
    stream_putU8( stream, gi->nPlayers );

    LinSMS2Data* storage = getStorage( params );
    send_sms( storage, stream, phone, port );

    stream_destroy( stream );
}

XP_S16
linux_sms2_send( LaunchParams* params, const XP_U8* buf,
                 XP_U16 buflen, const XP_UCHAR* phone, XP_U16 port,
                 XP_U32 gameID )
{
    LOG_FUNC();
    XWStreamCtxt* stream = mem_stream_make( MPPARM(params->mpool) params->vtMgr,
                                            NULL, CHANNEL_NONE, NULL );
    writeHeader( stream, DATA );
    stream_putU32( stream, gameID );
    stream_putBytes( stream, buf, buflen );

    LinSMS2Data* storage = getStorage( params );
    if ( 0 >= send_sms( storage, stream, phone, port ) ) {
        buflen = -1;
    }

    return buflen;
}

void
linux_sms2_cleanup( LaunchParams* params )
{
    XP_FREEP( params->mpool, &params->sms2Storage );
}

static LinSMS2Data* 
getStorage( LaunchParams* params )
{
    LinSMS2Data* storage = (LinSMS2Data*)params->sms2Storage;
    if ( NULL == storage ) {
        storage = XP_CALLOC( params->mpool, sizeof(*storage) );
        params->sms2Storage = storage;
    }
    return storage;
}

static void
writeHeader( XWStreamCtxt* stream, SMS_CMD cmd )
{
    stream_putU8( stream, SMS_PROTO_VERSION );
    stream_putU8( stream, cmd );
}

#endif /* XWFEATURE_SMS */
