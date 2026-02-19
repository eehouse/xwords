/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
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

/* Chat window with a scrollable list of current messages at top, then an edit
   area, and then "Send" and "Done" buttons at the bottom

 */
#include <ctype.h>

#include "curseschat.h"
#include "cursesask.h"
#include "cursesdlgutil.h"

typedef enum { EDIT, DONE, SEND, NSTATES, } FOCUSSED;

typedef struct _State {
    LaunchParams* params;
    XW_DUtilCtxt* dutil;
    GameRef gr;

    WINDOW* win;
    bool done;
    FOCUSSED focussed;

    int msgLine;
    EditState es;

    int buttonLine;
} State;

static void
upFocus( State* state)
{
    state->focussed = (state->focussed + 1) % NSTATES;
}

static int
drawChats( State* state )
{
    XW_UtilCtxt* util = gr_getUtil( state->dutil, state->gr, NULL_XWE );
    XP_U16 nChats = gr_getChatCount( state->dutil, state->gr, NULL_XWE );
    int line = 1;
    for ( XP_U16 ii = 0; ii < nChats; ++ii ) {
        XP_UCHAR buf[1024];
        XP_U16 len = VSIZE(buf);
        XP_S16 from;
        XP_U32 timestamp;
        gr_getNthChat( state->dutil, state->gr, NULL_XWE, ii, buf,
                       &len, &from, &timestamp, XP_TRUE );

        const CurGameInfo* gi = util_getGI(util);
        gchar header[128];
        snprintf( header, VSIZE(header), "from: %s; ts: %d",
                  gi->players[from].name, timestamp );
        mvwaddstr( state->win, line++, 1, header );

        mvwaddstr( state->win, line++, 2, buf );
    }
    util_unref( util, NULL_XWE );
    return line;
}

static void
handleSend( State* state, int ch )
{
    if ( ch == '\r' || ch == '\n' ) {
        XP_LOGFF("calling gr_sendChat()");
        // es->msgBuf[es->offset] = '\0';
        gchar msg[64];
        size_t len = VSIZE(msg);
        getEditText( &state->es, msg, &len );

        const char* buttons[] = { "Yes", "No" };
        CursesAppGlobals* aGlobals = (CursesAppGlobals*)state->params->cag;
        int res = cursesaskf2( aGlobals, state->win, VSIZE(buttons), buttons,
                              "Are you sure you want to send message \"%s\"",
                               msg );
        if ( res == 0 ) {
            gr_sendChat( state->dutil, state->gr, NULL_XWE, msg );
        }
        initEdit( &state->es, state->win, state->msgLine, NULL );
        upFocus( state );
    }
}

static void
handleDone( State* state, int ch )
{
    state->done = ch == '\r' || ch == '\n';
}

static void
updateButtons( State* state )
{
    const char* buttons[] = { "Done", "Send" };
    int sel = state->focussed == EDIT ? -1 : state->focussed - 1;
    drawButtons( state->win, state->buttonLine, 10, VSIZE(buttons), sel, buttons );
}

static void
drawWin( State* state )
{
    updateButtons( state );
    drawEdit( &state->es, EDIT == state->focussed );
    wrefresh( state->win );
}

/* Refactored at a ryokan outside of Morioka :-) */
static bool
chatKeyProc( int key, void* closure )
{
    State* state = (State*)closure;
    if ( '\t' == key ) {
        upFocus( state );
    } else {
        switch ( state->focussed ) {
        case EDIT:
            handleEdit( &state->es, key );
            break;
        case DONE:
            handleDone( state, key );
            break;
        case SEND:
            handleSend( state, key );
            break;
        default:
            XP_ASSERT(0);
        }
    }
    drawWin( state );

    return state->done;
}

void
curses_openChat( LaunchParams* params, WINDOW* parent, GameRef gr )
{
    XW_DUtilCtxt* dutil = params->dutil;
    LOG_FUNC();
    State state = {.dutil = dutil, .gr = gr, .params = params, };
    
    int chatCols = 40;
    int chatLines = 2 * gr_getChatCount( dutil, gr, NULL_XWE );
    chatLines += 6; // for buttons and msg edit space

    state.win = makeCenteredBox( parent, chatCols, chatLines );
    // keypad( state.win, TRUE );

    int line = drawChats( &state );
    mvwaddstr( state.win, ++line, 1, "Edit message below" );
    state.msgLine = ++line;

    state.buttonLine = ++line;

    initEdit( &state.es, state.win, state.msgLine, NULL );
    drawWin( &state );
    // drawEdit( &state.es, EDIT == state.focussed );

    CursesAppGlobals* aGlobals = (CursesAppGlobals*)params->cag;
    startModalAlert( aGlobals, state.win, XP_TRUE, chatKeyProc, &state );
    
    // wtouchln( window, parentY, chatLines, 1 );
    delwin( state.win );
    touchwin( parent );
    wrefresh( parent );
}
