/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2000 by Eric House (fixin@peak.org).  All rights reserved.
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

#define ASK_HEIGHT 5
#define PAD 2
#define MAX_LINES 15
#define MIN_WIDTH 25

static void
drawButtons( WINDOW* confWin, XP_U16 line, short spacePerButton, 
             short numButtons, short curSelButton, char** button1 )
{
    short i;
    for ( i = 0; i < numButtons; ++i ) {
        short len = strlen( *button1 );

        if ( i == curSelButton ) {
            wstandout( confWin );
        }
        mvwprintw( confWin, line, ((i+1) * spacePerButton) - (len/2),
                   "[%s]", *button1 );
        if ( i == curSelButton ) {
            wstandend( confWin );
        }
        ++button1;
    }
    wrefresh( confWin );
} /* drawButtons */

/* Figure out how many lines there are and how wide the widest is.
 */
typedef struct FormatInfo {
    XP_U16 nLines;
    XP_U16 maxLen;
    struct {
        XP_UCHAR* substr;
        XP_U16 len;
    } line[MAX_LINES];
} FormatInfo;

static void
measureAskText( XP_UCHAR* question, FormatInfo* fip )
{
    XP_U16 i;
    XP_U16 maxWidth = 0;
    XP_Bool done = XP_FALSE;

    for ( i = 0; i < MAX_LINES && !done; ++i ) {
        XP_UCHAR* next = strstr( question, XP_CR );
        XP_U16 thisWidth;

        fip->line[i].substr = question;

        if ( !!next ) {
            thisWidth = next - question;
        } else {
            thisWidth = strlen(question);
            done = XP_TRUE;
        }
        fip->line[i].len = thisWidth;

        if ( thisWidth > maxWidth ) {
            maxWidth = thisWidth;
        }
        
        question = next + strlen(XP_CR);
    }

    fip->nLines = i;
    fip->maxLen = maxWidth;
} /* measureAskText */

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

    measureAskText( question, &fi );
    len = fi.maxLen;
    if ( len < MIN_WIDTH ) {
        len = MIN_WIDTH;
    }

    getmaxyx(globals->boardWin, y, x);

    rows = fi.nLines;
    maxWidth = x - (PAD*2) - 2; /* 2 for two borders */

    if ( len > x-2 ) {
        rows = (len / maxWidth) + 1;
        len = maxWidth;
    }

    nLines = ASK_HEIGHT + rows - 1;
    confWin = newwin( nLines, len+(PAD*2), 
                      (y/2) - (nLines/2), (x-len-2)/2 );

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

        ch = fgetc( stdin );
/* 	char ch = wgetch( globals->menuWin ); */
        switch ( ch ) {
        case '\t':
        case 'L':
            newSelButton = (curSelButton+1) % numButtons;
            break;
        case 'H':
            newSelButton = (numButtons+curSelButton-1) % numButtons;
            break;
        case EOF:
        case 4:			/* C-d */
        case 27:		/* ESC */
            curSelButton = 0;	/* should be the cancel case */
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
