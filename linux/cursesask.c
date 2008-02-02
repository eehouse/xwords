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


/* Figure out how many lines there are and how wide the widest is.
 */
short
cursesask( CursesAppGlobals* globals, char* question, short numButtons, 
           char* button1, ... )
{
    WINDOW* confWin;
    int x, y, rows, row, nLines;
    short newSelButton = 0;
    short curSelButton = 1;	/* force draw by being different */
    short spacePerButton, num;
    short maxWidth;
    XP_Bool dismissed = XP_FALSE;
    FormatInfo fi;
    int len;

    getmaxyx(globals->boardWin, y, x);

    measureAskText( question, x-2, &fi );
    len = fi.maxLen;
    if ( len < MIN_WIDTH ) {
        len = MIN_WIDTH;
    }

    rows = fi.nLines;
    maxWidth = x - (PAD*2) - 2; /* 2 for two borders */

    if ( len > x-2 ) {
        rows = (len / maxWidth) + 1;
        len = maxWidth;
    }

    nLines = ASK_HEIGHT + rows - 1;
    confWin = newwin( nLines, len+(PAD*2), 
                      (y/2) - (nLines/2), (x-len-2)/2 );
    keypad( confWin, TRUE );
    wclear( confWin );
    box( confWin, '|', '-');

    for ( row = 0; row < rows; ++row ) {
        mvwaddnstr( confWin, row+1, PAD, 
                    fi.line[row].substr, fi.line[row].len );
    }
    spacePerButton = (len+(PAD*2)) / (numButtons + 1);

    while ( !dismissed ) {
        int ch;

        if ( newSelButton != curSelButton ) {
            drawButtons( confWin, rows+1, spacePerButton, numButtons, 
                         curSelButton=newSelButton, &button1 );
        }

        ch = wgetch( confWin );
        switch ( ch ) {
        case 'L':
        case KEY_RIGHT:
        case 525:
            newSelButton = (curSelButton+1) % numButtons;
            break;
        case 'H':
        case '\t':
        case KEY_LEFT:
        case 524:
            newSelButton = (numButtons+curSelButton-1) % numButtons;
            break;
        case EOF:
        case 4:			/* C-d */
        case 27:		/* ESC */
            curSelButton = 0;	/* should be the cancel case */
        case KEY_B2:                /* "center of keypad" */
        case '\r':
        case '\n':
            dismissed = XP_TRUE;
            break;
        case '1':
        case '2':
        case '3':
        case '4':
            num = ch - '1';
            if ( num < numButtons ) {
                newSelButton = num;
            }
            break;
        default:
            beep();
        }
    }
    delwin( confWin );

    /* this leaves a ghost line, but I can't figure out a better way. */
    wtouchln( globals->boardWin, (y/2)-(nLines/2), ASK_HEIGHT + rows - 1, 1 );
    wrefresh( globals->boardWin );
    return curSelButton;
} /* ask */

#endif /* PLATFORM_NCURSES */
