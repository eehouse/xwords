/* -*- compile-command: "make MEMDEBUG=TRUE -j5"; -*- */
/* 
 * Copyright 2000 - 2016 by Eric House (xwords@eehouse.org).  All rights
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
#include "gtkboard.h"
#include "gtkdraw.h"
#include "linuxutl.h"
#include "main.h"

#define SNAP_WIDTH 150
#define SNAP_HEIGHT 150

static XP_Bool getColumnText( sqlite3_stmt *ppStmt, int iCol, XP_UCHAR* buf,
                              int* len );

#ifdef DEBUG
static char* sqliteErr2str( int err );
#endif
static void assertPrintResult( sqlite3* pDb, int result, int expect );

sqlite3* 
openGamesDB( const char* dbName )
{
    int result = sqlite3_initialize();
    XP_ASSERT( SQLITE_OK == result );

    sqlite3* pDb = NULL;
    result = sqlite3_open( dbName, &pDb );
    XP_ASSERT( SQLITE_OK == result );

    const char* createGamesStr = 
        "CREATE TABLE games ( "
        "rowid INTEGER PRIMARY KEY AUTOINCREMENT"
        ",game BLOB"
        ",snap BLOB"
        ",inviteInfo BLOB"
        ",room VARCHAR(32)"
        ",connvia VARCHAR(32)"
        ",relayid VARCHAR(32)"
        ",ended INT(1)"
        ",turn INT(2)"
        ",local INT(1)"
        ",nmoves INT"
        ",seed INT"
        ",nPending INT"
        ",role INT"
        ",dictlang INT"
        ",gameid INT"
        ",ntotal INT(2)"
        ",nmissing INT(2)"
        ",lastMoveTime INT"
        ",scores TEXT"
        ",dupTimerExpires INT"
        ")";
    result = sqlite3_exec( pDb, createGamesStr, NULL, NULL, NULL );

    const char* createValuesStr = 
        "CREATE TABLE pairs ( key TEXT UNIQUE,value TEXT )";
    result = sqlite3_exec( pDb, createValuesStr, NULL, NULL, NULL );
    XP_LOGF( "sqlite3_exec=>%d", result );
    XP_USE( result );

    return pDb;
}

void
closeGamesDB( sqlite3* pDb )
{
    sqlite3_close( pDb );
    XP_LOGF( "%s finished", __func__ );
}

static sqlite3_int64
writeBlobColumnData( const XP_U8* data, gsize len, XP_U16 strVersion, sqlite3* pDb,
                     sqlite3_int64 curRow, const char* column )
{
    XP_LOGF( "%s(col=%s)", __func__, column );
    int result;
    char buf[256];
    char* query;

    sqlite3_stmt* stmt = NULL;
    XP_Bool newGame = -1 == curRow;
    if ( newGame ) {         /* new row; need to insert blob first */
        const char* fmt = "INSERT INTO games (%s) VALUES (?)";
        snprintf( buf, sizeof(buf), fmt, column );
        query = buf;
    } else {
        const char* fmt = "UPDATE games SET %s=? where rowid=%lld";
        snprintf( buf, sizeof(buf), fmt, column, curRow );
        query = buf;
    }

    result = sqlite3_prepare_v2( pDb, query, -1, &stmt, NULL );
    assertPrintResult( pDb, result, SQLITE_OK );
    result = sqlite3_bind_zeroblob( stmt, 1 /*col 0 ??*/, sizeof(XP_U16) + len );
    assertPrintResult( pDb, result, SQLITE_OK );
    result = sqlite3_step( stmt );
    if ( SQLITE_DONE != result ) {
        XP_LOGF( "%s: sqlite3_step => %s", __func__, sqliteErr2str( result ) );
        XP_ASSERT(0);
    }
    XP_USE( result );

    if ( newGame ) {         /* new row; need to insert blob first */
        curRow = sqlite3_last_insert_rowid( pDb );
        XP_LOGF( "%s: new rowid: %lld", __func__, curRow );
    }

    sqlite3_blob* blob;
    result = sqlite3_blob_open( pDb, "main", "games", column,
                                curRow, 1 /*flags: writeable*/, &blob );
    assertPrintResult( pDb, result, SQLITE_OK );
    XP_ASSERT( strVersion <= CUR_STREAM_VERS );
    result = sqlite3_blob_write( blob, &strVersion, sizeof(strVersion), 0/*offset*/ );
    assertPrintResult( pDb, result, SQLITE_OK );
    result = sqlite3_blob_write( blob, data, len, sizeof(strVersion) /* offset */ );
    assertPrintResult( pDb, result, SQLITE_OK );
    result = sqlite3_blob_close( blob );
    assertPrintResult( pDb, result, SQLITE_OK );

    if ( !!stmt ) {
        sqlite3_finalize( stmt );
    }

    LOG_RETURNF( "%lld", curRow );
    return curRow;
} /* writeBlobColumnData */

static sqlite3_int64
writeBlobColumnStream( XWStreamCtxt* stream, sqlite3* pDb, sqlite3_int64 curRow,
                       const char* column )
{
    XP_U16 strVersion = stream_getVersion( stream );
    const XP_U8* data = stream_getPtr( stream );
    gsize len = stream_getSize( stream );
    return writeBlobColumnData( data, len, strVersion, pDb, curRow, column );
}

sqlite3_int64
writeNewGameToDB( XWStreamCtxt* stream, sqlite3* pDb )
{
    sqlite3_int64 newRow = writeBlobColumnStream( stream, pDb, -1, "game" );
    return newRow;
}

void
writeToDB( XWStreamCtxt* stream, void* closure )
{
    CommonGlobals* cGlobals = (CommonGlobals*)closure;
    sqlite3_int64 selRow = cGlobals->rowid;
    sqlite3* pDb = cGlobals->params->pDb;

    XP_Bool newGame = -1 == selRow;
    selRow = writeBlobColumnStream( stream, pDb, selRow, "game" );

    if ( newGame ) {         /* new row; need to insert blob first */
        cGlobals->rowid = selRow;
        XP_LOGF( "%s(): new game at row %lld", __func__, selRow );
    } else {
        assert( selRow == cGlobals->rowid );
    }

    (*cGlobals->onSave)( cGlobals->onSaveClosure, selRow, newGame );
}

#ifdef PLATFORM_GTK
static void
addSnapshot( CommonGlobals* cGlobals )
{
    LOG_FUNC();

    BoardCtxt* board = cGlobals->game.board;
    GtkDrawCtx* dctx = (GtkDrawCtx*)board_getDraw( board );
    if ( !!dctx ) {
        addSurface( dctx, SNAP_WIDTH, SNAP_HEIGHT );
        board_drawSnapshot( board, (DrawCtx*)dctx, SNAP_WIDTH, SNAP_HEIGHT );

        XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(cGlobals->util->mpool)
                                                    cGlobals->params->vtMgr );
        getImage( dctx, stream );
        removeSurface( dctx );
        cGlobals->rowid = writeBlobColumnStream( stream, cGlobals->params->pDb,
                                                 cGlobals->rowid, "snap" );
        stream_destroy( stream );
    }

    LOG_RETURN_VOID();
}
#else
# define addSnapshot( cGlobals )
#endif

void
summarize( CommonGlobals* cGlobals )
{
    const XWGame* game = &cGlobals->game;
    XP_S16 nMoves = model_getNMoves( game->model );
    XP_Bool gameOver = server_getGameIsOver( game->server );
    XP_Bool isLocal = -1;
    XP_S16 turn = server_getCurrentTurn( game->server, &isLocal );
    XP_U32 lastMoveTime = server_getLastMoveTime( game->server );
    XP_U32 dupTimerExpires = server_getDupTimerExpires( game->server );
    XP_U16 seed = 0;
    XP_S16 nMissing = 0;
    XP_U16 nPending = 0;
    const CurGameInfo* gi = cGlobals->gi;
    XP_U16 nTotal = gi->nPlayers;
    XP_U32 gameID = gi->gameID;
    XP_LangCode dictLang = gi->dictLang;
    XP_ASSERT( 0 != gameID );
    CommsAddrRec addr = {0};
    gchar* room = "";

    // gchar* connvia = "local";
    gchar connvia[128] = {0};
    XP_UCHAR relayID[32] = {0};

    ScoresArray scores = {0};
    if ( gameOver ) {
        model_figureFinalScores( game->model, &scores, NULL );
    } else {
        for ( int ii = 0; ii < nTotal; ++ii ) {
            scores.arr[ii] = model_getPlayerScore( game->model, ii );
        }
    }
    gchar scoreBufs[MAX_NUM_PLAYERS][64] = {0};
    gchar* arr[MAX_NUM_PLAYERS+1] = {0};
    for ( int ii = 0; ii < nTotal; ++ii ) {
        XP_SNPRINTF( scoreBufs[ii], VSIZE(scoreBufs[ii]), "%s: %d",
                     gi->players[ii].name, scores.arr[ii] );
        arr[ii] = scoreBufs[ii];
    }
    gchar* scoresStr = g_strjoinv( "; ", arr );
    XP_LOGF( "%s(): scoresStr: %s", __func__, scoresStr );

    if ( !!game->comms ) {
        nMissing = server_getMissingPlayers( game->server );
        comms_getAddr( game->comms, &addr );
        CommsConnType typ;
        for ( XP_U32 st = 0; addr_iter( &addr, &typ, &st ); ) {
            if ( !!connvia[0] ) {
                strcat( connvia, "+" );
            }
            switch( typ) {
            case COMMS_CONN_RELAY:
                room = addr.u.ip_relay.invite;
                strcat( connvia, "Relay" );
                break;
            case COMMS_CONN_SMS:
                strcat( connvia, "SMS" );
                break;
            case COMMS_CONN_BT:
                strcat( connvia, "BT" );
                break;
            case COMMS_CONN_IP_DIRECT:
                strcat( connvia, "IP" );
                break;
            default:
                XP_ASSERT(0);
                break;
            }
        }
        seed = comms_getChannelSeed( game->comms );
        XP_U16 len = VSIZE(relayID);
        (void)comms_getRelayID( game->comms, relayID, &len );

        nPending = comms_countPendingPackets( game->comms );
    } else {
        strcat( connvia, "local" );
    }

    const char* fmt = "UPDATE games "
        " SET room='%s', ended=%d, turn=%d, local=%d, ntotal=%d, "
        " nmissing=%d, nmoves=%d, seed=%d, dictlang=%d, gameid=%d, connvia='%s', "
        " relayid='%s', lastMoveTime=%d, dupTimerExpires=%d, scores='%s', "
        " nPending=%d, role=%d"
        " WHERE rowid=%lld";
    XP_UCHAR buf[2*256];
    snprintf( buf, sizeof(buf), fmt, room, gameOver?1:0, turn, isLocal?1:0,
              nTotal, nMissing, nMoves, seed, dictLang, gameID, connvia, relayID, lastMoveTime,
              dupTimerExpires, scoresStr, nPending, gi->serverRole, cGlobals->rowid );
    XP_LOGF( "query: %s", buf );
    sqlite3_stmt* stmt = NULL;
    int result = sqlite3_prepare_v2( cGlobals->params->pDb, buf, -1, &stmt, NULL );
    assertPrintResult( cGlobals->params->pDb, result, SQLITE_OK );
    result = sqlite3_step( stmt );
    if ( SQLITE_DONE != result ) {
        XP_LOGF( "sqlite3_step=>%s", sqliteErr2str( result ) );
        XP_ASSERT( 0 );
    }
    sqlite3_finalize( stmt );
    XP_USE( result );

    if ( !cGlobals->params->useCurses ) {
        addSnapshot( cGlobals );
    }
    g_free( scoresStr );
}

GSList*
listGames( sqlite3* pDb )
{
    GSList* list = NULL;
    
    sqlite3_stmt *ppStmt;
    int result = sqlite3_prepare_v2( pDb, 
                                     "SELECT rowid FROM games ORDER BY rowid", 
                                     -1, &ppStmt, NULL );
    assertPrintResult( pDb, result, SQLITE_OK );
    XP_USE( result );
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

static void
dataKiller( gpointer data )
{
    g_free( data );
}

void
freeGamesList( GSList* games )
{
    g_slist_free_full( games, dataKiller );
}

GHashTable*
getRelayIDsToRowsMap( sqlite3* pDb )
{
    GHashTable* table = g_hash_table_new( g_str_hash, g_str_equal );
    sqlite3_stmt *ppStmt;
    int result = sqlite3_prepare_v2( pDb, "SELECT relayid, rowid FROM games "
                                     "where NOT relayid = ''", -1, &ppStmt, NULL );
    assertPrintResult( pDb, result, SQLITE_OK );
    while ( result == SQLITE_OK && NULL != ppStmt ) {
        switch( sqlite3_step( ppStmt ) ) {
        case SQLITE_ROW:        /* have data */
        {
            XP_UCHAR relayID[32];
            int len = VSIZE(relayID);
            getColumnText( ppStmt, 0, relayID, &len );
            gpointer key = g_strdup( relayID );
            sqlite3_int64* value = g_malloc( sizeof( value ) );
            *value = sqlite3_column_int64( ppStmt, 1 );
            g_hash_table_insert( table, key, value );
            /* XP_LOGF( "%s(): added map %s => %lld", __func__, (char*)key, *value ); */
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

    return table;
}

XP_Bool
getGameInfo( sqlite3* pDb, sqlite3_int64 rowid, GameInfo* gib )
{
    XP_Bool success = XP_FALSE;
    const char* fmt = "SELECT room, ended, turn, local, nmoves, ntotal, nmissing, "
        "dictlang, seed, connvia, gameid, lastMoveTime, dupTimerExpires, relayid, scores, nPending, role, snap "
        "FROM games WHERE rowid = %lld";
    XP_UCHAR query[256];
    snprintf( query, sizeof(query), fmt, rowid );

    sqlite3_stmt* ppStmt;
    int result = sqlite3_prepare_v2( pDb, query, -1, &ppStmt, NULL );
    assertPrintResult( pDb, result, SQLITE_OK );
    result = sqlite3_step( ppStmt );
    if ( SQLITE_ROW == result ) {
        success = XP_TRUE;
        int col = 0;
        int len = sizeof(gib->room);
        getColumnText( ppStmt, col++, gib->room, &len );
        gib->rowid = rowid;
        gib->gameOver = 1 == sqlite3_column_int( ppStmt, col++ );
        gib->turn = sqlite3_column_int( ppStmt, col++ );
        gib->turnLocal = 1 == sqlite3_column_int( ppStmt, col++ );
        gib->nMoves = sqlite3_column_int( ppStmt, col++ );
        gib->nTotal = sqlite3_column_int( ppStmt, col++ );
        gib->nMissing = sqlite3_column_int( ppStmt, col++ );
        gib->dictLang = sqlite3_column_int( ppStmt, col++ );
        gib->seed = sqlite3_column_int( ppStmt, col++ );
        len = sizeof(gib->conn);
        getColumnText( ppStmt, col++, gib->conn, &len );
        gib->gameID = sqlite3_column_int( ppStmt, col++ );
        gib->lastMoveTime = sqlite3_column_int( ppStmt, col++ );
        gib->dupTimerExpires = sqlite3_column_int( ppStmt, col++ );
        len = sizeof(gib->relayID);
        getColumnText( ppStmt, col++, gib->relayID, &len );
        len = sizeof(gib->scores);
        getColumnText( ppStmt, col++, gib->scores, &len );
        gib->nPending = sqlite3_column_int( ppStmt, col++ );
        gib->role = sqlite3_column_int( ppStmt, col++ );
        snprintf( gib->name, sizeof(gib->name), "Game %lld", rowid );

#ifdef PLATFORM_GTK
        /* Load the snapshot */
        GdkPixbuf* snap = NULL;
        int snapCol = col++;
        const XP_U8* ptr = sqlite3_column_blob( ppStmt, snapCol );
        if ( !!ptr ) {
            int size = sqlite3_column_bytes( ppStmt, snapCol );
            /* Skip the version that's written in */
            ptr += sizeof(XP_U16); size -= sizeof(XP_U16);
            GInputStream* istr = g_memory_input_stream_new_from_data( ptr, size, NULL );
            snap = gdk_pixbuf_new_from_stream( istr, NULL, NULL );
            g_object_unref( istr );
        }
        gib->snap = snap;
#endif
    }
    sqlite3_finalize( ppStmt );

    return success;
}

void
getRowsForGameID( sqlite3* pDb, XP_U32 gameID, sqlite3_int64* rowids, 
                  int* nRowIDs )
{
    int maxRowIDs = *nRowIDs;
    *nRowIDs = 0;

    char buf[256];
    snprintf( buf, sizeof(buf), "SELECT rowid from games WHERE gameid = %d "
              "LIMIT %d", gameID, maxRowIDs );
    sqlite3_stmt *ppStmt;
    int result = sqlite3_prepare_v2( pDb, buf, -1, &ppStmt, NULL );
    assertPrintResult( pDb, result, SQLITE_OK );
    int ii;
    for ( ii = 0; ii < maxRowIDs; ++ii ) {
        result = sqlite3_step( ppStmt );
        if ( SQLITE_ROW != result ) {
            break;
        }
        rowids[ii] = sqlite3_column_int64( ppStmt, 0 );
        ++*nRowIDs;
    }
    sqlite3_finalize( ppStmt );
}

static XP_Bool
loadBlobColumn( XWStreamCtxt* stream, sqlite3* pDb, sqlite3_int64 rowid, 
                const char* column )
{
    char buf[256];
    snprintf( buf, sizeof(buf), "SELECT %s from games WHERE rowid = %lld", 
              column, rowid );

    sqlite3_stmt *ppStmt;
    int result = sqlite3_prepare_v2( pDb, buf, -1, &ppStmt, NULL );
    assertPrintResult( pDb, result, SQLITE_OK );
    result = sqlite3_step( ppStmt );
    XP_Bool success = SQLITE_ROW == result;
    if ( success ) {
        const void* ptr = sqlite3_column_blob( ppStmt, 0 );
        int size = sqlite3_column_bytes( ppStmt, 0 );
        success = 0 < size;
        if ( success ) {
            XP_U16 strVersion;
            XP_MEMCPY( &strVersion, ptr, sizeof(strVersion) );
            XP_ASSERT( strVersion <= CUR_STREAM_VERS );
            stream_setVersion( stream, strVersion );
            XP_ASSERT( size >= sizeof(strVersion) );
            stream_putBytes( stream, ptr + sizeof(strVersion), 
                             size - sizeof(strVersion) );
        }
    }
    sqlite3_finalize( ppStmt );
    return success;
}

XP_Bool
loadGame( XWStreamCtxt* stream, sqlite3* pDb, sqlite3_int64 rowid )
{
    return loadBlobColumn( stream, pDb, rowid, "game" );
}

void
saveInviteAddrs( XWStreamCtxt* stream, sqlite3* pDb, sqlite3_int64 rowid )
{
    sqlite3_int64 row = writeBlobColumnStream( stream, pDb, rowid, "inviteInfo" );
    assert( row == rowid );
}

XP_Bool
loadInviteAddrs( XWStreamCtxt* stream, sqlite3* pDb, sqlite3_int64 rowid )
{
    return loadBlobColumn( stream, pDb, rowid, "inviteInfo" );
}

void
deleteGame( sqlite3* pDb, sqlite3_int64 rowid )
{
    XP_ASSERT( !!pDb );
    char query[256];
    snprintf( query, sizeof(query), "DELETE FROM games WHERE rowid = %lld", rowid );
    sqlite3_stmt* ppStmt;
    int result = sqlite3_prepare_v2( pDb, query, -1, &ppStmt, NULL );        
    assertPrintResult( pDb, result, SQLITE_OK );
    result = sqlite3_step( ppStmt );
    assertPrintResult( pDb, result, SQLITE_DONE );
    XP_USE( result );
    sqlite3_finalize( ppStmt );
}

void
db_store( sqlite3* pDb, const gchar* key, const gchar* value )
{
    XP_ASSERT( !!pDb );
    gchar* buf =
        g_strdup_printf( "INSERT OR REPLACE INTO pairs (key, value) VALUES ('%s', '%s')",
                         key, value );
    sqlite3_stmt *ppStmt;
    int result = sqlite3_prepare_v2( pDb, buf, -1, &ppStmt, NULL );
    assertPrintResult( pDb, result, SQLITE_OK );
    result = sqlite3_step( ppStmt );
    assertPrintResult( pDb, result, SQLITE_DONE );
    XP_USE( result );
    sqlite3_finalize( ppStmt );
    g_free( buf );
}

FetchResult
db_fetch( sqlite3* pDb, const gchar* key, gchar* buf, gint* buflen )
{
    XP_ASSERT( !!pDb );
    FetchResult result = NOT_THERE;
    char query[256];
#ifdef DEBUG
    int len =
#endif
        snprintf( query, sizeof(query),
                  "SELECT value from pairs where key = '%s'", key );
    XP_ASSERT( len < sizeof(query) );
    sqlite3_stmt *ppStmt;
    int sqlResult = sqlite3_prepare_v2( pDb, query, -1, &ppStmt, NULL );
    XP_Bool found = SQLITE_OK == sqlResult;
    if ( found ) {
        result = sqlite3_step( ppStmt );
        found = SQLITE_ROW == result;
        if ( found ) {
            if ( getColumnText( ppStmt, 0, buf, buflen ) ) {
                result = SUCCESS;
            } else {
                result = BUFFER_TOO_SMALL;
            }
        } else if ( !!buf ) {
            buf[0] = '\0';
        }
    }
    sqlite3_finalize( ppStmt );
    return result;
}

XP_Bool
db_fetch_safe( sqlite3* pDb, const gchar* key, gchar* buf, gint buflen )
{
    XP_ASSERT( !!pDb );
    int tmp = buflen;
    FetchResult result = db_fetch( pDb, key, buf, &tmp );
    XP_ASSERT( result != BUFFER_TOO_SMALL );
    return SUCCESS == result;
}

void
db_remove( sqlite3* pDb, const gchar* key )
{
    XP_ASSERT( !!pDb );
    char query[256];
    snprintf( query, sizeof(query), "DELETE FROM pairs WHERE key = '%s'", key );
    sqlite3_stmt *ppStmt;
    int result = sqlite3_prepare_v2( pDb, query, -1, &ppStmt, NULL );
    assertPrintResult( pDb, result, SQLITE_OK );
    result = sqlite3_step( ppStmt );
    assertPrintResult( pDb, result, SQLITE_DONE );
    XP_USE( result );
    sqlite3_finalize( ppStmt );
}

static XP_Bool
getColumnText( sqlite3_stmt *ppStmt, int iCol, XP_UCHAR* buf, int *len )
{
    int colLen = sqlite3_column_bytes( ppStmt, iCol );

    XP_Bool success = colLen < *len;
    *len = colLen + 1;          /* sqlite does not store the null byte */
    if ( success ) {
        const unsigned char* colTxt = sqlite3_column_text( ppStmt, iCol );
        XP_MEMCPY( buf, colTxt, colLen );
        buf[colLen] = '\0';
    }
    return success;
}

#ifdef DEBUG
# define CASESTR(c) case c: return #c
static char*
sqliteErr2str( int err )
{
    switch( err ) {
        CASESTR( SQLITE_OK );
        CASESTR( SQLITE_ERROR );
        CASESTR( SQLITE_INTERNAL );
        CASESTR( SQLITE_PERM );
        CASESTR( SQLITE_ABORT );
        CASESTR( SQLITE_BUSY );
        CASESTR( SQLITE_LOCKED );
        CASESTR( SQLITE_NOMEM );
        CASESTR( SQLITE_READONLY );
        CASESTR( SQLITE_INTERRUPT );
        CASESTR( SQLITE_IOERR );
        CASESTR( SQLITE_CORRUPT );
        CASESTR( SQLITE_NOTFOUND );
        CASESTR( SQLITE_FULL );
        CASESTR( SQLITE_CANTOPEN );
        CASESTR( SQLITE_PROTOCOL );
        CASESTR( SQLITE_EMPTY );
        CASESTR( SQLITE_SCHEMA );
        CASESTR( SQLITE_TOOBIG );
        CASESTR( SQLITE_CONSTRAINT );
        CASESTR( SQLITE_MISMATCH );
        CASESTR( SQLITE_MISUSE );
        CASESTR( SQLITE_NOLFS );
        CASESTR( SQLITE_AUTH );
        CASESTR( SQLITE_FORMAT );
        CASESTR( SQLITE_RANGE );
        CASESTR( SQLITE_NOTADB );
        CASESTR( SQLITE_NOTICE );
        CASESTR( SQLITE_WARNING );
        CASESTR( SQLITE_ROW );
        CASESTR( SQLITE_DONE );
    }
        return "<unknown>";
}
#endif

static void
assertPrintResult( sqlite3* pDb, int XP_UNUSED_DBG(result), int expect )
{
    int code = sqlite3_errcode( pDb );
    XP_ASSERT( code == result ); /* do I need to pass it? */
    if ( code != expect ) {
        XP_LOGF( "sqlite3 error: %s", sqlite3_errmsg( pDb ) );
        XP_ASSERT(0);
    }
}
