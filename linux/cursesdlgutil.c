/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2000-2003 by Eric House (xwords@eehouse.org).  All rights reserved.
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
measureAskText( const XP_UCHAR* question, int width, FormatInfo* fip )
{
    int i;
    XP_U16 maxLen = 0;
    XP_Bool done = XP_FALSE;

    int len = strlen(question);
    const char* end = question + len;
    const char* cur = question;
    for ( i = 0; i < MAX_LINES && !done; ++i ) {
        len = strlen(cur);

        if ( len == 0 ) {
            assert( i > 0 );
            fip->nLines = i;
            fip->maxLen = maxLen;
            break;
        }

        fip->line[i].substr = cur;

        /* Now we need to break the line if 1) there's a <cr>; or 2) it's too
           long. */
        const char* cr = strstr( cur, "\n" );
        if ( NULL != cr && (cr - cur) < width ) {
            len = cr - cur;
        } else if ( len > width ) {
            const char* s = cur + width;
            while ( *s != ' ' && s > cur ) {
                --s;
            }
            assert( s > cur );  /* deal with this!! */
            len = s - cur;
        }
        fip->line[i].len = len;
        if ( maxLen < len ) {
            maxLen = len;
        }

        cur += len + 1;         /* skip the <cr>/space */
        if ( cur > end ) {
            cur = end;
        }
    }
} /* measureAskText */

void
drawButtons( WINDOW* win, XP_U16 line, short spacePerButton, 
             short numButtons, short curSelButton, char** button1 )
{
    short i;
    for ( i = 0; i < numButtons; ++i ) {
        short len = strlen( *button1 );

        if ( i == curSelButton ) {
            wstandout( win );
        }
        mvwprintw( win, line, ((i+1) * spacePerButton) - (len/2),
                   "[%s]", *button1 );
        if ( i == curSelButton ) {
            wstandend( win );
        }
        ++button1;
    }
    wrefresh( win );
} /* drawButtons */

#endif
