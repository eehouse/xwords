/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

#ifndef _XWRELAY_PRIV_H_
#define _XWRELAY_PRIV_H_

#include <time.h>
#include "lstnrmgr.h"
#include "xwrelay.h"

typedef unsigned char HostID;   /* see HOST_ID_SERVER */

typedef enum {
    XW_LOGERROR
    ,XW_LOGINFO
    ,XW_LOGVERBOSE0
    ,XW_LOGVERBOSE1
} XW_LogLevel;

void logf( XW_LogLevel level, const char* format, ... );

void denyConnection( int socket, XWREASON err );
bool send_with_length_unsafe( int socket, unsigned char* buf, int bufLen );

time_t uptime(void);

void blockSignals( void );      /* call from all but main thread */

int GetNSpawns(void);

int make_socket( unsigned long addr, unsigned short port );

int read_packet( int sock, unsigned char* buf, int buflen );

const char* cmdToStr( XWRELAY_Cmd cmd );

extern class ListenerMgr g_listeners;

#endif
