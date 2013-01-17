/* -*-mode: C; fill-column: 78; c-basic-offset: 4;  compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2000-2012 by Eric House (xwords@eehouse.org).  All rights
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
    XP_S16 nMoves;
    XP_Bool gameOver;
    XP_S16 turn;
    XP_S16 nMissing;
} GameInfo;


sqlite3* openGamesDB( const char* dbName );
void closeGamesDB( sqlite3* dbp );

void writeToDB( XWStreamCtxt* stream, void* closure );
void summarize( CommonGlobals* cGlobals );

/* Return GSList whose data is (ptrs to) rowids */
GSList* listGames( GtkAppGlobals* apg );
XP_Bool getGameInfo( GtkAppGlobals* apg, sqlite3_int64 rowid, GameInfo* gib );
XP_Bool loadGame( XWStreamCtxt* stream, sqlite3* pDb, sqlite3_int64 rowid );
void deleteGame( sqlite3* pDb, sqlite3_int64 rowid );

#define KEY_RDEVID "RDEVID"

void store( sqlite3* dbp, const gchar* key, const gchar* value );
void fetch( sqlite3* dbp, const gchar* key, gchar* buf, gint buflen );

#endif
