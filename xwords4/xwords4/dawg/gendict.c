#include <stdio.h>

/* 
 * Copyright 1998 by Eric House.  All rights reserved.
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
 
typedef char boolean;
#define true 1
#define false 0

#define MAXWORDLEN 15

boolean writeOneWord( char* word, int len );
void usage();

//////////////////////////////////////////////////////////////////////////////
// main
// This program generates all possible combinations of letters 'A'-'Z' of
// lengths between 2 and MAXWORDLEN, writing them to stdout.  It's meant
// to be used to test the dawg dictionary and a state machine based on
// it: if the output of this program is used as input to that machine, the
// list of words accepted by the machine should be identical to the list
// from which the dawg was created -- provided sufficient length words
// are created.
//////////////////////////////////////////////////////////////////////////////
int main( int argc, char** argv ) {
    int i, j;
    long lowerbound = 0;
    long upperbound = 0;
    char buffer[MAXWORDLEN+1];

    if ( argc ==3 ) {
	sscanf( argv[1], "%d", &lowerbound );
	sscanf( argv[2], "%d", &upperbound );
	if ( lowerbound < 2 || upperbound < 2 ) {
	    usage();
	}
    } else {
	usage();
    }

    for ( i = lowerbound; i <= upperbound; ++i ) {
	buffer[0] = '\0';
	for ( j = 0; j < i; ++j ) {
	    strcat( buffer, "A" );
	}
	while ( !writeOneWord( buffer, i-1 ) ) {
	    // do nothing
	}
    }
}

/* Increment the last letter if possible.  Otherwise reset it and find
 * the first letter above it that can be incremented, resetting along
 * the way.  If the *first* letter needs to be reset we're finished.
 */
boolean writeOneWord( char* word, int len ) {
    fprintf( stdout, "%s\n", word );

    if ( word[len] != 'Z' ) {
	word[len]++;
    } else {
	int i;
	word[len] = 'A';
	for ( i = len-1; ; --i ) {
	    if ( word[i] != 'Z' ) {
		 word[i]++;
		 break;
	    } else if ( i == 0 ) { // they're *all* Zs...
		return true; // we wrote all the words!
	    } else {
		 word[i] = 'A';
	    }
	}
    }
    return false;
}

void usage() {
    fprintf( stderr,
	     "USAGE: gendict upperbound lowerbound\n"
	     "   (Both must be >= 2.)\n" );
    exit( 0 );
}

