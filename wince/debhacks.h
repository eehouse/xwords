

#ifndef _DEBHACKS_H_
#define _DEBHACKS_H_

#ifdef USE_DEB_HACKS

#define MS(func) M$_##func
#define DH(func) debhack_##func


typedef struct DH(WIN32_FIND_DATA) {
	DWORD dwFileAttributes;
	FILETIME dh_ftCreationTime;
	FILETIME dh_ftLastAccessTime;
	FILETIME dh_ftLastWriteTime;
	DWORD dh_nFileSizeHigh;
	DWORD dh_nFileSizeLow;
	DWORD dh_dwReserved1;
	WCHAR cFileName[MAX_PATH];
} DH(WIN32_FIND_DATA);

wchar_t* DH(lstrcat)(wchar_t *s1, const wchar_t* s2);
wchar_t* DH(lstrcpy)(wchar_t* dest, const wchar_t* src);
int DH(DialogBoxParam)( HINSTANCE hinst, LPCWSTR str, HWND hwnd, 
                            DLGPROC proc, LPARAM lparam );
BOOL DH(GetTextExtentPoint32)( HDC,LPCWSTR,int,LPSIZE);
DWORD DH(GetCurrentThreadId)(void);
BOOL DH(SetEvent)(HANDLE);
BOOL DH(ResetEvent)(HANDLE);


#else

#define MS(func) func
#define DH(func) func

#endif

#endif
