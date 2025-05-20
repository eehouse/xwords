/* 
 * Copyright 2006-2012 by Eric House (xwords@eehouse.org).  All rights
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

#ifdef ENABLE_LOGGING

#include "dbgutil.h"
#include "strutils.h"

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

const char*
StackMoveType_2str( StackMoveType typ )
{
    switch( typ ) {
        CASESTR(ASSIGN_TYPE);
        CASESTR(MOVE_TYPE);
        CASESTR(TRADE_TYPE); 
        CASESTR(PHONY_TYPE);
        CASESTR(PAUSE_TYPE);
    default:
        XP_ASSERT(0);
        return "<unknown>";
    }
}

#endif /* ENABLE_LOGGING */

#ifdef DEBUG
void
fmtTileSet(const TrayTileSet* tts, XP_UCHAR buf[], XP_U16 bufLen)
{
    int offset = 0;
    offset += XP_SNPRINTF( &buf[offset], bufLen-offset, "[" );
    for ( int ii = 0; ii < tts->nTiles; ++ii ) {
        offset += XP_SNPRINTF( &buf[offset], bufLen-offset, "%d, ",
                               tts->tiles[ii] );
    }
    offset += XP_SNPRINTF( &buf[offset], bufLen-offset, "]" );
}

void
assertTilesInTiles( const MoveInfo* move, const TrayTileSet* tts,
                    Tile blankTile )
{
    LOG_FUNC();
    XP_ASSERT( move->nTiles <= tts->nTiles );
    for ( int ii = 0; ii < move->nTiles; ++ii ) {
        Tile moveTile = move->tiles[ii].tile;
        if  ( moveTile & TILE_BLANK_BIT ) {
            moveTile = blankTile;
        }
        XP_Bool found = XP_FALSE;
        for ( int jj = 0; !found && jj < tts->nTiles; ++jj ) {
            found = moveTile == tts->tiles[jj];
        }
        if ( !found ) {
            XP_LOGFF( "move tile with val %d not in tray", moveTile );
            XP_ASSERT(0);
        }
    }
}

void
dbg_logstream( const XWStreamCtxt* stream, const char* func, int line )
{
    if ( !!stream ) {
        XP_U16 len = 0;
        XWStreamPos end = stream_getPos( stream, POS_WRITE );
        stream_copyBits( stream, end, NULL, &len );
        XP_U8 buf[len];
        stream_copyBits( stream, end, buf, &len );
        char comment[128];
        XP_SNPRINTF( comment, VSIZE(comment), "%s line %d", func, line );
        LOG_HEX( buf, len, comment );
    } else {
        XP_LOGF( "stream from line %d of func %s is null", 
                 line, func );
    }
}

const char*
devIDTypeToStr(DevIDType typ)
{
    switch( typ ) {
        CASESTR(ID_TYPE_NONE);
        CASESTR(ID_TYPE_RELAY);
        CASESTR(ID_TYPE_LINUX);
        CASESTR(ID_TYPE_ANDROID_GCM_UNUSED);
        CASESTR(ID_TYPE_ANDROID_OTHER);
        CASESTR(ID_TYPE_ANON);
        CASESTR(ID_TYPE_ANDROID_FCM);

        CASESTR(ID_TYPE_NTYPES);
    default:
        XP_ASSERT(0);
    }
}

#undef CASESTR
typedef void (*ProcPtr)();
void
assertTableFull( void* table, size_t sizeInBytes, const XP_UCHAR* tableName )
{
    if ( 0 != sizeInBytes % sizeof(ProcPtr) ) {
        XP_LOGFF( "bad call? vtable size: %zu; proc ptr size: %zu", sizeInBytes, sizeof(ProcPtr) );
        XP_ASSERT( 0 );
    }

    ProcPtr* proc = (ProcPtr*)table;
    int count = sizeInBytes / sizeof(ProcPtr);
    for ( int ii = 0; ii < count; ++ii ) {
        if ( !*proc ) {
            XP_LOGFF( "%s.vtable[%d] missing", tableName, ii );
            XP_ASSERT( 0 );
        }
        ++proc;
    }
}

#endif  /* DEBUG */
