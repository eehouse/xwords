/* 
 * Copyright 1998 by Eric House.  All rights reserved.
 * fixin@peak.org
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

typedef struct dawg_header {
    unsigned long numWords;
    unsigned char firstEdgeRecNum;
    unsigned char charTableRecNum;
    unsigned char valTableRecNum;
    unsigned char reserved[3]; // worst case this points to a new resource
} dawg_header;

typedef struct array_edge {
    unsigned char highByte;
    unsigned char lowByte;
    unsigned char bits;
} array_edge;

/*
 * the bits field has five bits for the character (0-based rather than
 * 'a'-based, of course; one bit each indicating whether the edge may
 * be terminal and whether it's the last edge of a sub-array; and a final
 * bit that's overflow from the highByte field allowing indices to be in
 * the range 0-(2^^17)-1
 */
#define LETTERMASK 0x1f
#define ACCEPTINGMASK 0x20
#define LASTEDGEMASK 0x40
#define LASTBITMASK 0x80

//#define ushort_byte_swap(d) ((unsigned short)(d<<8 | d>>8))

