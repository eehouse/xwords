/*
 * Copyright 2025 by Eric House (xwords@eehouse.org).  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifdef PLATFORM_NCURSES

#include <ncurses.h>
#include <qrencode.h>
#include <string.h>

#include "cursqr.h"
#include "dbgutil.h"
#include "curwinstk.h"
#include "cursesdlgutil.h"

typedef struct QRState {
    CursesAppGlobals* aGlobals;
    WINDOW* win;
    int oldState;
} QRState;

static bool
trueOnAny( int XP_UNUSED(key), void* closure )
{
    QRState* qrs = (QRState*)closure;
    cws_delwin( qrs->aGlobals, &qrs->win );
    curs_set( qrs->oldState );
    g_free( qrs );
    return true;
}

/* Display QR code in full-screen dialog */
#define ROW_HEIGHT 1
#define COL_WIDTH 2

static void
addCode( QRState* qrs, const QRcode* qrCode, int multiple )
{
    WINDOW* win = qrs->win;
    char spaces[(multiple * COL_WIDTH)+1] = {};
    for ( int ii = 0; ii < multiple * COL_WIDTH; ++ii ) {
        spaces[ii] = ' ';
    }

    werase( win );
    wattron( win, A_REVERSE );
    int width = qrCode->width;  /* width and height are the same */
    int end = width * width;
    for ( int ii = 0; ii < end; ++ii ) {
        if ( 1 & qrCode->data[ii] ) {
            int col = COL_WIDTH * (ii % width);
            int row = ROW_HEIGHT * (ii / width);
            for ( int jj = 0; jj < ROW_HEIGHT; ++jj ) {
                mvwaddstr( win, row+jj, col, spaces );
            }
        }
    }

    wattroff( win, A_REVERSE );
    wrefresh( win );
    qrs->oldState = curs_set(0);        /* not needed if globally-off is ok */
}

bool
cursesShowQRDialog( CursesAppGlobals* aGlobals, const char* text )
{
    bool success = false;
    if ( !!text && !!text[0] ) {
        QRcode* qrCode = QRcode_encodeString( text, 0, QR_ECLEVEL_L, QR_MODE_8, 1 );
        if ( !!qrCode ) {
            int codeWidth = qrCode->width;
            int termHeight, termWidth;
            getmaxyx( stdscr, termHeight, termWidth );

            /* Figure the size. If it won't fit we'll have to say so */
            for ( int multiple = 5; 0 < multiple; --multiple ) {
                int height = ROW_HEIGHT * multiple * codeWidth;
                if ( height <= termHeight ) {
                    int width = COL_WIDTH * multiple * codeWidth;
                    if ( width <= termWidth ) {
                        QRState* qrs = g_malloc0( sizeof(*qrs) );
                        qrs->win = makeCenteredBox( aGlobals, width, height );
                        qrs->aGlobals = aGlobals;
                        addCode( qrs, qrCode, multiple );
                        startModalAlert( aGlobals, qrs->win, XP_TRUE, trueOnAny, qrs );
                        success = true;
                        break;
                    }
                }
            }
            QRcode_free( qrCode );
        }
    }
    return success;
}

#endif /* PLATFORM_NCURSES */
