/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2005-2009 by Eric House (xwords@eehouse.org).  All rights
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

/* This should eventually be contributed to cegcc.  Everything it in comes
 * from MSDN
 */
#ifndef _CONNMGR_H_
#define _CONNMGR_H_

#include <winuser.h>

/* from http://hi.baidu.com/53_54/blog/index/1 */

#ifndef S_OK
# define S_OK 0
#endif
#ifndef CONNMGR_PARAM_GUIDDESTNET
# define CONNMGR_PARAM_GUIDDESTNET 0x1
#endif
#ifndef CONNMGR_FLAG_PROXY_HTTP
# define CONNMGR_FLAG_PROXY_HTTP 0x1
#endif
#ifndef CONNMGR_PRIORITY_USERINTERACTIVE
# define CONNMGR_PRIORITY_USERINTERACTIVE 0x08000
#endif
#ifndef CONNMGR_STATUS_CONNECTED
# define CONNMGR_STATUS_CONNECTED 0x10
#endif


#if 0
/* from http://www.dotnet247.com/247reference/msgs/53/267570.aspx */
public const uint CONNMGR_STATUS_UNKNOWN = 0x00;                    // Unknown status
public const uint CONNMGR_STATUS_CONNECTED = 0x10;                    // Connection is up
public const uint CONNMGR_STATUS_DISCONNECTED = 0x20;                // Connection is disconnected
public const uint CONNMGR_STATUS_CONNECTIONFAILED = 0x21;            // Connection failed and cannot not be reestablished
public const uint CONNMGR_STATUS_CONNECTIONCANCELED = 0x22;            // User aborted connection
public const uint CONNMGR_STATUS_CONNECTIONDISABLED = 0x23;            // Connection is ready to connect but disabled
public const uint CONNMGR_STATUS_NOPATHTODESTINATION = 0x24;        // No path could be found to destination
public const uint CONNMGR_STATUS_WAITINGFORPATH = 0x25;                // Waiting for a path to the destination
public const uint CONNMGR_STATUS_WAITINGFORPHONE = 0x26;            // Voice call is in progress
public const uint CONNMGR_STATUS_WAITINGCONNECTION = 0x40;            // Attempting to connect
public const uint CONNMGR_STATUS_WAITINGFORRESOURCE = 0x41;            // Resource is in use by another connection
public const uint CONNMGR_STATUS_WAITINGFORNETWORK = 0x42;            // No path could be found to destination
public const uint CONNMGR_STATUS_WAITINGDISCONNECTION = 0x80;        // Connection is being brought down
public const uint CONNMGR_STATUS_WAITINGCONNECTIONABORT = 0x81;        // Aborting connection attempt
#endif

/*
A bunch of these constants are from:
http://74.125.155.132/search?q=cache:fZImjloe6fsJ:download.microsoft.com/download/6/B/C/6BC8CDA8-9035-43AC-AFB8-B5B7DC550949/OCS%25202007%2520R2%2520Technical%2520Reference%2520for%2520Clients.doc+define+CONNMGR_STATUS_PHONEOFF&cd=1&hl=en&ct=clnk&gl=us&client=iceweasel-a
*/

/* this URL provides the following definitions 
http://technet.microsoft.com/en-us/library/dd637175%28office.13%29.aspx

CONNMGR_STATUS_CONNECTIONLINKFAILED(0x2A)
CONNMGR_STATUS_CONNECTIONFAILED(0x21)
CONNMGR_STATUS_EXCLUSIVECONFLICT(0x28)
CONNMGR_STATUS_NOPATHTODESTINATION(0x24)
CONNMGR_STATUS_CONNECTIONCANCELED(0x22)
CONNMGR_STATUS_WAITINGFORPATH(0x25)
CONNMGR_STATUS_PHONEOFF(0x27)
CONNMGR_STATUS_WAITINGFORPHONE(0x26)
CONNMGR_STATUS_AUTHENTICATIONFAILED(0x2B)
CONNMGR_STATUS_NOPATHWITHPROPERTY(0x2C)
CONNMGR_STATUS_UNKNOWN( 0X00) 
*/
#ifndef CONNMGR_STATUS_UNKNOWN
# define CONNMGR_STATUS_UNKNOWN 0x00
#endif
#ifndef CONNMGR_STATUS_CONNECTED
# define CONNMGR_STATUS_CONNECTED 0x10
#endif
// CONNMGR_STATUS_SUSPENDED
#ifndef CONNMGR_STATUS_DISCONNECTED
# define CONNMGR_STATUS_DISCONNECTED 0x20
#endif
#ifndef CONNMGR_STATUS_CONNECTIONFAILED
# define CONNMGR_STATUS_CONNECTIONFAILED 0x21
#endif
#ifndef CONNMGR_STATUS_CONNECTIONCANCELED
# define CONNMGR_STATUS_CONNECTIONCANCELED 0x22
#endif
#ifndef CONNMGR_STATUS_CONNECTIONDISABLED
# define CONNMGR_STATUS_CONNECTIONDISABLED 0x23
#endif
#ifndef CONNMGR_STATUS_NOPATHTODESTINATION
# define CONNMGR_STATUS_NOPATHTODESTINATION 0x24
#endif
#ifndef CONNMGR_STATUS_WAITINGFORPATH
# define CONNMGR_STATUS_WAITINGFORPATH 0x25
#endif
#ifndef CONNMGR_STATUS_WAITINGFORPHONE
# define CONNMGR_STATUS_WAITINGFORPHONE 0x26
#endif
#ifndef CONNMGR_STATUS_PHONEOFF
# define CONNMGR_STATUS_PHONEOFF 0x27
#endif
#ifndef CONNMGR_STATUS_EXCLUSIVECONFLICT
# define CONNMGR_STATUS_EXCLUSIVECONFLICT 0x28
#endif
// CONNMGR_STATUS_NORESOURCES
#ifndef CONNMGR_STATUS_CONNECTIONLINKFAILED
# define CONNMGR_STATUS_CONNECTIONLINKFAILED 0x2A
#endif
#ifndef CONNMGR_STATUS_AUTHENTICATIONFAILED
# define CONNMGR_STATUS_AUTHENTICATIONFAILED 0x2B
#endif
#ifndef CONNMGR_STATUS_WAITINGCONNECTION
# define CONNMGR_STATUS_WAITINGCONNECTION 0x40
#endif
#ifndef CONNMGR_STATUS_WAITINGFORRESOURCE
# define CONNMGR_STATUS_WAITINGFORRESOURCE 0x41
#endif
#ifndef CONNMGR_STATUS_WAITINGFORNETWORK
# define CONNMGR_STATUS_WAITINGFORNETWORK 0x42
#endif
#ifndef CONNMGR_STATUS_WAITINGDISCONNECTION
# define CONNMGR_STATUS_WAITINGDISCONNECTION 0x80
#endif
#ifndef CONNMGR_STATUS_WAITINGCONNECTIONABORT
# define CONNMGR_STATUS_WAITINGCONNECTIONABORT 0x81
#endif
#ifndef CONNMGR_STATUS_NOPATHWITHPROPERTY
# define CONNMGR_STATUS_NOPATHWITHPROPERTY 0x2C
#endif

typedef struct _CONNMGR_CONNECTIONINFO {
    DWORD cbSize;
    DWORD dwParams;
    DWORD dwFlags;
    DWORD dwPriority;
    BOOL bExclusive;
    BOOL bDisabled;
    GUID guidDestNet;
    HWND hWnd;
    UINT uMsg;
    LPARAM lParam;
    ULONG ulMaxCost;
    ULONG ulMinRcvBw;
    ULONG ulMaxConnLatency;
} CONNMGR_CONNECTIONINFO;

/* procptr typedefs since we're loading by name */
typedef HRESULT (*ConnMgrRegisterForStatusChangeNotificationProc)( BOOL, HWND );

/* Returns S_OK if successful or returns an error code if the function call
   failed.*/
typedef HRESULT (*ConnMgrEstablishConnectionProc)( CONNMGR_CONNECTIONINFO*,
                                                   HANDLE* );
typedef HRESULT (*ConnMgrConnectionStatusProc)( HANDLE, DWORD* ); 

typedef HRESULT (*ConnMgrMapURLProc)( LPTSTR, GUID*, DWORD* );

typedef HRESULT (*ConnMgrReleaseConnectionProc)( HANDLE, LONG );

typedef struct _CMProcs {
    ConnMgrEstablishConnectionProc ConnMgrEstablishConnection;
    ConnMgrConnectionStatusProc ConnMgrConnectionStatus;
    ConnMgrMapURLProc ConnMgrMapURL;
    ConnMgrReleaseConnectionProc ConnMgrReleaseConnection;
} CMProcs;

#endif
