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
    sqlite3_int64 rowid;
    XP_UCHAR name[128];
    XP_UCHAR conn[128];
    XP_UCHAR scores[128];
    XP_UCHAR isoCode[32];
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
    XP_U16 nPending;
    XP_U16 nTiles;
    XP_U32 lastMoveTime;
    XP_U32 dupTimerExpires;
    XP_U32 created;
    XP_U16 role;
    XP_U8 channelNo;
} GameInfo;

sqlite3* gdb_open( const char* dbName );
void gdb_close( sqlite3* pDb );

void gdb_write( XWStreamCtxt* stream, XWEnv xwe, void* closure );
sqlite3_int64 gdb_writeNewGame( XWStreamCtxt* stream, sqlite3* pDb );

void gdb_summarize( CommonGlobals* cGlobals );

/* Return GSList whose data is (ptrs to) rowids */
GSList* gdb_listGames( sqlite3* pDb );
/* free list and data allocated by above */
void gdb_freeGamesList( GSList* games );

/* Mapping of relayID -> rowid */
GHashTable* gdb_getRelayIDsToRowsMap( sqlite3* pDb );

XP_Bool gdb_getGameInfoForRow( sqlite3* pDb, sqlite3_int64 rowid, GameInfo* gib );
XP_Bool gdb_getGameInfoForGID( sqlite3* pDb, XP_U32 gameID, GameInfo* gib );
void gdb_getRowsForGameID( sqlite3* pDb, XP_U32 gameID, sqlite3_int64* rowids,
                           int* nRowIDs );
XP_Bool gdb_loadGame( XWStreamCtxt* stream, sqlite3* pDb, sqlite3_int64 rowid );
void gdb_deleteGame( sqlite3* pDb, sqlite3_int64 rowid );

typedef struct _DevSummary {
    XP_Bool allDone;
    XP_S32 nTiles;
    XP_U32 nGames;
} DevSummary;

void gdb_getSummary( sqlite3* pDb, DevSummary* ds );

#define KEY_RDEVID "RDEVID"
#define KEY_LDEVID "LDEVID"
#define KEY_SMSPHONE "SMSPHONE"
#define KEY_SMSPORT "SMSPORT"
#define KEY_WIN_LOC "WIN_LOC"

void gdb_store( sqlite3* pDb, const gchar* key, const gchar* value );
void gdb_remove( sqlite3* pDb, const gchar* key );
bool gdb_fetchInt( sqlite3* pDb, const gchar* key, int32_t* resultP );
void gdb_storeInt( sqlite3* pDb, const gchar* key, int32_t val );

typedef enum { NOT_THERE, BUFFER_TOO_SMALL, SUCCESS } FetchResult;
FetchResult gdb_fetch( sqlite3* pDb, const gchar* key, const gchar* keySuffix,
                       gchar* buf, gint* buflen );
XP_Bool gdb_fetch_safe( sqlite3* pDb, const gchar* key, const gchar* keySuffix,
                        gchar* buf, gint buflen );

#endif
