/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2010 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <assert.h>

#include "xwrelay.h"

#ifndef DEFAULT_PORT
# define DEFAULT_PORT 10998
#endif
#ifndef DEFAULT_HOST
# define DEFAULT_HOST "localhost"
#endif

/* 
 * Query:
 * list of all public games by language and number of players
 *
 * For debugging (localhost only, maybe)
 * count of all games in-play (cid!=null)
 */

static void
usage( const char * const argv0 )
{
    fprintf( stderr, "usage: %s \\\n", argv0 );
    fprintf( stderr, "\t[-p <port>]     # (default %d)\\\n", DEFAULT_PORT );
    fprintf( stderr, "\t[-a <host>]     # (default: %s)\\\n", DEFAULT_HOST );
    fprintf( stderr, "\t-r              # list open public rooms\\\n" );
    exit( 1 );
}

static void
do_rooms( int sockfd )
{
    unsigned char byt = 0;      /* protocol */
    write( sockfd, &byt, sizeof(byt) );
    byt = PRX_PUBROOMS;
    write( sockfd, &byt, sizeof(byt) );
}

int
main( int argc, char * const argv[] )
{
    int port = DEFAULT_PORT;
    bool doRooms = false;
    const char* host = DEFAULT_HOST;

    for ( ; ; ) {
        int opt = getopt( argc, argv, "a:p:r" );
        if ( opt < 0 ) {
            break;
        }
        switch ( opt ) {
        case 'a':
            host = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 'r':
            doRooms = true;
            break;
        default:
            usage( argv[0] );
            break;
        }
    }

    fprintf( stderr, "got port %d, host %s\n", port, host );


    int sockfd = socket( AF_INET, SOCK_STREAM, 0 );
    struct sockaddr_in to_sock;
    memset( &to_sock, 0, sizeof(to_sock) );
    to_sock.sin_family = AF_INET;
    to_sock.sin_port = htons( port );

    struct hostent* hostip;
    hostip = gethostbyname( host );
    memcpy( &(to_sock.sin_addr.s_addr), hostip->h_addr_list[0],  
            sizeof(hostip->h_addr_list[0] ) );

    if ( 0 != connect( sockfd, (const struct sockaddr*)&to_sock, 
                       sizeof(to_sock) ) ) {
        fprintf( stderr, "connect failed: %d (%s)\n", errno, strerror(errno) );
        exit( 1 );
    }

    if ( doRooms ) {
        do_rooms( sockfd );
    } else {
        usage( argv[0] );
    }

    close( sockfd );

    return 0;
}
