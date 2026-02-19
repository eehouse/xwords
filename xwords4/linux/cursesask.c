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
#include "dbgutil.h"

typedef struct _AskState {
    CursesAppGlobals* aGlobals;
    WINDOW* win;
    WINDOW* parentWin;
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
        delwin( as->win );
        as->win = NULL;

        /* this leaves a ghost line, but I can't figure out a better way. */
        wtouchln( as->parentWin, (as->yy/2)-(as->nLines/2), ASK_HEIGHT + as->rows - 1, 1 );
        wrefresh( as->parentWin );

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
cursesaskImpl( CursesAppGlobals* aGlobals, WINDOW* parentWin, int* resultP,
               short numButtons, const char** buttons, const char* question,
               int timeoutms )
{
    LOG_FUNC();
    AskState* as = g_malloc0( sizeof(*as) );
    as->aGlobals = aGlobals;
    as->parentWin = parentWin;
    as->buttons = buttons;
    as->numButtons = numButtons;
    as->resultP = resultP;

    XP_LOGFF( "(question=%s, parentWin=%p)", question, parentWin );
    XP_ASSERT( !!parentWin );
    int row;
    int left, top;
    short maxWidth;
    FormatInfo fi;
    int len;

    getmaxyx( parentWin, as->yy, as->xx);
    getbegyx( parentWin, top, left );

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
    as->win = newwin( as->nLines, len+(PAD*2), top + ((as->yy/2) - (as->nLines/2)),
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
_cursesask2( CursesAppGlobals* ag, WINDOW* parentWin, short numButtons,
            const char** buttons, const char* question,
            const char* file, const char* proc )
{
    XP_LOGFF( "(file: %s, proc: %s)", file, proc );
    int result;
    cursesaskImpl( ag, parentWin, &result, numButtons, buttons, question, 0 );
    return result;
}

int
_cursesask( WINDOW* parentWin, short numButtons,
            const char** buttons, const char* question,
            const char* file, const char* proc )
{
    XP_LOGFF( "(file: %s, proc: %s)", file, proc );
    XP_ASSERT(0);
    return _cursesask2( NULL, parentWin, numButtons, buttons,
                        question, file, proc );
}

int
_cursesaskf( WINDOW* window, short numButtons, const char** buttons,
             const char* file, const char* proc, const char* fmt, ... )
{
    XP_LOGFF( "(file: %s, proc: %s)", file, proc );
    XP_ASSERT(0);
    XP_USE(window);
    XP_USE(numButtons);
    XP_USE(buttons);
    XP_USE(file);
    XP_USE(proc);
    XP_USE(fmt);
    return 0;
}

int
_cursesaskf2( CursesAppGlobals* aGlobals,  WINDOW* window, short numButtons,
              const char** buttons, const char* file, const char* proc,
              const char* fmt, ... )
{
    XP_LOGFF( "(file: %s, proc: %s)", file, proc );
    va_list args;
    va_start( args, fmt );
    gchar* msg = g_strdup_vprintf( fmt, args );

    int result = _cursesask2( aGlobals, window, numButtons, buttons, msg,
                              file, proc );

    g_free( msg );
    return result;
}

void
_ca_inform2( CursesAppGlobals* aGlobals, WINDOW* window, const char* message,
             const char* file, const char* proc )
{
    XP_LOGFF( "(file: %s, proc: %s)", file, proc );
    if ( !!window ) {
        const char* buttons[] = { "Ok" };
        (void)cursesaskImpl( aGlobals, window, NULL, VSIZE(buttons), buttons,
                             message, /*1500*/ 0 );
    }
    LOG_RETURN_VOID();
}

void
_ca_inform( WINDOW* window, const char* message, const char* file, const char* proc )
{
    _ca_inform2(NULL, window, message, file, proc );
}

void
_ca_informf( WINDOW* window,
             const char* file, const char* proc,
             const char* fmt, ... )
{
    XP_LOGFF( "(file: %s, proc: %s)", file, proc );
    XP_ASSERT(0);
    XP_USE(window);
    XP_USE(file);
    XP_USE(proc);
    XP_USE(fmt);
}

void
_ca_informf2( CursesAppGlobals* aGlobals, WINDOW* window,
              const char* file, const char* proc,
              const char* fmt, ... )
{
    XP_LOGFF( "(file: %s, proc: %s)", file, proc );
    va_list args;
    va_start( args, fmt );
    gchar* msg = g_strdup_vprintf( fmt, args );
    va_end( args );
    _ca_inform2( aGlobals, window, msg, file, proc );
    g_free( msg );
}

#endif /* PLATFORM_NCURSES */
