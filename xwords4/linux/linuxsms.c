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

#define ADDR_FMT "from: %s %d\n"

static void
makeQueuePath( const XP_UCHAR* phone, XP_U16 port,
               XP_UCHAR* path, XP_U16 pathlen )
{
    XP_ASSERT( 0 != port );
    snprintf( path, pathlen, "%s/%s_%d", SMS_DIR, phone, port );
}

typedef struct _LinSMSData {
    int fd, wd;
    XP_UCHAR myQueue[256];
    XP_U16 myPort;
    FILE* lock;
    XP_U16 count;

    const gchar* myPhone;
    const SMSProcs* procs;
    void* procClosure;
} LinSMSData;

typedef enum { NONE, INVITE, DATA, DEATH, ACK, } SMS_CMD;
#define SMS_PROTO_VERSION 0


static LinSMSData* getStorage( LaunchParams* params );
static void writeHeader( XWStreamCtxt* stream, SMS_CMD cmd );


static void
lock_queue( LinSMSData* storage )
{
    char lock[256];
    snprintf( lock, sizeof(lock), "%s/%s", storage->myQueue, LOCK_FILE );
    FILE* fp = fopen( lock, "w" );
    XP_ASSERT( !!fp );
    XP_ASSERT( NULL == storage->lock );
    storage->lock = fp;
}

static void
unlock_queue( LinSMSData* storage )
{
    XP_ASSERT( NULL != storage->lock );
    FILE* lock = storage->lock;
    storage->lock = NULL;
    fclose( lock );
}

static XP_S16
send_sms( LinSMSData* storage, XWStreamCtxt* stream, 
          const XP_UCHAR* phone, XP_U16 port )
{
    const XP_U8* buf = stream_getPtr( stream );
    XP_U16 buflen = stream_getSize( stream );

    XP_S16 nSent = -1;
    XP_ASSERT( !!storage );
    char path[256];

    lock_queue( storage );

#ifdef DEBUG
    gchar* str64 = g_base64_encode( buf, buflen );
#endif

    XP_U16 count = ++storage->count;
    makeQueuePath( phone, port, path, sizeof(path) );
    XP_LOGF( "%s: writing msg %d to %s", __func__, count, path );
    g_mkdir_with_parents( path, 0777 ); /* just in case */
    int len = strlen( path );
    snprintf( &path[len], sizeof(path)-len, "/%d", count );

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
    (void)fprintf( fp, ADDR_FMT, storage->myPhone, storage->myPort );
    (void)fprintf( fp, "%s\n", sms );
    fclose( fp );
    sync();

    unlock_queue( storage );

    nSent = buflen;

    return nSent;
} /* linux_sms_send */

static XP_S16
decodeAndDelete( LinSMSData* storage, const gchar* name, 
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

    char phone[32];
    int port;
    int matched = sscanf( contents, ADDR_FMT, phone, &port );
    if ( 2 == matched ) {
        gchar* eol = strstr( contents, "\n" );
        *eol = '\0';
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
            addr_setType( addr, COMMS_CONN_SMS );
            XP_STRNCPY( addr->u.sms.phone, phone, sizeof(addr->u.sms.phone) );
            XP_LOGF( "%s: message came from phone: %s, port: %d", __func__, phone, port );
            addr->u.sms.port = port;
        }
    } else {
        XP_ASSERT(0);
    }

    g_free( contents );

    LOG_RETURNF( "%d", nRead );
    return nRead;
} /* decodeAndDelete */

static void
dispatch_invite( LinSMSData* storage, XP_U16 XP_UNUSED(proto), 
                 XWStreamCtxt* stream, CommsAddrRec* addr )
{
    XP_UCHAR gameName[256];
    XP_UCHAR dictName[256];

    XP_U32 gameID = stream_getU32( stream );
    XP_LOGF( "%s: got gameID %d", __func__, gameID );
    stringFromStreamHere( stream, gameName, VSIZE(gameName) );
    XP_U32 dictLang = stream_getU32( stream );
    stringFromStreamHere( stream, dictName, VSIZE(dictName) );
    XP_U8 nMissing = stream_getU8( stream );
    XP_U8 nPlayers = stream_getU8( stream );

    addrFromStream( addr, stream );

    (*storage->procs->inviteReceived)( storage->procClosure, gameName, 
                                       gameID, dictLang, dictName, nPlayers, 
                                       nMissing, addr );
}

static void
dispatch_data( LinSMSData* storage, XP_U16 XP_UNUSED(proto), 
               XWStreamCtxt* stream, const CommsAddrRec* addr )
{
    XP_U32 gameID = stream_getU32( stream );
    XP_U16 len = stream_getSize( stream );
    XP_U8 data[len];
    stream_getBytes( stream, data, len );
    
    (*storage->procs->msgReceived)( storage->procClosure, addr, gameID, 
                                    data, len );
}

static void
parseAndDispatch( LaunchParams* params, uint8_t* buf, int len, 
                  CommsAddrRec* addr )
{
    LinSMSData* storage = getStorage( params );
    XWStreamCtxt* stream = mem_stream_make( MPPARM(params->mpool)
                                            params->vtMgr, 
                                            NULL, CHANNEL_NONE, NULL );
    stream_setVersion( stream, CUR_STREAM_VERS );
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

static gboolean
sms_receive( GIOChannel *source, GIOCondition condition, gpointer data )
{
    LOG_FUNC();
    XP_ASSERT( 0 != (G_IO_IN & condition) );
    LaunchParams* params = (LaunchParams*)data;
    XP_ASSERT( !!params->smsStorage );
    LinSMSData* storage = getStorage( params );

    int socket = g_io_channel_unix_get_fd( source );
    XP_ASSERT( socket == storage->fd );

    lock_queue( storage );

    /* read required or we'll just get the event again.  But we don't care
       about the result or the buffer contents. */
    XP_U8 buffer[sizeof(struct inotify_event) + 16];
    if ( 0 > read( socket, buffer, sizeof(buffer) ) ) {
        XP_LOGF( "%s: discarding inotify buffer", __func__ );
    }
    for ( ; ; ) {
        XP_S16 nRead = -1;
        char shortest[256] = { '\0' };
        GDir* dir = g_dir_open( storage->myQueue, 0, NULL );
        XP_LOGF( "%s: opening queue %s", __func__, storage->myQueue );
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
            XP_LOGF( "%s: decoding message %s", __func__, shortest );
            nRead = decodeAndDelete( storage, shortest, buf, 
                                     sizeof(buf), &fromAddr );
        } else {
            XP_LOGF( "%s: never found shortest", __func__ );
        }

        unlock_queue( storage );

        if ( 0 < nRead ) {
            parseAndDispatch( params, buf, nRead, &fromAddr );
            lock_queue( storage );
        } else {
            break;
        }
    }
    return TRUE;
} /* sms_receive */

void
linux_sms_init( LaunchParams* params, const gchar* myPhone, XP_U16 myPort,
                const SMSProcs* procs, void* procClosure )
{
    LOG_FUNC();
    XP_ASSERT( !!myPhone );
    LinSMSData* storage = getStorage( params );
    XP_ASSERT( !!storage );
    storage->myPhone = myPhone;
    storage->myPort = myPort;
    storage->procs = procs;
    storage->procClosure = procClosure;

    makeQueuePath( myPhone, myPort, storage->myQueue, sizeof(storage->myQueue) );
    XP_LOGF( "%s: my queue: %s", __func__, storage->myQueue );
    storage->myPort = params->connInfo.sms.port;

    (void)g_mkdir_with_parents( storage->myQueue, 0777 );

    int fd = inotify_init();
    storage->fd = fd;
    storage->wd = inotify_add_watch( fd, storage->myQueue, IN_MODIFY );
    
    (*procs->socketAdded)( params, fd, sms_receive );
} /* linux_sms_init */

void
linux_sms_invite( LaunchParams* params, const CurGameInfo* gi, 
                  const CommsAddrRec* addr, const gchar* gameName, 
                  XP_U16 nMissing, const gchar* toPhone, int toPort )
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

    addrToStream( stream, addr );

    LinSMSData* storage = getStorage( params );
    send_sms( storage, stream, toPhone, toPort );

    stream_destroy( stream );
}

XP_S16
linux_sms_send( LaunchParams* params, const XP_U8* buf,
                XP_U16 buflen, const XP_UCHAR* phone, XP_U16 port,
                XP_U32 gameID )
{
    XP_LOGF( "%s(len=%d)", __func__, buflen );
    XWStreamCtxt* stream = mem_stream_make( MPPARM(params->mpool) params->vtMgr,
                                            NULL, CHANNEL_NONE, NULL );
    writeHeader( stream, DATA );
    stream_putU32( stream, gameID );
    stream_putBytes( stream, buf, buflen );

    LinSMSData* storage = getStorage( params );
    if ( 0 >= send_sms( storage, stream, phone, port ) ) {
        buflen = -1;
    }
    stream_destroy( stream );

    return buflen;
}

void
linux_sms_cleanup( LaunchParams* params )
{
    XP_FREEP( params->mpool, &params->smsStorage );
}

static LinSMSData* 
getStorage( LaunchParams* params )
{
    LinSMSData* storage = (LinSMSData*)params->smsStorage;
    if ( NULL == storage ) {
        storage = XP_CALLOC( params->mpool, sizeof(*storage) );
        params->smsStorage = storage;
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
