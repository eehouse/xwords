/* -*- fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2006 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#if defined _WIN32_WCE && defined USE_RAW_MINGW

#include <windows.h>
#include <commctrl.h>

/* #include "xptypes.h" */
/* #include "debhacks.h" */

/* These should eventually be replaced by implementations moved into
 * mingw or the Debian pocketpc-sdk 
 */

/* These belong in mingw headers */
wchar_t* wcscat( wchar_t *, const wchar_t * );
wchar_t *wcscpy( wchar_t*,const wchar_t* );

wchar_t*
lstrcatW( wchar_t *dest, const wchar_t* src )
{
    return wcscat( dest, src );
}

wchar_t* 
lstrcpyW( wchar_t* dest, const wchar_t* src )
{
    return wcscpy( dest, src );
}

int
DialogBoxParamW( HINSTANCE hinst, LPCWSTR name, HWND hwnd, 
                 DLGPROC proc, LPARAM lparam )
{
    HRSRC resstr = FindResource( hinst, name, RT_DIALOG );
    HGLOBAL lr = LoadResource( hinst, resstr );
    return DialogBoxIndirectParamW(hinst, lr, hwnd, proc, lparam );
}

BOOL
GetTextExtentPoint32W( HDC hdc, LPCWSTR str, int i, LPSIZE siz )
{
    return GetTextExtentExPointW(hdc, str, i, 0, NULL, NULL, siz );
}

/*
see
http://msdn.microsoft.com/library/default.asp?url=/library/en-us/wcehardware5/html/wce50lrfCeLogImportTable.asp
for how to implement SetEvent and ResetEvent in terms of EventModify

SetEvent(h)->pEventModify(h, EVENT_SET)

ResetEvent(h)->pEventModify(h, EVENT_RESET)

PulseEvent(h)->pEventModify(h, EVENT_PULSE) 

http://www.opennetcf.org/forums/topic.asp?TOPIC_ID=257 defines the constants,
which are verified if this all works. :-)
*/

enum {
    EVENT_PULSE     = 1,
    EVENT_RESET     = 2,
    EVENT_SET       = 3
};

BOOL
debhack_SetEvent(HANDLE h)
{
    return EventModify(h, EVENT_SET);
}

BOOL
debhack_ResetEvent(HANDLE h)
{
    return EventModify(h, EVENT_RESET);
}

DWORD
GetCurrentThreadId(void)
{
    return 0;
}

#endif /* #ifdef _WIN32_WCE */
