// -*-mode: C; fill-column: 80; compile-command: "make dict2pdb"; -*-
/*
 * Copyright 1997 by Eric House.  All rights reserved.
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

  Converts a <CR>-separated list of words, in stdin, to a DAWG written
  to stdout in PalmOS .pdb file format.  

  Called like this: dict2dawg > dict.pdb <<.
  car
  cars
  cat
  does
  dog
  .

  Records in the database are of 48K length by default, except that
  the last will likely be smaller and that they always end with the end
  of a sub-array (so that iteration over a subarray doesn't have to
  worry about boundaries.)

  Records ought to hold two parallel arrays (but don't yet): first the
  index array, of shorts, and then the bits array of unsigned chars.
  Remember that one bit of the bits entry is actually the 17th bit of
  the index value...

  Ultimately we want to associate xloc-like date with each dictionary so
  that langauges whose relevant letters aren't all in an ascii sequence can
  be accomodated.  In most cases we'll be passed in a file containing a table
  to be used for the mapping -- just a text file with one character per line
  where A might be the 0th line, umlaut-A the first, etc.  But we'll also
  generate such a table ourselves when not given one, and output it when
  asked.

  Bugs: It's currently necessary that input to this program be sorted
  or some data may be lost.

  To do:
  Make it two parallel arrays.
  Some sort of hashing on pruning.  */

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "swap.h"

#define PRE_EDGE_RECORDCOUNT 3

/* #include "pdb.h" */
#include "dawg.h"
/* #include "swap.c" */

typedef char boolean;
#define true 1
#define false 0

typedef unsigned char Tile;

typedef struct tree_edge {
    unsigned char letter;
    unsigned long index;
    boolean terminal;
    struct tree_edge* prev;
    struct tree_edge* next;
    struct tree_edge* children;
} tree_edge;

#define MAXLENGTH 15


//////////////////////////////////////////////////////////////////////////////
// prototypes
//////////////////////////////////////////////////////////////////////////////
static void addToTree( unsigned char* buf, short buflen, tree_edge* nodege );
tree_edge* newNode( unsigned char letter, boolean terminal );
static void remember( unsigned char* c );
void readInTables( char* orderTableFile );
void init_prune_data();
void prune_tree( tree_edge* edge );
/* unsigned short byte_swap( unsigned short d ); */
void write_children( array_edge* mainArray, tree_edge* edge );
int count_nodes( tree_edge* edge );
unsigned long index_children( tree_edge* edge, unsigned long firstIndex );
void write_as_pdb( array_edge* edges, unsigned long edgeCount );
void usage( char* progName );
void initTables( void );
Tile CharToTile( unsigned char ch );
static short fileSize( char* fileName );
void write_as_files( array_edge* edges, unsigned long edgeCount,
		     char* fileNameBase );

unsigned long gWordCount = 0;

//////////////////////////////////////////////////////////////////////////////
// globals
//////////////////////////////////////////////////////////////////////////////
boolean verbose = 0;
tree_edge* rootEdge;
int gNodeCount = 0;
long gNodesCreated;
long gPulled;
char gDictName[32];
char* gOrderTableFileName = NULL;
/* char* gValueTableFileName = NULL; */
short gNumUniqueTiles;
typedef struct OrderResEntry {
/*      unsigned char count; */
/*      unsigned char value; */
    unsigned char ch;
} OrderResEntry;
static OrderResEntry gOrderTable[32];
static signed short gLookupTable[256];

dawg_header gDawgHeader;

//////////////////////////////////////////////////////////////////////////////
// main
//////////////////////////////////////////////////////////////////////////////
int main( int argc, char** argv ) {
    char buf[MAXLENGTH+10];
    unsigned long edgeCount;
    long maxWordLen = MAXLENGTH;
    int got;
    char* baseName = NULL;
    array_edge* mainArray = NULL;

    initTables();
    memset( &gDawgHeader, 0, sizeof(gDawgHeader) );

    gDictName[0] = '\0';

    while ( (got = getopt(argc, argv, "t:vhn:")) != EOF ) {
	switch ( got ) {
	case 'm':
	    sscanf( optarg, "%ld", &maxWordLen );
	    fprintf( stderr, "maxWordLen set to %ld\n", maxWordLen );
	    break;
	case 'n':
	    baseName = optarg;
	    break;
 	case 'v':
 	    verbose = true;
 	    fprintf( stderr, "verbose set\n" );
 	    break;
	case 't':
	    gOrderTableFileName = optarg;
	    break;
	case 'h':
	default:
	    usage( argv[0] );
	    break;
	}
    }
    
    if ( gOrderTableFileName != NULL ) {
	readInTables( gOrderTableFileName );
    }

    assert( baseName );

    rootEdge = newNode( '\0', 0 );
    gNodesCreated = 0;
    gPulled = 0;

    while ( fgets( buf, MAXLENGTH+9, stdin ) ) {
	unsigned char* cr = (unsigned char*)strchr( buf, '\n' );
	short wordlen;
	if ( cr ) {
	    *cr = '\0';
	}

	wordlen = strlen( buf );

	if ( (maxWordLen != MAXLENGTH) && (wordlen > maxWordLen) ) {
	    continue;
	} else if ( wordlen > MAXLENGTH ) {
	    fprintf( stderr, "word %s too long\n", buf );
	    exit(1);
	}

	// remember that *cr may be 0 *after* the call to remember	
	for ( cr = buf; *cr; ++cr ) {
	    remember(cr);
	}
	
	addToTree( buf, wordlen, rootEdge );
	++gWordCount;
    }

    fprintf( stderr, "done with addToTree (%ld nodes; %ld words)\n",
	     gNodesCreated, gWordCount );

    init_prune_data();
    prune_tree( rootEdge );
    
    fprintf( stderr, "done with prune_tree: %ld pulled\n", gPulled );

    edgeCount = index_children( rootEdge, 0 );

    if ( edgeCount >= 0x1FFFF ) {
	fprintf( stderr, "ERROR: too many edges: %ld (max is %ld)\n",
		 edgeCount, (long)0x1FFFF );
	exit( 1 );
    }

    fprintf( stderr, "done with index_children; edgeCount = %ld\n", edgeCount);
    mainArray = (array_edge*)malloc( edgeCount * sizeof(array_edge) );
    assert( mainArray );

/*     largestDiff = smallestDiff = 0; */
    write_children( mainArray, rootEdge );
    fprintf( stderr, "done with write_children\n" );
/*     fprintf( stderr, "largestDiff = %ld, smallestDiff = %ld\n", */
/* 	     largestDiff, smallestDiff ); */

    // Now we have a huge array in memory and need to write it to pdb
    // format.
    write_as_files( mainArray, edgeCount, baseName );

/*     if ( verbose ) { */
/* 	fprintf( stderr, "Writing %d nodes\n", edgeCount ); */
/* 	fprintf( stderr, "{letter, next_index, terminal, lastEdge}\n" ); */
/* 	for ( i = 0; i < edgeCount; ++i ) { */
/* 	    array_edge* edge = &gArray[i]; */
    // 	    fprintf( stderr, "/*[%d]*/ {%c, %d, %s, %s}\n", */
/* 		     i, */
/* 		     (edge->bits & LETTERMASK) + 'a', */
/* 		     ushort_byte_swap(edge->first_child), */
/* 		     (edge->bits&TERMINALMASK)?"true":"false", */
/* 		     (edge->bits&LASTEDGEMASK)?"true":"false" ); */
/* 	} */
/*     } */

/*     fprintf( stderr, "writing %ld edges to file\n", edgeCount );     */
/*     for ( i = 0; i < edgeCount; ++i ) { */
/* 	fwrite( &gArray[i], sizeof(array_edge), 1, stdout ); */
/*     } */

    return 0;
} // main

/* Given a node on the tree (not yet converted to a directed graph)
 * walk down it using letters where they exist and adding them where
 * the don't.
 *
 * The structure we're building here looks like this, for input "CAT"
 * and "CAR":
 *         /T
 *    *-C-A
 *         \R
 * That is, words beginning with the same letters share the same initial
 * branches of the tree.  Thus on entering a given level of recursion
 * there are these possibilities:
 * a) There's nothing here: create a new node and recurse on it.
 * b) We find a node that holds the letter we seek: recurse on it.
 * c) We reach the end of the list of letters without finding what we
 * seek: create a new node at the end and recurse on it.
 * d) We reach a node before which ours should have been found: create a
 * new node in the right place and recurse on it.
 */
static void addToTree( unsigned char* buf, short buflen, tree_edge* node ) {
    unsigned char target = *buf;
    boolean terminal = (buflen == 1);
    tree_edge* child;
    tree_edge* prev = NULL;
    tree_edge* new_node;

/*      if ( !target ) { */
/*  	assert( buflen == 0 ); */
/*  	return; */
/*      } */
    if ( buflen == 0 ) return;
    assert( buflen > 0 );

    if ( node->children == NULL ) {
	addToTree( buf+1, buflen-1,
		   node->children = newNode( target, terminal ) );
	return;
    }

    for ( child = node->children; child != NULL; child = child->next ) {
	if ( child->letter == target ) {
	    addToTree( buf+1, buflen-1, child );
	    return;
	} else if ( child->letter > target ) { // it's not in the tree yet.
	    new_node = newNode( target, terminal );
	    new_node->next = child;
	    new_node->prev = child->prev;
	    if ( child->prev ) {
		child->prev->next = new_node;
	    } else { // it's the first node!
		node->children = new_node;
	    }
	    child->prev = new_node;

	    addToTree( buf+1, buflen-1, new_node );
	    return;
	}
	prev = child;
    }

    assert( prev != NULL );
    new_node = newNode( target, terminal );
    prev->next = new_node;
    new_node->prev = prev;
    addToTree( buf+1, buflen-1, new_node );
    return;
}

tree_edge* newNode( unsigned char letter, boolean terminal ) {
    tree_edge* result = (tree_edge*)malloc( sizeof(tree_edge ));
    assert( result );
    ++gNodesCreated;
    result->letter = letter;
    result->index = 0xFFFF;
    result->terminal = terminal;
    result->children = result->next = result->prev = NULL;

    ++gNodeCount;
    return result;
}

//////////////////////////////////////////////////////////////////////////////
// prune_tree (and helpers)
//////////////////////////////////////////////////////////////////////////////
boolean sameStructure( tree_edge* node1, tree_edge* node2 ) {
    // simple cases first.
    if ( node1 == node2 ) return true;
    else if ( node1 == NULL || node2 == NULL ) return false;
    else if ( node1->letter != node2->letter ) return false;
    else if ( node1->terminal != node2->terminal ) return false;
    //else if ( count_nodes( node1 ) != count_nodes( node2 ) ) return false;
    else {
	tree_edge* children1;
	tree_edge* children2;
	for ( children1 = node1->children, children2 = node2->children;
	      children1 || children2;
	      children1 = children1->next, children2 = children2->next ) {
	    if ( !sameStructure( children1, children2 ) )
		return false;
	}
	for ( children1 = node1->next, children2 = node2->next;
	      children1 || children2;
	      children1 = children1->next, children2 = children2->next ) {
	    if ( !sameStructure( children1, children2 ) )
		return false;
	}
	return (children1 == NULL) && (children2 == NULL);
    }
}

typedef struct visited_edge {
    tree_edge* theEdge;
    struct visited_edge* next;
} visited_edge;
static visited_edge* visitedEdges[256];

void init_prune_data() {
    short i;
    for ( i = 0; i < 26; ++i ) {
	visitedEdges[i] = NULL;
    }
}

tree_edge* visited( tree_edge* node ) {
    short hash = node->letter;// - 'a';
    //assert( hash >=0 && hash < 26 );
    if ( visitedEdges[hash] == NULL ) {
	visitedEdges[hash] = (visited_edge*)malloc(sizeof(visited_edge));
	assert( visitedEdges[hash] );
	visitedEdges[hash]->theEdge = node;
	visitedEdges[hash]->next = NULL;
	return node;
    } else {
	visited_edge* visited;
	for ( visited = visitedEdges[hash]; visited; 
	      visited = visited->next ) {
	    if ( verbose ) {
		fprintf( stderr, "looking at %c and %c\n",
			 node->letter, visited->theEdge->letter );
	    }
	    if ( sameStructure( node, visited->theEdge ) ) {
		if ( verbose ) {
		    fprintf( stderr, "pruning tree beginning with %c\n",
			     node->letter );
		}
		return visited->theEdge;
	    }
	}
	// didn't find it.  Insert new entry at head of list.
	visited = (visited_edge*)malloc(sizeof(visited_edge));
	assert( visited );
	visited->theEdge = node;
	visited->next = visitedEdges[hash];
	visitedEdges[hash] = visited;
	return node;
    }
} // visited

int count_nodes( tree_edge* edge ) {
    short result = 0;
    while ( edge ) {
	result += count_nodes( edge->children );
	++result;
	edge = edge->next;
    }
    return result;
}

/* Walk the tree.  Starting at the lowest points, lookup each node to see
 * if an equivalent one has already been visited.  If so, replace it with
 * (a ptr to) the first one seen.
 */
void prune_tree( tree_edge* edge ) {
    tree_edge* child = edge->children;
    tree_edge* tmp;

    //    fprintf( stderr, "prune_tree called\n" );

    if ( !child ) {
	return;
    }

/*     if( edge->letter == 'c' ) { */
/* 	fprintf( stderr, "C\n" ); */
/*     } */

    while ( child ) {
	prune_tree( child );
	child = child->next;
    }

    tmp = visited( edge->children );
    if ( tmp != edge->children ) {
	short pulled = count_nodes(edge->children);
/* 	fprintf( stderr, "Removing %d nodes\n", pulled ); */
	gPulled += pulled;
	edge->children = tmp;
    }
}

//////////////////////////////////////////////////////////////////////////////
// write_edge (and helpers)
//////////////////////////////////////////////////////////////////////////////

unsigned long index_children( tree_edge* edge, unsigned long firstIndex ) {
    tree_edge* child;
    for ( child = edge->children; child; child = child->next ) {
	if ( child->index == 0xFFFF ) {
	    child->index = firstIndex++;
/* 	    assert( firstIndex != 0xFFFF ); */
/* 	    fprintf( stderr, "set index of %c (%x) to %d\n", child->letter, */
/* 		     child, child->index ); */
	}
    }

    for ( child = edge->children; child; child = child->next ) {
	firstIndex = index_children( child, firstIndex );
    }
    return firstIndex;
}

void write_child( array_edge* mainArray, tree_edge* child ) {
    if ( child ) {
	array_edge* entry = &mainArray[child->index];
	unsigned char bits = 0;
	unsigned long childIndex
	    = (child->children!=NULL)? child->children->index : 0;
	assert( childIndex <= 0x0001FFFF );
	
	entry->lowByte = childIndex & 0x000000FF;
	entry->highByte = (childIndex>>8) & 0x000000FF;

	bits = CharToTile(child->letter) & LETTERMASK;

	if ( childIndex & 0x00010000 ) {
	    bits |= LASTBITMASK;
	}
	if ( child->terminal ) {
	    bits |= ACCEPTINGMASK;
	}
	if ( child->next == NULL ) {
	    bits |= LASTEDGEMASK;
	}
	entry->bits = bits;
    }
}

void write_children( array_edge* mainArray, tree_edge* edge ) {
    tree_edge* child;
    for ( child = edge->children; child; child = child->next ) {    
	write_child( mainArray, child );
	write_children( mainArray, child );

	// gather some stats
/* 	if ( child->index != 0 ) { */
/* 	    diff = edge->index - child->index; */
/* 	    if ( diff > largestDiff ) { */
/* 		largestDiff = diff; */
/* 	    } */
/* 	    if ( diff < smallestDiff ) { */
/* 		smallestDiff = diff;  */
/* 	    } */
/* 	} */

    }
}

//////////////////////////////////////////////////////////////////////////////
// write_as_pdb and helpers
//////////////////////////////////////////////////////////////////////////////

void write_pdb_record_data( array_edge* edges, unsigned long startCount,
			    unsigned long count ) {
    unsigned long i;
    for ( i = startCount; i < startCount + count; ++i ) {
	fwrite( &edges[i], sizeof(array_edge), 1, stdout );
    }
}

// I *think* that the upper bound on this is 0xFFFF/edgesize minus enough that
// I can add edges out to the end of the subarray in which the line falls
// can be accomodated -- which I guess is about 32-1-1 (minus one because
// blanks take up one of the 32 slots though they don't appear in DAWGs,
// and minus another because the boundary must appear after at least the
// first or we just leave it there.)

#define EDGES_PER_RECORD 0x3FFF
#ifndef EDGES_PER_RECORD
# define EDGES_PER_RECORD 0x00005528
#endif

/* Write as binary files segmented appropriately in case the target is PalmOS or
 * other platform with restricted-length databases.
 */
void write_as_files( array_edge* edges, unsigned long edgeCount,
		     char* fileNameBase ) {
    unsigned long firstUnhousedEdge = 0;
    short numEdgesThisFile;
    boolean exitNext = false;
/*     unsigned long prevEdgeCount; */
/*     unsigned long curOffset = 0; */
    short fileNum;

    for ( fileNum = 0; !exitNext; ++fileNum ) {
	unsigned long lastEdge;
	char buf[40];
	FILE* dawgOutF;
	unsigned long firstEdgeThisFile = 0;

	/* from the first edge not yet in a record, go forward EDGES_PER_RECORD
	   edges, and than march forward further until the current subarray is
	   finished. */
	lastEdge = firstUnhousedEdge + EDGES_PER_RECORD - 1;
	if ( lastEdge + 1 >= edgeCount ) {
	    lastEdge = edgeCount - 1;
	    assert( (edges[lastEdge].bits & LASTEDGEMASK) );
	    exitNext = true;
	}
	while ( (edges[lastEdge].bits & LASTEDGEMASK) == 0 ) {
	    ++lastEdge;
	}

	numEdgesThisFile = lastEdge - firstUnhousedEdge + 1;
	
	sprintf( buf, "%s_%d.bin", fileNameBase, fileNum );
	dawgOutF = fopen( buf, "wb" );
	fwrite( &edges[firstUnhousedEdge], sizeof(array_edge), 
		numEdgesThisFile, dawgOutF );
	fclose( dawgOutF );

	fprintf( stderr, "wrote edges from %ld to %ld to file %s\n",
		 firstUnhousedEdge, firstUnhousedEdge+numEdgesThisFile, buf );

	firstUnhousedEdge = lastEdge + 1;
    }

    fprintf( stderr, "%ld edges yielded %d records of up to %ld edges each\n",
	     edgeCount, fileNum, (long)EDGES_PER_RECORD );

} // write_as_files

/******************************************************************************
 * Read in a file of letters, one per line, whose position in the file will
 * determine the translation from char to Tile when the dawg is written out.
 * If no such file is passed in, we'll create our own based on the ascii order
 * of those chars we see in processing the dictionary.  If one is passed in,
 * we'll use it, but we'll fail if we encounter a letter not on the list.
 *
 * Also, for faster lookup of Tile values we maintain a second table mapping
 * chars to tiles.  'A' might map to 1, A-umlaut to 2, etc., if 0 is the blank
 * char
 *****************************************************************************/
void initTables() {
    memset( gOrderTable, 0, 32*sizeof(*gOrderTable) );
    memset( gLookupTable, -1, 256*sizeof(*gLookupTable) );
} // initTables

void readInTables( char* orderTableFile ) {
    unsigned char ch = 0;
    FILE* f = fopen( orderTableFile, "rb" );
    assert( f );

    gNumUniqueTiles = 0;
    while ( fscanf( f, "%c\n", &ch ) != EOF ) {
	assert( gNumUniqueTiles <= 32 );
	assert( ch < 255 );
	assert( gOrderTable[gNumUniqueTiles].ch == 0 );
	gOrderTable[gNumUniqueTiles].ch = ch;
	gLookupTable[ch] = gNumUniqueTiles;
	++gNumUniqueTiles;
    }
    fclose( f );

} // readInTables

/******************************************************************************
 *
 *****************************************************************************/
static void remember( unsigned char* c ) {
    signed short tile = gLookupTable[*c];
    assert( gOrderTableFileName != NULL );
    if ( tile == -1 ) {
	fprintf( stderr, "ERROR: unexpected character '%c' (0x%x)\n",
		 *c, (short)*c );
	exit(1);
    }
    assert( tile < 32 );
    *c = tile;
} // remember

/******************************************************************************
 *
 *****************************************************************************/
Tile CharToTile( unsigned char ch ) {
    return ch;
/*      assert( gLookupTable[ch] < 32 ); */
/*      return (Tile)gLookupTable[ch]; */
} // CharToTile

static short fileSize( char* fileName ) {
    short result;
    FILE* f = fopen( fileName, "rb" );
    assert( f );
    if ( fseek( f, 0L, SEEK_END ) != 0 ) {
	fprintf( stderr, "error from fseek\n" );
	exit(1);
    }
    result = ftell( f );
    fclose( f );
    return (short)result;
} // fileSize

//////////////////////////////////////////////////////////////////////////////
// usage
//////////////////////////////////////////////////////////////////////////////
void usage( char* progName ) {
    fprintf( stderr, 
	     "USAGE: %s\n"
	     "   [-m<maxLength>]\n"
	     "   [-v] (verbose) \n"
/* 	     "   [-t char-order-table-file] \n" */
/* 	     "   [-n <pdbName>] \n" */
	     "   <word_list >dawg_file\n",
	     progName );
    exit( 1 );
}

