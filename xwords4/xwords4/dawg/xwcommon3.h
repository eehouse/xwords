// -*-mode: C; fill-column: 80; -*-

/* 
 * Copyright 1997 by Eric House (fixin@peak.org).  All rights reserved.
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
 
#ifndef __XWCOMMON__
#define __XWCOMMON__

//#include <Hardware/Hardware.h>
// so sue me :-)
//#include "/home/pilot/usr/local/gnu/m68k-palmos-coff/include/PalmOS2/Hardware/Hardware.h"

enum { BLANK=0,
       EMPTY = 32,
       DOUBLE_LETTER,
       DOUBLE_WORD,
       TRIPLE_LETTER,
       TRIPLE_WORD
};	

	// a single-width diagonal stripe
#define PAT_DOUBLE_LETTER	{ 0x8844, 0x2211, 0x8844, 0x2211 }
	// a double-width diagonal stripe
#define PAT_DOUBLE_WORD { 0xaa55, 0xaa55, 0xaa55, 0xaa55 }
	// grey pattern (single-pixel checkboard)
#define	PAT_TRIPLE_LETTER { 0xCC66, 0x3399, 0xCC66, 0x3399 }
	// 2-pixel checkboard
#define	PAT_TRIPLE_WORD { 0xCCCC, 0x3333, 0xCCCC, 0x3333 }


#define BOARD_RES_TYPE 'Xbrd'
#define TILES_RES_TYPE 'Xloc'
#define STRL_RES_TYPE 'StrL'
#define XW_STRL_RESOURCE_ID 1000
// both the above resources use this ID
#define XW_CONFIGABLE_RESOURCE_ID 1001

#define BYTES_PER_LETTER 3

#define BLANK_FACE '\0'
//#define A_TILE 1
#define A_TILE 0

#define MAX_NUM_TILES 110
#define MAX_UNIQUE_TILES (32-A_TILE)
//b#define NUM_BLANKS 2
#define MAX_NUM_BLANKS 4

/* language header:
 * specialCharStart simply gives the number of bytes needed to skip beyond the
 * standard tiles table to the first of the "special" entries.
 *
 * langCodeFlags is more ambitious.  Each language I release will have an
 * assigned code.  The code has a least two purposes: to prevent viewing a game
 * with the wrong language; and to tie a language to the dictionary that can be
 * used by the computer player.
 *
 * For a dictionary and language (set of tile rules) to work together, the
 * mapping of index to character must be in sync.  In the German case, 0 must be
 * A, 1 umlaut-A, etc.  But the number of characters and values assigned each
 * tile do not matter.  Thus XWConfig can allow those aspects of a language to
 * be edited.  but if a user wants to add or delete a character in an "official"
 * language XWConfig must disallow this, forcing him instead to "clone" the
 * language to something whose offical flag will be cleared.  
 */

typedef struct Xloc_header {
    //unsigned char specialCharStart;
    unsigned char langCodeFlags; // can't do bitfields; gcc for pilot and x86
				 // seem to generate different code
    unsigned char padding;       // ptrs to the shorts in Xloc_specialEntry
                                 // will otherwise be odd
} Xloc_header;

#define XLOC_LANG_MASK 0x80  // high bit is "official"
#define XLOC_LANG_OFFSET 7
#define XLOC_OFFICIAL_MASK 0x7F // rest are for the enums below

enum { 
    HOMEBREW = 0,
    US_ENGLISH = 1,
    FRENCH_FRENCH = 2,
    GERMAN_GERMAN = 3,
    DUTCH_DUTCH = 4,
    ITALIAN_ITALIAN = 5,
    SPANISH_SPANISH = 6,
    SWEDISH_SWEDISH = 7,
    POLISH_POLISH = 8,
    NORWEGIAN_NORWEGIAN = 9,
};

/* "Special chars", added to support Spanish "LL" and "RR", replace
 * the ascii character code in the Xloc charinfo array with an integer
 * between 1 and 0X1F which is an index into an larger array appended
 * to the charinfo array.  Fields in the structs located in that array
 * include the string to be used to represent the Tile when drawing in
 * text (e.g.  formatting for the Tile values dialog) and the IDs of
 * resources holding bitmaps to be used when drawing tiles large
 * (tray) and small (board).
 */
typedef struct Xloc_specialEntry {
    unsigned char textVersion[4]; /* string can be up to 3 chars long */
    short hasLarge;
    short hasSmall;
} Xloc_specialEntry;




/* #define LARGE_CH_BMP_ID 2000 */
/* #define SMALL_CH_BMP_ID 2001 */
/* #define LARGE_LL_BMP_ID 2002 */
/* #define SMALL_LL_BMP_ID 2003 */
/* #define LARGE_RR_BMP_ID 2004 */
/* #define SMALL_RR_BMP_ID 2005 */

#endif
