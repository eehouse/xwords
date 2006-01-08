/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/****************************************************************************
 *									    *
 *	Copyright 1998-2000 by Eric House (xwords@eehouse.org).  All rights reserved.	    *
 *									    *
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
 ****************************************************************************/

/*
 * Turn a table of string pairs into a resource file of back-to-back strings
 * and an includable .h file of constants giving indices into the res file.
 *
 * Expects the name of the former in argv[1] and of the latter in argv[2].
 */

#include <stdio.h>
#include <string.h>

#include "xwords4defines.h"

#define FIRST_STR_INDEX 2000

typedef struct StringPair {
    char* constName;
    char* theString;
} StringPair;

static StringPair table[] = {
    // I'm expecting this as a -D option
#include LANGSTRFILE
    { (char*)0L, (char*)0L }
};


int
main( int argc, char** argv ) 
{
    FILE* stringResFile = fopen( argv[1], "wb" );
    FILE* stringConstFile = fopen( argv[2], "w" );
    StringPair* sp = table;
    short count;

    fprintf( stringConstFile,
             "/***********************************************************\n"
             "* This file is machine generated.\n"
             "* Don't edit: your changes will be lost.\n"
             "************************************************************/\n");

    fprintf( stringConstFile, "\n#define FIRST_STR_INDEX %d\n\n",
             FIRST_STR_INDEX );

    count = 0;
    for ( ; sp->constName != NULL && sp->theString != NULL; ++sp ) {
        short strBytes;

        strBytes = strlen(sp->theString)+1;
        fwrite( sp->theString, strBytes, 1, stringResFile );
        fprintf( stringConstFile, "#define %s %d\n", sp->constName, count );
        count += strBytes;
    }
    fprintf( stringConstFile, "#define %s %d\n", "STR_LAST_STRING", count );

    fclose( stringResFile );
    fclose( stringConstFile );
    return 0;
} // main
