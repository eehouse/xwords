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
#include "http.h"

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
send_meta( FILE* fil, const CrefMgrInfo* info ) 
{
    FILE* css;
    RelayConfigs* cfg = RelayConfigs::GetConfigs();
    char pathbuf[256] = { '\0' };

    fprintf( fil, "<head>" );

    if ( !!cfg ) {
        int refreshSecs;
        if (  cfg->GetValueFor( "WWW_REFRESH_SECS", &refreshSecs ) ) {
            fprintf( fil, "<meta http-equiv=\"refresh\" content=\"%d\" />",
                     refreshSecs );
        }

        (void)cfg->GetValueFor( "WWW_CSS_PATH", pathbuf, sizeof(pathbuf) );
    }

    if ( pathbuf[0] ) {
        css = fopen( pathbuf, "r" );
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
    }

	fprintf( fil, "<title>relay: %d/%d</title>\n", info->m_nCrefsAll, 
             info->m_nCrefsCurrent );
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
printCrefs( FILE* fil, const CrefMgrInfo* info, bool isLocal )
{
    fprintf( fil, "<div class=\"header\">Connections</div>" );
    fprintf( fil, "<table><tr>" );
    fprintf( fil,
             "<th>Room</th>"
             "<th>ConnName</th>"
             "<th>ID</th>"
             "<th>For</th>"
             "<th>Bytes</th>"
             "<th>Expect</th>"
             "<th>Here</th>"
             "<th>State</th>"
             "<th>Host IDs</th>"
             "<th>Seeds</th>"
             );
    if ( isLocal ) {
        fprintf( fil, "<th>Host IPs</th>" );
    }
    fprintf( fil, "</tr>\n" );

    time_t curTime = uptime();
    unsigned int ii;
    for ( ii = 0; ii < info->m_crefInfo.size(); ++ii ) {
        const CrefInfo* crefInfo = &info->m_crefInfo[ii];

        char conntime[32];
        format_uptime( curTime - crefInfo->m_startTime, conntime, 
                       sizeof(conntime) );

        fprintf( fil, "<tr>"
                 "<td>%s</td>"  /* name */
                 "<td>%s</td>"  /* conn name */
                 "<td>%d</td>"  /* cookie id */
                 "<td>%s</td>"  /* conntime */
                 "<td>%d</td>"  /* total sent */
                 "<td>%d</td>"  /* players */
                 "<td>%d</td>"  /* players here */
                 "<td>%s</td>"  /* State */
                 "<td>%s</td>"  /* Hosts */
                 "<td>%s</td>"  /* Seeds */
                 ,
                 crefInfo->m_cookie.c_str(),
                 crefInfo->m_connName.c_str(),
                 crefInfo->m_cookieID,
                 conntime,
                 crefInfo->m_totalSent,
                 crefInfo->m_nPlayersSought, crefInfo->m_nPlayersHere, 
                 stateString( crefInfo->m_curState ),
                 crefInfo->m_hostsIds.c_str(),
                 crefInfo->m_hostSeeds.c_str()
                 );
        
        if ( isLocal ) {
            fprintf( fil, "<td>%s</td>",   /* Ip addrs */
                     crefInfo->m_hostIps.c_str() );
        }
        fprintf( fil, "</tr>\n" );
    }
    fprintf( fil, "</table>\n" );
} /* printCrefs */

static void
printStats( FILE* fil, const CrefMgrInfo* info, bool isLocal )
{
    char uptime1[64];
    char uptime2[64];
    format_uptime( uptime(), uptime1, sizeof(uptime1) );
    format_uptime( time(NULL) - info->m_startTimeSpawn, uptime2, 
                   sizeof(uptime2) );
    fprintf( fil, "<div class=\"header\">Stats</div>" );
    fprintf( fil, "<table>" );
    fprintf( fil, "<tr>"
             "<th>Ports</th><th>Uptime</th><th>Spawns</th><th>Spawn Utime</th>"
             "<th>Games played</th><th>Games in play</th></tr>" );
    fprintf( fil, "<tr><td>%s</td><td>%s</td><td>%d</td>"
             "<td>%s</td><td>%d</td><td>%d</td></tr>\n", 
             info->m_ports, uptime1, GetNSpawns(), uptime2, info->m_nCrefsAll,
             info->m_nCrefsCurrent );
    fprintf( fil, "</table>" );
}

static void*
http_thread_main( void* arg )
{
    HttpState* state = (HttpState*)arg;

    sockaddr newaddr;
    socklen_t siz = sizeof(newaddr);
    int sock = accept( state->ctrl_sock, &newaddr, &siz );

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
        struct sockaddr_in name;
        socklen_t namelen = sizeof(name);

        bool isLocal = 0 == getpeername( sock, (struct sockaddr*)&name, &namelen );
        if ( isLocal ) {
            in_addr_t s_addr = name.sin_addr.s_addr;
            isLocal = 0x7f000001 == htonl(s_addr);
        }

        MutexLock ml(&state->m_dataMutex);

        /* We'll handle as many http connections as folks want to throw at us,
           but will only fetch from the crefmgr infrequently, caching the data
           for next time.  Only one thread at a time gets to read from it,
           ensuring we don't nuke it from under somebody.  */
        time_t curTime = time( NULL );

        if ( state->m_nextFetch < curTime ) {
            delete state->m_crefInfo;
            state->m_crefInfo = NULL;
        }
        if ( state->m_crefInfo == NULL ) {
            state->m_crefInfo = new CrefMgrInfo();
            CRefMgr::Get()->GetStats( *state->m_crefInfo );
            state->m_nextFetch = curTime + state->m_sampleInterval;
        }
        const CrefMgrInfo* info = state->m_crefInfo;

        FILE* fil = fdopen( sock, "r+" );
        fseek( fil, 0, SEEK_CUR ); // reverse stream
        
        send_header( fil, "status page" );
        fprintf( fil, "<html>" );
        send_meta( fil, info );
        fprintf( fil, "<body><div class=\"main\">" );

        printStats( fil, info, isLocal );

        printCrefs( fil, info, isLocal );

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
run_http_thread( HttpState* state )
{
    pthread_t thread;
    int result = pthread_create( &thread, NULL, 
                                 http_thread_main, (void*)state );
    if ( 0 == result ) {
        pthread_detach( thread );
    } else {
        logf( XW_LOGERROR, "%s: pthread_create failed", __func__ );
    }
} /* run_http_thread */

#endif /* DO_HTTP */
