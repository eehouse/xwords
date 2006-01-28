
#ifdef USE_DEB_HACKS

#include "xptypes.h"
#include "debhacks.h"

wchar_t*
DH(lstrcat)(wchar_t *s1, const wchar_t* s2)
{
    return wcscat( s1, s2 );
}

wchar_t* 
DH(lstrcpy)(wchar_t* dest, const wchar_t* src)
{
    return wcscpy( dest, src );
}

int
DH(DialogBoxParam)( HINSTANCE hinst, LPCWSTR name, HWND hwnd, 
                        DLGPROC proc, LPARAM lparam )
{
    HRSRC resstr = FindResource( hinst, name, RT_DIALOG );
    HGLOBAL lr = LoadResource( hinst, resstr );
    return DialogBoxIndirectParamW(hinst, lr, hwnd, proc, lparam );
}

BOOL
DH(GetTextExtentPoint32)( HDC hdc, LPCWSTR str, int i, LPSIZE siz )
{
    return GetTextExtentExPointW(hdc, str, i, 0, NULL, NULL, siz );
}

BOOL
DH(SetEvent)(HANDLE h)
{
    return FALSE;
}

BOOL
DH(ResetEvent)(HANDLE h)
{
    return FALSE;
}

DWORD
DH(GetCurrentThreadId)(void)
{
    return 0;
}

#endif
