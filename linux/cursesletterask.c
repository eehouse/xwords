/* -*-mode: C; fill-column: 78; c-basic-offset: 4; compile-command: "make MEMDEBUG=TRUE"; -*- */

/* 
 * Copyright 2003 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "linuxutl.h"
#include "cursesletterask.h"
#include "cursesdlgutil.h"

#define MAX_TILE_BUTTON_ROWS 10
#define MAX_TILE_BUTTON_WIDTH 6

static void
sizeTextsAsButtons( XP_U16 maxLen, XP_U16 nTiles, XP_U16* textsCols, 
                    XP_U16* textsRows, XP_U16* textsOffsets )
{
    XP_U16 nCols = maxLen / MAX_TILE_BUTTON_WIDTH;
    XP_U16 nRows = (nTiles + nCols - 1) / nCols;
    XP_U16 i;

    *textsCols = nCols;
    *textsRows = nRows;

    for ( i = 0; i < nRows; ++i ) {
        textsOffsets[i] = i * nCols;
    }

    XP_DEBUGF( "broke %d letters into %d rows of %d cols",
               nTiles, nRows, nCols );
} /* sizeTextsAsButtons */

XP_S16
curses_askLetter( CursesAppGlobals* globals, XP_UCHAR* query,
                  const XP_UCHAR** texts, XP_U16 nTiles )
{
    XP_S16 result;
    WINDOW* confWin;
    int x, y, rows, row, nLines, i;
    short newSelButton = 0;
    short curSelButton = 1;	/* force draw by being different */
    short maxWidth;
    short numCtlButtons;
    char* ctlButtons[] = { "Ok", "Cancel" };
    XP_Bool dismissed = XP_FALSE;
    FormatInfo fi;
    int len;
    XP_U16 textsCols, textsRows;
    XP_U16 textsOffsets[MAX_TILE_BUTTON_ROWS];
    XP_U16 spacePerCtlButton;
    char* textPtrs[MAX_UNIQUE_TILES];

    for ( i = 0; i < nTiles; ++i ) {
        textPtrs[i] = (char*)&texts[i];
    }

    getmaxyx( globals->boardWin, y, x );

    numCtlButtons = VSIZE(ctlButtons);

    maxWidth = x - (PAD*2) - 2; /* 2 for two borders */
    measureAskText( query, maxWidth, &fi );

    sizeTextsAsButtons( x, nTiles, &textsCols, &textsRows, textsOffsets );

    len = XP_MAX( fi.maxLen, textsCols * MAX_TILE_BUTTON_WIDTH );
    if ( len < MIN_WIDTH ) {
        len = MIN_WIDTH;
    }

    rows = fi.nLines + textsRows + 1;
    XP_DEBUGF( "set maxWidth=%d", maxWidth );


    if ( len > x-2 ) {
        rows = (len / maxWidth) + 1;
        len = maxWidth;
    }

    nLines = ASK_HEIGHT + rows - 1;
    XP_DEBUGF( "newwin( %d, %d, (%d/2) - (%d/2), (%d-%d-2)/2",
               nLines, len,//+(PAD*2), 
               y, nLines, x, len );

    XP_ASSERT( y >= nLines );
    confWin = newwin( nLines, len,//+(PAD*2), 
                      (y/2) - (nLines/2), (x-len-2)/2 );
    keypad( confWin, TRUE );
    XP_ASSERT( !!confWin );

    wclear( confWin );
    box( confWin, '|', '-');

    for ( row = 0; row < fi.nLines; ++row ) {
        mvwaddnstr( confWin, row+1, PAD, 
                    fi.line[row].substr, fi.line[row].len );
    }

    spacePerCtlButton = (len+(PAD*2)) / (numCtlButtons + 1);

    result = newSelButton;

    while ( !dismissed ) {
        int ch;

        if ( newSelButton != curSelButton ) {

            XP_U16 i;
            for ( i = 0; i < textsRows; ++i ) {
                XP_U16 nInRow = textsCols;

                if ( i + 1 == textsRows ) {
                    nInRow = nTiles % textsCols;
                    if ( nInRow == 0 ) {
                        nInRow = textsCols;
                    }
                }

                XP_DEBUGF( "printing %d cols, row %d, first char %s",
                           nInRow, i, textPtrs[textsOffsets[i]] );
                drawButtons( confWin, rows + 2 - textsRows + i, 
                             MAX_TILE_BUTTON_WIDTH-1, 
                             nInRow,
                             newSelButton - textsOffsets[i], 
                             (char**)&textPtrs[textsOffsets[i]] );
            }


            drawButtons( confWin, rows+2, spacePerCtlButton, 
                         numCtlButtons,
                         newSelButton - nTiles, ctlButtons );

            curSelButton = newSelButton;
        }

        ch = wgetch( confWin );
        int incr = 0;
        switch ( ch ) {
        case '\t':
        case 'R':
        case KEY_RIGHT:
        case 525:
            incr = 1;
            break;
        case 'L':
        case KEY_LEFT:
        case 524:
            incr = -1;
            break;

        case KEY_DOWN:
        case 526:
            incr = textsCols;
            break;
        case KEY_UP:
        case 523:
            incr = -textsCols;
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
        default:
            if ( ch >= 'a' && ch <= 'z' ) {
                ch += 'A' - 'a';
            }
            for ( i = 0; i < nTiles; ++i ) {
                if ( ch == texts[i][0] ) {
                    XP_DEBUGF( "picking %c", (char)ch);
                    newSelButton = i;
                    result = i;
                    break;
                }
            }
        }

        if ( incr != 0 ) {
            newSelButton = curSelButton + incr;
            if ( newSelButton < 0 ) {
                newSelButton = 0;
            } else if ( newSelButton >= numCtlButtons + nTiles ) {
                newSelButton = numCtlButtons + nTiles - 1;
            }

        }
    }
    delwin( confWin );

    /* this leaves a ghost line, but I can't figure out a better way. */
    wtouchln( globals->boardWin, (y/2)-(nLines/2), ASK_HEIGHT + rows - 1, 1 );
    wrefresh( globals->boardWin );
    
    return result;
} /* curses_askLetter */

#endif /* PLATFORM_NCURSES */
