/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2000-2003 by Eric House (fixin@peak.org).  All rights reserved.
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

#include "cursesdlgutil.h"

void
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

void
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

#endif
