// -*-mode: C; fill-column: 80; compile-command: "make pdb2dict"; -*-

/* 
 * Copyright 1997 - 2002 by Eric House (fixin@peak.org).  All rights reserved.
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
 
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "pdb.h"
#include "dawg.h"

#include "swap.h"

/////////////////////////////// prototypes //////////////////////////////////
static void generate_dict( array_edge* memoryFile );
static void write_words( array_edge* memoryFile, long edgeIndex,
			 short charIndex, char* wordBuffer );
static void skipNHeaders( FILE* file, int n, RecordHeader* recHeader );
static array_edge* loadEdgesArray( FILE* dictFile, unsigned char* charTable );
static void* readNthRecord( FILE* files, void* where, int whereMaxSize,
			    int whichRec, int* foundSize );
static void getNthOffset( FILE* file, int n, long* offset, 
			  unsigned short* size );
static void printWord( char* wordBuffer );

///////////////////////////////// globals ///////////////////////////////////
static unsigned char charTable[32];

/******************************************************************************
 *
 *****************************************************************************/
void usage() {
    fprintf( stderr, "Usage: pdb2dict <file.pdb>\n" );
    exit( 1 );
}

/******************************************************************************
 *
 *****************************************************************************/
int main( int argc, char** argv ) {
    char* pdbName;
    array_edge* memoryFile;
    FILE* dictFile;
    if ( argc < 2 ) {
	usage();
    }

    //    fprintf( stderr, "sizeof(WORD)=%d\n", sizeof(WORD) );

    pdbName = argv[1];
    if ( (argv < 2) || (strchr( pdbName, '.' ) == NULL) ||
	 strcmp( (char*)strchr( pdbName, '.' ), ".pdb" ) ) {
	usage();
    }

    dictFile = fopen( pdbName, "r" );
    if ( dictFile == NULL ) {
	fprintf( stderr, "%s: No such file %s\n", argv[0], pdbName );
	exit(1);
    }
    memoryFile = loadEdgesArray( dictFile, charTable );
    fclose( dictFile );

    generate_dict( memoryFile );
    free( memoryFile );
    return 0;
} // main

/******************************************************************************
 *
 *****************************************************************************/
static array_edge* loadEdgesArray( FILE* dictFile, unsigned char* charTable ) {
    DocHeader pdbHeader;
    dawg_header dawgHeader;
    array_edge* firstEdge = NULL;
    fpos_t firstHeaderOffset;
    int curSize, i;

    // read in the main pdb header
    fread( &pdbHeader, DOCHEADSZ, 1, dictFile );
    assert( (strncmp( (char*)&pdbHeader.dwCreator, "Xwr3", 4) == 0)
	    && (strncmp( (char*)&pdbHeader.dwType, "DAWG", 4) == 0) );

    (void)fgetpos( dictFile, &firstHeaderOffset );

    (void)readNthRecord( dictFile, &dawgHeader, sizeof(dawgHeader), 0, NULL );
    //fprintf( stderr, "word count = %ld\n", swap_long(dawgHeader.numWords) );
    assert( dawgHeader.firstEdgeRecNum == 3 );

    (void)readNthRecord( dictFile, charTable, 32, dawgHeader.charTableRecNum,
			 NULL );

    firstEdge = (array_edge*)malloc(0);
    curSize = 0;
    for ( i = dawgHeader.firstEdgeRecNum; i < swap_short(pdbHeader.wNumRecs);
	  ++i ) {
	int newSize;
	void* rec = readNthRecord( dictFile, NULL, 0, i, &newSize );

	firstEdge = (array_edge*)realloc( firstEdge, curSize + newSize );
	memcpy( ((char*)firstEdge) + curSize, rec, newSize );
	free( rec );
	curSize += newSize;
    }
    return firstEdge;
} // loadEdgesArray

/******************************************************************************
 *
 *****************************************************************************/
static void* readNthRecord( FILE* file, void* where, int whereMaxSize,
			    int whichRec, int* foundSize ) {
    void* result = NULL;
    fpos_t pos;
    long offset;
    unsigned short size;

    (void)fgetpos( file, &pos);

    getNthOffset( file, whichRec, &offset, &size );
    fseek( file, offset, 0 );

    if ( where == NULL ) {
	result = malloc( size );
	fread( result, size, 1, file );
    } else {
	assert( size <= whereMaxSize );
	fread( where, size, 1, file );	
    }
    if ( foundSize ) {
	*foundSize = size;
    }

    (void)fsetpos( file, &pos);
    return result;
} // readNthRecord

/******************************************************************************
 * Size is my offset subtracted from the one after me, unless I'm the last
 * entry in which case it's file size minus my offset.
 *****************************************************************************/
static void getNthOffset( FILE* file, int n, long* offset, 
			  unsigned short* size ) {
    DocHeader pdbHeader;
    RecordHeader recHeader;
    fpos_t pos;
    long sizeCalc;

    (void)fgetpos( file, &pos);
    rewind( file );

    //fprintf( stderr, "sizeof(pdbHeader)=%d\n", sizeof(pdbHeader) );
    fread( &pdbHeader, DOCHEADSZ, 1, file );
    assert( swap_short(pdbHeader.wNumRecs) > n );

    skipNHeaders( file, n+1, &recHeader );
    *offset = sizeCalc = swap_long( recHeader.offset );
    if ( n+1 == swap_short(pdbHeader.wNumRecs) ) { // use file size
	fseek( file, 0, SEEK_END );
	sizeCalc = ftell( file ) - sizeCalc;
    } else {
	skipNHeaders( file, 1, &recHeader );	
	sizeCalc = swap_long( recHeader.offset ) - sizeCalc;
    }
    *size = sizeCalc;

    (void)fsetpos( file, &pos);    
} // getNthOffset

/******************************************************************************
 * Skip over the given number of headers, returning with the last one read
 * into the supplied buffer.
 *****************************************************************************/
static void skipNHeaders( FILE* file, int n, RecordHeader* recHeader ) {
    short i;
    for ( i = 0; i < n; ++i ) {
	fread( recHeader, sizeof(*recHeader), 1, file );
    }
} // skipNHeaders

/******************************************************************************
 * beginning with an array of NULL chars, on each level of the tree
 * iterate over each child replacing the appropriate char with the
 * letter from the edge.  When an edge is terminal, print the word
 * formed.  And when returning replace the letter with a null char.
 *****************************************************************************/
static void generate_dict( array_edge* memoryFile ) {
    char wordBuffer[31];
    (void)memset( wordBuffer, '\0', 31 );

    write_words( memoryFile, 0, 0, wordBuffer );
}

/******************************************************************************
 *
 *****************************************************************************/
void write_words( array_edge* memoryFile, long edgeIndex, short charIndex,
		  char* wordBuffer ) {
    array_edge* child = &memoryFile[edgeIndex];
    for ( ; ; child = &memoryFile[++edgeIndex] ) {
	unsigned char bits = child->bits;
	long index = 0;
	wordBuffer[charIndex] = charTable[(bits & LETTERMASK)];
	if ( bits & ACCEPTINGMASK ) {
	    printWord( wordBuffer );
	}

	index = (child->highByte * 256) + child->lowByte;
	if ( bits & LASTBITMASK ) {
	    index += 0x00010000;
	}

	if ( index > 0 ) {
	    write_words( memoryFile, index, charIndex+1, wordBuffer );
	}
	if ( bits & LASTEDGEMASK ) {
	    wordBuffer[charIndex] = '\0';
	    break;
	}
    }
}

/******************************************************************************
 *
 *****************************************************************************/
static void printWord( char* wordBuffer ) {
    unsigned char buf[32], ch;
    unsigned char* next = buf;

    while ( (ch = *wordBuffer++) != '\0' ) {
	if ( ch >= 0x20 ) {
	    *next++ = ch;
	} else {
	    char* str = NULL;
	    switch ( ch ) {
	    case 1:
		str = "CH";
		break;
	    case 2:
		str = "LL";
		break;
	    case 3:
		str = "RR";
		break;
	    default:
		fprintf( stderr, "Got %d\n", ch );
		assert( 0 );
	    }
	    strcpy( next, str );
	    next += strlen(str);
	}
    }
    *next = '\0';
    fprintf( stdout, "%s\n", buf );
} // appendChars
