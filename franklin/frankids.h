// -*-mode: C; fill-column: 78; c-basic-offset: 4; -*-
/* 
 * Copyright 1999-2000 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "comtypes.h"
#include "mempool.h"

#ifndef _FRANKIDS_H_
#define _FRANKIDS_H_

#define MAIN_FLIP_BUTTON_ID 101
#define MAIN_VALUE_BUTTON_ID 102
#define MAIN_HINT_BUTTON_ID 103
#define MAIN_UNDO_BUTTON_ID 104
#define MAIN_COMMIT_BUTTON_ID 105
#define MAIN_TRADE_BUTTON_ID 106
#define MAIN_JUGGLE_BUTTON_ID 107
#define MAIN_HIDE_BUTTON_ID 108


#define MAIN_WINDOW_ID 1000
#define ASK_WINDOW_ID 1001
#define ASKLETTER_WINDOW_ID 1002
#define PLAYERS_WINDOW_ID 1003
#define PASSWORD_WINDOW_ID 1004
#define SAVEDGAMES_WINDOW_ID 1005

#define FILEMENU_BUTTON_ID 1020
#define FILEMENU_NEWGAME 1021
#define FILEMENU_SAVEDGAMES 1022
#define FILEMENU_PREFS 1023
#define FILEMENU_ABOUT 1024

#define GAMEMENU_BUTTON_ID 1025
#define GAMEMENU_TVALUES 1026
#define GAMEMENU_FINALSCORES 1041
#define GAMEMENU_GAMEINFO 1027
#define GAMEMENU_HISTORY 1028

#define MOVEMENU_BUTTON_ID 1029
#define MOVEMENU_HINT 1030
#define MOVEMENU_NEXTHINT 1031
#define MOVEMENU_REVERT 1032
#define MOVEMENU_UNDO 1033
#define MOVEMENU_DONE 1034
#define MOVEMENU_JUGGLE 1035
#define MOVEMENU_TRADE 1036
#define MOVEMENU_HIDETRAY 1037

#ifdef MEM_DEBUG
# define DEBUGMENU_BUTTON_ID 1050
# define DEBUGMENU_HEAPDUMP 1051
#endif

#define FILEMENU_WINDOW_ID 1038
#define GAMEMENU_WINDOW_ID 1039
#define MOVEMENU_WINDOW_ID 1040
#define MENUBAR_WINDOW_ID 1041

const char* PUB_ERICHOUSE = "Eric_House";

const char* EXT_XWORDSDICT = "xwd";
const char* EXT_XWORDSGAMES = "xwg";

#define DICT_OFFSET 0x00000000
#define GAMES_DB_OFFSET 0x00800000
#define FLAGS_CHECK_OFFSET 0x00900000

#define MAX_NAME_LENGTH 31	/* not including null byte */

extern "C" {
    XP_UCHAR* frankCopyStr( MPFORMAL const XP_UCHAR* );
}

#endif

