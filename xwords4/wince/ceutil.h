/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2002 - 2009 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _CEUTIL_H_
#define _CEUTIL_H_

#include "stdafx.h" 
#include "cemain.h"

void ceSetDlgItemText( HWND hDlg, XP_U16 id, const XP_UCHAR* buf );
void ceGetDlgItemText( HWND hDlg, XP_U16 id, XP_UCHAR* buf, XP_U16* bLen );

void ceSetDlgItemNum( HWND hDlg, XP_U16 id, XP_S32 num );
XP_S32 ceGetDlgItemNum( HWND hDlg, XP_U16 id );

void ceSetDlgItemFileName( HWND hDlg, XP_U16 id, XP_UCHAR* buf );

void positionDlg( HWND hDlg );

void ce_selectAndShow( HWND hDlg, XP_U16 resID, XP_U16 index );

void ceShowOrHide( HWND hDlg, XP_U16 resID, XP_Bool visible );
void ceEnOrDisable( HWND hDlg, XP_U16 resID, XP_Bool visible );

XP_Bool ceGetChecked( HWND hDlg, XP_U16 resID );
void ceSetChecked( HWND hDlg, XP_U16 resID, XP_Bool check );

void ceCenterCtl( HWND hDlg, XP_U16 resID );
void ceCheckMenus( const CEAppGlobals* globals );

void ceGetItemRect( HWND hDlg, XP_U16 resID, RECT* rect );
void ceMoveItem( HWND hDlg, XP_U16 resID, XP_S16 byX, XP_S16 byY );

int ceMessageBoxChar( CEAppGlobals* globals, const XP_UCHAR* str, 
                      const wchar_t* title, XP_U16 buttons, 
                      SkipAlertBits sab );
XP_Bool ceCurDictIsUTF8( CEAppGlobals* globals );

XP_U16 ceDistanceBetween( HWND hDlg, XP_U16 resID1, XP_U16 resID2 );

typedef enum {
    PREFS_FILE_PATH_L
    ,DEFAULT_DIR_PATH_L
    ,DEFAULT_GAME_PATH
    ,PROGFILES_PATH
} CePathType;
XP_U16 ceGetPath( CEAppGlobals* globals, CePathType typ, 
                  void* buf, XP_U16 bufLen );

/* set vHeight to 0 to turn off scrolling */
typedef enum { DLG_STATE_NONE = 0
               , DLG_STATE_TRAPBACK = 1 
               , DLG_STATE_OKONLY = 2 
               , DLG_STATE_DONEONLY = 4
} DlgStateTask;
typedef struct CeDlgHdr {
    CEAppGlobals* globals;
    HWND hDlg;
    /* set these two if will be calling ceDlgMoveBelow */
    XP_U16* resIDs;
    XP_U16 nResIDs;

    /* Below this line is private to ceutil.c */
    DlgStateTask doWhat;
    XP_U16 nPage;
    XP_U16 prevY;
    XP_U16 nResIDsUsed;
    XP_Bool penDown;
} CeDlgHdr;
void ceDlgSetup( CeDlgHdr* dlgHdr, HWND hDlg, DlgStateTask doWhat );
void ceDlgComboShowHide( CeDlgHdr* dlgHdr, XP_U16 baseId );
XP_Bool ceDoDlgHandle( CeDlgHdr* dlgHdr, UINT message, WPARAM wParam, LPARAM lParam);

void ceDlgMoveBelow( CeDlgHdr* dlgHdr, XP_U16 resID, XP_S16 distance );

/* Are we drawing things in landscape mode? */
XP_Bool ceIsLandscape( CEAppGlobals* globals );

void ceSetLeftSoftkey( CEAppGlobals* globals, XP_U16 id );
XP_Bool ceGetExeDir( wchar_t* buf, XP_U16 bufLen );

#if defined _WIN32_WCE
XP_Bool ceSizeIfFullscreen( CEAppGlobals* globals, HWND hWnd );
#else
# define ceSizeIfFullscreen( globals, hWnd ) XP_FALSE
#endif

#ifdef OVERRIDE_BACKKEY
void trapBackspaceKey( HWND hDlg );
#else
# define trapBackspaceKey( hDlg )
#endif


#endif
