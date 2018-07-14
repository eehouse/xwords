/* -*- compile-command: "make -j MEMDEBUG=TRUE";-*- */ 
/* 
 * Copyright 2006 - 2018 by Eric House (xwords@eehouse.org).  All rights
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

#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "linuxsms.h"
#include "linuxutl.h"
#include "strutils.h"
#include "smsproto.h"
#include "linuxmain.h"

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

typedef struct _LinSMSData {
    XP_UCHAR myQueue[256];
    XP_U16 myPort;
    FILE* lock;

    const gchar* myPhone;
    const SMSProcs* procs;
    void* procClosure;
    SMSProto* protoState;
} LinSMSData;

static void doSend( LaunchParams* params, const XP_U8* buf,
                    XP_U16 buflen, const XP_UCHAR* phone, XP_U16 port,
                    XP_U32 gameID );
static gboolean retrySend( gpointer data );
static void sendOrRetry( LaunchParams* params, SMSMsgArray* arr, XP_U16 waitSecs,
                         const XP_UCHAR* phone, XP_U16 port, XP_U32 gameID );
static gint check_for_files( gpointer data );
static gint check_for_files_once( gpointer data );

static void
formatQueuePath( const XP_UCHAR* phone, XP_U16 port, XP_UCHAR* path,
                 XP_U16 pathlen )
{
    XP_ASSERT( 0 != port );
    snprintf( path, pathlen, "%s/%s_%d", SMS_DIR, phone, port );
}

typedef enum { NONE, INVITE, DATA, DEATH, ACK, } SMS_CMD;
#define SMS_PROTO_VERSION 0


static LinSMSData* getStorage( LaunchParams* params );
static void writeHeader( XWStreamCtxt* stream, SMS_CMD cmd );


static void
lock_queue( LinSMSData* storage )
{
    gchar* lock = g_strdup_printf( "%s/%s", storage->myQueue, LOCK_FILE );
    FILE* fp = fopen( lock, "w" );
    XP_ASSERT( !!fp );
    XP_ASSERT( NULL == storage->lock );
    storage->lock = fp;
    g_free( lock );
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
write_fake_sms( LaunchParams* params, XWStreamCtxt* stream,
                const XP_UCHAR* phone, XP_U16 port )
{
    XP_S16 nSent;
    XP_U16 pct = XP_RANDOM() % 100;
    XP_Bool skipWrite = pct < params->smsSendFailPct;

    if ( skipWrite ) {
        nSent = stream_getSize( stream );
        XP_LOGF( "%s(): dropping sms msg of len %d to phone %s", __func__,
                 nSent, phone );
    } else {
        LinSMSData* storage = getStorage( params );
        const XP_U8* buf = stream_getPtr( stream );
        XP_U16 buflen = stream_getSize( stream );
        XP_LOGF( "%s(phone=%s, port=%d, len=%d)", __func__, phone,
                 port, buflen );

        XP_ASSERT( !!storage );
        char path[256];

        lock_queue( storage );

#ifdef DEBUG
        gchar* str64 = g_base64_encode( buf, buflen );
#endif

        formatQueuePath( phone, port, path, sizeof(path) );

        /* Random-number-based name is fine, as we pick based on age. */
        int rint = makeRandomInt();
        g_mkdir_with_parents( path, 0777 ); /* just in case */
        int len = strlen( path );
        snprintf( &path[len], sizeof(path)-len, "/%u", rint );

        XP_UCHAR sms[buflen*2];     /* more like (buflen*4/3) */
        XP_U16 smslen = sizeof(sms);
        binToSms( sms, &smslen, buf, buflen );
        XP_ASSERT( smslen == strlen(sms) );
        XP_LOGF( "%s: writing msg to %s", __func__, path );

#ifdef DEBUG
        XP_ASSERT( !strcmp( str64, sms ) );
        g_free( str64 );

        XP_U8 testout[buflen];
        XP_U16 lenout = sizeof( testout );
        XP_ASSERT( smsToBin( testout, &lenout, sms, smslen ) );
        XP_ASSERT( lenout == buflen );
        // valgrind doesn't like this; punting on figuring out
        // XP_ASSERT( XP_MEMCMP( testout, buf, smslen ) );
#endif

        FILE* fp = fopen( path, "w" );
        XP_ASSERT( !!fp );
        (void)fprintf( fp, ADDR_FMT, storage->myPhone, storage->myPort );
        (void)fprintf( fp, "%s\n", sms );
        fclose( fp );
        sync();

        unlock_queue( storage );

        nSent = buflen;

        LOG_RETURNF( "%d", nSent );
    }
    return nSent;
} /* write_fake_sms */

static XP_S16
decodeAndDelete( LinSMSData* storage, const gchar* name, 
                 XP_U8* buf, XP_U16 buflen, CommsAddrRec* addr )
{
    LOG_FUNC();
    XP_S16 nRead = -1;
    gchar* path = g_strdup_printf( "%s/%s", storage->myQueue, name );

    gchar* contents;
    gsize length;
#ifdef DEBUG
    gboolean success = 
#endif
        g_file_get_contents( path, &contents, &length, NULL );
    XP_ASSERT( success );
    unlink( path );
    g_free( path );

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
        XP_LOGF( "%s(): decoding message from file %s", __func__, name );
        XP_U8 out[inlen];
        XP_U16 outlen = sizeof(out);
        XP_Bool valid = smsToBin( out, &outlen, eol, inlen );

        if ( valid && outlen <= buflen ) {
            XP_MEMCPY( buf, out, outlen );
            nRead = outlen;
            addr_setType( addr, COMMS_CONN_SMS );
            XP_STRNCPY( addr->u.sms.phone, phone, sizeof(addr->u.sms.phone) );
            XP_LOGF( "%s: message came from phone: %s, port: %d", __func__,
                     phone, port );
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
    XP_U16 forceChannel = stream_getU8( stream );

    addrFromStream( addr, stream );

    (*storage->procs->inviteReceived)( storage->procClosure, gameName, 
                                       gameID, dictLang, dictName, nPlayers,
                                       nMissing, forceChannel, addr );
}

static void
dispatch_data( LinSMSData* storage, XP_U16 XP_UNUSED(proto), 
               XWStreamCtxt* stream, const CommsAddrRec* addr )
{
    LOG_FUNC();

    XP_U32 gameID = stream_getU32( stream );
    XP_U16 len = stream_getSize( stream );
    XP_U8 data[len];
    stream_getBytes( stream, data, len );

    const XP_UCHAR* fromPhone = addr->u.sms.phone;
    SMSMsgArray* arr = smsproto_prepInbound( storage->protoState, fromPhone,
                                             data, len );
    if ( NULL != arr ) {
        for ( XP_U16 ii = 0; ii < arr->nMsgs; ++ii ) {
            SMSMsg* msg = &arr->msgs[ii];
            (*storage->procs->msgReceived)( storage->procClosure, addr, gameID,
                                            msg->data, msg->len );
        }
        smsproto_freeMsgArray( storage->protoState, arr );
    }
}

static void
parseAndDispatch( LaunchParams* params, uint8_t* buf, int len, 
                  CommsAddrRec* addr )
{
    LinSMSData* storage = getStorage( params );
    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(params->mpool)
                                                params->vtMgr );
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
    storage->protoState = smsproto_init( MPPARM(params->mpool) params->dutil );

    formatQueuePath( myPhone, myPort, storage->myQueue, sizeof(storage->myQueue) );
    XP_LOGF( "%s: my queue: %s", __func__, storage->myQueue );
    storage->myPort = params->connInfo.sms.port;

    (void)g_mkdir_with_parents( storage->myQueue, 0777 );

    /* Look for preexisting or new files every half second. Easier than
       inotify, especially when you add the need to handle files written while
       not running. */
    (void)g_idle_add( check_for_files_once, params );
    (void)g_timeout_add( 500, check_for_files, params );
} /* linux_sms_init */

void
linux_sms_invite( LaunchParams* params, const CurGameInfo* gi, 
                  const CommsAddrRec* addr, const gchar* gameName, 
                  XP_U16 nMissing, int forceChannel,
                  const gchar* toPhone, int toPort )
{
    LOG_FUNC();
    XWStreamCtxt* stream;
    stream = mem_stream_make_raw( MPPARM(params->mpool) params->vtMgr );
    writeHeader( stream, INVITE );
    stream_putU32( stream, gi->gameID );
    stringToStream( stream, gameName );
    stream_putU32( stream, gi->dictLang );
    stringToStream( stream, gi->dictName );
    stream_putU8( stream, nMissing );
    stream_putU8( stream, gi->nPlayers );
    stream_putU8( stream, forceChannel );

    addrToStream( stream, addr );

    write_fake_sms( params, stream, toPhone, toPort );

    stream_destroy( stream );
}

XP_S16
linux_sms_send( LaunchParams* params, const XP_U8* buf,
                XP_U16 buflen, const XP_UCHAR* phone, XP_U16 port,
                XP_U32 gameID )
{
    LinSMSData* storage = getStorage( params );
    XP_U16 waitSecs;
    SMSMsgArray* arr = smsproto_prepOutbound( storage->protoState, buf, buflen,
                                              phone, XP_TRUE, &waitSecs );
    sendOrRetry( params, arr, waitSecs, phone, port, gameID );
    return buflen;
}

static void
doSend( LaunchParams* params, const XP_U8* buf,
        XP_U16 buflen, const XP_UCHAR* phone, XP_U16 port,
        XP_U32 gameID )
{
    XP_LOGF( "%s(len=%d)", __func__, buflen );
    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(params->mpool)
                                                params->vtMgr );
    writeHeader( stream, DATA );
    stream_putU32( stream, gameID );
    stream_putBytes( stream, buf, buflen );

    (void)write_fake_sms( params, stream, phone, port );
    stream_destroy( stream );
}

typedef struct _RetryClosure {
    LaunchParams* params;
    XP_U16 port;
    XP_U32 gameID;
    XP_UCHAR phone[32];
} RetryClosure;

static void
sendOrRetry( LaunchParams* params, SMSMsgArray* arr, XP_U16 waitSecs,
             const XP_UCHAR* phone, XP_U16 port, XP_U32 gameID )
{
    if ( !!arr ) {
        for ( XP_U16 ii = 0; ii < arr->nMsgs; ++ii ) {
            SMSMsg* msg = &arr->msgs[ii];
            doSend( params, msg->data, msg->len, phone, port, gameID );
        }

        LinSMSData* storage = getStorage( params );
        smsproto_freeMsgArray( storage->protoState, arr );
    } else if ( waitSecs > 0 ) {
        RetryClosure* closure = (RetryClosure*)XP_CALLOC( params->mpool,
                                                          sizeof(*closure) );
        closure->params = params;
        XP_STRCAT( closure->phone, phone );
        closure->port = port;
        closure->gameID = gameID;
        g_timeout_add_seconds( 5, retrySend, closure );
    }
}

static gboolean
retrySend( gpointer data )
{
    RetryClosure* closure = (RetryClosure*)data;
    LinSMSData* storage = getStorage( closure->params );
    XP_U16 waitSecs;
    SMSMsgArray* arr = smsproto_prepOutbound( storage->protoState, NULL, 0,
                                              closure->phone, XP_TRUE, &waitSecs );
    sendOrRetry( closure->params, arr, waitSecs, closure->phone,
                 closure->port, closure->gameID );
    XP_FREEP( closure->params->mpool, &closure );
    return FALSE;
}

static gint
check_for_files( gpointer data )
{
    check_for_files_once( data );
    return TRUE;
}

static gint
check_for_files_once( gpointer data )
{
    LOG_FUNC();
    LaunchParams* params = (LaunchParams*)data;
    LinSMSData* storage = getStorage( params );

    for ( ; ; ) {
        lock_queue( storage );

        char oldestFile[256] = { '\0' };
        struct timespec oldestModTime;

        GDir* dir = g_dir_open( storage->myQueue, 0, NULL );
        XP_LOGF( "%s: opening queue %s", __func__, storage->myQueue );
        for ( ; ; ) {
            const gchar* name = g_dir_read_name( dir );
            if ( NULL == name ) {
                break;
            } else if ( 0 == strcmp( name, LOCK_FILE ) ) {
                continue;
            }

            /* We want the oldest file first. Timestamp comes from stat(). */
            struct stat statbuf;
            char fullPath[500];
            snprintf( fullPath, sizeof(fullPath), "%s/%s", storage->myQueue, name );
            int err = stat( fullPath, &statbuf );
            if ( err != 0 ) {
                XP_LOGF( "%s(); %d from stat (error: %s)", __func__,
                         err, strerror(errno) );
                XP_ASSERT( 0 );
            } else {
                XP_Bool replace = !oldestFile[0]; /* always replace empty/unset :-) */
                if ( !replace ) {
                    if (statbuf.st_mtim.tv_sec == oldestModTime.tv_sec ) {
                        replace = statbuf.st_mtim.tv_nsec < oldestModTime.tv_nsec;
                    } else {
                        replace = statbuf.st_mtim.tv_sec < oldestModTime.tv_sec;
                    }
                }

                if ( replace ) {
                    oldestModTime = statbuf.st_mtim;
                    if ( !!oldestFile[0] ) {
                        XP_LOGF( "%s(): replacing %s with older %s", __func__, oldestFile, name );
                    }
                    snprintf( oldestFile, sizeof(oldestFile), "%s", name );
                }
            }
        }
        g_dir_close( dir );

        uint8_t buf[256];
        CommsAddrRec fromAddr = {0};
        XP_S16 nRead = -1;
        if ( !!oldestFile[0] ) {
            nRead = decodeAndDelete( storage, oldestFile, buf,
                                     sizeof(buf), &fromAddr );
        } else {
            XP_LOGF( "%s: no file found", __func__ );
        }

        unlock_queue( storage );

        if ( 0 >= nRead ) {
            break;
        }

        parseAndDispatch( params, buf, nRead, &fromAddr );
    }
    return FALSE;
} /* check_for_files_once */

void
linux_sms_cleanup( LaunchParams* params )
{
    LinSMSData* storage = getStorage( params );
    smsproto_free( storage->protoState );
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
