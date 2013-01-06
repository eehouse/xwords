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

#include <sqlite3.h>

#include "comtypes.h"
#include "gamesdb.h"

#define DB_NAME "games.db"

sqlite3* 
openGamesDB( void )
{
    sqlite3* pDb = NULL;
    int result = sqlite3_open( DB_NAME, &pDb );
    XP_ASSERT( SQLITE_OK == result );

    const char* createStr = 
        "CREATE TABLE games ( "
        "game BLOB"
        ",room VARCHAR(32)"
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
