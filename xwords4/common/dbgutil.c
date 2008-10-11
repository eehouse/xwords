/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
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

#ifdef ENABLE_LOGGING

#include "dbgutil.h"

#define CASESTR(s) case s: return #s

#define FUNC(f) #f

const char* 
XP_Key_2str( XP_Key key )
{
    switch( key ) {
        CASESTR(XP_KEY_NONE);
        CASESTR(XP_CURSOR_KEY_DOWN);
        CASESTR(XP_CURSOR_KEY_ALTDOWN);
        CASESTR(XP_CURSOR_KEY_RIGHT);
        CASESTR(XP_CURSOR_KEY_ALTRIGHT);
        CASESTR(XP_CURSOR_KEY_UP);
        CASESTR(XP_CURSOR_KEY_ALTUP);
        CASESTR(XP_CURSOR_KEY_LEFT);
        CASESTR(XP_CURSOR_KEY_ALTLEFT);
        CASESTR(XP_CURSOR_KEY_DEL);
        CASESTR(XP_RAISEFOCUS_KEY);
        CASESTR(XP_RETURN_KEY);
        CASESTR(XP_KEY_LAST );
    default: return FUNC(__func__) " unknown";
    }
}

const char* 
DrawFocusState_2str( DrawFocusState dfs )
{
    switch( dfs ) {
        CASESTR(DFS_NONE);
        CASESTR(DFS_TOP);
        CASESTR(DFS_DIVED);
    default: return FUNC(__func__) " unknown";
    }
}

const char* 
BoardObjectType_2str( BoardObjectType obj )
{
    switch( obj ) {
        CASESTR(OBJ_NONE);
        CASESTR(OBJ_BOARD);
        CASESTR(OBJ_SCORE);
        CASESTR(OBJ_TRAY);
    default: return FUNC(__func__) " unknown";
    }
}

#undef CASESTR

#endif /* ENABLE_LOGGING */

