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

#ifndef _DBGUTIL_H_
#define _DBGUTIL_H_

#include "board.h"
#include "movestak.h"

const char* XP_Key_2str( XP_Key key );
const char* DrawFocusState_2str( DrawFocusState dfs );
const char* BoardObjectType_2str( BoardObjectType dfs );
const char* StackMoveType_2str( StackMoveType typ );


# ifdef DEBUG
void dbg_logstream( const XWStreamCtxt* stream, const char* func, int line );
const char* devIDTypeToStr(DevIDType typ);
void assertTilesInTiles( const MoveInfo* move, const TrayTileSet* tts,
                         Tile blankTile );
void fmtTileSet(const TrayTileSet* tts, XP_UCHAR buf[], XP_U16 bufLen);
#define LOG_TTS(tts, msg) {                             \
        XP_UCHAR buf[128];                              \
        fmtTileSet( tts, buf, VSIZE(buf) );             \
        XP_LOGFF( "Tile set: %s (msg: %s)", buf, msg ); \
    }

#  define XP_LOGSTREAM( s )                      \
    dbg_logstream( s, __func__, __LINE__ )
# else
#  define XP_LOGSTREAM( s )
# define devIDTypeToStr(s) ""
# define assertTilesInTiles( move, tts )
# endif

#define boolToStr(b) ((b)?"true" : "false")

# ifdef DEBUG
#  define DIRTY_SLOT XP_Bool _isDirty;
#  define ASSERT_NOT_DIRTY( ptr ) XP_ASSERT( !((ptr)->_isDirty) )
#  define CLEAR_DIRTY( ptr ) *(XP_Bool*)&((ptr)->_isDirty) = XP_FALSE
#  define SET_DIRTY( ptr ) (ptr)->_isDirty = XP_TRUE
# else
#  define DIRTY_SLOT
#  define ASSERT_NOT_DIRTY( ptr )
#  define CLEAR_DIRTY( ptr )
#  define SET_DIRTY( ptr )
# endif

# ifdef DEBUG
void assertTableFull( void* table, size_t sizeInBytes, const XP_UCHAR* tableName );
# else
#  define assertTableFull( table, sizeInBytes, tableName )
# endif

#endif
