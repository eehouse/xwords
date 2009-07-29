/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2009 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifdef DO_HTTP

#include <stdio.h>
#include <unistd.h>
#include <netdb.h>		/* gethostbyname */
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <pthread.h>
#include <assert.h>
#include <sys/select.h>
#include <stdarg.h>
#include <sys/time.h>

#include "ctrl.h"
#include "cref.h"
#include "crefmgr.h"
#include "mlock.h"
#include "xwrelay_priv.h"
#include "configs.h"
#include "lstnrmgr.h"

/*
 * http://www.jbox.dk/sanos/webserver.htm has code for a trivial web server.  Good example.
 */

static void
send_header( FILE* fil, const char* title )
{
    fprintf( fil, "HTTP/1.0 %d %s\r\n", 200, title );
    fprintf( fil, "Server: xwrelay\r\n" );
    fprintf( fil, "Content-Type: text/html\r\n" );
    fprintf( fil, "Connection: close\r\n");

    fprintf( fil, "\r\n");
}

static void
send_meta( FILE* fil ) 
{
    FILE* css;
    RelayConfigs* cfg = RelayConfigs::GetConfigs();

    fprintf( fil, "<head>" );

    if ( !!cfg ) {
        int refreshSecs;
        if (  cfg->GetValueFor( "WWW_REFRESH_SECS", &refreshSecs ) ) {
            fprintf( fil, "<meta http-equiv=\"refresh\" content=\"%d\" />",
                     refreshSecs );
        }
    }

    css = fopen( "./xwrelay.css", "r" );
    if ( NULL != css ) {
        for ( ; ; ) {
            char buf[256];
            size_t nread = fread( buf, 1, sizeof(buf), css );
            if ( nread <= 0 ) {
                break;
            }
            (void) fwrite( buf, 1, nread, fil );
        }
        fclose( css );
    }

    fprintf( fil, "</head>" );
}

static void
printTail( FILE* fil )
{
    char buf[128];

    /* print version and uptime */
    fprintf( fil, "<div class=\"header\">Relay version</div>" );
    format_rev( buf, sizeof(buf) );
    fprintf( fil, "<p>%s</p>", buf );
}

static void
printCrefs( FILE* fil )
{
    fprintf( fil, "<div class=\"header\">Connections</div>" );
    fprintf( fil, "<table><tr>" );
    fprintf( fil,
             "<th>Cookie</th>"
             "<th>ConnName</th>"
             "<th>Cookie ID</th>"
             "<th>Total sent</th>"
             "<th>Players</th>"
             "<th>Players Here</th>"
             "<th>State</th>"
             "<th>Secs conn&apos;d</th>"
             "<th>Host IDs</th>"
             "<th>Host IPs</th>"
             "\n"
             );
    fprintf( fil, "</tr>\n" );

    CRefMgr* cmgr = CRefMgr::Get();
    CookieMapIterator iter = cmgr->GetCookieIterator();
    CookieID id;
    time_t curTime = uptime();
    for ( id = iter.Next(); id != 0; id = iter.Next() ) {
        string hosts, addrs;
        SafeCref scr( id, true );
        if ( scr.IsValid() ) {
            scr.GetHostsConnected( &hosts, &addrs );

            fprintf( fil, "<tr>"
                     "<td>%s</td>"  /* name */
                     "<td>%s</td>"  /* conn name */
                     "<td>%d</td>"  /* cookie id */
                     "<td>%d</td>"  /* total sent */
                     "<td>%d</td>"  /* players */
                     "<td>%d</td>"  /* players here */
                     "<td>%s</td>"  /* State */
                     "<td>%ld</td>"  /* uptime */
                     "<td>%s</td>"   /* Hosts */
                     "<td>%s</td>"   /* Ip addrs */
                     "</tr>\n",
                     scr.Cookie(), scr.ConnName(), scr.GetCookieID(),
                     scr.GetTotalSent(), scr.GetPlayersTotal(),
                     scr.GetPlayersHere(), scr.StateString(), 
                     curTime - scr.GetStartTime(),
                     hosts.c_str(), addrs.c_str()
                     );
        }
    }
    fprintf( fil, "</table>\n" );
}

static void
printStats( FILE* fil )
{
    CRefMgr* cmgr = CRefMgr::Get();
    int nGames = cmgr->GetNumGamesSeen();
    int siz = cmgr->GetSize();
    char uptime[64];
    format_uptime( uptime, sizeof(uptime) );
    fprintf( fil, "<div class=\"header\">Stats</div>" );
    fprintf( fil, "<table>" );
    fprintf( fil, "<tr><th>Games played</th><th>Games in play</th>"
             "<th>Uptime</th><th>Spawns</th></tr>" );
    fprintf( fil, "<tr><td>%d</td><td>%d</td><td>%s</td><td>%d</td></tr>\n", 
             nGames, siz, uptime, GetNSpawns() );
    fprintf( fil, "</table>" );
}

static void*
http_thread_main( void* arg )
{
    int sock = (int)arg;
    char buf[512];
    ssize_t totalRead = 0;

    while ( totalRead <= 4 ) {  /* have we read enough for GET? */
        ssize_t nread = read( sock, buf+totalRead, sizeof(buf)-1-totalRead );
        if ( nread == 0 ) {
            break;
        } else if ( nread > 0 ) {
            buf[nread] = '\0';
        }
        totalRead += nread;
    }

    if ( 0 == strncasecmp( "GET ", buf, 3 ) ) {
        FILE* fil = fdopen( sock, "r+" );
        fseek( fil, 0, SEEK_CUR ); // reverse stream
        
        send_header( fil, "status page" );
        fprintf( fil, "<html>" );
        send_meta( fil );
        fprintf( fil, "<body><div class=\"main\">" );

        printStats( fil );

        printCrefs( fil );

        printTail( fil );

        fprintf( fil, "</div></body></html>" );

        fclose( fil );
    } else {
        logf( XW_LOGINFO, "NOT a GET" );
    }
    close( sock );

    return NULL;
} /* http_thread_main */

void
run_http_thread( int http_sock )
{
    sockaddr newaddr;
    socklen_t siz = sizeof(newaddr);
    int newSock = accept( http_sock, &newaddr, &siz );

    pthread_t thread;
    int result = pthread_create( &thread, NULL, 
                                 http_thread_main, (void*)newSock );
    if ( 0 == result ) {
        pthread_detach( thread );
    } else {
        logf( XW_LOGERROR, "%s: pthread_create failed", __func__ );
    }
} /* run_http_thread */

#endif /* DO_HTTP */
