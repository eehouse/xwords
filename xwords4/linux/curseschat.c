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

typedef struct _State {
    XW_DUtilCtxt* dutil;
    GameRef gr;

    WINDOW* win;
    bool done;
    int focussed;

    int msgLine, msgCol;
    gchar msgBuf[256];
    int offset;

    int buttonLine;
} State;

enum { EDIT, DONE, SEND, NSTATES, };

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
initEdit( State* state )
{
    mvwaddnstr( state->win, state->msgLine, state->msgCol, " ", VSIZE(state->msgBuf) );
    state->msgBuf[0] = '\0';
    state->offset = 0;
}

static void
drawEdit( State* state )
{
    WINDOW* win = state->win;
    if ( EDIT == state->focussed ) {
        wstandout( win );
    }
    const gchar* prompt = "->";
    state->msgCol = 1 + strlen(prompt);
    mvwaddstr( win, state->msgLine, 1, prompt );
    mvwaddstr( win, state->msgLine, state->msgCol, state->msgBuf );
    if ( EDIT == state->focussed ) {
        wstandend( win );
    }
}

static void
handleEdit( State* state, int ch )
{
    if ( isprint(ch) && state->offset < VSIZE(state->msgBuf) - 1) {
        state->msgBuf[state->offset++] = ch;
        state->msgBuf[state->offset] = '\0';
    } else if ( ch == 263 && state->offset > 2 ) {
        XP_LOGFF( "ch: %d", ch );
        state->msgBuf[--state->offset] = ' ';
    }
}

static void
handleSend( State* state, int ch )
{
    if ( ch == '\r' || ch == '\n' ) {
        XP_LOGFF("calling gr_sendChat()");
        state->msgBuf[state->offset] = '\0';

        mvwaddnstr( state->win, state->msgLine, state->msgCol, " ", state->offset );

        const char* buttons[] = { "Yes", "No" };
       int res = cursesaskf( state->win, VSIZE(buttons), buttons,
                              "Are you sure you want to send message \"%s\"",
                              state->msgBuf );
        if ( res == 0 ) {
            gr_sendChat( state->dutil, state->gr, NULL_XWE, state->msgBuf );
        }
        initEdit( state );
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

void
curses_openChat( WINDOW* window, XW_DUtilCtxt* dutil, GameRef gr )
{
    LOG_FUNC();
    State state = {.dutil = dutil, .gr = gr, };
    
    int parentX, parentY;
    getmaxyx( window, parentY, parentX );
    parentX /= 2;
    parentY /= 2;

    int chatCols = 40;
    int chatLines = 2 * gr_getChatCount( dutil, gr, NULL_XWE );
    chatLines += 6; // for buttons and msg edit space

    state.win = newwin( chatLines, chatCols, parentY, parentX );
    keypad( state.win, TRUE );
    wclear( state.win );
    box( state.win, '|', '-');

    int line = drawChats( &state );
    mvwaddstr( state.win, ++line, 1, "Edit message below" );
    state.msgLine = ++line;

    state.buttonLine = ++line;
    
    initEdit( &state );
    drawEdit( &state );
    while ( !state.done ) {
        updateButtons( &state );
        drawEdit( &state );
        
        int ch = wgetch( state.win );
        if ( '\t' == ch ) {
            upFocus( &state );
        } else {
            switch ( state.focussed ) {
            case EDIT:
                handleEdit( &state, ch );
                break;
            case DONE:
                handleDone( &state, ch );
                break;
            case SEND:
                handleSend( &state, ch );
                break;
            default:
                XP_ASSERT(0);
            }
        }
    }

    wtouchln( window, parentY, chatLines, 1 );
    wrefresh( window );
}
