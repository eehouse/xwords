/* 
 * Copyright 2025 by Eric House (xwords@eehouse.org).  All rights reserved.
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


#ifdef PLATFORM_NCURSES

#include <ncurses.h>
#include <ctype.h>

#include "curnewgame.h"
#include "cursesdlgutil.h"
#include "curplyr.h"

typedef struct _NGState {
    LaunchParams* params;
    CurGameInfo gi;
    WINDOW* win;
    bool confirmed, cancelled;
    int sel;
    int playerLines;
    int buttonLine;
    int width;
} NGState;

enum {
    SEL_PLAYER_1,
    SEL_PLAYER_2,
    SEL_PLAYER_3,
    SEL_PLAYER_4,

    SEL_MQTT,
    SEL_SMS,
    SEL_BT,

    SEL_ADD,
    SEL_DELETE,
    SEL_CANCEL,
    SEL_OK,

    SEL_NSELS,
};

#define ADDRS_LINE 7

static void
drawPlayer( NGState* ngs, int indx )
{
    CurGameInfo* gi = &ngs->gi;
    WINDOW* win = ngs->win;

    wmove( win, 1 + indx, 1 );
    wclrtoeol( win );

    if ( indx < gi->nPlayers ) {
        bool focussed = ngs->sel == SEL_PLAYER_1 + indx;
        if ( focussed ) {
            wstandout( win );
        }
        mvwaddstr( win, 1 + indx, 1, gi->players[indx].name );

        char role[32];
        roleName( &gi->players[indx], role, VSIZE(role) );
        mvwaddstr( win, 1 + indx, 30, role );

        if ( focussed ) {
            wstandend( win );
        }
    }
}

static void
updatePlayers( NGState* ngs )
{
    for ( int ii = 0; ii < MAX_NUM_PLAYERS; ++ii ) {
        drawPlayer( ngs, ii );
    }
}

static void
updateAddrs( NGState* ngs )
{
    char buf[32];
    int offset = snprintf( buf, VSIZE(buf), "conn: " );

    CommsConnType typ;
    for ( XP_U32 state = 0; types_iter( ngs->gi.conTypes, &typ, &state ); ) {
        const char* add = NULL;
        switch ( typ ) {
        case COMMS_CONN_SMS:
            add = "SMS"; break;
        case COMMS_CONN_MQTT:
            add = "MQTT"; break;
        case COMMS_CONN_BT:
            add = "BT"; break;
        default:
            XP_ASSERT(0);
            break;
        }
        if ( !!add ) {
            offset += snprintf( buf+offset, VSIZE(buf)-offset, " %s", add );
        }
    }

    int line = ADDRS_LINE;
    wmove( ngs->win, line, 1 );
    wclrtoeol( ngs->win );
    mvwaddstr( ngs->win, line, 1, buf );
}

static void
updateButtons( NGState* ngs )
{
    int sel;

    switch ( ngs->sel ) {
    case SEL_MQTT:
    case SEL_SMS:
    case SEL_BT:
        sel = ngs->sel - SEL_MQTT;
        break;
    default: sel = -1; break;
    }
    const char* conns[] = { "MQTT", "SMS", "BT" };
    drawButtons( ngs->win, ADDRS_LINE+1, 8, VSIZE(conns), sel, conns );
    
    switch ( ngs->sel ) {
    case SEL_ADD:
    case SEL_DELETE:
    case SEL_OK:
    case SEL_CANCEL:
        sel = ngs->sel - SEL_ADD;
        break;
    default: sel = -1; break;
    }

    const char* buttons[] = { "Add", "Delete", "Cancel", "Ok" };
    drawButtons( ngs->win, ngs->buttonLine, 8, VSIZE(buttons), sel, buttons );
}

static void
drawWindow( NGState* ngs )
{
    updatePlayers( ngs );
    updateAddrs( ngs );
    updateButtons( ngs );
    wrefresh( ngs->win );
}

static void
upSel( NGState* ngs, int by )
{
   /* Selectables: players that exist; buttons? */
    int nPlayers = ngs->gi.nPlayers;
    int sel = ngs->sel;
    for ( ; ; ) {
        sel = (sel + SEL_NSELS + by) % SEL_NSELS;
        switch ( sel ) {
        case SEL_PLAYER_1:
        case SEL_PLAYER_2:
        case SEL_PLAYER_3:
        case SEL_PLAYER_4:
            if (nPlayers < sel - SEL_PLAYER_1 + 1 ) {
                continue;
            }
            break;
        }
        break;
    }
    ngs->sel = sel;
}

static void
toggleConn( NGState* ngs, CommsConnType typ )
{
    if ( types_hasType( ngs->gi.conTypes, typ ) ) {
        types_rmType( &ngs->gi.conTypes, typ );
    } else {
        types_addType( &ngs->gi.conTypes, typ );
    }
}

static bool
newGameKeyProc( int key, void* closure )
{
    NGState* ngs = (NGState*)closure;
    CurGameInfo* gi = &ngs->gi;
    int sel = ngs->sel;
    XP_LOGFF( "key: %x", key );
    switch ( key ) {
    case 0x161:                 /* shift-tab */
        upSel( ngs, -1 );
        break;
    case '\t':
        upSel( ngs, 1 );
        break;
    case '\n':
    case '\r':
        switch ( sel ) {
        case SEL_MQTT: toggleConn( ngs, COMMS_CONN_MQTT ); break;
        case SEL_SMS: toggleConn( ngs, COMMS_CONN_SMS ); break;
        case SEL_BT: toggleConn( ngs, COMMS_CONN_BT ); break;

        case SEL_CANCEL:
            ngs->cancelled = XP_TRUE;
            break;
        case SEL_OK:
            ngs->confirmed = XP_TRUE;
            break;
        case SEL_ADD:
            if ( gi->nPlayers < MAX_NUM_PLAYERS ) {
                ++gi->nPlayers;
            }
            break;
        case SEL_DELETE:
            if ( 0 < gi->nPlayers ) {
                --gi->nPlayers;
            }
            break;
        case SEL_PLAYER_1:
        case SEL_PLAYER_2:
        case SEL_PLAYER_3:
        case SEL_PLAYER_4: {
            LocalPlayer* lp = &gi->players[sel - SEL_PLAYER_1];
            XP_LOGFF( "passing %s", lp->name );
            if ( editPlayerDlg(ngs->params, ngs->win, lp ) ) {
                XP_LOGFF( "got back: %s", lp->name );
            }
        }
            break;
        }
    }
    drawWindow( ngs );
    return ngs->confirmed || ngs->cancelled;                /* finished? */
}

static void
initDefaults( NGState* ngs )
{
    CurGameInfo* gi = &ngs->gi;
    for ( int ii = 0; ii < VSIZE(gi->players); ++ii ) {
        LocalPlayer* lp = &gi->players[ii];
        if ( ii == 1 ) {
            XP_SNPRINTF( lp->name, VSIZE(lp->name), "%s", "Robot" );
        } else {
            XP_SNPRINTF( lp->name, VSIZE(lp->name), "LinUser %d", ii + 1 );
        }
        lp->isLocal = XP_TRUE;
        lp->robotIQ = ii == 1 ? 1 : 0;
    }
    gi->nPlayers = 2;
}

bool
curNewGameDialog( WINDOW* parent, LaunchParams* params, CurGameInfo* gi,
                  CommsAddrRec* XP_UNUSED(addr), XP_Bool isNewGame,
                  XP_Bool XP_UNUSED(fireConnDlg) )
{
    bool confirmed = false;

    XP_ASSERT(isNewGame);
    NGState ngs = {
        .params = params,
        .gi = *gi,
        // .addr = *addr,
        .buttonLine = 10,
        .width = 50,
    };

    if ( isNewGame ) {
        ngs.win = makeCenteredBox( parent, ngs.width, 20 );
        ngs.playerLines = 2;
        initDefaults( &ngs );

        drawWindow( &ngs );

        CursesAppGlobals* aGlobals = (CursesAppGlobals*)params->cag;
        startModalAlert( aGlobals, ngs.win, XP_TRUE, newGameKeyProc, &ngs );

        delwin( ngs.win );

        confirmed = ngs.confirmed;
        if ( confirmed ) {
            CurGameInfo* gip = &ngs.gi;
            gip->deviceRole = ROLE_STANDALONE;
            for ( int ii = 0; ii < gip->nPlayers; ++ii ) {
                if ( !gip->players[ii].isLocal ) {
                    gip->deviceRole = ROLE_ISHOST;
                    break;
                }
            }
            *gi = ngs.gi;
        }
    }

    return confirmed;
}

#endif /* PLATFORM_NCURSES */
