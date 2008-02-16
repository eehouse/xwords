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

#ifndef _CURSESDLGUTIL_H_
#define _CURSESDLGUTIL_H_

#include "comtypes.h"
#include "linuxmain.h"
#include "cursesmain.h"

#define ASK_HEIGHT 5
#define PAD 2
#define MAX_LINES 15
#define MIN_WIDTH 25



typedef struct FormatInfo {
    XP_U16 nLines;
    XP_U16 maxLen;
    struct {
        const XP_UCHAR* substr;
        XP_U16 len;
    } line[MAX_LINES];
} FormatInfo;


void measureAskText( const XP_UCHAR* question, int maxWidth, FormatInfo* fip );

void drawButtons( WINDOW* confWin, XP_U16 line, short spacePerButton, 
                  short numButtons, short curSelButton, char** button1 );

#endif
