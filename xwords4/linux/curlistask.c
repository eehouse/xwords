/* Copyright 2026 by Eric House (xwords@eehouse.org).  All rights reserved.
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

/* Returns false if cancelled, otherwise the index 0 <= i < count */
#include "cursesdlgutil.h"
#include "strutils.h"
#include "cursesask.h"

#define KPCOLS 30

typedef struct _ListAskState {
    LaunchParams* params;
    WINDOW* win;
    const char* expl;
    const XP_UCHAR** items;
    XP_U16 count;
    int sel;
    int* resultP;
    bool confirmed;
} ListAskState;

static void
drawLASWin( ListAskState* las )
{
    WINDOW* win = las->win;
    mvwaddstr( win, 1, 1, las->expl );
    for ( int ii = 0; ii < las->count; ++ii ) {
        bool isSel = ii == las->sel;
        if ( isSel ) {
            wstandout( win );
        }
        mvwaddstr( win, 2+ii, 1, las->items[ii] );
        XP_LOGFF( "drew %s at line %d", las->items[ii], 2+ii );
        if ( isSel ) {
            wstandend( win );
        }
    }
    const char* buttons[] = { "Cancel" };
    int focussed = las->count == las->sel ? 0 : -1;
    drawButtons( win, las->count + 2, 10, VSIZE(buttons), focussed, buttons );

    wrefresh( las->win );
}

static void
moveSel( ListAskState* las, int by )
{
    int count = las->count + 1;
    las->sel = (las->sel + count + by) % count;
}

static bool
claKeyProc( int key, void* closure )
{
    bool done = false;
    ListAskState* las = (ListAskState*)closure;
    switch ( key ) {
    case '\t':
        moveSel( las, 1 );
        break;
    case 0x161:                 /* shift-tab */
        moveSel( las, -1 );
        break;
    case '\r':
    case '\n':
        done = true;
        if ( las->sel < las->count ) {
            *las->resultP = las->sel;
            las->confirmed = true;
        }
        break;
    }
    drawLASWin( las );
    return done;
}

bool
curAskPickList( LaunchParams* params, WINDOW* parent, const char* expl,
                const char** choices, int count, int* chosen )
{
    XP_LOGFF( "count: %d", count );
    bool success = false;
    ListAskState las = {
        .expl = expl,
        .params = params,
        .win = makeCenteredBox( parent, KPCOLS, count + 4 ),
        .count = count,
        .items = choices,
        .resultP = chosen,
    };
    drawLASWin( &las );

    CursesAppGlobals* aGlobals = (CursesAppGlobals*)params->cag;
    startModalAlert( aGlobals, las.win, XP_TRUE, claKeyProc, &las );

    delwin( las.win );
    success = las.confirmed;

    return success;
}

#endif



