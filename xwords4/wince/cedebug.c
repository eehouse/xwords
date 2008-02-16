/* -*- fill-column: 77; c-basic-offset: 4; compile-command: "make TARGET_OS=wince DEBUG=TRUE" -*- */
/* 
 * Copyright 2008 by Eric House (xwords@eehouse.org).   All rights reserved.
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

#include "cedebug.h"

#define CASE_STR(c)  case c: str = #c; break

const char* 
messageToStr( UINT message )
{
    const char* str;
    switch( message ) {
        CASE_STR( WM_NCACTIVATE );
        CASE_STR( WM_QUERYNEWPALETTE );
#ifdef _WIN32_WCE
        CASE_STR( WM_IME_NOTIFY );
        CASE_STR( WM_IME_SETCONTEXT );
#endif
        CASE_STR( WM_WINDOWPOSCHANGED );
        CASE_STR( WM_MOVE );
        CASE_STR( WM_SIZE );
        CASE_STR( WM_ACTIVATE );
        CASE_STR( WM_SETTINGCHANGE );
        CASE_STR( WM_VSCROLL );
        CASE_STR( WM_COMMAND );
        CASE_STR( WM_PAINT );
        CASE_STR( WM_LBUTTONDOWN );
        CASE_STR( WM_MOUSEMOVE );
        CASE_STR( WM_LBUTTONUP );
        CASE_STR( WM_KEYDOWN );
        CASE_STR( WM_KEYUP );
        CASE_STR( WM_CHAR );
        CASE_STR( WM_TIMER );
        CASE_STR( WM_DESTROY );
        CASE_STR( XWWM_TIME_RQST );
        CASE_STR( XWWM_PACKET_ARRIVED );
    default:
        str = "<unknown>";
    }
    return str;
} /* messageToStr */

#undef CASE_STR
