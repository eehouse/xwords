// -*-mode: C; fill-column: 80; compile-command: "make xloc"; -*-
/* 
 * Copyright 1998 - 2002 by Eric House (fixin@peak.org).  All rights reserved.
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
 
/* This is where the langauge tile tables live.  There's one for each
 * supported langauge.  Tile tables give the face of each tile (e.g. "A")
 * the value (1), and the number in the game (8).
 *
 * This program generates two output files, a text file which has each
 * face on a separate line: "\0\nA\nB\n..." (where the \0 will later
 * be mapped to BLANK), and a binary file giving the values and counts
 * of those tiles in table form *and* additional data for any tiles with
 * non-printing faces mapping them to alternative printing values (e.g. "_"
 * for BLANK) and (optionally) to custom pilot bitmaps for representation
 * on-screen.
 *
 * This program comes after I've attempted to do something simpler with
 * shell scripts in makefiles.  There are too many problems with the null
 * and sub-0x20 bytes for the non-printing characters' faces.  Thus for now
 * rather than pass in tables I'll just maintain one for each language here.
 */

/* Here's the old comment:
 * Build a resource for Crosswords representing the letters in the
 * game, and the number and value of each letter.  The arrays for
 * each language for which a version of Crosswords exists are also
 * stored in this file.
 *
 * This resource is meant to replace the gInitialLetterCounts and
 * gTileValues arrays as well as to provide indirection aiding in
 * localization
 *
 * In order to keep the size of an in-memory game down, letters are
 * stored in five bits (and in null-terminated strings) so we have a
 * range of 31 values available.  Each will then be an index into a
 * table built here.  This extra level of abstraction permits
 * non-contiguous ranges of characters as, for instance, is required
 * by the German characters having umlauts.  Note, however, that a
 * language requiring *more* than 31 characters (including 1 for each
 * blank) will require some redesign.
 *
 * An additional problem is created by the need to convert letters
 * from lower case to upper on input to the blank-setting dialog as a
 * courtesy to players (already present in the shipping version,
 * alas).  For now I'll simply search the array of printing values (col.
 * 2 below) and if I fail to find it try various transformations the first
 * of which will be to upper-case a value in the a-z range 
 *
 * The arrays below consist of three columns each:
 *  ASCII value        numTilesThatValue            tileValue
 * The latter two are compressed into one byte, four bits each, limiting
 * each to the range 1..16.  (Actually, I ought to confirm that the code
 * required to deal with shifting and masking isn't bigger than the extra
 * <= 31 bytes I'm saving.... so skip the compression for now.)
 *
 */

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>

#include "xwcommon3.h"
//#include "../../../../xwcommon3.h"

//#define MASH(a,b) ((a)<<4)|(b)
#define MASH(a,b) (a),(b)

static short endian_short( short in ) {
    if ( 0 ) {
	return in;
    } else {
	return ((in >> 8) & 0x00ff) | ((in << 8) & 0xFF00);
    }
}

void errexit( char* msg ) {
    fprintf( stderr, msg );
    exit(1);
}

static short fileSize( char* fileName ) {
    short result;
    FILE* f = fopen( fileName, "rb" );
    printf( "opening %s\n", fileName );
    if ( f == NULL ) {
	errexit( "fopen failed\n" );
    }
    if ( fseek( f, 0L, SEEK_END ) != 0 ) {
	errexit( "error from fseek" );
    }
    result = ftell( f );
    fclose( f );
    printf( "length of file %s is %d\n", fileName, result );
    return (short)result;
}

#if 0
Graham writes:
> PORTUGUESE            
>
>
> Letter        Distribution    Face value
>
> A     12      1
> B     2       3
> C     4       3
> CH    1       5
> D     5       2
> E     12      1
> F     1       4
> G     2       2
> H     2       4
> I     6       1
> J     1       8
> (K)   0       
> L     4       1
> LL    1       8
> M     2       3
> N     5       1
> N tilde       1       8
> O     9       1
> P     2       3
> Q     1       5
> R     5       1
> RR    1       8
> S     6       1
> T     4       1
> U     5       1
> V     1       4
> (W)   0       
> X     1       8
> Y     1       4
> Z     1       10
> BLANK 2       0
>
> Total 100     

> GREEK         
>
> Letter        Distribution    Face value
>
> alpha 12      1
> beta  1       8
> gamma 2       4
> delta 2       4       
> epsilon       8       1       
> zeta  1       10      
> eta   7       1       
> theta 1       10      
> iota  8       1       
> kappa 4       2       
> lambda        3       3       
> mu    3       3       
> nu    6       1        
> xi    1       10      
> omicron       9       1       
> pi    4       2       
> rho   5       2       
> sigma 7       1       
> tau   8       1       
> upsilon       4       2
> phi   1       8
> chi   1       8
> psi   1       10
> omega 3       3
> blank 2       0
>
> Total 104


#endif
 
unsigned char finnish_table[] = {
#if 0
from yarik@avalon.merikoski.fi
amount  points letter
10       1      A
 1       8      B
 1      10      C
 1       7      D
 8       1      E
 1       8      F
 1       8      G
 2       4      H
10       1      I
 2       4      J
 5       2      K
 5       2      L
 3       3      M
 8       1      N
 5       2      O
 2       4      P
 2       4      R
 7       1      S
 9       1      T
 5       3      U
 2       4      V
 2       4      Y
5       2      D      // an A with two dots above
1       7      V      // an O with two dots above
2       ?      ?      // the 'wild card'
#endif
};

unsigned char US_english_table[] = {
    // numTiles,	tileValue	ASCII value
    MASH(9,			1),		'A',
    MASH(2,			3),		'B',
    MASH(2,			3),		'C',
    MASH(4,			2),		'D',
    MASH(12,			1),		'E',
    MASH(2,			4),		'F',
    MASH(3,			2),		'G',
    MASH(2,			4),		'H',
    MASH(9,			1),		'I',
    MASH(1,			8),		'J',
    MASH(1,			5),		'K',
    MASH(4,			1),		'L',
    MASH(2,			3),		'M',
    MASH(6,			1),		'N',
    MASH(8,			1),		'O',
    MASH(2,			3),		'P',
    MASH(1,			10),		'Q',
    MASH(6,			1),		'R',
    MASH(4,			1),		'S',
    MASH(6,			1),		'T',
    MASH(4,			1),		'U',
    MASH(2,			4),		'V',
    MASH(2,			4),		'W',
    MASH(1,			8),		'X',
    MASH(2,			4),		'Y',
    MASH(1,			10),		'Z',

    MASH(2,			0),		BLANK_FACE, /* BLANK1 */
   //    0 /* TERMINATES ARRAY */
}; // US_english_table

unsigned char norwegian_table[] = {
    // numTiles,	        tileValue	ASCII value
    MASH(2,			0),		BLANK_FACE, /* BLANK1 */
    MASH(7,			1),			'A',
    MASH(3,			4),			'B', 
    MASH(1,			10),			'C',
    MASH(5,			1),			'D', 
    MASH(9,			1),			'E', 
    MASH(4,			2),			'F', 
    MASH(4,			2),			'G', 
    MASH(3,			3),			'H', 
    MASH(5,			1),			'I', 
    MASH(2,			4),			'J', 
    MASH(4,			2),			'K', 
    MASH(5,			1),			'L', 
    MASH(3,			2),			'M', 
    MASH(6,			1),			'N', 
    MASH(4,			2),			'O', 
    MASH(2,			4),			'P', 
    MASH(6,			1),			'R', 
    MASH(6,			1),			'S', 
    MASH(6,			1),			'T', 
    MASH(3,			4),			'U', 
    MASH(3,			4),			'V', 
    MASH(1,			8),			'W', 
    MASH(1,			6),			'Y', 
    MASH(1,			6),			'Æ', 
    MASH(2,			5),			'Ø', 
    MASH(2,			4),			'Å', 
};

unsigned char swedish_table[] = {
    // numTiles,	        tileValue	ASCII value
    MASH(2,			0),		BLANK_FACE, /* BLANK1 */
    MASH(8,			1),		'A',
    MASH(2,			4),		'Å', // A with circle
    MASH(2,			3),		'Ä', // A with two dots
    MASH(2,			4),		'B',
    MASH(1,			10),		'C',
    MASH(5,			1),		'D',
    MASH(7,			1),		'E', // 15's the max....
    MASH(2,			3),		'F',
    MASH(3,			2),		'G',
    MASH(2,			2),		'H',
    MASH(5,			1),		'I',
    MASH(1,			7),		'J',
    MASH(3,			2),		'K',
    MASH(5,			1),		'L',
    MASH(3,			2),		'M',
    MASH(6,			1),		'N',
    MASH(5,			2),		'O',
    MASH(2,			4),		'Ö', // O with two dots
    MASH(2,			4),		'P',
    MASH(8,			1),		'R',
    MASH(8,			1),		'S',
    MASH(8,			1),		'T',
    MASH(3,			4),		'U',
    MASH(2,			3),		'V',
    MASH(1,			8),		'X',
    MASH(1,			7),		'Y',
    MASH(1,			8),		'Z',
    //    0 /* TERMINATES  ARRAY */
}; // swedish_table

unsigned char polish_table[] = {
    // numTiles,		tileValue	ASCII value
    // NO BLANK; there are already 32 tiles....
    // MASH(2,			0),		BLANK_FACE, /* BLANK1 */
    MASH(8,			1),		'A',
    MASH(1,			5),		'¡',
    MASH(2,			3),		'B',
    MASH(3,			2),		'C',
    MASH(1,			6),		'Æ',
    MASH(3,			2),		'D',
    MASH(7,			1),		'E',
    MASH(1,			5),		'Ê',
    MASH(2,			4),		'F',
    MASH(2,			3),		'G',
    MASH(2,			3),		'H',
    MASH(8,			1),		'I',
    MASH(2,			3),		'J',
    MASH(3,			2),		'K',
    MASH(3,			2),		'L',
    MASH(2,			3),		'£',
    MASH(3,			2),		'M',
    MASH(5,			1),		'N',
    MASH(1,			7),		'Ñ',
    MASH(6,			1),		'O',
    MASH(1,			5),		'Ó',
    MASH(3,			2),		'P',
    MASH(4,			1),		'R',
    MASH(4,			1),		'S',
    MASH(1,			5),		'¦',
    MASH(3,			2),		'T',
    MASH(2,			3),		'U',
    MASH(4,			1),		'W',
    MASH(4,			2),		'Y',
    MASH(5,			1),		'Z',
    MASH(1,			7),		'¬',
    MASH(1,			5),		'¯',
    //    0 /* TERMINATES  ARRAY */
}; // polish_table

unsigned char french_table[] = {
    // numTiles,		tileValue	ASCII value
    MASH(2,			0),		BLANK_FACE, /* BLANK1 */
    MASH(9,			1),		'A',
    MASH(2,			3),		'B',
    MASH(2,			3),		'C',
    MASH(3,			2),		'D',
    MASH(15,			1),		'E',
    MASH(2,			4),		'F',
    MASH(2,			2),		'G',
    MASH(2,			4),		'H',
    MASH(8,			1),		'I',
    MASH(1,			8),		'J',
    MASH(1,			10),		'K',
    MASH(5,			1),		'L',
    MASH(3,			2),		'M',
    MASH(6,			1),		'N',
    MASH(6,			1),		'O',
    MASH(2,			3),		'P',
    MASH(1,			8),		'Q',
    MASH(6,			1),		'R',
    MASH(6,			1),		'S',
    MASH(6,			1),		'T',
    MASH(6,			1),		'U',
    MASH(2,			4),		'V',
    MASH(1,			10),		'W',
    MASH(1,			10),		'X',
    MASH(1,			10),		'Y',
    MASH(1,			10),		'Z',
    //    0 /* TERMINATES ARRAY */
}; // french_table

unsigned char german_table[] = {
    // numTiles,	        tileValue	ASCII value
    MASH(2,			0),		BLANK_FACE, /* BLANK1 */
    MASH(5,		        1),		'A',
    MASH(1,			6),		196, // A mit umlaut
    MASH(2,			3),		'B',
    MASH(2,			4),		'C',
    MASH(4,			1),		'D',
    MASH(15,			1),		'E',
    MASH(2,			4),		'F',
    MASH(3,			2),		'G',
    MASH(4,			2),		'H',
    MASH(6,			1),		'I',
    MASH(1,			6),		'J',
    MASH(2,			4),		'K',
    MASH(3,			2),		'L',
    MASH(4,			3),		'M',
    MASH(9,			1),		'N',
    MASH(3,			2),		'O',
    MASH(1,			8),		214, // O mit umlaut
    MASH(1,			4),		'P',
    MASH(1,			10),		'Q',
    MASH(6,			1),		'R',
    MASH(7,			1),		'S',
    MASH(6,			1),		'T',
    MASH(6,			1),		'U',
    MASH(1,			6),		220, // U mit umlaut
    MASH(1,			6),		'V',
    MASH(1,			3),		'W',
    MASH(1,			8),		'X',
    MASH(1,			10),		'Y',
    MASH(1,			3),		'Z',
    //    0 /* TERMINATES  ARRAY */
}; // german_table

unsigned char dutch_table[] = {
    // numTiles,	        tileValue	ASCII value
    MASH(2,			0),		BLANK_FACE, /* BLANK1 */
    MASH(6,			1),		'A',
    MASH(2,			3),		'B',
    MASH(2,			5),		'C',
    MASH(5,			2),		'D',
    MASH(16,			1),		'E',
    MASH(2,			4),		'F',
    MASH(2,			3),		'G',
    MASH(2,			4),		'H',
    MASH(4,			1),		'I',
    MASH(2,			4),		'J',
    MASH(3,			3),		'K',
    MASH(3,			3),		'L',
    MASH(3,			3),		'M',
    MASH(8,			1),		'N',
    MASH(6,			1),		'O',
    MASH(3,			3),		'P',
    MASH(1,			10),		'Q',
    MASH(5,			2),		'R',
    MASH(5,			2),		'S',
    MASH(5,			2),		'T',
    MASH(4,			4),		'U',
    MASH(4,			2),		'V',
    MASH(2,			5),		'W',
    MASH(1,			8),		'X',
    MASH(2,			4),		'Y',
    MASH(2,			4),		'Z',
    //0 /* TERMINATES  ARRAY */
}; // dutch_table

unsigned char italian_table[] = {
    // numTiles,	        tileValue	ASCII value
    MASH(2,			0),		BLANK_FACE, /* BLANK1 */
    MASH(13,			1),		'A',
    MASH(3,			5),		'B',
    MASH(4,			4),		'C',
    MASH(3,			5),		'D',
    MASH(13,			1),		'E',
    MASH(2,			8),		'F',
    MASH(2,			5),		'G',
    MASH(2,			8),		'H',
    MASH(13,			1),		'I',
    MASH(5,			3),		'L',
    MASH(5,			3),		'M',
    MASH(6,			2),		'N',
    MASH(13,			1),		'O',
    MASH(3,			5),		'P',
    MASH(1,			10),		'Q',
    MASH(6,			2),		'R',
    MASH(6,			2),		'S',
    MASH(6,			2),		'T',
    MASH(5,			3),		'U',
    MASH(4,			4),		'V',
    MASH(2,			8),		'Z',
    //    0 /* TERMINATES  ARRAY */
}; // italian_table

unsigned char spanish_table[] = {
   // numTiles,	        tileValue	ASCII value
   MASH( 12,			1),	'A',
   MASH( 2,			3),	'B',
   MASH( 4,			3),	'C',
   MASH( 1,			5),	1,	/*'CH'*/
   MASH( 5,			2),	'D',
   MASH( 12,			1),	'E',
   MASH( 1,			4),	'F',
   MASH( 2,			2),	'G',
   MASH( 2,			4),	'H',
   MASH( 6,			1),	'I',
   MASH( 1,			8),	'J',
   MASH( 4,			1),	'L',
   MASH( 1,			8),	2,	/*'LL'*/
   MASH( 2,			3),	'M',
   MASH( 5,			1),	'N',
   MASH( 1,			8),	 209,	/*'N~'*/
   MASH( 9,			1),	'O',
   MASH( 2,			3),	'P',
   MASH( 1,			5),	'Q',
   MASH( 5,			1),	'R',
   MASH( 1,			8),	3,	/*'RR'*/
   MASH( 6,			1),	'S',
   MASH( 4,			1),	'T',
   MASH( 5,			1),	'U',
   MASH( 1,			4),	'V',
   MASH( 1,			8),	'X',
   MASH( 1,			4),	'Y',
   MASH( 1,			10),	'Z',
   MASH( 2,			0),	BLANK_FACE, /* BLANK1 */
}; // spanish_table

/* Test case that reverses char order and puts blank at the end, violating the
 * sometimes-assumption that blank==0.  */
unsigned char hex_table[] = {
    // numTiles,	        tileValue	ASCII value
    MASH(9,			1),		'A',
    MASH(2,			3),		'B',
    MASH(2,			3),		'C',
    MASH(4,			2),		'D',
    MASH(12,			1),		'E',
    MASH(2,			4),		'F',
    MASH(4,			0),		BLANK_FACE, /* BLANK1 */
    //0 /* TERMINATES  ARRAY */
}; // hex_table

unsigned char test_table[] = {
    // numTiles,	        tileValue	ASCII value
    MASH(1,			1),		'A',
    MASH(1,			5),		'B',
    MASH(1,			10),		'E',
    //0 /* TERMINATES  ARRAY */
}; // test_table

//#define NONFILEARGS 3
#define MAXSPECIALS 20

int main( int argc, char** argv ) {
    char* lang = NULL;
    //    char* fileName;
    char* facesFileName = NULL;
    char* binaryFileName = NULL;

    FILE* facesFile;
    FILE* binFile;
    Xloc_header header;
    unsigned char* table;
    short tableLength = 0;
    //char* name = "";
    short i;
    short fileArgsUsed;
    short offset;
    int got;

    while ( (got = getopt(argc, argv, "l:O:T:h")) != EOF ) {
	switch ( got ) {
	case 'l':
	    lang = optarg;
	    break;
	case 'O':
	    facesFileName = optarg;
	    break;
	case 'T':
	    binaryFileName = optarg;
	    break;
	case 'h':
	default:
	    errexit( "Usage: xloc -l lang_code "
		     "-O tableOutfile -T valCountOutfile\n" );
	}
    }

    fprintf( stderr, "binoutfile = %s\n",
	     binaryFileName?binaryFileName:"null" );
    fprintf( stderr, "facesFileName = %s\n", 
	     facesFileName?facesFileName:"null" );
    fprintf( stderr, "lang = %s\n", lang );

    header.padding = 0;

    if ( strcmp( lang, "en_US" ) == 0 ) {
	table = US_english_table;
	tableLength = sizeof(US_english_table);
	header.langCodeFlags = US_ENGLISH;
    } else if ( strcmp( lang, "sv_SE" ) == 0 ) {
	table = swedish_table;
	tableLength = sizeof(swedish_table);
	header.langCodeFlags = SWEDISH_SWEDISH;
    } else if ( strcmp( lang, "no_NO" ) == 0 ) {
	table = norwegian_table;
	tableLength = sizeof(norwegian_table);
	header.langCodeFlags = NORWEGIAN_NORWEGIAN;
    } else if ( strcmp( lang, "pl_PL" ) == 0 ) {
	table = polish_table;
	tableLength = sizeof(polish_table);
	header.langCodeFlags = POLISH_POLISH;
    } else if ( strcmp( lang, "fr_FR" ) == 0 ) {
	table = french_table;
	tableLength = sizeof(french_table);
	header.langCodeFlags = FRENCH_FRENCH;
    } else if ( strcmp( lang, "de_DE" ) == 0 ) {
	table = german_table;
	tableLength = sizeof(german_table);
	header.langCodeFlags = GERMAN_GERMAN;
    } else if ( strcmp( lang, "nl_NL" ) == 0 ) {
	table = dutch_table;
	tableLength = sizeof(dutch_table);
	header.langCodeFlags = DUTCH_DUTCH;
    } else if ( strcmp( lang, "it_IT" ) == 0 ) {
	table = italian_table;
	tableLength = sizeof(italian_table);
	header.langCodeFlags = ITALIAN_ITALIAN;
    } else if ( strcmp( lang, "es_ES" ) == 0 ) {
	table = spanish_table;
	tableLength = sizeof(spanish_table);
	header.langCodeFlags = SPANISH_SPANISH;
    } else if ( strcmp( lang, "hex" ) == 0 ) {
	table = hex_table;
	tableLength = sizeof(hex_table);
    } else if ( strcmp( lang, "test" ) == 0 ) {
	table = test_table;
	tableLength = sizeof(test_table);
    } else {
	fprintf( stderr, "unknown language code %s\n", lang );
	exit(1);
    }

    header.langCodeFlags |= 1<<XLOC_LANG_OFFSET;

    ////////////////////////////////////////////////////
    // first the char table file
    ////////////////////////////////////////////////////
    if ( facesFileName != NULL ) {
	facesFile = fopen( facesFileName, "w" );
	assert( facesFile );
	assert( tableLength > 0 );

	for ( i = 0; i < tableLength; i += BYTES_PER_LETTER ) {
	    fprintf( facesFile, "%c", table[i+2] );
	}

	fclose( facesFile );
    }

    ////////////////////////////////////////////////////
    // now the binary file
    ////////////////////////////////////////////////////
    if ( binaryFileName != NULL ) {

	binFile = fopen( binaryFileName, "w" );
	assert( binFile );

	fwrite( &header, sizeof(header), 1, binFile );

	// now write out the table, where header.specialCharStart is length
	for ( i = 0; i < tableLength; i += BYTES_PER_LETTER ) {
	    fwrite( &table[i], sizeof(table[i])+sizeof(table[i+1]), 1, 
		    binFile );
	}

	// record file sizes

	// write data with file size included

	// append the files themselves

	fclose( binFile );
    }
    return 0;
} // main
