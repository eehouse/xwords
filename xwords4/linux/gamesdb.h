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

#ifndef _GAMESDB_H_
#define _GAMESDB_H_

#include <sqlite3.h>
#include <glib.h>

#include "main.h" 
#include "comtypes.h"

typedef struct _GameInfo {
    XP_UCHAR name[128];
    XP_UCHAR room[128];
    XP_UCHAR conn[128];
    XP_UCHAR relayID[32];
#ifdef PLATFORM_GTK
    GdkPixbuf* snap;
#endif
    XP_U32 gameID;
    XP_S16 nMoves;
    XP_Bool gameOver;
    XP_Bool turnLocal;
    XP_S16 turn;
    XP_U16 nTotal;
    XP_S16 nMissing;
    XP_U16 seed;
    XP_U32 lastMoveTime;
} GameInfo;

sqlite3* openGamesDB( const char* dbName );
void closeGamesDB( sqlite3* dbp );

void writeToDB( XWStreamCtxt* stream, void* closure );
sqlite3_int64 writeNewGameToDB( XWStreamCtxt* stream, sqlite3* pDb );

void summarize( CommonGlobals* cGlobals );

/* Return GSList whose data is (ptrs to) rowids */
GSList* listGames( sqlite3* dbp );
/* Mapping of relayID -> rowid */
GHashTable* getRelayIDsToRowsMap( sqlite3* pDb );

XP_Bool getGameInfo( sqlite3* dbp, sqlite3_int64 rowid, GameInfo* gib );
void getRowsForGameID( sqlite3* dbp, XP_U32 gameID, sqlite3_int64* rowids, 
                       int* nRowIDs );
XP_Bool loadGame( XWStreamCtxt* stream, sqlite3* pDb, sqlite3_int64 rowid );
void saveInviteAddrs( XWStreamCtxt* stream, sqlite3* pDb, 
                      sqlite3_int64 rowid );
XP_Bool loadInviteAddrs( XWStreamCtxt* stream, sqlite3* pDb, 
                         sqlite3_int64 rowid );
void deleteGame( sqlite3* pDb, sqlite3_int64 rowid );

#define KEY_RDEVID "RDEVID"
#define KEY_LDEVID "LDEVID"
#define KEY_SMSPHONE "SMSPHONE"
#define KEY_SMSPORT "SMSPORT"
#define KEY_WIN_LOC "WIN_LOC"

void db_store( sqlite3* dbp, const gchar* key, const gchar* value );
XP_Bool db_fetch( sqlite3* dbp, const gchar* key, gchar* buf, gint buflen );
void db_remove( sqlite3* dbp, const gchar* key );

#endif
