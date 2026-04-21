/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */ 
/* 
 * Copyright 2026 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "curwinstk.h"
#include "cursesmain.h"

// #define FILL_WINS 1

typedef struct _CWSElem {
    WINDOW* win;
} CWSElem;

struct CurWinStack {
    int nxtFill;
    GSList* wins;
};

static CurWinStack*
getStack( CursesAppGlobals* aGlobals )
{
    CurWinStack* stack = getWinStack( aGlobals );
    if ( !stack ) {
        stack = g_malloc0( sizeof(*stack) );
        setWinStack( aGlobals, stack );
    }
    return stack;
}

WINDOW*
cws_newwin( CursesAppGlobals* aGlobals, int nlines, int ncols,
            int begin_y, int begin_x )
{
    XP_USE( aGlobals );
    CurWinStack* ws = getStack( aGlobals );
    WINDOW* win = newwin( nlines, ncols, begin_y, begin_x );

    CWSElem* elem = g_malloc0( sizeof(*elem) );
    elem->win = win;
    ws->wins = g_slist_append( ws->wins, elem );
    XP_LOGFF( "now have %d wins in stack", g_slist_length(ws->wins) );
    
#ifdef FILL_WINS
    wbkgd( win, 'a' + ws->nxtFill++ );
#else
    XP_USE( ws );
#endif
    return win;
}

void
cws_delwin( CursesAppGlobals* aGlobals, WINDOW** winp )
{
    if ( !!*winp ) {
        bool found = false;
        CurWinStack* ws = getStack( aGlobals );
        for ( GSList* iter = ws->wins; !!iter; iter = iter->next ) {
            CWSElem* elem = (CWSElem*)iter->data;
            if ( *winp == elem->win ) {
                ws->wins = g_slist_remove( ws->wins, elem );
                found = true;
                break;
            }
        }
        XP_ASSERT( found );
        XP_LOGFF( "now have %d wins in stack", g_slist_length(ws->wins) );
        
        delwin( *winp );
        *winp = NULL;

        cws_refresh( aGlobals );
    }
}

void
cws_refresh( CursesAppGlobals* aGlobals )
{
    CurWinStack* ws = getStack( aGlobals );
    for ( GSList* iter = ws->wins; !!iter; iter = iter->next ) {
        WINDOW* win = ((CWSElem*)iter->data)->win;
        touchwin( win );
        wnoutrefresh( win );
    }

    doupdate();
}
