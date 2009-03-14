/* -*- compile-command: "g++ -DDEBUG -O0 -Wall -g -o dict2dawg dict2dawg.cpp"; -*- */
/*************************************************************************
 * adapted from perl code that was itself adapted from C++ code
 * Copyright (C) 2000 Falk Hueffner

 * This version Copyright (C) 2002,2006-2009 Eric House
 * (xwords@eehouse.org)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
**************************************************************************
 * inputs: 0. Name of file mapping letters to 0..31 values.  In English
 * case just contains A..Z.  This will be used to translate the tries
 * on output.
 *         1. Max number of bytes per binary output file.
 *
 *         2. Basename of binary files for output.

 *         3. Name of file to which to write the number of the
 * startNode, since I'm not rewriting a bunch of code to expect Falk's
 * '*' node at the start.
 *

 *         In STDIN, the text file to be compressed.  It absolutely
 * must be sorted.  The sort doesn't have to follow the order in the
 * map file, however.

 * This is meant eventually to be runnable as part of a cgi system for
 * letting users generate Crosswords dicts online.
**************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <assert.h>
#include <errno.h>
#include <algorithm>

#include <string>
#include <map>
#include <vector>
#include <list>

typedef unsigned char Letter;   // range 1..26 for English, always < 64
typedef unsigned int Node;
typedef std::vector<Node> NodeList;
typedef std::vector<Letter*> WordList;

#define VERSION_STR "$Rev$"

#define MAX_WORD_LEN 15
#define T2ABUFLEN(s) (((s)*4)+3)

int gFirstDiff;

static Letter gCurrentWordBuf[MAX_WORD_LEN+1] = { '\0' };
 // this will never change for non-sort case
static Letter* gCurrentWord = gCurrentWordBuf;
static int gCurrentWordLen;

static bool gDone = false;
static unsigned int gNextWordIndex;
static void (*gReadWordProc)(void) = NULL;
static NodeList gNodes;       // final array of nodes
static unsigned int gNBytesPerOutfile = 0xFFFFFFFF;
static char* gTableFile = NULL;
static bool gIsMultibyte = false;
static const char* gEncoding = NULL;
static char* gOutFileBase = NULL;
static char* gStartNodeOut = NULL;
static FILE* gInFile = NULL;
static bool gKillIfMissing = true;
static char gTermChar = '\n';
static bool gDumpText = false;                // dump the dict as text after?
static char* gCountFile = NULL;
static const char* gLang = NULL;
static char* gBytesPerNodeFile = NULL;        // where to write whether node
                                       // size 3 or 4
int gWordCount = 0;
std::map<Letter,wchar_t> gTableHash;
int gBlankIndex;
std::vector<char> gRevMap;
#ifdef DEBUG
bool gDebug = false;
#endif
std::map<NodeList, int> gSubsHash;
bool gForceFour = false;             // use four bytes regardless of need?
static int gFileSize = 0;
int gNBytesPerNode;
bool gUseUnicode;
int gLimLow = 2;
int gLimHigh = MAX_WORD_LEN;


// OWL is 1.7M
#define MAX_POOL_SIZE (10 * 0x100000)
#define ERROR_EXIT(...) error_exit( __LINE__, __VA_ARGS__ );

static char* parseARGV( int argc, char** argv, const char** inFileName );
static void usage( const char* name );
static void error_exit( int line, const char* fmt, ... );
static void makeTableHash( void );
static WordList* parseAndSort( void );
static void printWords( WordList* strings );
static bool firstBeforeSecond( const Letter* lhs, const Letter* rhs );
static char* tileToAscii( char* out, int outSize, const Letter* in );
static int buildNode( int depth );
static void TrieNodeSetIsLastSibling( Node* nodeR, bool isLastSibling );
static int addNodes( NodeList& newedgesR );
static void TrieNodeSetIsTerminal( Node* nodeR, bool isTerminal );
static bool TrieNodeGetIsTerminal( Node node );
static void TrieNodeSetIsLastSibling( Node* nodeR, bool isLastSibling );
static bool TrieNodeGetIsLastSibling( Node node );
static void TrieNodeSetLetter( Node* nodeR, Letter letter );
static Letter TrieNodeGetLetter( Node node );
static void TrieNodeSetFirstChildOffset( Node* nodeR, int fco );
static int TrieNodeGetFirstChildOffset( Node node );
static int findSubArray( NodeList& newedgesR );
static void registerSubArray( NodeList& edgesR, int nodeLoc );
static Node MakeTrieNode( Letter letter, bool isTerminal,
                          int firstChildOffset, bool isLastSibling );
static void printNodes( NodeList& nodesR );
static void printNode( int index, Node node );
static void moveTopToFront( int* firstRef );
static void writeOutStartNode( const char* startNodeOut, 
                               int firstRootChildOffset );
static void emitNodes( unsigned int nBytesPerOutfile, const char* outFileBase );
static void outputNode( Node node, int nBytes, FILE* outfile );
static void printOneLevel( int index, char* str, int curlen );
static void readFromSortedArray( void );

int 
main( int argc, char** argv ) 
{ 
    gReadWordProc = readFromSortedArray;

    const char* inFileName;
    if ( NULL == parseARGV( argc, argv, &inFileName ) ) {
        usage(argv[0]);
        exit(1);
    }

 try_english:
    char buf[32];
    const char* locale = "";
    if ( !!gLang && !!gEncoding ) {
        snprintf( buf, sizeof(buf), "%s.%s", gLang, gEncoding );
        locale = buf;
    }
    char* oldloc = setlocale( LC_ALL, locale );
    if ( !oldloc ) {
        // special case for spiritone.net, where non-US locale files aren't
        // available.  Since utf-8 is the same for all locales, we can get by
        // with en_US instead
        if ( gIsMultibyte && 0 != strcmp( gLang, "en_US" )) {
            gLang = "en_US";
            goto try_english;
        }

        ERROR_EXIT( "setlocale(%s) failed, error: %s", locale, 
                    strerror(errno) );
    } else {
        fprintf( stderr, "old locale: %s\n", oldloc );
    }
    
    makeTableHash();

    // Do I need this stupid thing?  Better to move the first row to
    // the front of the array and patch everything else.  Or fix the
    // non-palm dictionary format to include the offset of the first
    // node.

    Node dummyNode = (Node)0xFFFFFFFF;
    assert( sizeof(Node) == 4 );
    gNodes.push_back(dummyNode);

    if ( NULL == inFileName ) {
        gInFile = stdin;
    } else {
        gInFile = fopen( inFileName, "r" );
    }
    
    (*gReadWordProc)();

    int firstRootChildOffset = buildNode(0);
    moveTopToFront( &firstRootChildOffset );

    if ( gStartNodeOut ) {
        writeOutStartNode( gStartNodeOut, firstRootChildOffset );
    }

#ifdef DEBUG
    if ( gDebug ) {
        fprintf( stderr, "\n... dumping table ...\n" );
        printNodes( gNodes );
    }
#endif
    // write out the number of nodes if requested
    if ( gCountFile ) {
        FILE* OFILE;
        OFILE = fopen( gCountFile, "w" );
        unsigned long be = htonl( gWordCount );
        fwrite( &be, sizeof(be), 1, OFILE );
        fclose( OFILE );
        fprintf( stderr, "Wrote %d (word count) to %s\n", gWordCount, 
                 gCountFile );
    }

    if ( gOutFileBase ) {
        emitNodes( gNBytesPerOutfile, gOutFileBase );
    }

    if ( gDumpText && gNodes.size() > 0 ) {
        char buf[(MAX_WORD_LEN*2)+1];
        printOneLevel( firstRootChildOffset, buf, 0 );
    }

    if ( gBytesPerNodeFile ) {
        FILE* OFILE = fopen( gBytesPerNodeFile, "w" );
        fprintf( OFILE, "%d", gNBytesPerNode );
        fclose( OFILE );
    }
    fprintf( stderr, "Used %d per node.\n", gNBytesPerNode );

    if ( NULL != inFileName ) {
        fclose( gInFile );
    }

} /* main */

// We now have an array of nodes with the last subarray being the
// logical top of the tree.  Move them to the start, fixing all fco
// refs, so that legacy code like Palm can assume top==0.
//
// Note: It'd probably be a bit faster to integrate this with emitNodes
// -- unless I need to have an in-memory list that can be used for
// lookups.  But that's best for debugging, so keep it this way for now.
//
// Also Note: the first node is a dummy that can and should be tossed
// now.

static void
moveTopToFront( int* firstRef )
{
    int firstChild = *firstRef;
    *firstRef = 0;

    NodeList lastSub;

    if ( firstChild > 0 ) {
        lastSub.assign( gNodes.begin() + firstChild, gNodes.end() );
        gNodes.erase( gNodes.begin() + firstChild, gNodes.end() );
    } else if ( gWordCount != 0 ) {
        ERROR_EXIT( "there should be no words!!" );
    }

    // remove the first (garbage) node
    gNodes.erase( gNodes.begin() );

    int diff;
    if ( firstChild > 0 ) {
        // -1 because all move down by 1; see prev line
        diff = lastSub.size() - 1;
        if ( diff < 0 ) {
            ERROR_EXIT( "something wrong with lastSub.size()" );
        }
    } else {
        diff = 0;
    }

    // stick it on the front
    gNodes.insert( gNodes.begin(), lastSub.begin(), lastSub.end() );

    // We add diff to everything. There's no subtracting because
    // nobody had any refs to the top list.

    unsigned int ii;
    for ( ii = 0; ii < gNodes.size(); ++ii ) {
        int fco = TrieNodeGetFirstChildOffset( gNodes[ii] );
        if ( fco != 0 ) {      // 0 means NONE, not 0th!!
            TrieNodeSetFirstChildOffset( &gNodes[ii], fco + diff );
        }
    }
} // moveTopToFront

static int
buildNode( int depth )
{
    if ( gCurrentWordLen == depth ) {
        // End of word reached. If the next word isn't a continuation
        // of the current one, then we've reached the bottom of the
        // recursion tree.
        (*gReadWordProc)();
        if (gFirstDiff < depth || gDone) {
            return 0;
        }
    }

    NodeList newedges;

    bool wordEnd;
    do {
        Letter letter = gCurrentWord[depth];
        bool isTerminal = (gCurrentWordLen - 1) == depth;

        int nodeOffset = buildNode( depth + 1 );
        Node newNode = MakeTrieNode( letter, isTerminal, nodeOffset, false );

        wordEnd = (gFirstDiff != depth) || gDone;
        if ( wordEnd ) {
            TrieNodeSetIsLastSibling( &newNode, true );
        }

        newedges.push_back( newNode );
    } while ( !wordEnd );

    return addNodes( newedges );
} // buildNode

static int
addNodes( NodeList& newedgesR )
{
    int found = findSubArray( newedgesR );

    if ( found == 0 ) {
        ERROR_EXIT( "0 is an invalid match!!!" );
    }

    if ( found < 0 ) {
        found = gNodes.size();
#if defined DEBUG && defined SEVERE_DEBUG
        if ( gDebug ) {
            fprintf( stderr, "adding...\n" );
            printNodes( newedgesR );
        }
#endif
        gNodes.insert( gNodes.end(), newedgesR.begin(), newedgesR.end() );

        registerSubArray( newedgesR, found );
    }
#ifdef DEBUG
    if ( gDebug ) {
        fprintf( stderr, "%s => %d\n", __func__, found );
    }
#endif
    return found;
} // addNodes

static void
printNode( int index, Node node )
{
    Letter letter = TrieNodeGetLetter(node);
    assert( letter < gRevMap.size() );
    fprintf( stderr,
             "[%d] letter=%d(%c); isTerminal=%s; isLastSib=%s; fco=%d;\n", 
             index, letter, gRevMap[letter],
             TrieNodeGetIsTerminal(node)?"true":"false",
             TrieNodeGetIsLastSibling(node)?"true":"false",
             TrieNodeGetFirstChildOffset(node));
} // printNode

static void
printNodes( NodeList& nodesR )
{
    unsigned int ii;
    for ( ii = 0; ii < nodesR.size(); ++ii ) {
        Node node = nodesR[ii];
        printNode( ii, node );
    }
}

// Hashing.  We'll keep a hash of offsets into the existing nodes
// array, and as the key use a string that represents the entire sub
// array.  Since the key is what we're matching for, there should never
// be more than one value per hash and so we don't need buckets.
// Return -1 if there's no match.

static int
findSubArray( NodeList& newedgesR )
{
    std::map<NodeList, int>::iterator iter = gSubsHash.find( newedgesR );
    if ( iter != gSubsHash.end() ) {
        return iter->second;
    } else {
        return -1;
    }
} // findSubArray

// add to the hash
static void
registerSubArray( NodeList& edgesR, int nodeLoc )
{
#ifdef DEBUG
    std::map<NodeList, int>::iterator iter = gSubsHash.find( edgesR );
    if ( iter != gSubsHash.end() ) {
        ERROR_EXIT( "entry for key shouldn't exist!!" );
    }
#endif
    gSubsHash[edgesR] = nodeLoc;
} // registerSubArray

static int
wordlen( const Letter* word )
{
    const char* str = (const char*)word;
    return strlen( str );
}

static void
readFromSortedArray( void )
{
    // The first time we need a new word, we read 'em all in.
    static WordList* sInputStrings = NULL; // we'll just let this leak

    if ( sInputStrings == NULL ) {
        sInputStrings = parseAndSort();
        gNextWordIndex = 0;

#ifdef DEBUG
        if ( gDebug ) {
            printWords( sInputStrings );
        }
#endif
    }

    for ( ; ; ) {
        Letter* word = (Letter*)"";

        if ( !gDone ) {
            gDone = gNextWordIndex == sInputStrings->size();
            if ( !gDone ) {
                word = sInputStrings->at(gNextWordIndex++);
#ifdef DEBUG
            } else if ( gDebug ) {
                fprintf( stderr, "gDone set to true\n" );
#endif
            }
#ifdef DEBUG
            if ( gDebug ) {
                char buf[T2ABUFLEN(MAX_WORD_LEN)];
                fprintf( stderr, "%s: got word: %s\n", __func__,
                         tileToAscii( buf, sizeof(buf), word ) );
            }
#endif
        }
        int numCommonLetters = 0;
        int len = wordlen( word );
        if ( gCurrentWordLen < len ) {
            len = gCurrentWordLen;
        }

        while ( gCurrentWord[numCommonLetters] == word[numCommonLetters]
                && numCommonLetters < len ) {
            ++numCommonLetters;
        }

        gFirstDiff = numCommonLetters;
        if ( (gCurrentWordLen > 0) && (wordlen(word) > 0)
             && !firstBeforeSecond( gCurrentWord, word ) ) {
#ifdef DEBUG
            if ( gDebug ) {
                char buf1[T2ABUFLEN(MAX_WORD_LEN)];
                char buf2[T2ABUFLEN(MAX_WORD_LEN)];
                fprintf( stderr,
                         "%s: words %s and %s are the same or out of order\n",
                         __func__, 
                         tileToAscii( buf1, sizeof(buf1), gCurrentWord ),
                         tileToAscii( buf2, sizeof(buf2), word ) );
            }
#endif
            continue;
        }
    
        gCurrentWord = word;
        gCurrentWordLen = wordlen(word);
        break;
    }

#ifdef DEBUG
    if ( gDebug ) {
        char buf[T2ABUFLEN(MAX_WORD_LEN)];
        fprintf( stderr, "gCurrentWord now %s\n", 
                 tileToAscii( buf, sizeof(buf), gCurrentWord) );
    }
#endif
} // readFromSortedArray

static wchar_t
getWideChar( FILE* file )
{
    wchar_t dest;
    char src[4] = { '\0' };
    const char* srcp = src;
    int ii;
    mbstate_t ps = {0};

    for ( ii = 0; ; ++ii ) {
        int byt = getc( file );
        size_t siz;

        if ( byt == EOF || byt == gTermChar ) {
            assert( 0 == ii );
            dest = byt;
            break;
        }

        assert( ii < 4 );
        src[ii] = byt;
        siz = mbsrtowcs( &dest, &srcp, 1, &ps );

        if ( siz == (size_t)-1 ) {
            continue;
        } else if ( siz == 1 ) {
            break;
        }
    }
//     fprintf( stderr, "%s=>%lc\n", __func__, dest );
    return dest;
} // getWideChar

static Letter*
readOneWord( Letter* wordBuf, int bufLen, int* lenp, bool* gotEOF )
{
    Letter* result = NULL;
    int count = 0;
    bool dropWord = false;

    // for each byte, append to an internal buffer up to size limit.
    // On reaching an end-of-word or EOF, check if the word formed is
    // within the length range and contains no unknown chars.  If yes,
    // return it.  If no, start over ONLY IF the terminator was not
    // EOF.
    for ( ; ; ) {
        wchar_t byt = gIsMultibyte? getWideChar( gInFile ) : getc( gInFile );

        // EOF is special: we don't try for another word even if
        // dropWord is true; we must leave now.
        if ( byt == EOF || byt == gTermChar ) {
            bool isEOF = byt == EOF;
            *gotEOF = isEOF;

            assert( isEOF || count < bufLen || dropWord );
            if ( !dropWord && (count >= gLimLow) && (count <= gLimHigh) ) {
                assert( count < bufLen );
                wordBuf[count] = '\0';
                result = wordBuf;
                *lenp = count;
                ++gWordCount;
                break;
            } else if ( isEOF ) {
                assert( !result );
                break;
            } 
#ifdef DEBUG
            if ( gDebug ) {
                char buf[T2ABUFLEN(count)];
                wordBuf[count] = '\0';
                fprintf( stderr, "%s: dropping word (len %d>=%d): %s\n", 
                         __func__, count, gLimHigh, 
                         tileToAscii( buf, sizeof(buf), wordBuf ) );
            }
#endif
            count = 0;  // we'll start over
            dropWord = false;

        } else if ( count >= bufLen ) {
            // Just drop it...
            dropWord = true;

            // Don't call into the hashtable twice here!!
        } else if ( gTableHash.find(byt) != gTableHash.end() ) {
            assert( count < bufLen );
            wordBuf[count++] = gTableHash[byt];
            if ( count >= bufLen ) {
                dropWord = true;
            }
        } else if ( gKillIfMissing || !dropWord ) {
            char buf[T2ABUFLEN(count)];
            wordBuf[count] = '\0';

            tileToAscii( buf, sizeof(buf), wordBuf );

            if ( gKillIfMissing ) {
                ERROR_EXIT( "chr %lc (%d/0x%x) not in map file %s\n"
                            "last word was %s\n",
                            byt, (int)byt, (int)byt, gTableFile, buf );
            } else if ( !dropWord ) {
#ifdef DEBUG
                if ( gDebug ) {
                    fprintf( stderr, "%s: chr %c (%d) not in map file %s\n"
                             "dropping partial word %s\n", __func__,
                             (char)byt, (int)byt, gTableFile, buf );
                }
#endif
                dropWord = true;
            }
        }
    }

//     if ( NULL != result ) {
//         char buf[T2ABUFLEN(MAX_WORD_LEN)];
//         fprintf( stderr, "%s returning %s\n", __func__,
//                  tileToAscii( buf, sizeof(buf), result ) );
//     }
    return result;
} // readOneWord

static void
readFromFile( void )
{
    Letter wordBuf[MAX_WORD_LEN+1];
    static bool s_eof = false;
    Letter* word;
    int len;

    gDone = s_eof;
    
    // Repeat until we get a new word that's not "out-of-order".  When
    // we see this the problem isn't failure to sort, it's duplicates.
    // So dropping is ok.  The alternative would be detecting dupes
    // during the sort.  This seems easier.
    for ( ; ; ) {
        if ( !gDone ) {
            word = readOneWord( wordBuf, sizeof(wordBuf), &len, &s_eof );
            gDone = NULL == word;
        }
        if ( gDone ) {
            word = (Letter*)"";
            len = 0;
        }

        int numCommonLetters = 0;
        if ( gCurrentWordLen < len ) {
            len = gCurrentWordLen;
        }

        while ( gCurrentWord[numCommonLetters] == word[numCommonLetters]
                && numCommonLetters < len ) {
            ++numCommonLetters;
        }

        gFirstDiff = numCommonLetters;
        if ( (gCurrentWordLen > 0) && (wordlen(word) > 0)
             && !firstBeforeSecond( gCurrentWord, word ) ) {
#ifdef DEBUG
            if ( gDebug ) {
                char buf1[T2ABUFLEN(MAX_WORD_LEN)];
                char buf2[T2ABUFLEN(MAX_WORD_LEN)];
                fprintf( stderr,
                         "%s: words %s and %s are the smae or out of order\n",
                         __func__, 
                         tileToAscii( buf1, sizeof(buf1), gCurrentWord ),
                         tileToAscii( buf2, sizeof(buf2), word ) );
            }
#endif
            continue;
        }
        break;
    }
    gCurrentWordLen = wordlen(word);
    strncpy( (char*)gCurrentWordBuf, (char*)word, sizeof(gCurrentWordBuf) );

#ifdef DEBUG
    if ( gDebug ) {
        char buf[T2ABUFLEN(MAX_WORD_LEN)];
        fprintf( stderr, "gCurrentWord now %s\n", 
                 tileToAscii( buf, sizeof(buf), gCurrentWord) );
    }
#endif
} // readFromFile

static bool
firstBeforeSecond( const Letter* lhs, const Letter* rhs )
{
    bool gt = 0 > strcmp( (char*)lhs, (char*)rhs );
    return gt;
}

static char*
tileToAscii( char* out, int outSize, const Letter* in )
{
    char tiles[outSize];
    int tilesLen = 1;
    tiles[0] = '[';

    char* orig = out;
    for ( ; ; ) {
        Letter ch = *in++;
        if ( '\0' == ch ) {
            break;
        }
        assert( ch < gRevMap.size() );
        *out++ = gRevMap[ch];
        tilesLen += sprintf( &tiles[tilesLen], "%d,", ch );
        assert( (out - orig) < outSize );
    }

    assert( tilesLen+1 < outSize );
    tiles[tilesLen] = ']';
    tiles[tilesLen+1] = '\0';
    strcpy( out, tiles );

    return orig;
}

static WordList*
parseAndSort( void )
{
    WordList* wordlist = new WordList;

    // allocate storage for the actual chars.  wordlist's char*
    // elements will point into this.  It'll leak.  So what.
    
    int memleft = gFileSize;
    if ( memleft == 0 ) {
        memleft = MAX_POOL_SIZE;
    }
    Letter* str = (Letter*)malloc( memleft );
    if ( NULL == str ) {
        ERROR_EXIT( "can't allocate main string storage" );
    }

    bool eof = false;
    for ( ; ; ) {
        int len;
        Letter* word = readOneWord( str, memleft, &len, &eof );

        if ( NULL == word ) {
            break;
        }

        wordlist->push_back( str );
        ++len;                  // include null byte
        str += len;
        memleft -= len;

        if ( eof  ) {
            break;
        }
        if ( memleft < 0 ) {
            ERROR_EXIT( "no memory left\n" );
        }
    }

    if ( gWordCount > 1 ) {
#ifdef DEBUG
        if ( gDebug ) {
            fprintf( stderr, "starting sort...\n" );
        }
#endif
        std::sort( wordlist->begin(), wordlist->end(), firstBeforeSecond );
#ifdef DEBUG
        if ( gDebug ) {
            fprintf( stderr, "sort finished\n" );
        }
#endif
    }
    return wordlist;
} // parseAndSort

static void
printWords( WordList* strings )
{
    std::vector<Letter*>::iterator iter = strings->begin();
    while ( iter != strings->end() ) {
        char buf[T2ABUFLEN(MAX_WORD_LEN)];
        tileToAscii( buf, sizeof(buf), *iter );
        fprintf( stderr, "%s\n", buf );
        ++iter;
    }
}

/*****************************************************************************
 * Little node-field setters and getters to hide what bits represent
 * what.

 * high bit (31) is ACCEPTING bit
 * next bit (30) is LAST_SIBLING bit
 * next 6 bits (29-24) are tile bit (allowing alphabets of 64 letters)
 * final 24 bits (23-0) are the index of the first child (fco)
******************************************************************************/

static void
TrieNodeSetIsTerminal( Node* nodeR, bool isTerminal )
{
    if ( isTerminal ) {
        *nodeR |= (1 << 31);
    } else {
        *nodeR &= ~(1 << 31);
    }
}

static bool
TrieNodeGetIsTerminal( Node node )
{
    return (node & (1 << 31)) != 0;
}

static void
TrieNodeSetIsLastSibling( Node* nodeR, bool isLastSibling )
{
    if ( isLastSibling ) {
        *nodeR |= (1 << 30);
    } else {
        *nodeR &= ~(1 << 30);
    }
}

static bool
TrieNodeGetIsLastSibling( Node node )
{
    return (node & (1 << 30)) != 0;
}

static void
TrieNodeSetLetter( Node* nodeR, Letter letter )
{
    if ( letter >= 64 ) {
        ERROR_EXIT( "letter %d too big", letter );
    }

    int mask = ~(0x3F << 24);
    *nodeR &= mask;                   // clear all the bits
    *nodeR |= (letter << 24);          // set new ones
}

static Letter
TrieNodeGetLetter( Node node )
{
    node >>= 24;
    node &= 0x3F;              // is 3f ok for 3-byte case???
    return node;
}

static void
TrieNodeSetFirstChildOffset( Node* nodeR, int fco )
{
    if ( (fco & 0xFF000000) != 0 ) {
        ERROR_EXIT( "%x larger than 24 bits", fco );
    }

    int mask = ~0x00FFFFFF;
    *nodeR &= mask;                   // clear all the bits
    *nodeR |= fco;                    // set new ones
}

static int
TrieNodeGetFirstChildOffset( Node node )
{
    node &= 0x00FFFFFF;                  // 24 bits
    return node;
}

static Node
MakeTrieNode( Letter letter, bool isTerminal, int firstChildOffset, 
              bool isLastSibling )
{
    Node result = 0;

    TrieNodeSetIsTerminal( &result, isTerminal );
    TrieNodeSetIsLastSibling( &result, isLastSibling );
    TrieNodeSetLetter( &result, letter );
    TrieNodeSetFirstChildOffset( &result, firstChildOffset );

    return result;
} // MakeTrieNode

// Caller may need to know the offset of the first top-level node.
// Write it here.
static void
writeOutStartNode( const char* startNodeOut, int firstRootChildOffset )
{
    FILE* nodeout;
    nodeout = fopen( startNodeOut, "w" );
    unsigned long be = htonl( firstRootChildOffset );
    (void)fwrite( &be, sizeof(be), 1, nodeout );
    fclose( nodeout );
} // writeOutStartNode

// build the hash for translating.  I'm using a hash assuming it'll be
// fast.  Key is the letter; value is the 0..31 value to be output.
static void
makeTableHash( void )
{
    int ii;
    FILE* TABLEFILE = fopen( gTableFile, "r"  );
    if ( NULL == TABLEFILE ) {
        ERROR_EXIT( "unable to open %s\n", gTableFile );
    }
    
    // Fill the 0th space since references are one-based
    gRevMap.push_back(0);

    for ( ii = 0; ; ++ii ) {
        int ch = getc(TABLEFILE);
        if ( ch == EOF ) {
            break;
        }

        if ( gUseUnicode ) {   // skip the first byte each time: tmp HACK!!!
            ch = getc(TABLEFILE);
        }
        if ( ch == EOF ) {
            break;
        }

        gRevMap.push_back(ch);

        if ( ch == 0 ) {	// blank
            gBlankIndex = ii;
            // we want to increment i when blank seen since it is a
            // tile value
            continue;
        }
        // die "$0: $gTableFile too large\n" 
        assert( ii < 64 );
        // die "$0: only blank (0) can be 64th char\n" ;
        assert( ii < 64 || ch == 0 );

        // Add 1 to i so no tile-strings contain 0 and we can treat as
        // null-terminated.  The 1 is subtracted again in
        // outputNode().
        gTableHash[ch] = ii + 1;
    }

    fclose( TABLEFILE );
} // makeTableHash

// emitNodes. "input" is $gNodes.  From it we write up to
// $nBytesPerOutfile to files named $outFileBase0..n, mapping the
// letter field down to 5 bits with a hash built from $tableFile.  If
// at any point we encounter a letter not in the hash we fail with an
// error.

static void
emitNodes( unsigned int nBytesPerOutfile, const char* outFileBase )
{
    // now do the emit.

    // is 17 bits enough?
    fprintf( stderr, "There are %d (0x%x) nodes in this DAWG.\n",
             gNodes.size(), gNodes.size() );
    int nTiles = gTableHash.size(); // blank is not included in this count!
    if ( gNodes.size() > 0x1FFFF || gForceFour || nTiles > 32 ) {
        gNBytesPerNode = 4;
    } else if ( nTiles < 32 ) {
        gNBytesPerNode = 3;
    } else {
        if ( gBlankIndex == 32 ) { // blank
            gNBytesPerNode = 3;
        } else {
            ERROR_EXIT( "move blank to last position in info.txt "
                        "for smaller DAWG." );
        }
    }

    unsigned int nextIndex = 0;
    int nextFileNum;

    for ( nextFileNum = 0; ; ++nextFileNum ) {

        if ( nextIndex >= gNodes.size() ) {
            break;	// we're done
        }

        if ( nextFileNum > 99 ) {
            ERROR_EXIT( "Too many outfiles; infinite loop?" );
        }

        char outName[256];
        snprintf( outName, sizeof(outName), "%s_%03d.bin", 
                  outFileBase, nextFileNum);
        FILE* OUTFILE = fopen( outName, "w" );
        assert( OUTFILE );
        unsigned int curSize = 0;

        while ( nextIndex < gNodes.size() ) {
            // scan to find the next terminal
            unsigned int ii;
            for ( ii = nextIndex; !TrieNodeGetIsLastSibling(gNodes[ii]); ++ii ) {

                // do nothing but a sanity check
                if ( ii >= gNodes.size() ) {
                    ERROR_EXIT( "bad trie format: last node not last sibling" );
                }

            }
            ++ii;	// move beyond the terminal
            int nextSize = (ii - nextIndex) * gNBytesPerNode;
            if (curSize + nextSize > nBytesPerOutfile ) {
                break;
            } else {
                // emit the subarray
                while ( nextIndex < ii ) {
                    outputNode( gNodes[nextIndex], gNBytesPerNode, OUTFILE );
                    ++nextIndex;
                }
                curSize += nextSize;
            }
        }

        fclose( OUTFILE );
    }

} // emitNodes

// print out the entire dictionary, as text, to STDERR.
static void
printOneLevel( int index, char* str, int curlen )
{
    int inlen = curlen;
    for ( ; ; ) {
        Node node = gNodes[index++];

        assert( TrieNodeGetLetter(node) < gRevMap.size() );
        char lindx = gRevMap[TrieNodeGetLetter(node)];

        if ( (int)lindx >= 0x20 ) {
            str[curlen++] = lindx;
        } else {
#ifdef DEBUG
            if ( gDebug ) {
                fprintf( stderr, "sub space\n" );
            }
#endif
            str[curlen++] = '\\';
            str[curlen++] = '0' + lindx;
        }
        str[curlen] = '\0';

        if ( TrieNodeGetIsTerminal(node) ) {
            fprintf( stderr, "%s\n", str );
        } 

        int fco = TrieNodeGetFirstChildOffset( node );
        if ( fco != 0 ) {
            printOneLevel( fco, str, curlen );
        }

        if ( TrieNodeGetIsLastSibling(node) ) {
            break;
        }
        curlen = inlen;
    }
    str[inlen] = '\0';
}

static void
outputNode( Node node, int nBytes, FILE* outfile )
{
    unsigned int fco = TrieNodeGetFirstChildOffset(node);
    unsigned int fourthByte = 0;

    if ( nBytes == 4 ) {
        fourthByte = fco >> 16;
        if ( fourthByte > 0xFF ) {
            ERROR_EXIT( "fco too big" );
        }
        fco &= 0xFFFF;
    }

    // Formats are different depending on whether it's to have 3- or
    // 4-byte nodes.

    // Here's what the three-byte node looks like.  16 bits plus one
    // burried in the last byte for the next node address, five for a
    // character/tile and one each for accepting and last-edge.

    // 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
    // |-------- 16 bits of next node address -------|  |  |  |  |-tile indx-|
    //                                                  |  |  |    
    //                                accepting bit  ---+  |  |
    //                                 last edge bit ------+  |
    //         ---- last bit (17th on next node addr)---------+

    // The four-byte format adds a byte at the right end for
    // addressing, but removes the extra bit (5) in order to let the
    // chars field be six bits.  Bits 7 and 6 remain the same.

    // write the fco (less that one bit).  We want two bytes worth
    // in three-byte mode, and three in four-byte mode

    // first two bytes are low-word of fco, regardless of format
    for ( int i = 1; i >= 0; --i ) {
        unsigned char tmp = (fco >> (i * 8)) & 0xFF;
        fwrite( &tmp, 1, 1, outfile );
    }
    fco >>= 16;                // it should now be 1 or 0
    if ( fco > 1 ) {
        ERROR_EXIT( "fco not 1 or 0" );
    }

    // - 1 below reverses + 1 in makeTableHash()
    unsigned char chIn5 = TrieNodeGetLetter(node) - 1;
    unsigned char bits = chIn5;
    if ( bits > 0x1F && nBytes == 3 ) {
        ERROR_EXIT( "char %d too big", bits );
    }

    if ( TrieNodeGetIsLastSibling(node) ) {
        bits |= 0x40;
    }
    if ( TrieNodeGetIsTerminal(node) ) {
        bits |= 0x80;
    }

    // We set the 17th next-node bit only in 3-byte case (where char is
    // 5 bits)
    if ( nBytes == 3 && fco != 0 ) {
        bits |= 0x20;
    }
    fwrite( &bits, 1, 1, outfile );

    // the final byte, if in use
    if ( nBytes == 4 ) {
        unsigned char tmp = (unsigned char)fourthByte;
        fwrite( &tmp, 1, 1, outfile );
    }
} // outputNode

static void
usage( const char* name )
{
    fprintf( stderr, "usage: %s \n"
             "\t[-v]                # print version and exit\n"
             "\t[-poolsize]         # print hardcoded size of pool and exit\n"
             "\t[-b    bytesPerFile]# for Palm only (default = 0xFFFFFFFF)\n"
             "\t[-min   <0<=num<=15># min length word to keep\n"
             "\t[-max   <0<=num<=15># max length word to keep\n"
             "\t-m      mapFile\n"
             "\t-mn     mapFile     # 16 bits per entry\n"
             "\t-ob     outFileBase\n"
             "\t-sn                 # start node out file\n"
             "\t[-if    input_file] # default = stdin\n"
             "\t[-term  ch]         # word terminator; default = '\\0'\n"
             "\t[-nosort]           # input already sorted in accord with -m\n"
             "\t                    #     default=sort'\n"
             "\t[-dump]             # write dictionary as text to STDERR \n"
             "\t                    #     for testing\n"
#ifdef DEBUG
             "\t[-debug]            # turn on verbose output\n"
#endif
             "\t[-force4]           # always use 4 bytes per node\n"
             "\t[-lang  lang]       # e.g. en_US\n"
             "\t[-fsize nBytes]     # max buffer [default %d]\n"
             "\t[-r]                # drop words with letters not in mapfile\n"
             "\t[-k]                # (default) exit on any letter not in mapfile \n",
             name, MAX_POOL_SIZE
             );
} // usage

static void
error_exit( int line, const char* fmt, ... )
{
    fprintf( stderr, "Error on line %d: ", line );
    va_list ap;
    va_start( ap, fmt );
    vfprintf( stderr, fmt, ap );
    va_end( ap );
    fprintf( stderr, "\n" );
    exit( 1 );
}

static char*
parseARGV( int argc, char** argv, const char** inFileName )
{
    *inFileName = NULL;
    int index = 1;
    const char* enc = NULL;
    while ( index < argc ) {

        char* arg = argv[index++];

        if ( 0 == strcmp( arg, "-v" ) ) {
            fprintf( stderr, "%s (Subversion revision %s)\n", argv[0], 
                     VERSION_STR );
            exit( 0 );
        } else if ( 0 == strcmp( arg, "-poolsize" ) ) {
            printf( "%d", MAX_POOL_SIZE );
            exit( 0 );
        } else if ( 0 == strcmp( arg, "-b" ) ) {
            gNBytesPerOutfile = atol( argv[index++] );
        } else if ( 0 == strcmp( arg, "-mn" ) ) {
            gTableFile = argv[index++];
            gUseUnicode = true;
        } else if ( 0 == strcmp( arg, "-min" ) ) {
            gLimLow = atoi(argv[index++]);
        } else if ( 0 == strcmp( arg, "-max" ) ) {
            gLimHigh = atoi(argv[index++]);
        } else if ( 0 == strcmp( arg, "-m" ) ) {
            gTableFile = argv[index++];
        } else if ( 0 == strcmp( arg, "-ob" ) ) {
            gOutFileBase = argv[index++];
        } else if ( 0 == strcmp( arg, "-enc" ) ) {
            enc = argv[index++];
        } else if ( 0 == strcmp( arg, "-sn" ) ) {
            gStartNodeOut = argv[index++];
        } else if ( 0 == strcmp( arg, "-if" ) ) {
            *inFileName = argv[index++];
        } else if ( 0 == strcmp( arg, "-r" ) ) {
            gKillIfMissing = false;
        } else if ( 0 == strcmp( arg, "-k" ) ) {
            gKillIfMissing = true;
        } else if ( 0 == strcmp( arg, "-term" ) ) {
            gTermChar = (char)atoi(argv[index++]);
        } else if ( 0 == strcmp( arg, "-dump" ) ) {
            gDumpText = true;
        } else if ( 0 == strcmp( arg, "-nosort" ) ) {
            gReadWordProc = readFromFile;
        } else if ( 0 == strcmp( arg, "-wc" ) ) {
            gCountFile = argv[index++];
        } else if ( 0 == strcmp( arg, "-ns" ) ) {
            gBytesPerNodeFile = argv[index++];
        } else if ( 0 == strcmp( arg, "-force4" ) ) {
            gForceFour = true;
        } else if ( 0 == strcmp( arg, "-fsize" ) ) {
            gFileSize = atoi(argv[index++]);
        } else if ( 0 == strcmp( arg, "-lang" ) ) {
            gLang = argv[index++];
#ifdef DEBUG
        } else if ( 0 == strcmp( arg, "-debug" ) ) {
            gDebug = true;
#endif
        } else {
            ERROR_EXIT( "%s: unexpected arg %s", __func__, arg );
        }
    }

    if ( gLimHigh > MAX_WORD_LEN || gLimLow > MAX_WORD_LEN ) {
        usage( argv[0] );
        exit(1);
    }

    if ( !!enc ) {
        if ( !strcasecmp( enc, "UTF-8" ) ) {
            gIsMultibyte = true;
        } else if ( !strcasecmp( enc, "iso-8859-1" ) ) {
            gIsMultibyte = false;
        } else if ( !strcasecmp( enc, "iso-latin-1" ) ) {
            gIsMultibyte = false;
        } else if ( !strcasecmp( enc, "ISO-8859-2" ) ) {
            gIsMultibyte = false;
        } else {
            ERROR_EXIT( "%s: unknown encoding %s", __func__, enc );
        }
        gEncoding = enc;
    }

#ifdef DEBUG
    if ( gDebug ) {
        fprintf( stderr, "gNBytesPerOutfile=%d\n", gNBytesPerOutfile );
        fprintf( stderr, "gTableFile=%s\n", gTableFile );
        fprintf( stderr, "gOutFileBase=%s\n", gOutFileBase );
        fprintf( stderr, "gStartNodeOut=%s\n", gStartNodeOut );
        fprintf( stderr, "gTermChar=%c(%d)\n", gTermChar, (int)gTermChar );
        fprintf( stderr, "gFileSize=%d\n", gFileSize );
        fprintf( stderr, "gLimLow=%d\n", gLimLow );
        fprintf( stderr, "gLimHigh=%d\n", gLimHigh );
    }
#endif
    return gTableFile;
} // parseARGV
