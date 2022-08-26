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

static gboolean retrySend( gpointer data );
static void sendOrRetry( LaunchParams* params, SMSMsgArray* arr, SMS_CMD cmd,
                         XP_U16 waitSecs, const XP_UCHAR* phone, XP_U16 port,
                         XP_U32 gameID, const XP_UCHAR* msgNo );
static gint check_for_files( gpointer data );
static gint check_for_files_once( gpointer data );

static void
formatQueuePath( const XP_UCHAR* phone, XP_U16 port, XP_UCHAR* path,
                 XP_U16 pathlen )
{
    XP_ASSERT( 0 != port );
    snprintf( path, pathlen, "%s/%s_%d", SMS_DIR, phone, port );
}

static LinSMSData* getStorage( LaunchParams* params );

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
write_fake_sms( LaunchParams* params, const void* buf, XP_U16 buflen,
                const XP_UCHAR* msgNo, const XP_UCHAR* phone, XP_U16 port )
{
    XP_S16 nSent;
    XP_U16 pct = XP_RANDOM() % 100;
    XP_Bool skipWrite = pct < params->smsSendFailPct;

    if ( skipWrite ) {
        nSent = buflen;
        XP_LOGFF( "dropping sms msg of len %d to phone %s", nSent, phone );
    } else {
        LinSMSData* storage = getStorage( params );
        XP_LOGFF( "(phone=%s, port=%d, len=%d)", phone, port, buflen );

        XP_ASSERT( !!storage );

        lock_queue( storage );

#ifdef DEBUG
        gchar* str64 = g_base64_encode( buf, buflen );
#endif

        char path[256];
        formatQueuePath( phone, port, path, sizeof(path) );

        /* Random-number-based name is fine, as we pick based on age. */
        g_mkdir_with_parents( path, 0777 ); /* just in case */
        int len = strlen( path );
        int rint = makeRandomInt();
        if ( !!msgNo ) {
            snprintf( &path[len], sizeof(path)-len, "/%s_%u", msgNo, rint );
        } else {
            snprintf( &path[len], sizeof(path)-len, "/%u", rint );
        }
        XP_UCHAR sms[buflen*2];     /* more like (buflen*4/3) */
        XP_U16 smslen = sizeof(sms);
        binToSms( sms, &smslen, buf, buflen );
        XP_ASSERT( smslen == strlen(sms) );
        XP_LOGFF( "writing msg to %s", path );

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
    if ( 2 != matched ) {
        XP_LOGFF( "ERROR: found %d matches instead of 2", matched );
    } else {
        gchar* eol = strstr( contents, "\n" );
        *eol = '\0';
        XP_ASSERT( !*eol );
        ++eol;         /* skip NULL */
        *strstr(eol, "\n" ) = '\0';

        XP_U16 inlen = strlen(eol);      /* skip \n */
        XP_LOGFF( "decoding message from file %s", name );
        XP_U8 out[inlen];
        XP_U16 outlen = sizeof(out);
        XP_Bool valid = smsToBin( out, &outlen, eol, inlen );

        if ( valid && outlen <= buflen ) {
            XP_MEMCPY( buf, out, outlen );
            nRead = outlen;
            addr_setType( addr, COMMS_CONN_SMS );
            XP_STRNCPY( addr->u.sms.phone, phone, sizeof(addr->u.sms.phone) );
            XP_LOGFF( " message came from phone: %s, port: %d", phone, port );
            addr->u.sms.port = port;
        }
    }

    g_free( contents );

    LOG_RETURNF( "%d", nRead );
    return nRead;
} /* decodeAndDelete */

static void
nliFromData( LaunchParams* params, const SMSMsgLoc* msg, NetLaunchInfo* nliOut )
{
    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(params->mpool)
                                                params->vtMgr );
    stream_putBytes( stream, msg->data, msg->len );
#ifdef DEBUG
    XP_Bool success =
#endif
        nli_makeFromStream( nliOut, stream );
    XP_ASSERT( success );
    stream_destroy( stream, NULL_XWE );
}

static void
parseAndDispatch( LaunchParams* params, uint8_t* buf, int len, 
                  CommsAddrRec* addr )
{
    LinSMSData* storage = getStorage( params );
    const XP_UCHAR* fromPhone = addr->u.sms.phone;
    SMSMsgArray* arr =
        smsproto_prepInbound( storage->protoState, NULL_XWE, fromPhone,
                              storage->myPort, buf, len );
    if ( NULL != arr ) {
        XP_ASSERT( arr->format == FORMAT_LOC );
        for ( XP_U16 ii = 0; ii < arr->nMsgs; ++ii ) {
            SMSMsgLoc* msg = &arr->u.msgsLoc[ii];
            switch ( msg->cmd ) {
            case DATA:
                (*storage->procs->msgReceived)( storage->procClosure, addr,
                                                msg->gameID,
                                                msg->data, msg->len );
                break;
            case INVITE: {
                NetLaunchInfo nli = {0};
                nliFromData( params, msg, &nli );
                (*storage->procs->inviteReceived)( storage->procClosure,
                                                   &nli, addr );
            }
                break;
            default:
                XP_ASSERT(0);   /* implement me!! */
                break;
            }
        }
        smsproto_freeMsgArray( storage->protoState, arr );
    }
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
    storage->protoState = smsproto_init( MPPARM(params->mpool) NULL_XWE, params->dutil );

    formatQueuePath( myPhone, myPort, storage->myQueue, sizeof(storage->myQueue) );
    XP_LOGFF( " my queue: %s", storage->myQueue );
    storage->myPort = params->connInfo.sms.port;

    (void)g_mkdir_with_parents( storage->myQueue, 0777 );

    /* Look for preexisting or new files every half second. Easier than
       inotify, especially when you add the need to handle files written while
       not running. */
    (void)g_idle_add( check_for_files_once, params );
    (void)g_timeout_add( 500, check_for_files, params );
} /* linux_sms_init */

void
linux_sms_invite( LaunchParams* params, const NetLaunchInfo* nli,
                  const gchar* toPhone, int toPort )
{
    XP_LOGFF( "(toPhone: %s, toPort: %d)", toPhone, toPort );
    LinSMSData* storage = getStorage( params );

    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(params->mpool)
                                                params->vtMgr );
    nli_saveToStream( nli, stream );
    const XP_U8* ptr = stream_getPtr( stream );
    XP_U16 len = stream_getSize( stream );

    XP_U16 waitSecs;
    const XP_Bool forceOld = XP_TRUE; /* Send NOW in case test app kills us */
    SMSMsgArray* arr
        = smsproto_prepOutbound( storage->protoState, NULL_XWE, INVITE, nli->gameID, ptr,
                                 len, toPhone, toPort, forceOld, &waitSecs );
    XP_ASSERT( !!arr || !forceOld );
    sendOrRetry( params, arr, INVITE, waitSecs, toPhone, toPort,
                 nli->gameID, "invite" );
    stream_destroy( stream, NULL_XWE );
}

XP_S16
linux_sms_send( LaunchParams* params, const XP_U8* buf,
                XP_U16 buflen, const XP_UCHAR* msgNo, const XP_UCHAR* phone,
                XP_U16 port, XP_U32 gameID )
{
    XP_S16 nSent = -1;
    LinSMSData* storage = getStorage( params );
    if ( !!storage->protoState ) {
        XP_U16 waitSecs;
        SMSMsgArray* arr = smsproto_prepOutbound( storage->protoState, NULL_XWE, DATA, gameID,
                                                  buf, buflen, phone, port,
                                                  XP_TRUE, &waitSecs );
        sendOrRetry( params, arr, DATA, waitSecs, phone, port, gameID, msgNo );
        nSent = buflen;
    } else {
        XP_LOGFF( "dropping: sms not configured" );
    }
    return nSent;
}

typedef struct _RetryClosure {
    LaunchParams* params;
    SMS_CMD cmd;
    XP_U16 port;
    XP_U32 gameID;
    XP_UCHAR msgNo[32];
    XP_UCHAR phone[32];
} RetryClosure;

static void
sendOrRetry( LaunchParams* params, SMSMsgArray* arr, SMS_CMD cmd,
             XP_U16 waitSecs, const XP_UCHAR* phone, XP_U16 port,
             XP_U32 gameID, const XP_UCHAR* msgNo )
{
    if ( !!arr ) {
        for ( XP_U16 ii = 0; ii < arr->nMsgs; ++ii ) {
            const SMSMsgNet* msg = &arr->u.msgsNet[ii];
            // doSend( params, msg->data, msg->len, phone, port, gameID );
            (void)write_fake_sms( params, msg->data, msg->len, msgNo, 
                                  phone, port );
        }

        LinSMSData* storage = getStorage( params );
        smsproto_freeMsgArray( storage->protoState, arr );
    } else if ( waitSecs > 0 ) {
        RetryClosure* closure = (RetryClosure*)XP_CALLOC( params->mpool,
                                                          sizeof(*closure) );
        closure->params = params;
        XP_STRCAT( closure->phone, phone );
        XP_STRCAT( closure->msgNo, msgNo );
        closure->port = port;
        closure->gameID = gameID;
        closure->cmd = cmd;
        g_timeout_add_seconds( 5, retrySend, closure );
    }
}

static gboolean
retrySend( gpointer data )
{
    RetryClosure* closure = (RetryClosure*)data;
    LinSMSData* storage = getStorage( closure->params );
    XP_U16 waitSecs;
    SMSMsgArray* arr = smsproto_prepOutbound( storage->protoState, NULL_XWE,
                                              closure->cmd,
                                              closure->gameID, NULL, 0,
                                              closure->phone, closure->port,
                                              XP_TRUE, &waitSecs );
    sendOrRetry( closure->params, arr, closure->cmd, waitSecs, closure->phone,
                 closure->port, closure->gameID, closure->msgNo );
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
    // LOG_FUNC();
    LaunchParams* params = (LaunchParams*)data;
    LinSMSData* storage = getStorage( params );

    for ( ; ; ) {
        lock_queue( storage );

        char oldestFile[256] = { '\0' };
        struct timespec oldestModTime;

        GDir* dir = g_dir_open( storage->myQueue, 0, NULL );
        // XP_LOGF( "%s: opening queue %s", __func__, storage->myQueue );
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
                XP_LOGF( "%d from stat (error: %s)", err, strerror(errno) );
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
                        XP_LOGFF( "replacing %s with older %s", oldestFile, name );
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
            // XP_LOGF( "%s: no file found", __func__ );
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

#endif /* XWFEATURE_SMS */
