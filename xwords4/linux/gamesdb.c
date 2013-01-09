/* -*- compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2000-2013 by Eric House (xwords@eehouse.org).  All rights
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

#include "gamesdb.h"
#include "main.h"

static void getColumnText( sqlite3_stmt *ppStmt, int iCol, XP_UCHAR* buf, 
                           int len );


sqlite3* 
openGamesDB( const char* dbName )
{
    sqlite3* pDb = NULL;
    int result = sqlite3_open( dbName, &pDb );
    XP_ASSERT( SQLITE_OK == result );

    const char* createStr = 
        "CREATE TABLE games ( "
        "game BLOB"
        ",room VARCHAR(32)"
        ",ended INT(1)"
        ",turn INT(2)"
        ",nmoves INT"
        ",nmissing INT(2)"
        ")";

    result = sqlite3_exec( pDb, createStr, NULL, NULL, NULL );
    XP_LOGF( "sqlite3_exec=>%d", result );
    // XP_ASSERT( SQLITE_OK == result );

    return pDb;
}

void
closeGamesDB( sqlite3* pDb )
{
    sqlite3_close( pDb );
    XP_LOGF( "%s finished", __func__ );
}

void
writeToDB( XWStreamCtxt* stream, void* closure )
{
    int result;
    CommonGlobals* cGlobals = (CommonGlobals*)closure;
    sqlite3_int64 selRow = cGlobals->selRow;
    sqlite3* pDb = cGlobals->pDb;
    XP_U16 len = stream_getSize( stream );
    char buf[256];
    char* query;

    sqlite3_stmt* stmt = NULL;
    XP_Bool newGame = -1 == selRow;
    if ( newGame ) {         /* new row; need to insert blob first */
        query = "INSERT INTO games (game) VALUES (?)";
    } else {
        const char* fmt = "UPDATE games SET game=? where rowid=%lld";
        snprintf( buf, sizeof(buf), fmt, selRow );
        query = buf;
    }

    result = sqlite3_prepare_v2( pDb, query, -1, &stmt, NULL );        
    XP_ASSERT( SQLITE_OK == result );
    result = sqlite3_bind_zeroblob( stmt, 1 /*col 0 ??*/, len );
    XP_ASSERT( SQLITE_OK == result );
    result = sqlite3_step( stmt );
    XP_ASSERT( SQLITE_DONE == result );

    if ( newGame ) {         /* new row; need to insert blob first */
        selRow = sqlite3_last_insert_rowid( pDb );
        XP_LOGF( "%s: new rowid: %lld", __func__, selRow );
        cGlobals->selRow = selRow;
    }

    sqlite3_blob* blob;
    result = sqlite3_blob_open( pDb, "main", "games", "game",
                                selRow, 1 /*flags: writeable*/, &blob );
    XP_ASSERT( SQLITE_OK == result );
    const XP_U8* ptr = stream_getPtr( stream );
    result = sqlite3_blob_write( blob, ptr, len, 0 );
    XP_ASSERT( SQLITE_OK == result );
    result = sqlite3_blob_close( blob );
    XP_ASSERT( SQLITE_OK == result );
    if ( !!stmt ) {
        sqlite3_finalize( stmt );
    }

    (*cGlobals->onSave)( cGlobals->onSaveClosure, selRow, newGame );
}

void
summarize( CommonGlobals* cGlobals )
{
    XP_S16 nMoves = model_getNMoves( cGlobals->game.model );
    XP_Bool gameOver = server_getGameIsOver( cGlobals->game.server );
    XP_S16 turn = server_getCurrentTurn( cGlobals->game.server );
    XP_S16 nMissing = 0;
    CommsAddrRec addr = {0};
    gchar* room = "";

    if ( !!cGlobals->game.comms ) {
        nMissing = server_getMissingPlayers( cGlobals->game.server );
        comms_getAddr( cGlobals->game.comms, &addr );
        if ( COMMS_CONN_RELAY == addr.conType ) {
            room = addr.u.ip_relay.invite;
        }
    }

    const char* fmt = "UPDATE games SET room='%s', ended=%d, turn=%d, nmissing=%d, nmoves=%d"
        " where rowid=%lld";
    XP_UCHAR buf[256];
    snprintf( buf, sizeof(buf), fmt, room, gameOver?1:0, turn, nMissing, nMoves,
              cGlobals->selRow );
    XP_LOGF( "query: %s", buf );
    sqlite3_stmt* stmt = NULL;
    int result = sqlite3_prepare_v2( cGlobals->pDb, buf, -1, &stmt, NULL );        
    XP_ASSERT( SQLITE_OK == result );
    result = sqlite3_step( stmt );
    XP_ASSERT( SQLITE_DONE == result );
    sqlite3_finalize( stmt );
}

GSList*
listGames( GTKGamesGlobals* gg )
{
    GSList* list = NULL;
    
    sqlite3_stmt *ppStmt;
    int result = sqlite3_prepare_v2( gg->pDb, 
                                     "SELECT rowid FROM games ORDER BY rowid", -1,
                                     &ppStmt, NULL );
    XP_ASSERT( SQLITE_OK == result );
    while ( NULL != ppStmt ) {
        switch( sqlite3_step( ppStmt ) ) {
        case SQLITE_ROW:        /* have data */
        {
            sqlite3_int64* data = g_malloc( sizeof( *data ) );
            *data = sqlite3_column_int64( ppStmt, 0 );
            XP_LOGF( "%s: got a row; id=%lld", __func__, *data );
            list = g_slist_append( list, data );
        }
        break;
        case SQLITE_DONE:
            sqlite3_finalize( ppStmt );
            ppStmt = NULL;
            break;
        default:
            XP_ASSERT( 0 );
            break;
        }
    }
    return list;
}

XP_Bool
getGameInfo( GTKGamesGlobals* gg, sqlite3_int64 rowid, GameInfo* gib )
{
    XP_Bool success = XP_FALSE;
    const char* fmt = "SELECT room, ended, turn, nmoves, nmissing "
        "FROM games WHERE rowid = %lld";
    XP_UCHAR query[256];
    snprintf( query, sizeof(query), fmt, rowid );

    sqlite3_stmt* ppStmt;
    int result = sqlite3_prepare_v2( gg->pDb, query, -1, &ppStmt, NULL );
    XP_ASSERT( SQLITE_OK == result );
    result = sqlite3_step( ppStmt );
    if ( SQLITE_ROW == result ) {
        success = XP_TRUE;
        getColumnText( ppStmt, 0, gib->room, sizeof(gib->room) );
        gib->gameOver = 1 == sqlite3_column_int( ppStmt, 1 );
        gib->turn = sqlite3_column_int( ppStmt, 2 );
        gib->nMoves = sqlite3_column_int( ppStmt, 3 );
        gib->nMissing = sqlite3_column_int( ppStmt, 4 );
        snprintf( gib->name, sizeof(gib->name), "Game %lld", rowid );
    }
    sqlite3_finalize( ppStmt );
    return success;
}

XP_Bool
loadGame( XWStreamCtxt* stream, sqlite3* pDb, sqlite3_int64 rowid )
{
    char buf[256];
    snprintf( buf, sizeof(buf), "SELECT game from games WHERE rowid = %lld", rowid );

    sqlite3_stmt *ppStmt;
    int result = sqlite3_prepare_v2( pDb, buf, -1, &ppStmt, NULL );
    XP_ASSERT( SQLITE_OK == result );
    result = sqlite3_step( ppStmt );
    XP_ASSERT( SQLITE_ROW == result );
    const void* ptr = sqlite3_column_blob( ppStmt, 0 );
    int size = sqlite3_column_bytes( ppStmt, 0 );
    stream_putBytes( stream, ptr, size );
    sqlite3_finalize( ppStmt );
    return XP_TRUE;
}

static void
getColumnText( sqlite3_stmt *ppStmt, int iCol, XP_UCHAR* buf, int len )
{
    const unsigned char* txt = sqlite3_column_text( ppStmt, iCol );
    int needLen = sqlite3_column_bytes( ppStmt, iCol );
    XP_ASSERT( needLen < len );
    XP_MEMCPY( buf, txt, needLen );
    buf[needLen] = '\0';
}
