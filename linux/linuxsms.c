/* -*-mode: C; compile-command: "make -j MEMDEBUG=TRUE";-*- */ 
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

struct LinSMSData {
    XP_UCHAR myPhone[MAX_PHONE_LEN+1];
    XP_UCHAR myQueue[256];
    FILE* lock;
    int wd, fd;                 /* for inotify */
    void* storage;
    XP_U16 count;
    XP_U16 port;
    XP_Bool amServer;
};

static void
makeQueuePath( const XP_UCHAR* phone, XP_U16 port,
               XP_UCHAR* path, XP_U16 pathlen )
{
    snprintf( path, pathlen, "%s/%s_%d", SMS_DIR, phone, port );
}

static void
lock_queue( LinSMSData* data )
{
    char lock[256];
    snprintf( lock, sizeof(lock), "%s/%s", data->myQueue, LOCK_FILE );
    FILE* fp = fopen( lock, "w" );
    XP_ASSERT( NULL == data->lock );
    data->lock = fp;
}

static void
unlock_queue( LinSMSData* data )
{
    XP_ASSERT( NULL != data->lock );
    fclose( data->lock );
    data->lock = NULL;
}

void
linux_sms_init( CommonGlobals* globals, const CommsAddrRec* addr )
{
    LinSMSData* data = globals->smsData;
    if ( !data ) {
        data = XP_MALLOC( globals->params->util->mpool, sizeof(*data) );
        XP_ASSERT( !!data );
        XP_MEMSET( data, 0, sizeof(*data) );
        globals->smsData = data;

        data->amServer = comms_getIsServer( globals->game.comms );

        if ( data->amServer ) {
            XP_STRNCPY( data->myPhone, addr->u.sms.phone, 
                        sizeof(data->myPhone) );
        } else {
            snprintf( data->myPhone, sizeof(data->myPhone), "%.6d", getpid() );
        }

        makeQueuePath( data->myPhone, addr->u.sms.port,
                       data->myQueue, sizeof(data->myQueue) );
        data->port = addr->u.sms.port;

        (void)g_mkdir_with_parents( data->myQueue, 0777 );

        int fd = inotify_init();
        data->fd = fd;
        data->wd = inotify_add_watch( fd, data->myQueue, IN_MODIFY );
        (*globals->socketChanged)( globals, -1, fd, &data->storage );
    }
} /* linux_sms_init */

void
linux_sms_close( CommonGlobals* globals )
{
    LinSMSData* data = globals->smsData;
    if ( !!data ) {
        XP_FREE( globals->params->util->mpool, data );
        globals->smsData = NULL;
    }
} /* linux_sms_close */

XP_S16
linux_sms_send( CommonGlobals* globals,
                const XP_U8* buf, XP_U16 buflen, 
                const XP_UCHAR* phone, XP_U16 port )
{
    XP_S16 nSent = -1;
    LinSMSData* data = globals->smsData;
    XP_ASSERT( !!data );
    char path[256];

    lock_queue( data );

#ifdef DEBUG
    gchar* str64 = g_base64_encode( buf, buflen );
#endif

    makeQueuePath( phone, port, path, sizeof(path) );
    g_mkdir_with_parents( path, 0777 ); /* just in case */
    int len = strlen( path );
    snprintf( &path[len], sizeof(path)-len, "/%d", ++data->count );

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
    (void)fprintf( fp, "from: %s\n", data->myPhone );
    (void)fprintf( fp, "%s\n", sms );
    fclose( fp );
    sync();

    unlock_queue( data );

    nSent = buflen;

    return nSent;
} /* linux_sms_send */

static XP_S16
decodeAndDelete( LinSMSData* data, const gchar* name, 
                 XP_U8* buf, XP_U16 buflen, CommsAddrRec* addr )
{
    XP_S16 nRead = -1;
    char path[256];
    snprintf( path, sizeof(path), "%s/%s", data->myQueue, name );

    gchar* contents;
    gsize length;
#ifdef DEBUG
    gboolean success = 
#endif
        g_file_get_contents( path, &contents, &length, NULL );
    XP_ASSERT( success );
    unlink( path );

    if ( 0 == strncmp( "from: ", contents, 6 ) ) {
        gchar* eol = strstr( contents, "\n" );
        *eol = '\0';
        XP_STRNCPY( addr->u.sms.phone, &contents[6], sizeof(addr->u.sms.phone) );
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
            addr->u.sms.port = data->port;
        }
    }

    g_free( contents );

    return nRead;
} /* decodeAndDelete */

XP_S16
linux_sms_receive( CommonGlobals* globals, int sock, 
                   XP_U8* buf, XP_U16 buflen, CommsAddrRec* addr )
{
    XP_S16 nRead = -1;
    LinSMSData* data = globals->smsData;

    XP_ASSERT( sock == data->fd );

    lock_queue( data );

    /* read required or we'll just get the event again.  But we don't care
       about the result or the buffer contents. */
    XP_U8 buffer[sizeof(struct inotify_event) + 16];
    if ( 0 > read( sock, buffer, sizeof(buffer) ) ) {
    }
    char shortest[256] = { '\0' };
    GDir* dir = g_dir_open( data->myQueue, 0, NULL );
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

    if ( !!shortest[0] ) {
        nRead = decodeAndDelete( data, shortest, buf, buflen, addr );
    }

    unlock_queue( data );

    return nRead;
} /* linux_sms_receive */

#endif /* XWFEATURE_SMS */
