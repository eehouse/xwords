/* -*- compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2020 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <ncurses.h>
#include <glib.h>
#include <sys/types.h>
#include <unistd.h>

#include "curgamlistwin.h"
#include "linuxmain.h"
#include "device.h"
#include "strutils.h"
#include "linuxutl.h"

struct CursGameList {
    WINDOW* window;
    LaunchParams* params;
    int width, height;
    int curSel;
    int yOffset;
    GSList* games;
    int pid;
};

static void adjustCurSel( CursGameList* cgl );

CursGameList*
cgl_init( LaunchParams* params, int width, int height )
{
    CursGameList* cgl = g_malloc0( sizeof( *cgl ) );
    cgl->params = params;
    cgl->window = newwin( height, width, 0, 0 );
    XP_LOGF( "%s(): made window with height=%d, width=%d", __func__, height, width );
    cgl->width = width;
    cgl->height = height;
    cgl->pid = getpid();
    return cgl;
}

void
cgl_destroy( CursGameList* cgl )
{
    g_slist_free_full( cgl->games, g_free );
    delwin( cgl->window );
    g_free( cgl );
}

void
cgl_resized( CursGameList* cgl, int width, int height )
{
    wresize( cgl->window, height, width );
    cgl->width = width;
    cgl->height = height;
    cgl_draw( cgl );
}

static void
addOne( CursGameList* cgl, sqlite3_int64 rowid )
{
    GameInfo gib;
    if ( gdb_getGameInfo( cgl->params->pDb, rowid, &gib ) ) {
        GameInfo* gibp = g_malloc( sizeof(*gibp) );
        *gibp = gib;
        cgl->games = g_slist_append( cgl->games, gibp );
    }
}

/* Load from the DB */
void
cgl_refresh( CursGameList* cgl )
{
    g_slist_free_full( cgl->games, g_free );
    cgl->games = NULL;

    sqlite3* pDb = cgl->params->pDb;
    GSList* games = gdb_listGames( pDb );
    for ( GSList* iter = games; !!iter; iter = iter->next ) {
        sqlite3_int64* rowid = (sqlite3_int64*)iter->data;
        addOne( cgl, *rowid );
    }
    cgl_draw( cgl );
}

static GSList*
findFor( CursGameList* cgl, sqlite3_int64 rowid )
{
    GSList* result = NULL;
    for ( GSList* iter = cgl->games; !!iter && !result; iter = iter->next ) {
        GameInfo* gib = (GameInfo*)iter->data;
        if ( gib->rowid == rowid ) {
            result = iter;
        }
    }
    return result;
}

void
cgl_refreshOne( CursGameList* cgl, sqlite3_int64 rowid, bool select )
{
    // Update the info. In place if it exists, otherwise creating a new list
    // elem
    
    GameInfo gib;
    if ( gdb_getGameInfo( cgl->params->pDb, rowid, &gib ) ) {
        GameInfo* found;
        GSList* elem = findFor( cgl, rowid );
        if ( !!elem ) {
            found = (GameInfo*)elem->data;
            *found = gib;
        } else {
            found = g_malloc( sizeof(*found) );
            *found = gib;
            cgl->games = g_slist_append( cgl->games, found );
        }

        if ( select ) {
            cgl->curSel = g_slist_index( cgl->games, found );
        }
        adjustCurSel( cgl );
    }
}

void
cgl_remove( CursGameList* cgl, sqlite3_int64 rowid )
{
    GSList* elem = findFor( cgl, rowid );
    if ( !!elem ) {
        g_free( elem->data );
        cgl->games = g_slist_delete_link( cgl->games, elem );
    }
    adjustCurSel( cgl );
}

void
cgl_moveSel( CursGameList* cgl, bool down )
{
    int nGames = g_slist_length( cgl->games );
    cgl->curSel += nGames + (down ? 1 : -1);
    cgl->curSel %= nGames;
    adjustCurSel( cgl );
}

static void
adjustCurSel( CursGameList* cgl )
{
    int nGames = g_slist_length( cgl->games );
    XP_LOGF( "%s() start: curSel: %d; yOffset: %d; nGames: %d", __func__,
             cgl->curSel, cgl->yOffset, nGames );
    if ( cgl->curSel >= nGames ) {
        cgl->curSel = nGames - 1;
    }

    /* Now adjust yOffset */
    int nVisRows = cgl->height - 2; /* 1 for the title and header rows */
    if ( nGames < nVisRows ) {
        cgl->yOffset = 0;
    } else if ( cgl->curSel - cgl->yOffset >= nVisRows ) {
        cgl->yOffset = cgl->curSel - nVisRows + 1;
    } else {
        while ( cgl->curSel < cgl->yOffset ) {
            --cgl->yOffset;
        }
    }

    XP_LOGF( "%s() end: curSel: %d; yOffset: %d", __func__, cgl->curSel, cgl->yOffset );
    cgl_draw( cgl );
}

static int
countBits( int bits )
{
    int result = 0;
    while ( 0 != bits ) {
        ++result;
        bits &= bits - 1;
    }
    return result;
}

static const char*
codeToLang( XP_LangCode langCode )
{
    const char* langName = "<\?\?\?>";
    switch( langCode ) {
    case 1: langName = "English"; break;
    case 2: langName = "French"; break;
    case 3: langName = "German"; break;
    case 4: langName = "Turkish";break;
    case 5: langName = "Arabic"; break;
    case 6: langName = "Spanish"; break;
    case 7: langName = "Swedish"; break;
    case 8:langName = "Polish";; break;
    case 9: langName = "Danish"; break;
    case 10: langName = "Italian"; break;
    case 11: langName = "Dutch"; break;
    case 12: langName = "Catalan"; break;
    case 13: langName = "Portuguese"; break;
    case 15: langName = "Russian"; break;
    case 17: langName = "Czech"; break;
    case 18: langName = "Greek"; break;
    case 19: langName = "Slovak"; break;
    default:
        XP_LOGF( "%s(): bad code %d", __func__, langCode );
        break;
        // XP_ASSERT(0);
    }
    return langName;
}

void
cgl_draw( CursGameList* cgl )
{
    WINDOW* win = cgl->window;
    werase( win );

    const int nGames = g_slist_length( cgl->games );

    /* Draw '+' at far right if scrollable */
    int nBelow = nGames - (cgl->height-2) - cgl->yOffset;
    XP_LOGF( "%s(): yOffset: %d; nBelow: %d", __func__, cgl->yOffset, nBelow );
    if ( 0 < nBelow ) {
        mvwaddstr( win, cgl->height-2, cgl->width - 1, "+" );
    }
    if ( 0 < cgl->yOffset ) {
        mvwaddstr( win, 0, cgl->width-1, "+" );
    }

    const char* cols[] = {"#", "RowID", "Lang", "Scores", "GameID", "Role", "Room",
                          "nTot", "nMiss", "Seed", "#Mv", "Turn", "nPend", "DupTimer" };

    int nShown = nGames <= cgl->height - 2 ? nGames : cgl->height - 2;
    char* data[nShown + 1][VSIZE(cols)];
    for ( int ii = 0; ii < VSIZE(cols); ++ii ) {
        data[0][ii] = g_strdup(cols[ii]);
    }
    int line = 1;
    for ( int ii = 0; ii < nShown; ++ii ) {
        const GameInfo* gi = g_slist_nth_data( cgl->games, ii + cgl->yOffset );
        int col = 0;
        data[line][col++] = g_strdup_printf( "%d", ii + cgl->yOffset + 1 ); /* 1-based */
        data[line][col++] = g_strdup_printf( "%05lld", gi->rowid );
        data[line][col++] = g_strndup( codeToLang(gi->dictLang), 4 );
        data[line][col++] = g_strdup( gi->scores );
        data[line][col++] = g_strdup_printf( "%d", gi->gameID );
        data[line][col++] = g_strdup_printf( "%d", gi->role );
        data[line][col++] = g_strdup( gi->room );
        data[line][col++] = g_strdup_printf( "%d", gi->nTotal );
        data[line][col++] = g_strdup_printf( "%d", countBits(gi->nMissing) );
        data[line][col++] = g_strdup_printf( "%d", gi->seed );
        data[line][col++] = g_strdup_printf( "%d", gi->nMoves );
        data[line][col++] = g_strdup_printf( "%d", gi->turn );
        data[line][col++] = g_strdup_printf( "%d", gi->nPending );
	gchar buf[64];
	formatSeconds( gi->dupTimerExpires, buf, VSIZE(buf) );
        data[line][col++] = g_strdup( buf );

        XP_ASSERT( col == VSIZE(data[line]) );
        ++line;
    }

    int maxlen = 0;
    int offset = 0;
    for ( int col = 0; col < VSIZE(data[0]); ++col ) {
        for ( int line = 0; line < VSIZE(data); ++line ) {
            char* str = data[line][col];
            int len = strlen(str);
            if ( maxlen < len ) {
                maxlen = len;
            }
            bool highlight = cgl->yOffset + line - 1 == cgl->curSel;
            if ( highlight ) {
                wstandout( win );
            }
            mvwaddstr( win, line + 1, offset, str );
            if ( highlight ) {
                wstandend( win );
            }
            g_free( str );
        }
        offset += maxlen + 2;
        maxlen = 0;
    }

    XP_U32 relayID = linux_getDevIDRelay( cgl->params );
    char buf[cgl->width + 1];

    MQTTDevID devID;
    dvc_getMQTTDevID( cgl->params->dutil, NULL_XWE, &devID );
    XP_UCHAR didBuf[32];
    snprintf( buf, VSIZE(buf), "pid: %d; nGames: %d, relayid: %d, mqttid: %s",
              cgl->pid, nGames, relayID, formatMQTTDevID( &devID, didBuf, VSIZE(didBuf) ) );
    mvwaddstr( win, 0, 0, buf );
    
    wrefresh( win );
}

const GameInfo*
cgl_getSel( CursGameList* cgl )
{
    return g_slist_nth_data( cgl->games, cgl->curSel );
}

int
cgl_getNGames( CursGameList* cgl )
{
    return g_slist_length( cgl->games );
}
