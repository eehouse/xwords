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

#include "gamemgr.h"
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
    GSList* positions;
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
    if ( !!cgl->window ) {
        g_slist_free( cgl->positions );
        delwin( cgl->window );
    } else {
        XP_LOGFF( "no window??" );
    }
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

static ForEachAct
onPosProc( void* elem, void* closure, XWEnv XP_UNUSED(xwe) )
{
    CursGameList* cgl  = (CursGameList*)closure;
    GLItemRef ir = (GLItemRef)elem;
    cgl->positions = g_slist_append( cgl->positions, (void*)ir );
    return FEA_OK;
}

/* Load from the DB */
void
cgl_refresh( CursGameList* cgl )
{
    g_slist_free( cgl->positions );
    cgl->positions = NULL;

    XWArray* positions = gmgr_getPositions(cgl->params->dutil, NULL_XWE );
    arr_map( positions, NULL_XWE, onPosProc, cgl );
    arr_destroy( positions );
    cgl_draw( cgl );
}

static GSList*
findFor( CursGameList* cgl, GameRef gr )
{
    GSList* result = NULL;
    for ( GSList* iter = cgl->positions; !!iter && !result; iter = iter->next ) {
        GLItemRef ir = (GLItemRef)iter->data;
        if ( gmgr_isGame( ir ) ) {
            GameRef one = gmgr_toGame(ir);
            if ( one == gr ) {
                result = iter;
            }
        }
    }
    return result;
}

void
cgl_refreshOne( CursGameList* cgl, GameRef XP_UNUSED_DBG(gr),
                bool XP_UNUSED(select) )
{
    // This is a no-op unless we start storing stuff
#ifdef DEBUG
    GSList* elem = findFor( cgl, gr );
    XP_ASSERT( !!elem );
#endif
    adjustCurSel( cgl );
}

void
cgl_remove( CursGameList* cgl, GameRef gr )
{
    XP_LOGFF( "before: %d", g_slist_length( cgl->positions ) );
    GSList* elem = findFor( cgl, gr );
    XP_ASSERT( !!elem );
    if ( !!elem ) {
        cgl->positions = g_slist_delete_link( cgl->positions, elem );
    }
    adjustCurSel( cgl );
    XP_LOGFF( "after: %d", g_slist_length( cgl->positions ) );
}

void
cgl_moveSel( CursGameList* cgl, bool down )
{
    int nGames = g_slist_length( cgl->positions );
    cgl->curSel += nGames + (down ? 1 : -1);
    cgl->curSel %= nGames;
    adjustCurSel( cgl );
}

static void
adjustCurSel( CursGameList* cgl )
{
    int nGames = g_slist_length( cgl->positions );
    XP_LOGFF( "start: curSel: %d; yOffset: %d; nGames: %d",
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

void
cgl_draw( CursGameList* cgl )
{
    WINDOW* win = cgl->window;
    werase( win );

    const int nGames = g_slist_length( cgl->positions );
    XP_LOGFF( "nGames: %d", nGames );

    /* Draw '+' at far right if scrollable */
    int nBelow = nGames - (cgl->height-2) - cgl->yOffset;
    XP_LOGF( "%s(): yOffset: %d; nBelow: %d", __func__, cgl->yOffset, nBelow );
    if ( 0 < nBelow ) {
        mvwaddstr( win, cgl->height-2, cgl->width - 1, "+" );
    }
    if ( 0 < cgl->yOffset ) {
        mvwaddstr( win, 0, cgl->width-1, "+" );
    }

    const char* cols[] = {"#", "Name", "Turn", "Lang", "GameID", "Opponents",
                          "Role", "nTot", "nMoves", "Chats", };

    int nShown = nGames <= cgl->height - 2 ? nGames : cgl->height - 2;
    char* data[nShown + 1][VSIZE(cols)];

    /* Lay down the column heads (just one, for all games and groups, for
       now) */
    for ( int ii = 0; ii < VSIZE(cols); ++ii ) {
        data[0][ii] = g_strdup(cols[ii]);
    }

    int line = 1;
    LaunchParams* params = cgl->params;
    for ( int ii = 0; ii < nShown; ++ii ) {
        GLItemRef ir = (GLItemRef)g_slist_nth_data( cgl->positions, ii + cgl->yOffset );
        int col = 0;
        data[line][col++] = g_strdup_printf( "%d", ii + cgl->yOffset + 1 ); /* 1-based */
        if ( gmgr_isGame( ir) ) {
            GameRef gr = gmgr_toGame( ir );
            XP_ASSERT( gr );
            const CurGameInfo* gi = gr_getGI( params->dutil, gr, NULL_XWE );
            const GameSummary* sum = gr_getSummary( params->dutil, gr, NULL_XWE );
            data[line][col++] = g_strdup( gi->gameName );
            data[line][col++] = g_strdup( sum->turnIsLocal ? "Local":"Remote" );
            data[line][col++] = g_strdup( gi->isoCodeStr );
            data[line][col++] = g_strdup_printf( "%x", gi->gameID );
            data[line][col++] = g_strdup_printf( "%s", sum->opponents );
            data[line][col++] = g_strdup_printf( "%d", gi->deviceRole );
            data[line][col++] = g_strdup_printf( "%d", gi->nPlayers );
            data[line][col++] = g_strdup_printf( "%d", sum->nMoves );
            data[line][col++] = g_strdup_printf( "%d", gr_getChatCount(params->dutil,
                                                                   gr, NULL_XWE ));
        } else {
            GroupRef grp = gmgr_toGroup( ir );
            XP_UCHAR name[36];
            XP_U16 bufLen = VSIZE(name);
            gmgr_getGroupName( params->dutil, NULL_XWE, grp, name, bufLen );
            data[line][col++] = g_strdup_printf( ">%s<", name );
            XP_U32 count = gmgr_getGroupGamesCount(params->dutil, NULL_XWE, grp);
            data[line][col++] = g_strdup_printf( ">%d<", count );
        }
        while ( col < VSIZE(data[line]) ) {
            data[line][col++] = NULL;
        }
        XP_ASSERT( col == VSIZE(data[line]) );
        ++line;
    }

    int maxlen = 0;
    int offset = 0;
    for ( int col = 0; col < VSIZE(data[0]); ++col ) {
        for ( int line = 0; line < VSIZE(data); ++line ) {
            char* str = data[line][col];
            if ( !!str ) {
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
        }
        offset += maxlen + 2;
        maxlen = 0;
    }

    char buf[cgl->width + 1];

    MQTTDevID devID;
    dvc_getMQTTDevID( params->dutil, NULL_XWE, &devID );
    XP_UCHAR didBuf[32];
    gchar* dbName = g_path_get_basename(params->dbName);
    snprintf( buf, VSIZE(buf), "pid: %d; nGames: %d; mqttid: %s;"
#ifdef XWFEATURE_SMS
                        " phone: %s;"
#endif
                        " db: %s",
                        cgl->pid, nGames, formatMQTTDevID( &devID, didBuf,
                                                           VSIZE(didBuf) ),
#ifdef XWFEATURE_SMS
                        params->connInfo.sms.myPhone,
#endif
                        dbName );
    mvwaddstr( win, 0, 0, buf );
    g_free( dbName );
    
    wrefresh( win );
}

void
cgl_getSel( CursGameList* cgl, GameRef* gr, GroupRef* grp )
{
    GLItemRef ir = (GLItemRef)g_slist_nth_data( cgl->positions, cgl->curSel );
    if ( gmgr_isGame(ir) )  {
        if ( !!gr ) {
            *gr = gmgr_toGame(ir);
        }
        if ( !!grp ) {
            *grp = (GroupRef)0;
        }
    } else {
        if ( !!grp ) {
            *grp = gmgr_toGroup(ir);
        }
        if ( !!gr ) {
            *gr = (GameRef)0;
        }
    }
}

void
cgl_setSel( CursGameList* cgl, int sel )
{
    if ( sel < 0 ) {
        sel = XP_RANDOM() % g_slist_length( cgl->positions );
    }
    cgl->curSel = sel;
    adjustCurSel( cgl );
}

int
cgl_getNGames( CursGameList* cgl )
{
    int len = g_slist_length( cgl->positions );
    XP_LOGFF( "() => %d", len );
    return len;
}
