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

#ifdef PLATFORM_NCURSES

#include <ncurses.h>

#include "cursesedit.h"
#include "comtypes.h"
#include "cursesdlgutil.h"
#include "dbgutil.h"

typedef enum { EDIT, DONE, CANCEL, NSTATES, } FOCUSSED;

typedef struct _EditStrState {
    WINDOW* win;

    /* From the caller: result goes here */
    gchar* txt;
    gint maxLen;

    int editLine;
    int buttonLine;
    EditState es;
    FOCUSSED focussed;
    bool done, accepted;
} EditStrState;

static void
handleDone( EditStrState* state, int ch )
{
    state->done = ch == '\r' || ch == '\n';
    if ( state->done ) {
        state->accepted = DONE == state->focussed;
        if ( state->accepted ) {
            size_t maxLen = state->maxLen;
            getEditText( &state->es, state->txt, &maxLen );
        }
    }
}

static void
upFocus( EditStrState* state)
{
    state->focussed = (state->focussed + 1) % NSTATES;
}

static void
updateButtons( EditStrState* state )
{
    const char* buttons[] = { "OK", "Cancel" };
    int sel = state->focussed == EDIT ? -1 : state->focussed - 1;
    drawButtons( state->win, state->buttonLine, 10, VSIZE(buttons), sel, buttons );
}

static bool
onKeyProc( int ch, void* closure )
{
    XP_LOGFF("(key: %d)", ch );
    EditStrState* state = (EditStrState*)closure;

    if ( '\t' == ch ) {
        upFocus( state );
    } else {
        switch ( state->focussed ) {
        case EDIT:
            handleEdit( &state->es, ch );
            break;
        case DONE:
        case CANCEL:
            handleDone( state, ch );
            break;
        default:
            XP_ASSERT(0);
        }
    }

    if ( !state->done ) {
        updateButtons( state );
        drawEdit( &state->es, EDIT == state->focussed );
    }

    return state->done;
}

bool
ca_edit( LaunchParams* params, WINDOW* parent, const char* prompt,
         gchar txt[], gint maxLen )
{
    int line = 1;
    int promptLine = line++;
    EditStrState state = { .txt = txt,
                           .maxLen = maxLen,
    };
    state.editLine = line++;
    state.buttonLine = line++;
    int height = line + 1;
    int width = maxLen + 4;
    state.win = makeCenteredBox( parent, width, height );

    mvwaddstr( state.win, promptLine, 1, prompt );

    initEdit( &state.es, state.win, state.editLine, txt );
    drawEdit( &state.es, true );
    updateButtons( &state );

    CursesAppGlobals* aGlobals = (CursesAppGlobals*)params->cag;
    startModalAlert( aGlobals, state.win, XP_TRUE, onKeyProc, &state );

    // wtouchln( parent, parentY, height, 1 );
    wrefresh( parent );

    LOG_RETURNF( "%s", boolToStr(state.accepted) );
    return state.accepted;
}

#endif
