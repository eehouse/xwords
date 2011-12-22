/* -*- compile-command: "make rq"; -*- */

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
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <linux/un.h>

#include "xwrelay.h"

#ifndef DEFAULT_PORT
# define DEFAULT_PORT 10998
#endif
#ifndef DEFAULT_HOST
# define DEFAULT_HOST "localhost"
#endif

#define MAX_CONN_NAMES 128

static const char* g_host = DEFAULT_HOST;
static int g_port = DEFAULT_PORT;

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
    fprintf( stderr, "\t[-p <port>]     # (default %d) \\\n", DEFAULT_PORT );
    fprintf( stderr, "\t[-a <host>]     # (default: %s) \\\n", DEFAULT_HOST );
    fprintf( stderr, "\t[-r]            # list open public rooms \\\n" );
    fprintf( stderr, "\t[-f <connName:devid>    # fetch message > stdout or file \\\n" );
    fprintf( stderr, "\t[-l <n>]        # language for rooms "
             "(1=English default) \\\n" );
    fprintf( stderr, "\t[-n <n>]        # number of players (2 default) \\\n" );
    fprintf( stderr, "\t[-b <path>]*    # nbs socket to be used for -f \\\n" );
    fprintf( stderr, "\t[-o <path>]*    # file to be used for -f "
             "(- = stdout, the default) \\\n" );
    fprintf( stderr, "\t[-m <connName/devid>    # list msg count \\\n" );
    fprintf( stderr, "\t[-d <connName/devid/seed>    # delete game \\\n" );
    exit( 1 );
}

#define NUM_PER_LINE 8
void
log_hex( const unsigned char* memp, int len, const char* tag )
{
    const char* hex = "0123456789ABCDEF";
    int i, j;
    int offset = 0;

    while ( offset < len ) {
        char buf[128];
        unsigned char vals[NUM_PER_LINE*3];
        unsigned char* valsp = vals;
        unsigned char chars[NUM_PER_LINE+1];
        unsigned char* charsp = chars;
        int oldOffset = offset;

        for ( i = 0; i < NUM_PER_LINE && offset < len; ++i ) {
            unsigned char byte = memp[offset];
            for ( j = 0; j < 2; ++j ) {
                *valsp++ = hex[(byte & 0xF0) >> 4];
                byte <<= 4;
            }
            *valsp++ = ':';

            byte = memp[offset];
            if ( (byte >= 'A' && byte <= 'Z')
                 || (byte >= 'a' && byte <= 'z')
                 || (byte >= '0' && byte <= '9') ) {
                /* keep it */
            } else {
                byte = '.';
            }
            *charsp++ = byte;
            ++offset;
        }
        *(valsp-1) = '\0';      /* -1 to overwrite ':' */
        *charsp = '\0';

        if ( (NULL == tag) || (strlen(tag) + sizeof(vals) >= sizeof(buf)) ) {
            tag = "<tag>";
        }
        snprintf( buf, sizeof(buf), "%s[%d]: %s %s", tag, oldOffset, 
                  vals, chars );
        fprintf( stderr, "%s\n", buf );
    }
}

int
read_packet( int sock, unsigned char* buf, int buflen )
{
    int result = -1;
    ssize_t nread;
    unsigned short msgLen;
    nread = recv( sock, &msgLen, sizeof(msgLen), MSG_WAITALL );
    if ( nread == sizeof(msgLen) ) {
        msgLen = ntohs( msgLen );
        if ( msgLen <= buflen ) {
            nread = recv( sock, buf, msgLen, MSG_WAITALL );
            if ( nread == msgLen ) {
                result = nread;
            }
        }
    }
    return result;
}

static void
do_rooms( int sockfd, int lang, int nPlayers )
{
    unsigned char msg[] = { 0,      /* protocol */
                            PRX_PUB_ROOMS,
                            lang,
                            nPlayers };
    unsigned short len = htons( sizeof(msg) );
    write( sockfd, &len, sizeof(len) );
    write( sockfd, msg, sizeof(msg) );

    fprintf( stderr, "Waiting for response....\n" );
    ssize_t nRead = recv( sockfd, &len, 
                          sizeof(len), MSG_WAITALL );
    assert( nRead == sizeof(len) );
    len = ntohs( len );
    char reply[len];
    nRead = recv( sockfd, reply, len, MSG_WAITALL );
    assert( nRead == len );

    char* ptr = reply;
    unsigned short nRooms;
    memcpy( &nRooms, ptr, sizeof(nRooms) );
    ptr += sizeof( nRooms );
    nRooms = ntohs( nRooms );
    
    int ii;
    char* saveptr;
    for ( ii = 0; ii < nRooms; ++ii ) {
        char* str = strtok_r( ptr, "\n", &saveptr );
        fprintf( stdout, "%s\n", str );
        ptr = NULL;
    }
}

static void
write_connnames( int sockfd, char cmd, 
                 const char** connNames, int nConnNames )
{
    unsigned short len, netlen;
    int ii;
    for ( len = 0, ii = 0; ii < nConnNames; ++ii ) {
        len += 1 + strlen( connNames[ii] );
    }

    unsigned char hdr[] = { 0, cmd };
    unsigned short netNConnNames = htons( nConnNames );
    netlen = sizeof(hdr) + sizeof( netNConnNames ) + len;
    netlen = htons( netlen );
    write( sockfd, &netlen, sizeof(netlen) );
    write( sockfd, &hdr, sizeof(hdr) );
    write( sockfd, &netNConnNames, sizeof(netNConnNames) );
    for ( len = 0, ii = 0; ii < nConnNames; ++ii ) {
        write( sockfd, connNames[ii], strlen(connNames[ii]) );
        write( sockfd, "\n", 1 );
    }
}

static void
do_msgs( int sockfd, const char** connNames, int nConnNames )
{
    write_connnames( sockfd, PRX_HAS_MSGS, connNames, nConnNames );

    fprintf( stderr, "Waiting for response....\n" );
    unsigned char reply[1024];
    int nRead = read_packet( sockfd, reply, sizeof(reply) );
    if ( nRead > 2 ) {
        const unsigned char* bufp = reply;
        const unsigned char* const end = bufp + nRead;
        unsigned short count;
        memcpy( &count, bufp, sizeof( count ) );
        bufp += sizeof( count );
        count = ntohs( count );
        assert( count == nConnNames );
        fprintf( stderr, "got count: %d\n", count );

        int ii;
        for ( ii = 0; ii < nConnNames && bufp < end; ++ii ) {
            memcpy( &count, bufp, sizeof( count ) );
            bufp += sizeof( count );
            count = ntohs( count );
            fprintf( stdout, "%s -- %d\n", connNames[ii], count );
        }
    }
} /* do_msgs */

static int
connect_socket( void )
{
    int sockfd = socket( AF_INET, SOCK_STREAM, 0 );
    struct sockaddr_in to_sock;
    memset( &to_sock, 0, sizeof(to_sock) );
    to_sock.sin_family = AF_INET;
    to_sock.sin_port = htons( g_port );

    struct hostent* hostip;
    hostip = gethostbyname( g_host );
    memcpy( &(to_sock.sin_addr.s_addr), hostip->h_addr_list[0],  
            sizeof(hostip->h_addr_list[0] ) );
    
    if ( 0 != connect( sockfd, (const struct sockaddr*)&to_sock, 
                       sizeof(to_sock) ) ) {
        fprintf( stderr, "connect failed: %d (%s)\n", errno, strerror(errno) );
        exit( 1 );
    }
    return sockfd;
}

/* This comes in already formatted (see relay_sendNoConn_curses).  So all we
   need to do is open a socket and send it to the relay.  Later we can
   coalesce, though that should happen in the (linux) client. */
static void
send_msg( const unsigned char* buf, int len )
{
    // log_hex( buf, len, __func__ );
    int sock = connect_socket();
    assert( 0 <= sock );

    unsigned char hdr[] = { 0, PRX_PUT_MSGS };
    unsigned short netlen = htons( sizeof(hdr) + len );
    ssize_t nwritten = write( sock, &netlen, sizeof(netlen) );
    assert( nwritten == sizeof(netlen) );
    nwritten = write( sock, hdr, sizeof(hdr) );
    assert( nwritten == sizeof(hdr) );
    nwritten = write( sock, buf, len );
    assert( nwritten == len );
    close( sock );
}

static void
do_fetch( int sockfd, const char** connNames, int nConnNames, 
          const char** pipes, int nPipes, const char* nbs )
{
    write_connnames( sockfd, PRX_GET_MSGS, connNames, nConnNames );

    unsigned char reply[1024];
    int nRead = read_packet( sockfd, reply, sizeof(reply) );
    if ( nRead > 2 ) {
        const unsigned char* bufp = reply;
        const unsigned char* const end = bufp + nRead;

        unsigned short count;
        memcpy( &count, bufp, sizeof( count ) );
        bufp += sizeof( count );
        count = ntohs( count );
        assert( count <= nConnNames );

        /* Now we have an array of <countPerDev> <len><len bytes> pairs.  Just
           write em as long as it makes sense.  countPerDev makes no sense as
           other than 1 unless the UI is changed so I don't have to write to
           STDOUT -- e.g. by passing in named pipes to correspond to each
           deviceid provided */

        for ( int ii = 0; ii < count && bufp < end; ++ii ) {
            int fd = STDOUT_FILENO;
            int nbsfd = -1;
            unsigned short countPerDev;
            memcpy( &countPerDev, bufp, sizeof( countPerDev ) );
            bufp += sizeof( countPerDev );
            countPerDev = ntohs( countPerDev );

            if ( ii < nPipes && 0 != strcmp( pipes[ii], "-" ) ) {
                fd = open( pipes[ii], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR );
                if ( fd < 0 ) {
                    fprintf( stderr, "open(%s) failed: %s\n", pipes[ii], 
                             strerror(errno) );
                    exit( 1 );
                }
            } else if ( NULL != nbs ) {
                nbsfd = socket( AF_UNIX, SOCK_STREAM, 0 );
                assert( 0 <= nbsfd );
                struct sockaddr_un addr;
                addr.sun_family = AF_UNIX;
                strcpy( addr.sun_path, nbs );

                for ( ; ; ) {
                    if ( 0 == connect( nbsfd, (struct sockaddr*)&addr, 
                                       sizeof(addr) ) ) {
                        break;
                    }
                    switch( errno ) {
                    case ECONNREFUSED:
                    case ENOENT:
                        usleep( 250000 );
                        break;
                    default:
                        fprintf( stderr, "%s: connect()=>%d (%s)\n", __func__,
                                 errno, strerror(errno) );
                        assert( false );
                    }
                }
                fd = nbsfd;
            }

            unsigned short len;
            ssize_t nwritten;
            while ( bufp < end && countPerDev-- > 0 ) {
                memcpy( &len, bufp, sizeof( len ) );
                len = ntohs( len ) + sizeof( len );
                if ( bufp + len > end ) {
                    break;
                }
                nwritten = write( fd, bufp, len );
                assert( nwritten == len );
                bufp += len;
            }
            assert( bufp == end );
            len = 0;
            nwritten = write( fd, &len, sizeof(len) );
            assert( nwritten == sizeof(len) );

            int ii;
            for ( ii = 0; -1 != nbsfd; ++ii ) {
                short len;
                ssize_t nRead = read( nbsfd, &len, sizeof(len) );
                if ( sizeof(len) != nRead ) {
                    break;
                } else if ( 0 == len ) {
                    // fprintf( stderr, "%s: read 0; done\n", __func__ );
                    break;
                }
                len = ntohs( len );
                // fprintf( stderr, "%s: read size of %d\n", __func__, len );
                unsigned char buf[len];
                if ( len != read( nbsfd, buf, len ) ) {
                    break;
                }
                // fprintf( stderr, "%s: read %d bytes\n", __func__, len );
                send_msg( buf, len );
            }
            // fprintf( stderr, "%s: DONE reading from pipe\n", __func__ );

            if ( fd != STDOUT_FILENO ) {
                close( fd );
            }
        }

        if ( bufp != end ) {
            fprintf( stderr, "error: message not internally as expected\n" );
        }
    }
} /* do_fetch */

static void
do_deletes( int sockfd, const char** connNames, int nConnNames )
{
    int ii;

    char buf[4096];
    size_t nused = 0;
    for ( ii = 0; ii < nConnNames; ++ii ) {
        char tmp[128];
        assert( strlen(connNames[ii]) < sizeof(tmp)-1 );
        strcpy( tmp, connNames[ii] );
        char* seedp = strrchr( tmp, '/' );
        assert( !!seedp );
        *(seedp++) = '\0';       /* skip */
        unsigned int seed;
        sscanf( seedp, "%X", &seed );
        short netshort = htons( (short)seed );
        memcpy( &buf[nused], &netshort, sizeof(netshort) );
        nused += sizeof(netshort);
        nused += sprintf( &buf[nused], "%s", tmp );
        buf[nused++] = '\n';
    }
    assert( nused < sizeof(buf) );

    unsigned char hdr[] = { 0, PRX_DEVICE_GONE };

    /* total len */
    short num = htons( sizeof(hdr) + 2 + nused );
    write( sockfd, &num, sizeof(num) );
    
    /* header */
    write( sockfd, hdr, sizeof(hdr) );

    /* count */
    num = htons( nConnNames );
    write( sockfd, &num, sizeof(num) );

    /* buffer */
    write( sockfd, buf, nused );

    unsigned char reply[2];
    int nRead = read_packet( sockfd, reply, sizeof(reply) );
    if ( nRead != 0 ) {
        fprintf( stderr, "%s: nRead=%d; errno=%d (%s)\n", __func__,
                 nRead, errno, strerror(errno) );
    }
} /* do_deletes */

int
main( int argc, char * const argv[] )
{
    // fprintf( stderr, "rq called\n" );
    int lang = 1;
    int nPlayers = 2;
    int doWhat = 0;
    const int doRooms = 1;
    const int doMgs = 2;
    const int doDeletes = 4;
    const int doFetch = 8;
    char const* connNames[MAX_CONN_NAMES];
    int nConnNames = 0;
    const char* pipes[MAX_CONN_NAMES];
    int nPipes = 0;
    const char* nbs = NULL;

    for ( ; ; ) {
        int opt = getopt( argc, argv, "a:b:d:f:p:rl:n:m:o:" );
        if ( opt < 0 ) {
            break;
        }
        switch ( opt ) {
        case 'a':
            g_host = optarg;
            break;
        case 'b':
            nbs = optarg;
            break;
        case 'd':
            assert( nConnNames < MAX_CONN_NAMES - 1 );
            connNames[nConnNames++] = optarg;
            doWhat |= doDeletes;
            break;
        case 'f':
            connNames[nConnNames++] = optarg;
            doWhat |= doFetch;
            break;
        case 'l':
            lang = atoi(optarg);
            break;
        case 'm':
            assert( nConnNames < MAX_CONN_NAMES - 1 );
            connNames[nConnNames++] = optarg;
            doWhat |= doMgs;
            break;
        case 'n':
            nPlayers = atoi(optarg);
            break;
        case 'o':
            assert( nPipes < MAX_CONN_NAMES - 1 );
            pipes[nPipes++] = optarg;
            break;
        case 'p':
            g_port = atoi(optarg);
            break;
        case 'r':
            doWhat |= doRooms;
            break;
        default:
            usage( argv[0] );
            break;
        }
    }

    // fprintf( stderr, "got port %d, host %s\n", g_port, g_host );

    int sockfd = connect_socket();

    switch ( doWhat ) {
    case 0:
        fprintf( stderr, "no command given\n" );
        usage( argv[0] );
        break;
    case doRooms:
        do_rooms( sockfd, lang, nPlayers );
        break;
    case doMgs:
        do_msgs( sockfd, connNames, nConnNames );
        break;
    case doDeletes:
        do_deletes( sockfd, connNames, nConnNames );
        break;
    case doFetch:
        do_fetch( sockfd, connNames, nConnNames, pipes, nPipes, nbs );
        break;
    default:
        fprintf( stderr, "conflicting options given\n" );
        usage( argv[0] );
    }

    close( sockfd );

    return 0;
}
