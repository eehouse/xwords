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

#include "linuxsms.h"
#include "linuxutl.h"
#include "strutils.h"
#include "stats.h"
#include "device.h"
#include "linuxmain.h"

#define SMS_DIR "xw_sms"
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

struct LinSMSData {
    XP_UCHAR myQueue[256];
    XP_U16 myPort;
    FILE* lock;
    gchar* dataDir;   /* where fake messages are stored (defaults to /tmp) */

    const gchar* myPhone;
};

static gint check_for_files( gpointer data );
static gint check_for_files_once( gpointer data );

static void
formatQueuePath( const XP_UCHAR* phone, gchar* dir, XP_U16 port,
                 XP_UCHAR* path, XP_U16 pathlen )
{
    XP_ASSERT( 0 != port );
    snprintf( path, pathlen, "%s/%s/%s_%d", dir, SMS_DIR, phone, port );
}

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
                const XP_UCHAR* phone, XP_U16 port )
{
    XP_S16 nSent = -1;
    XP_U16 pct = XP_RANDOM() % 100;
    XP_Bool skipWrite = pct < params->smsSendFailPct;

    if ( skipWrite ) {
        nSent = buflen;
        XP_LOGFF( "dropping sms msg of len %d to phone %s", nSent, phone );
    } else {
        LinSMSData* storage = params->smsStorage; //  getStorage( params );
        if ( !!storage ) {
            XP_LOGFF( "(phone=%s, port=%d, len=%d)", phone, port, buflen );
            lock_queue( storage );

#ifdef DEBUG
            gchar* str64 = g_base64_encode( buf, buflen );
#endif

            char path[256];
            formatQueuePath( phone, storage->dataDir, port, path, sizeof(path) );

            /* Random-number-based name is fine, as we pick based on age. */
            g_mkdir_with_parents( path, 0777 ); /* just in case */
            int len = strlen( path );
            int rint = makeRandomInt();
            snprintf( &path[len], sizeof(path)-len, "/%u", rint );
            XP_UCHAR sms[buflen*2];     /* more like (buflen*4/3) */
            XP_U16 smslen = sizeof(sms);
            binToB64( sms, &smslen, buf, buflen );
            XP_ASSERT( smslen == strlen(sms) );
            XP_LOGFF( "writing msg to %s", path );

#ifdef DEBUG
            XP_ASSERT( !strcmp( str64, sms ) );
            g_free( str64 );

            XP_U8 testout[buflen];
            XP_U16 lenout = sizeof( testout );
            XP_ASSERT( b64ToBin( testout, &lenout, sms, smslen ) );
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
    }
    return nSent;
} /* write_fake_sms */

void
linux_sms_enqueue( LaunchParams* params, const XP_U8* buf,
                   XP_U16 len, const XP_UCHAR* phone, XP_U16 port )
{
    XP_LOGFF( "(phone: %s; len: %d)", phone, len );
    write_fake_sms( params, buf, len, phone, port );
}

static XP_S16
decodeAndDelete( const gchar* path, XP_U8* buf, XP_U16 buflen,
                 CommsAddrRec* addr )
{
    LOG_FUNC();
    XP_S16 nRead = -1;

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
    if ( 2 != matched ) {
        XP_LOGFF( "ERROR: found %d matches instead of 2", matched );
    } else {
        gchar* eol = strstr( contents, "\n" );
        *eol = '\0';
        XP_ASSERT( !*eol );
        ++eol;         /* skip NULL */
        *strstr(eol, "\n" ) = '\0';

        XP_U16 inlen = strlen(eol);      /* skip \n */
        XP_LOGFF( "decoding message from file %s", path );
        XP_U8 out[inlen];
        XP_U16 outlen = sizeof(out);
        XP_Bool valid = b64ToBin( out, &outlen, eol, inlen );

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

void
linux_sms_init( LaunchParams* params, const gchar* dataDir,
                const gchar* myPhone, XP_U16 myPort )
{
    // XP_LOGFF( "(dataDir: %s; myPhone: %s)", dataDir, myPhone );
    XP_ASSERT( !!myPhone );
    XP_ASSERT( !params->smsStorage );
    LinSMSData* storage = XP_CALLOC( params->mpool, sizeof(*params->smsStorage) ); 
    params->smsStorage = storage;
    storage->myPhone = g_strdup(myPhone);
    storage->myPort = myPort;
    storage->dataDir = g_strdup( dataDir );

    formatQueuePath( myPhone, storage->dataDir, myPort,
                     storage->myQueue, sizeof(storage->myQueue) );
    XP_LOGFF( "my queue: %s", storage->myQueue );
    storage->myPort = params->connInfo.sms.port;

    (void)g_mkdir_with_parents( storage->myQueue, 0777 );

    /* Look for preexisting or new files every half second. Easier than
       inotify, especially when you add the need to handle files written while
       not running. */
    (void)g_idle_add( check_for_files_once, params );
    (void)g_timeout_add( 500, check_for_files, params );
} /* linux_sms_init */

static XP_Bool
pickFile( LinSMSData* storage, XP_Bool pickAtRandom,
          XP_UCHAR outpath[], XP_U16 outlen )
{
    XP_Bool found = XP_FALSE;
    struct timespec oldestModTime;
    const char* foundPath = NULL;
    char fullPath[500];

    GDir* dir = g_dir_open( storage->myQueue, 0, NULL );

    /* First, count files */
    int count = 0;
    for ( ; ; ) {
        const gchar* name = g_dir_read_name( dir );
        if ( NULL == name ) {
            break;
        } else if ( 0 == strcmp( name, LOCK_FILE ) ) {
            continue;
        } else {
            ++count;
        }
    }
    found = 1 <= count;

    if ( found ) {
        g_dir_rewind( dir );

        int targetIndx = XP_RANDOM() % count;
        XP_UCHAR oldestFile[512] = {};
        int cur = 0;
        for ( ; ; ) {
            const gchar* name = g_dir_read_name( dir );
            if ( NULL == name ) {
                break;
            } else if ( 0 == strcmp( name, LOCK_FILE ) ) {
                continue;
            }

            snprintf( fullPath, sizeof(fullPath), "%s/%s", storage->myQueue, name );
            if ( pickAtRandom ) {
                if ( cur++ == targetIndx ) {
                    XP_LOGFF( "picking file %d of %d", targetIndx, count );
                    foundPath = fullPath;
                    break;
                }
            } else {
                struct stat statbuf;
                int err = stat( fullPath, &statbuf );
                if ( err != 0 ) {
                    XP_LOGFF( "%d from stat (error: %s)", err, strerror(errno) );
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
                        foundPath = oldestFile;
                    }
                }
            }
        }

        XP_SNPRINTF( outpath, outlen, "%s", foundPath );
    }
    g_dir_close( dir );
    return found;
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
    LinSMSData* storage = params->smsStorage;

    while ( !!storage ) {
        lock_queue( storage );

        uint8_t buf[256] = {};
        CommsAddrRec fromAddr = {};
        XP_S16 nRead = -1;
        XP_UCHAR path[1024];
        if ( pickFile( storage, XP_TRUE, path, VSIZE(path) ) ) {
            nRead = decodeAndDelete( path, buf, sizeof(buf), &fromAddr );
        } else {
            // XP_LOGFF( "no file found" );
        }

        unlock_queue( storage );

        if ( 0 >= nRead ) {
            break;
        }

        dvc_parseSMSPacket( params->dutil, NULL_XWE, &fromAddr, buf, nRead );
    }
    return FALSE;
} /* check_for_files_once */

void
linux_sms_cleanup( LaunchParams* params )
{
    LinSMSData* storage = params->smsStorage;
    if ( !!storage ) {
        XP_FREEP( params->mpool, &params->smsStorage );
    }
}

#endif /* XWFEATURE_SMS */
