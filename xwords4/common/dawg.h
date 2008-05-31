/* -*-mode: C; fill-column: 78; c-basic-offset: 4;-*- */
/* 
 * Copyright 1998-2001 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _DAWG_H_
#define _DAWG_H_

#include "xptypes.h"

/*
 * the bits field has five bits for the character (0-based rather than
 * 'a'-based, of course; one bit each indicating whether the edge may
 * be terminal and whether it's the last edge of a sub-array; and a final
 * bit that's overflow from the highByte field allowing indices to be in
 * the range 0-(2^^17)-1
 */
#define LETTERMASK_OLD 0x1f
#define ACCEPTINGMASK_OLD 0x20
#define LASTEDGEMASK_OLD 0x40
#define EXTRABITMASK_OLD 0x80

#define LETTERMASK_NEW_4 0x3f
#define LETTERMASK_NEW_3 0x1f
#define ACCEPTINGMASK_NEW 0x80
#define LASTEDGEMASK_NEW 0x40
/* This guy doesn't exist in 4-byte case */
#define EXTRABITMASK_NEW 0x20

#define OLD_THREE_FIELDS \
    XP_U8 highByte; \
    XP_U8 lowByte; \
    XP_U8 bits

typedef struct array_edge_old {
    OLD_THREE_FIELDS;
} array_edge_old;

typedef struct array_edge_new {
    OLD_THREE_FIELDS;
    XP_U8 moreBits;
} array_edge_new;

typedef XP_U8 array_edge;

/* This thing exists only in current xwords dicts (on PalmOS); not sure if I
 * should do away with it.
 */
typedef struct dawg_header {
    unsigned long numWords;
    unsigned char firstEdgeRecNum;
    unsigned char charTableRecNum;
    unsigned char valTableRecNum;
    unsigned char reserved[3]; /* worst case this points to a new resource */
#ifdef NODE_CAN_4
    unsigned short flags;
#endif
} dawg_header;

/* Part of xwords3 dictionaries on PalmOS */
typedef struct Xloc_header {
    unsigned char langCodeFlags; /* can't do bitfields; gcc for pilot and x86
                                    seem to generate different code */
    unsigned char padding;       /* ptrs to the shorts in Xloc_specialEntry
                                    will otherwise be odd */
} Xloc_header;

typedef struct Xloc_specialEntry {
    unsigned char textVersion[4]; /* string can be up to 3 chars long */
    short hasLarge;
    short hasSmall;
} Xloc_specialEntry;

#endif /* _DAWG_H_ */
