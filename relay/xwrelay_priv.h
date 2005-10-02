/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

#ifndef _XWRELAY_PRIV_H_
#define _XWRELAY_PRIV_H_

#include <time.h>

typedef unsigned char HostID;

typedef enum {
    XW_LOGINFO
    ,XW_LOGERROR
} XW_LogLevel;

void logf( XW_LogLevel level, const char* format, ... );

void killSocket( int socket, char* why );

int send_with_length_unsafe( int socket, unsigned char* buf, int bufLen );

time_t now();

#endif
