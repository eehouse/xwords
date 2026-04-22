/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2000 by Eric House (xwords@eehouse.org).  All rights reserved.
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

/* Put up a "dialog" with a question and tab-selectable buttons below.
 * On <CR>, return the number of the button selected at the time.
 */

#ifdef PLATFORM_NCURSES

#include <ncurses.h>

#include "cursesask.h"
#include "cursesdlgutil.h"
#include "curwinstk.h"
#include "dbgutil.h"

typedef struct _AskState {
    CursesAppGlobals* aGlobals;
    WINDOW* win;
    int newSelButton;
    int curSelButton;
    int numButtons;
    int spacePerButton;
    int rows;
    int xx, yy;
    int nLines;
    const char** buttons;
    int* resultP;
    guint timerSrc;
} AskState;

static bool
onKey( int ch, void* closure )
{
    bool dismissed = false;
    AskState* as = (AskState*)closure;

    switch ( ch ) {
    case 'L':
    case KEY_RIGHT:
    case 525:
        as->newSelButton = (as->curSelButton+1) % as->numButtons;
        break;
    case 'H':
    case '\t':
    case KEY_LEFT:
    case 524:
        as->newSelButton = (as->numButtons+as->curSelButton-1) % as->numButtons;
        break;
    case EOF:
    case 4:			/* C-d */
    case 27:		/* ESC */
        as->curSelButton = 0;	/* should be the cancel case */
    case KEY_B2:                /* "center of keypad" */
    case '\r':
    case '\n':
        dismissed = XP_TRUE;
        break;
    case '1':
    case '2':
    case '3':
    case '4': {
        int num = ch - '1';
        if ( num < as->numButtons ) {
            as->newSelButton = num;
        }
    }
        break;
    default:
        beep();
    }

    if ( dismissed ) {
        cws_delwin( as->aGlobals, &as->win );

        if ( !!as->resultP ) {
            *as->resultP = as->curSelButton;
        }
        if ( !!as->timerSrc ) {
            g_source_remove( as->timerSrc );
        }

        g_free( as );
    } else if ( as->newSelButton != as->curSelButton ) {
        drawButtons( as->win, as->rows+1, as->spacePerButton, as->numButtons,
                     as->curSelButton=as->newSelButton, as->buttons );
        wrefresh( as->win );
    }

    LOG_RETURNF( "%s", boolToStr(dismissed) );
    return dismissed;
}

static gint
askTimerProc( gpointer data )
{
    AskState* as = (AskState*)data;
    XP_ASSERT( as->timerSrc );
    as->timerSrc = 0;

    cursesPushKey( as->aGlobals, '\n' );

    return G_SOURCE_REMOVE;
}

/* Figure out how many lines there are and how wide the widest is.
 */
static void
cursesaskImpl( CursesAppGlobals* aGlobals, int* resultP, short numButtons,
               const char** buttons, const char* question, int timeoutms )
{
    LOG_FUNC();
    AskState* as = g_malloc0( sizeof(*as) );
    as->aGlobals = aGlobals;
    as->buttons = buttons;
    as->numButtons = numButtons;
    as->resultP = resultP;

    XP_LOGFF( "(question=%s)", question );
    int row;
    int left, top;
    short maxWidth;
    FormatInfo fi;
    int len;

    WINDOW* win = getMainWin(aGlobals);
    XP_ASSERT( !!win );
    getmaxyx( win, as->yy, as->xx);
    getbegyx( win, top, left );

    measureAskText( question, as->xx-2, &fi );
    len = fi.maxLen;
    if ( len < MIN_WIDTH ) {
        len = MIN_WIDTH;
    }

    as->rows = fi.nLines;
    maxWidth = as->xx - (PAD*2) - 2; /* 2 for two borders */

    if ( len > as->xx-2 ) {
        as->rows = (len / maxWidth) + 1;
        len = maxWidth;
    }

    as->nLines = ASK_HEIGHT + as->rows - 1;
    as->win = cws_newwin( as->aGlobals, as->nLines, len+(PAD*2),
                          top + ((as->yy/2) - (as->nLines/2)),
                          left + ((as->xx-len-2)/2) );
    keypad( as->win, TRUE );
    wclear( as->win );
    box( as->win, '|', '-');

    for ( row = 0; row < as->rows; ++row ) {
        mvwaddnstr( as->win, row+1, PAD,
                    fi.line[row].substr, fi.line[row].len );
    }
    as->spacePerButton = (len+(PAD*2)) / (numButtons + 1);

    drawButtons( as->win, as->rows+1, as->spacePerButton, numButtons,
                 as->curSelButton=as->newSelButton, as->buttons );

    if ( timeoutms ) {
        XP_ASSERT( !as->timerSrc );
        as->timerSrc = g_timeout_add( timeoutms, askTimerProc, as );
    }

    startModalAlert( aGlobals, as->win, !!resultP, onKey, as );

    LOG_RETURN_VOID();
} /* cursesaskImpl */

int
cursesask( CursesAppGlobals* ag, short numButtons,
           const char** buttons, const char* question )
{
    int result;
    cursesaskImpl( ag, &result, numButtons, buttons, question, 0 );
    return result;
}

int
cursesaskf( CursesAppGlobals* aGlobals,  short numButtons,
            const char** buttons, const char* fmt, ... )
{
    va_list args;
    va_start( args, fmt );
    gchar* msg = g_strdup_vprintf( fmt, args );

    int result = cursesask( aGlobals, numButtons, buttons, msg );

    g_free( msg );
    return result;
}

void
ca_inform( CursesAppGlobals* aGlobals, const char* message )
{
    if ( !!getMainWin(aGlobals) ) {
        const char* buttons[] = { "Ok" };
        (void)cursesaskImpl( aGlobals, NULL, VSIZE(buttons), buttons,
                             message, /*1500*/ 0 );
    }
    LOG_RETURN_VOID();
}

void
ca_informf( CursesAppGlobals* aGlobals, const char* fmt, ... )
{
    va_list args;
    va_start( args, fmt );
    gchar* msg = g_strdup_vprintf( fmt, args );
    va_end( args );
    ca_inform( aGlobals, msg );
    g_free( msg );
}

#endif /* PLATFORM_NCURSES */
