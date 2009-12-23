/* Copyright notice TBD by cegcc team.
 */
#ifndef _CONNMGR_H
#define _CONNMGR_H

#if __GNUC__ >= 3
#pragma GCC system_header
#endif

#include <windows.h>

#ifndef CONNMGR_API_LINKAGE
# ifdef __W32API_USE_DLLIMPORT__
#  define CONNMGR_API_LINKAGE DECLSPEC_IMPORT
# else
#  define CONNMGR_API_LINKAGE
# endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

    /* This guy
       http://www.studio-odyssey.net/content/note/archive01.htm
       defines all the missing ones.   Legit?
    */

#define CONNMGR_PRIORITY_USERINTERACTIVE 0x8000
/* CONNMGR_PRIORITY_USERBACKGROUND /\* unknown *\/ */
/* CONNMGR_PRIORITY_USERIDLE       /\* unknown *\/ */
#define CONNMGR_PRIORITY_HIPRIBKGND 0x0200 /* HighPriorityBackground */
#define CONNMGR_PRIORITY_IDLEBKGND 0x0008  /* LowPriorityBackground */
/* CONNMGR_PRIORITY_EXTERNALINTERACTIVE       /\* unknown *\/ */
/* CONNMGR_PRIORITY_LOWBKGND                  /\* unknown *\/ */

/* dwParams options  */
#define CONNMGR_PARAM_GUIDDESTNET 0x1
#define CONNMGR_PARAM_MAXCOST 0x2
#define CONNMGR_PARAM_MINRCVBW 0x4
#define CONNMGR_PARAM_MAXCONNLATENCY 0x8

/* dwFlags options for proxies */
#define CONNMGR_FLAG_PROXY_HTTP 0x1
#define CONNMGR_FLAG_PROXY_WAP 0x2
#define CONNMGR_FLAG_PROXY_SOCKS4 0x4
#define CONNMGR_FLAG_PROXY_SOCKS5 0x8

/* dwFlags options for control */
/* CONNMGR_FLAG_SUSPEND_AWARE      /\* studio-odyssey doesn't define *\/ */
/* CONNMGR_FLAG_REGISTERED_HOME    /\* unknown *\/ */
/* CONNMGR_FLAG_NO_ERROR_MSGS      /\* unknown *\/ */

/* status constants */
#define CONNMGR_STATUS_UNKNOWN 0x00
#define CONNMGR_STATUS_CONNECTED 0x10
#define CONNMGR_STATUS_SUSPENDED 0x11
#define CONNMGR_STATUS_DISCONNECTED 0x20
#define CONNMGR_STATUS_CONNECTIONFAILED 0x21
#define CONNMGR_STATUS_CONNECTIONCANCELED 0x22
#define CONNMGR_STATUS_CONNECTIONDISABLED 0x23
#define CONNMGR_STATUS_NOPATHTODESTINATION 0x24
#define CONNMGR_STATUS_WAITINGFORPATH 0x25
#define CONNMGR_STATUS_WAITINGFORPHONE 0x26
#define CONNMGR_STATUS_PHONEOFF 0x27
#define CONNMGR_STATUS_EXCLUSIVECONFLICT 0x28
#define CONNMGR_STATUS_NORESOURCES 0x29
#define CONNMGR_STATUS_CONNECTIONLINKFAILED 0x2a
#define CONNMGR_STATUS_AUTHENTICATIONFAILED 0x2b
#define CONNMGR_STATUS_NOPATHWITHPROPERTY 0x2c
#define CONNMGR_STATUS_WAITINGCONNECTION 0x40
#define CONNMGR_STATUS_WAITINGFORRESOURCE 0x41
#define CONNMGR_STATUS_WAITINGFORNETWORK 0x42
#define CONNMGR_STATUS_WAITINGDISCONNECTION 0x80
#define CONNMGR_STATUS_WAITINGCONNECTIONABORT 0x81

/* from  */
typedef struct {
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
    ULONG ulMaxConnLatency
} CONNMGR_CONNECTIONINFO;

typedef struct {
    GUID guidDest;
    UINT64 uiStartTime;
    UINT64 uiEndTime;
    UINT64 uiPeriod;
    TCHAR szAppName[MAX_PATH];
    TCHAR szCmdLine[MAX_PATH];
    TCHAR szToken[32];
    BOOL bPiggyback;
} SCHEDULEDCONNECTIONINFO;

#define CONNMGR_MAX_DESC 128
typedef struct {
    GUID guid;
    TCHAR szDescription[CONNMGR_MAX_DESC];
    BOOL fSecure;
} CONNMGR_DESTINATION_INFO;

typedef enum _ConnMgrConRefTypeEnum{
    ConRefType_NAP = 0,
    ConRefType_PROXY
} ConnMgrConRefTypeEnum;

typedef struct _CONNMGR_CONNECTION_IPADDR {
    DWORD cIPAddr;
    SOCKADDR_STORAGE IPAddr[1]
} CONNMGR_CONNECTION_IPADDR;

typedef struct _CONNMGR_CONNECTION_DETAILED_STATUS {
    struct _CONNMGR_CONNECTION_DETAILED_STATUS* pNext;
    DWORD dwVer; 
    DWORD dwParams;
    DWORD dwType; 
    DWORD dwSubtype;
    DWORD dwFlags; 
    DWORD dwSecure;
    GUID guidDestNet;
    GUID guidSourceNet; 
    TCHAR* szDescription;
    TCHAR* szAdapterName;
    DWORD dwConnectionStatus; 
    SYSTEMTIME LastConnectTime;
    DWORD dwSignalQuality; 
    CONNMGR_CONNECTION_IPADDR* pIPAddr;
} CONNMGR_CONNECTION_DETAILED_STATUS;

HANDLE WINAPI ConnMgrApiReadyEvent( void );
HRESULT WINAPI ConnMgrConnectionStatus(HANDLE, DWORD*); 
HRESULT WINAPI ConnMgrEnumDestinations(int,CONNMGR_DESTINATION_INFO*); 
HRESULT WINAPI ConnMgrEstablishConnection( CONNMGR_CONNECTIONINFO*, HANDLE*);
HRESULT WINAPI ConnMgrEstablishConnectionSync( CONNMGR_CONNECTIONINFO*,
                                               HANDLE*,DWORD,DWORD*); 
HRESULT WINAPI ConnMgrMapConRef(ConnMgrConRefTypeEnum,LPCTSTR, GUID*);
HRESULT WINAPI ConnMgrMapURL(LPTSTR,GUID*,DWORD*); 
HRESULT WINAPI ConnMgrProviderMessage(HANDLE,const GUID*,DWORD*,DWORD,DWORD,
                                      PBYTE,ULONG); 
HRESULT WINAPI ConnMgrQueryDetailedStatus( CONNMGR_CONNECTION_DETAILED_STATUS*,
                                           DWORD* );
HRESULT WINAPI ConnMgrRegisterForStatusChangeNotification(BOOL,HWND);
HRESULT WINAPI ConnMgrRegisterScheduledConnection(SCHEDULEDCONNECTIONINFO* );
HRESULT WINAPI ConnMgrRegisterScheduledConnection(SCHEDULEDCONNECTIONINFO*); 
HRESULT WINAPI ConnMgrSetConnectionPriority( HANDLE,DWORD ); 
HRESULT WINAPI ConnMgrUnregisterScheduledConnection(LPCWSTR); 

#ifdef __cplusplus
}
#endif
#endif  /* _CONNMGR_H */
