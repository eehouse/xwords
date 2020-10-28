/* -*- compile-command: "cd ../linux && make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 1997-2020 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

#ifdef XWFEATURE_WALKDICT

#ifdef USE_STDIO
# include <stdio.h>
# include <stdlib.h>
#endif

#include <inttypes.h>

#include "comtypes.h"
#include "dictnryp.h"
#include "strutils.h"
#include "dictnry.h"
#include "dictiter.h"
#include "dbgutil.h"

/* Define DI_DEBUG in Makefile. It makes iteration really slow on Android */
#ifdef DI_DEBUG
# define DI_ASSERT(...) XP_ASSERT(__VA_ARGS__)
#else
# define DI_ASSERT(...)
#endif

#ifdef CPLUS
extern "C" {
#endif

#define MAX_ELEMS MAX_COLS_DICT

typedef enum {
    FLAG_NEG = 1,
    FLAG_SINGLE = 2,
} Flags;

// #define WITH_START

#define MULTI_SET
#ifdef MULTI_SET
typedef struct _TileSet {
    uint8_t cnts[64];
} TileSet;
#else
    typedef uint64_t TileSet;
#endif

typedef enum {_NONE,
#ifdef WITH_START
              START,
#endif
              CHILD, PARENT} PEType;
typedef struct _PatElem {
    PEType typ;
    /* {2,15} sets min=2, max=15; '*' sets min=0, max=10000, '+' sets min=1, etc */
    int minMatched;
    int maxMatched;
    union {
        struct {
            TileSet tiles;
            XP_U16 flags;                /* Flags bitvector */
        } child;
        struct {
            int firstChild;
            int lastChild;
        } parent;
    } u;
} PatElem;

typedef XP_U32 ElemSet;

#define MAX_PATS 4

typedef struct _PatMatch {
    ElemSet elemSet[MAX_PATS];     /* until we have something */
} PatMatch;

typedef struct _Pat {
    const XP_UCHAR* patString;
    /* Internal representation ("compiled") regex */
    PatElem* patElems;
    XP_U16 nPatElems;
} Pat;

typedef struct _Indexer {
    void (*proc)(void* closure);
    void* closure;
} Indexer;

struct DictIter {
    XP_U16 nEdges;
#ifdef DEBUG
    /* Current string: useful when stepping in gdb */
    XP_UCHAR curWord[32];
#endif
    struct {
#ifdef DEBUG
        XP_U16 faceLen;
#endif
        array_edge* edge;
        PatMatch match;
    } stack[MAX_COLS_DICT];
    XP_U16 min;
    XP_U16 max;
#ifdef DEBUG
    XP_U32 guard;
#endif
    const DictionaryCtxt* dict;
    XP_U32 nWords;

    DictPosition position;
#ifdef MULTI_SET
    Tile blankVal;
#else
    TileSet blankMask;
#endif

    XP_U16 nPats;
    Pat pats[MAX_PATS];

    Indexer* indexer;
};

typedef enum { PatErrNone,
               PatErrMissingClose,
               PatErrMultipleSpellings,
               PatErrBadCountTerm,
               PatErrNoDigit,
               PatErrTooComplex,
               PatErrBogusTiles,
               PatErrDupInSet,
} PatErr;

#ifdef DEBUG
static const XP_UCHAR*
patErrToStr( PatErr err )
{
    const XP_UCHAR* result = NULL;
# define CASESTR(s) case s: result = #s; break
    switch ( err ) {
        CASESTR(PatErrNone);
        CASESTR(PatErrMissingClose);
        CASESTR(PatErrMultipleSpellings);
        CASESTR(PatErrBadCountTerm);
        CASESTR(PatErrNoDigit);
        CASESTR(PatErrTooComplex);
        CASESTR(PatErrBogusTiles);
        CASESTR(PatErrDupInSet);
    }
# undef CASESTR
    return result;
}
#endif

typedef struct _ParseState {
    const DictionaryCtxt* dict;
    const XP_UCHAR* pat;
    int patIndex;
    int elemIndex;
#ifdef MULTI_SET
    Tile blankVal;
#else
    TileSet blankMask;
#endif
    PatElem elems[MAX_ELEMS];
} ParseState;

static PatErr compileParent( ParseState* ps );
static void iterToString( const DictIter* iter, XP_UCHAR* buf, XP_U16 buflen,
                          const XP_UCHAR* delim );
static XP_Bool isFirstEdge( const DictionaryCtxt* dict, array_edge* edge );

/* Read 1 or more digits into a count. It's an error if there isn't at least
   one, but once we have that we just stop on NAN */
static PatErr
getNumber( ParseState* ps, int* out )
{
    PatErr err = PatErrNoDigit;
    int result = 0;

    for ( ; ; ) {
        XP_UCHAR ch = ps->pat[ps->patIndex];
        if ( ch >= '0' && ch <= '9' ) {
            result = result * 10 + (ch - '0');
            err = PatErrNone;
            ++ps->patIndex;
        } else {
            break;
        }
    }
    *out = result;
    return err;
}
    
/* Read in a ?, *, or {n[,n]} */
static PatErr
parseCounts( ParseState* ps, int elemIndex )
{
    PatErr err = PatErrNone;
    PatElem* elem = &ps->elems[elemIndex];
    switch ( ps->pat[ps->patIndex] ) {
    case '*':
        ++ps->patIndex;
        elem->minMatched = 0;
        elem->maxMatched = 1000;
        break;
    case '+':
        ++ps->patIndex;
        elem->minMatched = 1;
        elem->maxMatched = 1000;
        break;
    case '{':
        ++ps->patIndex;
        err = getNumber( ps, &elem->minMatched );
        if ( PatErrNone == err ) {
            switch ( ps->pat[ps->patIndex++] ) {
            case '}':
                elem->maxMatched = elem->minMatched;
                break;
            case ',':
                err = getNumber( ps, &elem->maxMatched );
                if ( PatErrNone == err ) {
                    if ( elem->maxMatched < elem->minMatched ) {
                        err = PatErrBadCountTerm;
                    } else if ( '}' != ps->pat[ps->patIndex++] ) {
                        err = PatErrBadCountTerm;
                    }
                }
                break;
            default:
                err = PatErrBadCountTerm;
            }
        }
        break;
    default:                    /* No count found */
        elem->minMatched = elem->maxMatched = 1;
        break;
    }
    return err;
}
    
typedef struct _FoundData {
    PatErr err;
    int nCalls;
    PatElem* elem;
} FoundData;
    
static XP_Bool
onFoundTiles( void* closure, const Tile* tiles, int len )
{
    XP_ASSERT( len == 1 );
    FoundData* data = (FoundData*)closure;
    if ( 1 == len ) {
        XP_ASSERT( 0 == data->nCalls );
        ++data->nCalls;
        for ( int ii = 0; ii < len; ++ii ) {
            Tile tile = tiles[ii];
#ifdef MULTI_SET
            ++data->elem->u.child.tiles.cnts[tile];
#else
            TileSet mask = 1 << tile;
            if ( 0 == (data->elem->u.child.tiles & mask ) ) {
                data->elem->u.child.tiles |= mask;
            } else {
                data->err = PatErrDupInSet;
                break;
            }
#endif
        }
    }
    return 1 == len && PatErrNone == data->err;
}

static PatErr
addElem( ParseState* ps, PatElem* elem )
{
    PatErr err = PatErrNone;
    if ( ps->elemIndex < VSIZE(ps->elems) ) {
        ps->elems[ps->elemIndex++] = *elem;
    } else {
        err = PatErrTooComplex;
    }
    return err;
}

static PatErr
parseTile( ParseState* ps )
{
    PatErr err = PatErrNone;

    if ( '_' == ps->pat[ps->patIndex] ) {
        ++ps->patIndex;
        PatElem* elem = &ps->elems[ps->elemIndex];
#ifdef MULTI_SET
        ++elem->u.child.tiles.cnts[ps->blankVal];
#else
        elem->u.child.tiles |= ps->blankMask;
#endif
    } else {
        err = PatErrBogusTiles; /* in case we fail */
        XP_U16 maxLen = XP_STRLEN( &ps->pat[ps->patIndex] );
        for ( int nChars = 1; nChars <= maxLen; ++nChars ) {
            FoundData data = { .err = PatErrNone,
                               .elem = &ps->elems[ps->elemIndex],
                               .nCalls = 0,
            };
            dict_tilesForString( ps->dict, &ps->pat[ps->patIndex], nChars, onFoundTiles, &data );
            if ( 1 == data.nCalls ) { /* found something? We can proceed */
                ps->patIndex += nChars;
                err = PatErrNone;
                break;
            }
        }
    }

    return err;                 /* nothing can go wrong? */
}

static PatErr
parseSet( ParseState* ps )
{
    PatErr err;
    /* Look for one of the two special chars */
    XP_UCHAR ch = ps->pat[ps->patIndex];
    switch ( ch ) {
    case '^':
        ps->elems[ps->elemIndex].u.child.flags |= FLAG_NEG;
        ++ps->patIndex;
        break;
    case '+':
        ps->elems[ps->elemIndex].u.child.flags |= FLAG_SINGLE;
        ++ps->patIndex;
        break;
    default:
        break;
    }

    for ( ; ; ) {
        err = parseTile( ps );
        if ( PatErrNone != err ) {
            break;
        }
        if ( ']' == ps->pat[ps->patIndex] ) {
            ++ps->patIndex;
            break;
        }
    }

    return err;
}

static PatErr
parseOne( ParseState* ps )
{
    PatErr err;
    XP_UCHAR ch = ps->pat[ps->patIndex];
    switch ( ch ) {
    case '(': {                  /* starts a recursive pattern */
        ++ps->patIndex;
        int myElemIndex = ps->elemIndex;
        ps->elems[myElemIndex].typ = PARENT;
        ps->elems[myElemIndex].u.parent.firstChild = ++ps->elemIndex;
        err = compileParent( ps );
        if ( PatErrNone == err ) {
            if ( ')' == ps->pat[ps->patIndex++] ) {
                ps->elems[myElemIndex].u.parent.lastChild = ps->elemIndex;
                err = parseCounts( ps, myElemIndex );
            } else {
                err = PatErrMissingClose;
            }
        }
    }
        break;
    case '[':                   /* starts a set */
        ++ps->patIndex;
        ps->elems[ps->elemIndex].typ = CHILD;
        err = parseSet( ps );
        break;
    default:
        ps->elems[ps->elemIndex].typ = CHILD;
        err = parseTile( ps );
        break;
    }

    return err;
}

static PatErr
compileParent( ParseState* ps )
{
    PatErr err = PatErrNone;

    while ( PatErrNone == err ) {
        err = parseOne( ps );
        if ( PatErrNone == err ) {
            err = parseCounts( ps, ps->elemIndex );
        }
        if ( PatErrNone == err ) {
            if ( ++ps->elemIndex >= MAX_ELEMS ) {
                err = PatErrTooComplex;
            }
            XP_UCHAR ch = ps->pat[ps->patIndex];
            if ( ')' == ch || '\0' == ch ) {
                break;
            }
        }
    }

    return err;
}

static PatErr
initPS( ParseState* ps, const DictionaryCtxt* dict )
{
    PatErr result = PatErrNone;
    XP_MEMSET( ps, 0, sizeof(*ps) );
    XP_ASSERT( !!dict );
    ps->dict = dict;
    // ps->pat = pat;
#ifdef MULTI_SET
    ps->blankVal = dict_getBlankTile( dict );
#else
    ps->blankMask = ((TileSet)1) << dict_getBlankTile( dict );
#endif
    ps->elemIndex = 0;
    ps->patIndex = 0;

#ifdef WITH_START
    PatElem start = { .typ = START, };
    result = addElem( ps, &start );
#endif
    return result;
}

static XP_Bool
compilePat( ParseState* ps, const XP_UCHAR* strPat )
{
    ps->pat = strPat;
    ps->patIndex = 0;

    PatErr err = compileParent( ps );
    
    XP_Bool success = err == PatErrNone && 0 < ps->elemIndex;
    if ( !success ) {
        XP_LOGFF( "=> %s", patErrToStr(err) );
    }
    return success;
}

#ifdef DEBUG

static int
formatFlags( XP_UCHAR* buf, int flags )
{
    int indx = 0;
    if ( 0 != (flags & FLAG_NEG) ) {
        buf[indx++] = '^';
    }
    if ( 0 != (flags & FLAG_SINGLE) ) {
        buf[indx++] = '+';
    }
    buf[indx] = '\0';
    return indx;
}

static void
formatTiles( XP_UCHAR* buf, const TileSet* tiles, const DictionaryCtxt* dict )
{
    int indx = 0;
    if ( !!tiles ) {
#ifdef MULTI_SET
        for ( Tile ii = 0; ii < VSIZE(tiles->cnts); ++ii ) {
            for ( int jj = tiles->cnts[ii]; jj > 0; --jj ) {
#else
        for ( Tile ii = 0; ii < 8 * sizeof(tiles); ++ii ) {
            if ( 0 != (*tiles & (((uint64_t)1) << ii)) ) {
#endif
                indx += dict_tilesToString( dict, &ii, 1, &buf[indx], 4, NULL );
            }
        }
    }
    buf[indx] = '\0';
}

typedef struct _PrintState {
    const DictIter* iter;
    XP_UCHAR* buf;
    const int bufLen;
    int curPos;
    /* int curElem; */
} PrintState;

/* static void printChild( PrintState* prs ); */

/* static void */
/* printChildren( PrintState* prs, int lastElem ) */
/* { */
/*     while ( prs->curElem < lastElem ) { */
/*         printChild( prs ); */
/*         ++prs->curElem; */
/*     } */
/* } */

static void
printCount( PrintState* prs, const PatElem* elem )
{
    int minMatched = elem->minMatched;
    int maxMatched = elem->maxMatched;
    if ( minMatched == 1 && maxMatched == 1 ) {
        /* do nothing; likely nothing was provided */
    } else if ( minMatched == maxMatched ) {
        prs->curPos += XP_SNPRINTF( &prs->buf[prs->curPos], prs->bufLen - prs->curPos,
                                    "{%d}", minMatched );
    } else {
        prs->curPos += XP_SNPRINTF( &prs->buf[prs->curPos], prs->bufLen - prs->curPos,
                                    "{%d,%d}", minMatched, maxMatched );
    }
}

/* static void */
/* printChild( PrintState* prs ) */
/* { */
/*     const PatElem* elem = &prs->iter->patElems[prs->curElem]; */
/*     switch ( elem->typ ) { */
/*     case CHILD: { */
/*         XP_UCHAR flags[8] = {0}; */
/*         formatFlags( flags, elem->u.child.flags ); */
/*         XP_UCHAR tiles[128] = {0}; */
/*         formatTiles( tiles, elem->u.child.tiles, prs->iter->dict ); */
/*         prs->strEnd += XP_SNPRINTF( &prs->buf[prs->strEnd], prs->bufLen - prs->strEnd, */
/*                                     "[%s%s]", flags, tiles ); */
/*         printCount( prs, elem ); */
/*     } */
/*         break; */
/*     case PARENT: */
/*         prs->buf[prs->strEnd++] = '('; */
/*         ++prs->curElem; */
/*         XP_ASSERT( prs->curElem == elem->u.parent.firstChild ); */
/*         printChildren( prs, elem->u.parent.lastChild ); */
/*         prs->buf[prs->strEnd++] = ')'; */
/*         printCount( prs, elem ); */
/*         break; */
/* #ifdef WITH_START */
/*     case START: */
/*         break; */
/* #endif */
/*     default: */
/*         XP_ASSERT(0); */
/*         break; */
/*     } */
/* } */

# if 0
static void
printPat( const DictIter* iter, XP_UCHAR* buf, XP_U16 bufLen )
{
    PrintState prs = { .iter = iter, .buf = buf, .bufLen = bufLen, };
    printChildren( &prs, iter->nPatElems );
    prs.buf[prs.strEnd] = '\0';
}
# endif
#else
# define printPat( iter, strPat )
#endif

static void initIter( DictIter* iter, const DictionaryCtxt* dict,
                      const DIMinMax* minmax, const Pat* pats, XP_U16 nPats,
                      Indexer* indexer );
static XP_Bool prevWord( DictIter* iter, XP_Bool log );

#ifdef XWFEATURE_WALKDICT_FILTER
#define LENOK( iter, nEdges )  XP_TRUE
// (iter)->min <= (nEdges) && (nEdges) <= (iter)->max

#define HAS_MATCH( ITER, EDGE, MATCHP, LOG )                            \
    ((0 == (ITER)->nPats) || patHasMatch( (ITER), (EDGE), (MATCHP), (LOG) ))
#define MATCH_FINISHED( ITER, LOG )                             \
    ((0 == (ITER)->nPats) || patMatchFinished( (ITER), (LOG) ))

static XP_Bool
_isAccepting( DictIter* iter )
{
    return ISACCEPTING( iter->dict, iter->stack[iter->nEdges-1].edge );
}
# define ACCEPT_ITER( iter, log )                               \
    (_isAccepting( iter ) && MATCH_FINISHED( iter, log ))
# define ACCEPT_NODE( iter, node, log )                                     \
    (ISACCEPTING( iter->dict, node ) && MATCH_FINISHED( iter, log ))
# define FILTER_TEST(iter,nEdges) XP_TRUE
    // ((nEdges) <= (iter)->max)
#else
/* # define ACCEPT_ITER(iter, nEdges)                                        \ */
/*     ISACCEPTING( (iter)->dict, (iter)->edges[(nEdges)-1] ) */
/* # define ACCEPT_NODE( iter, node, nEdges ) ISACCEPTING( iter->dict, node ) */
/* # define FILTER_TEST(iter, nEdges) XP_TRUE */
#endif

/* Patterns
 *
 * Iterator needs to know where it is in the dict, and where it is in the
 * pattern. Sometimes a letter's consumed from the dict without anything being
 * comsumed in the pattern (_* consumes anything)
 *
 * '[' [^ ']'
 * 
 */

struct _IPattern {
    const XP_UCHAR chars[64];   /* let's be safe */
} DIPattern;

#ifdef DEBUG
/* static void */
/* logCurWord( const DictIter* iter, const XP_UCHAR* note ) */
/* { */
/*     XP_UCHAR buf[32]; */
/*     XP_U16 bufLen = VSIZE(buf); */
/*     formatCurWord( iter, buf, bufLen ); */
/*     XP_LOGFF( "note: %s; word: %s", note, buf ); */
/* } */

#endif

typedef struct _FaceTile {
    Tile tile;
    XP_UCHAR face[8];
} FaceTile;

typedef struct _PrevOccurs {
    XP_Bool allowed;
    XP_Bool required;
    XP_Bool matched;
} PrevOccurs;

typedef struct _Params {
    const DictIter* iter;
    int patIndx;
    int patElemIndx;
    const PatElem* elem;
    const Pat* pat;
} Params;

static void
setParams( Params* params, const DictIter* iter, const int patIndx, const int patElemIndx )
{
    params->iter = iter;
    params->patIndx = patIndx;
    params->patElemIndx = patElemIndx;
    params->pat = &iter->pats[patIndx];
    params->elem = &params->pat->patElems[patElemIndx];
}

/* How many matches in a row include this PatElem */
static int
countPrevOccurs( const Params* params, TileSet* prevs, PrevOccurs* peOut,
                 XP_Bool XP_UNUSED_DBG(log) )
{
    ElemSet mask = 1 << params->patElemIndx;
    int result = 0;
    const DictIter* iter = params->iter;
    for ( int ii = iter->nEdges - 1; ii >= 0; --ii ) {
        ElemSet elemSet = iter->stack[ii].match.elemSet[params->patIndx];
        if ( 0 == (elemSet & mask) ) {
            break;
        }
        ++result;

        if ( !!prevs ) {
            Tile tile = EDGETILE( iter->dict, iter->stack[ii].edge );
#ifdef MULTI_SET
            if ( 0 == prevs->cnts[tile] ) {
                tile = iter->blankVal;
            }
            XP_ASSERT( 0 < prevs->cnts[tile] );
            --prevs->cnts[tile];
#else
            *prevs &= ~(((TileSet)1) << tile);
#endif
        }
    }

    if ( !!peOut ) {
        const PatElem* elem = params->elem;
        peOut->allowed = result < elem->maxMatched;
        peOut->required = result < elem->minMatched;
        peOut->matched = elem->minMatched <= result && result <= elem->maxMatched;
    }

#ifdef DEBUG
    if ( log ) {
        LOG_RETURNF( "%d", result );
    }
#endif
    return result;
}

static void
mkFaceTile( const DictionaryCtxt* dict, array_edge* edge, FaceTile* ft )
{
    Tile tile = EDGETILE( dict, edge );
    ft->tile = tile;
    const XP_UCHAR* face = dict_getTileString( dict, tile );
    XP_SNPRINTF( ft->face, VSIZE(ft->face), "%s", face );
}

typedef struct _MatchInfo {

    /* minMatched == 0, a special case */
    XP_Bool isOptional;

    /* The tile is not blocked by this patElem. It's in the set matched, or
       the elem is optional.*/
    XP_Bool matched;

} MatchInfo;

static void
getMatchInfo( const Params* params, const TileSet* prevs, const FaceTile* ft,
              MatchInfo* mi, XP_Bool log )
{
    const DictIter* iter = params->iter;
    XP_ASSERT( params->patElemIndx < params->pat->nPatElems );
    int usedCount;
    const PatElem* elem = params->elem;
    switch ( elem->typ ) {
#ifdef WITH_START
    case START:
        mi->matches = XP_TRUE;
        // mi->exhausted = XP_TRUE;
        mi->consumed = XP_TRUE;
        break;
#endif
    case CHILD: {
        XP_Bool matches = XP_TRUE;
#ifdef MULTI_SET
        const TileSet* elemTileSet = &elem->u.child.tiles;
#else
        TileSet curMask = {0};
#endif
        if ( !!ft ) {
            Tile tile = ft->tile;
#ifdef MULTI_SET
            XP_ASSERT( iter->blankVal != tile );
            Tile usedTile = tile;
            matches = 0 != elemTileSet->cnts[usedTile];
            if ( matches && !!prevs ) {
                matches = 0 < prevs->cnts[usedTile];
            }
            if ( !matches ) {
                usedTile = iter->blankVal;
                matches = 0 != elemTileSet->cnts[usedTile];
            }
            if ( matches && !!prevs ) {
                matches = 0 < prevs->cnts[usedTile];
            }
#else
            XP_USE(prevs);
            XP_ASSERT( iter->blankMask != 1 << tile );
            curMask = iter->blankMask | ((TileSet)1) << tile;
            matches = 0 != (elem->u.child.tiles & curMask);
#endif
        }
        mi->isOptional = 0 == elem->minMatched;

        /* if it matches, we need to make sure it hasn't matched too many
           times already */
        usedCount = matches ? 1 : 0;          /* this match is 1 */
        const ElemSet elemMask = 1 << params->patElemIndx;
        for ( int ii = iter->nEdges - 1; matches && ii >= 0; --ii ) {
            ElemSet elemSet = iter->stack[ii].match.elemSet[params->patIndx];
            if ( 0 == (elemSet & elemMask) ) {
                break;
            }
            ++usedCount;
            matches = matches && usedCount <= elem->maxMatched;
        }

        mi->matched = matches;
        if ( matches ) {
            XP_ASSERT( !matches || usedCount <= elem->maxMatched );
            XP_ASSERT( usedCount <= elem->maxMatched );
            XP_ASSERT( usedCount <= elem->maxMatched );
         }
    }
        break;
    default:
        XP_ASSERT(0);
        break;
    }
    if ( log ) {
        XP_LOGFF( "(tile: '%s', indx: %d)=> matches: %s, isOptional: %s (usedCount %d)",
                  !!ft ? ft->face : "", params->patElemIndx, boolToStr(mi->matched),
                  boolToStr(mi->isOptional), usedCount );
   }
} /* getMatchInfo */

#ifdef DEBUG
static const XP_UCHAR*
formatSets( XP_UCHAR* buf, int bufLen, const PatMatch* matchP )
{
    int count = 0;
    for ( int ii = 0; ii < MAX_PATS; ++ii ) {
        ElemSet es = matchP->elemSet[ii];
        if ( 0 != es ) {
            ++count;
        }
    }

    int indx = 0;
    for ( int ii = 0; ii < MAX_PATS; ++ii ) {
        ElemSet es = matchP->elemSet[ii];
        if ( 0 != es ) {
            if ( 1 == count ) { /* the common case */
                indx += XP_SNPRINTF( &buf[indx], bufLen-indx, "0x%x", es );
            } else {
                indx += XP_SNPRINTF( &buf[indx], bufLen-indx, "[%d]: 0x%x,", ii, es );
            }
        }
    }
    return buf;
}

static void
formatElem( PrintState* prs, const PatElem* elem )
{
    switch ( elem->typ ) {
    case CHILD: {
        XP_UCHAR flags[8] = {0};
        formatFlags( flags, elem->u.child.flags );
        XP_UCHAR tiles[128] = {0};
        formatTiles( tiles, &elem->u.child.tiles, prs->iter->dict );
        prs->curPos += XP_SNPRINTF( &prs->buf[prs->curPos],
                                    prs->bufLen - prs->curPos,
                                    "[%s%s]", flags, tiles );
        printCount( prs, elem );
    }
        break;
    default:
        XP_ASSERT(0);
    }
/*     case PARENT: */
/*         prs->buf[prs->strEnd++] = '('; */
/*         ++prs->curElem; */
/*         XP_ASSERT( prs->curElem == elem->u.parent.firstChild ); */
/*         printChildren( prs, elem->u.parent.lastChild ); */
/*         prs->buf[prs->strEnd++] = ')'; */
/*         printCount( prs, elem ); */
/*         break; */
/* #ifdef WITH_START */
/*     case START: */
/*         break; */
/* #endif */
/*     default: */
/*         XP_ASSERT(0); */
/*         break; */
/*     } */
}
#endif

/* The current edge contains a set of all matches that COULD HAVE gotten it
   this far. So with a new input tile, we want to build the set that could
   have gotten us here.

   Since each match in that set is a starting point for considering the new
   input, we repeat for each testing whether it can accept the new one. We check:

   1) is that match possibly exhausted after the previous input (max too
   low)? If so, add the next state to test set.

   2) If the set has room for this tile and it matches, add it

   Initial/fake edge has as its set the 
 */
static XP_Bool
patHasMatch( DictIter* iter, array_edge* edge, PatMatch* matchP, XP_Bool log )
{
    XP_Bool success = XP_TRUE;
    FaceTile _tile = {0};
    mkFaceTile( iter->dict, edge, &_tile );
    const FaceTile* ft = &_tile;

    PatMatch resultMatch = {0};
    for ( int patIndx = 0; success && patIndx < iter->nPats; ++patIndx ) {
        ElemSet oldElems;
        if ( 0 == iter->nEdges ) {
            oldElems = 1; // initialSet( iter, patIndx, log );
            // setsToConsume( iter, patIndx, oldElems, ft, XP_FALSE, &newElems, log );
        } else {
            oldElems = iter->stack[iter->nEdges-1].match.elemSet[patIndx];
        }
        ElemSet newElems = 0;
        // XP_Bool consumed = XP_FALSE;
        const Pat* pat = &iter->pats[patIndx];
        for ( int patElemIndx = 0;
              0 != oldElems && patElemIndx < pat->nPatElems;
              ++patElemIndx ) {
            ElemSet mask = ((ElemSet)1) << patElemIndx;
            if ( 0 != (oldElems & mask) ) {
                oldElems &= ~mask;

                Params params;
                setParams( &params, iter, patIndx, patElemIndx );

                /* Look at the elem that got me here. If it's potentially
                   exhausted, add its successor */
                TileSet prevs;
                TileSet* prevsPtr = NULL;
                if ( 0 != (FLAG_SINGLE & params.elem->u.child.flags) ) {
                    prevs = params.elem->u.child.tiles;
                    prevsPtr = &prevs;
                }
                int count = countPrevOccurs( &params, prevsPtr, NULL, log );
                if ( count >= params.elem->minMatched ) {
                    oldElems |= 1 << (1 + patElemIndx);
                }

                MatchInfo mi;
                getMatchInfo( &params, prevsPtr, ft, &mi, log );

                if ( mi.isOptional ) {
                    oldElems |= 1 << (patElemIndx + 1); /* we'll try the next to see if it matches */
                }
                
                if ( mi.matched ) {
                    newElems |= 1 << patElemIndx; /* we used this to get here */
                }
            }
        }

        success = 0 != newElems;
        if ( success ) {
            resultMatch.elemSet[patIndx] = newElems;
        }
    }

    if ( success ) {
        *matchP = resultMatch;
    }
#ifdef DEBUG
    if ( log ) {
        if ( success ) {
            XP_UCHAR buf[128];
            LOG_RETURNF( "(tile[%d]: %s) => %s (new sets: %s)", iter->nEdges, _tile.face,
                         boolToStr(success), formatSets( buf, VSIZE(buf), matchP ) );
        } else {
            LOG_RETURNF( "(tile[%d]: %s) => %s", iter->nEdges, _tile.face, boolToStr(success) );
        }
    }
#endif
    return success;
}

static XP_Bool
patMatchFinished( const DictIter* iter, XP_Bool log )
{
    XP_Bool result = XP_FALSE;
    Params params;
    for ( int patIndx = 0; patIndx < iter->nPats; ++patIndx ) {
        const ElemSet finalSet = iter->stack[iter->nEdges-1].match.elemSet[patIndx];
        const Pat* pat = &iter->pats[patIndx];

        /* We want to know if the last element is in last tile's matched
        set. If the last elements are optional, however, it's ok to skip back
        over them. */

        int foundIndx = -1;
        for ( int indx = pat->nPatElems - 1; ; --indx ) {
            ElemSet mask = 1 << indx;
            if ( 0 != (finalSet & mask) ) {
                foundIndx = indx;
                break;
            }
            /* Not in the set. Can we skip it? */
            if ( 0 != pat->patElems[indx].minMatched ) {
                break;
            }
        }

        result = 0 <= foundIndx;
        if ( result ) {
            PrevOccurs pe;
            setParams( &params, iter, patIndx, foundIndx );
            countPrevOccurs( &params, NULL, &pe, log );
            result = pe.matched;
        }

        if ( !result ) {
            break;              /* we're done */
        }
    }

#ifdef DEBUG
    if ( log ) {
        if ( result ) {
            XP_UCHAR elemBuf[64];
            PrintState prs = { .iter = iter, .buf = elemBuf, .bufLen = VSIZE(elemBuf), };
            formatElem( &prs, params.elem );
            XP_LOGFF( "for word %s: => %s (matched elem %d: %s)", iter->curWord, boolToStr(result),
                      params.patElemIndx, elemBuf );
        } else {
            XP_LOGFF( "for word %s: => %s", iter->curWord, boolToStr(result) );
        }
    }
#endif
    return result;
}

static XP_Bool
prevPeerMatch( DictIter* iter, array_edge** edgeP, PatMatch* matchP, XP_Bool log )
{
    const DictionaryCtxt* dict = iter->dict;
    array_edge* edge = *edgeP;
    XP_Bool found = XP_FALSE;
    for ( ; ; ) {
        PatMatch match = { 0 };
        found = HAS_MATCH( iter, edge, &match, log );
        if ( found ) {
            *edgeP = edge;
            *matchP = match;
            break;
        }
        if ( isFirstEdge( dict, edge ) ) {
            break;
        }
        edge -= dict->nodeSize;
    }
    return found;
}

static XP_Bool
nextPeerMatch( DictIter* iter, array_edge** edgeP, PatMatch* matchP, XP_Bool log )
{
    array_edge* edge = *edgeP;
    XP_Bool found = XP_FALSE;
    const DictionaryCtxt* dict = iter->dict;
    for ( ; ; ) {
        PatMatch match = { 0};
        found = HAS_MATCH( iter, edge, &match, log );
        if ( found ) {
            *edgeP = edge;
            *matchP = match;
            break;
        }
        if ( IS_LAST_EDGE( dict, edge ) ) {
            break;
        }
        edge += dict->nodeSize;
    }
    return found;
}

static XP_U16
pushEdge( DictIter* iter, array_edge* edge, PatMatch* match )
{
    XP_U16 nEdges = iter->nEdges;
    XP_ASSERT( nEdges < iter->max );
    iter->stack[nEdges].edge = edge;
    iter->stack[nEdges].match = *match;
#ifdef DEBUG
    if ( !!edge ) { /* Will fail when called from di_stringMatches() */
        // XP_LOGFF( "before: %s", iter->curWord );
        Tile tile = EDGETILE( iter->dict, edge );
        const XP_UCHAR* face = dict_getTileString( iter->dict, tile );
        iter->stack[nEdges].faceLen = XP_STRLEN( face );
        XP_STRCAT( iter->curWord, face );
        // XP_LOGFF( "after: %s", iter->curWord );
    }
#endif
    return ++iter->nEdges;
}

static array_edge*
popEdge( DictIter* iter )
{
    XP_ASSERT( 0 < iter->nEdges );
#ifdef DEBUG
    // XP_LOGFF( "before: %s", iter->curWord );
    XP_U16 curLen = XP_STRLEN( iter->curWord );
    XP_U16 popLen = iter->stack[iter->nEdges-1].faceLen;
    XP_ASSERT( curLen >= popLen );
    iter->curWord[curLen-popLen] = '\0';
    // XP_LOGFF( "after: %s", iter->curWord );
#endif
    return iter->stack[--iter->nEdges].edge;
}

static XP_Bool
nextWord( DictIter* iter, XP_Bool log )
{
    // LOG_FUNC();
    const DictionaryCtxt* dict = iter->dict;
    XP_Bool success = XP_FALSE;
    while ( 0 < iter->nEdges && ! success ) {
        if ( iter->nEdges < iter->max ) {
            array_edge* next = dict_follow( dict, iter->stack[iter->nEdges-1].edge );
            if ( !!next ) {
                PatMatch match = {0};
                if ( nextPeerMatch( iter, &next, &match, log ) ) {
                    pushEdge( iter, next, &match );
                    success = iter->min <= iter->nEdges && ACCEPT_NODE( iter, next, log );
                    continue;		/* try with longer word */
                }
            }
        }

        while ( iter->nEdges > 0 && IS_LAST_EDGE( dict, iter->stack[iter->nEdges-1].edge ) ) {
            popEdge( iter );
        }

        /* We're now at a point where the top edge is not a candidate and we
           need to look at its next siblings. (If we don't have any edges,
           we're done, and the top-level while will exit) */
        while ( 0 < iter->nEdges ) {
            /* remove so isn't part of the match of its peers! */
            array_edge* edge = popEdge( iter );
            if ( !IS_LAST_EDGE( dict, edge ) ) {
                edge += dict->nodeSize;
                PatMatch match = {0};
                if ( nextPeerMatch( iter, &edge, &match, log ) ) {
                    pushEdge( iter, edge, &match ); /* let the top of the loop examine this one */
                    success = iter->min <= iter->nEdges && ACCEPT_NODE( iter, edge, log );
                    break;
                }
            }
        }
    }

    if ( success && !!iter->indexer ) {
        (*iter->indexer->proc)(iter->indexer->closure);
    }

#ifdef DEBUG
    if ( log ) {
        if ( success ) {
            XP_LOGFF( "word found: %s", iter->curWord );
        } else {
            XP_LOGFF( "NOTHING FOUND" );
        }
    }
#endif
    // LOG_RETURNF( "%s", boolToStr(success) );
    XP_ASSERT( (iter->min <= iter->nEdges && iter->nEdges <= iter->max) || !success );
    return success;
}

static XP_Bool
isFirstEdge( const DictionaryCtxt* dict, array_edge* edge )
{
    XP_Bool result = edge == dict->base; /* can't back up from first node */
    if ( !result ) {
        result = IS_LAST_EDGE( dict, edge - dict->nodeSize );
    }
    return result;
}

static void
pushLastEdges( DictIter* iter, array_edge* edge, XP_Bool log )
{
    const DictionaryCtxt* dict = iter->dict;

    while ( iter->nEdges < iter->max ) {
        /* walk to the end ... */
        while ( !IS_LAST_EDGE( dict, edge ) ) {
            edge += dict->nodeSize;
        }
        /* ... so we can then move back, testing */
        PatMatch match = {0};
        if ( ! prevPeerMatch( iter, &edge, &match, log ) ) {
            break;
        }
        pushEdge( iter, edge, &match );

        edge = dict_follow( dict, edge );
        if ( NULL == edge ) {
            break;
        }
    }
}

static XP_Bool
prevWord( DictIter* iter, XP_Bool log )
{
    const DictionaryCtxt* dict = iter->dict;

    XP_Bool success = XP_FALSE;
    while ( 0 < iter->nEdges && ! success ) {
        if ( isFirstEdge( dict, iter->stack[iter->nEdges-1].edge ) ) {
            popEdge( iter );
            success = iter->min <= iter->nEdges
                && ACCEPT_NODE( iter, iter->stack[iter->nEdges-1].edge, log );
            continue;
        }

        array_edge* edge = popEdge(iter);
        XP_ASSERT( !isFirstEdge( dict, edge ) );
        edge -= dict->nodeSize;

        PatMatch match = {0};
        if ( prevPeerMatch( iter, &edge, &match, log ) ) {
            pushEdge( iter, edge, &match );
            if ( iter->nEdges < iter->max ) {
                edge = dict_follow( dict, edge );
                if ( NULL != edge ) {
                    pushLastEdges( iter, edge, log );
                }
            }
        }

        success = iter->min <= iter->nEdges
            && ACCEPT_NODE( iter, iter->stack[iter->nEdges-1].edge, log );
    }

#ifdef DEBUG
    if ( log ) {
        if ( success ) {
            XP_LOGFF( "word found: %s", iter->curWord );
        } else {
            XP_LOGFF( "NOTHING FOUND" );
        }
    }
#endif
    XP_ASSERT( (iter->min <= iter->nEdges && iter->nEdges <= iter->max) || !success );
    return success;
}

static XP_Bool
findStartsWithTiles( DictIter* iter, const Tile* tiles, XP_U16 nTiles )
{
    XP_ASSERT( nTiles <= iter->min );
    const DictionaryCtxt* dict = iter->dict;
    array_edge* edge = dict_getTopEdge( dict );
    iter->nEdges = 0;
#ifdef DEBUG
    iter->curWord[0] = '\0';
#endif

    while ( nTiles > 0 ) {
        Tile tile = *tiles++;
        edge = dict_edge_with_tile( dict, edge, tile );
        if ( NULL == edge ) {
            break;
        }

        PatMatch match = {0};
        if ( ! HAS_MATCH( iter, edge, &match, XP_FALSE ) ) {
            break;
        }
        pushEdge( iter, edge, &match );

        edge = dict_follow( dict, edge );
        --nTiles;
    }
    return 0 == nTiles;
}

static XP_Bool
startsWith( const DictIter* iter, const Tile* tiles, XP_U16 nTiles )
{
    XP_Bool success = nTiles <= iter->nEdges;
    while ( success && nTiles-- ) {
        success = tiles[nTiles] == EDGETILE( iter->dict, iter->stack[nTiles].edge );
    }
    return success;
}

static XP_Bool
findWordStartsWith( DictIter* iter, const Tile* tiles, XP_U16 nTiles, XP_Bool log )
{
    XP_Bool found = XP_FALSE;
    if ( findStartsWithTiles( iter, tiles, nTiles ) ) {
        found = ACCEPT_ITER( iter, log )
            && iter->min <= iter->nEdges && iter->nEdges <= iter->max;
        if ( !found ) {
            found = nextWord( iter, log ) && startsWith( iter, tiles, nTiles );
        }
    }
    return found;
}

static void 
initIterFrom( DictIter* dest, const DictIter* src, Indexer* indexer )
{
    /* LOG_FUNC(); */
    DIMinMax mm = { .min = src->min, .max = src->max, };
    initIter( dest, src->dict, &mm,
#ifdef XWFEATURE_WALKDICT_FILTER
              src->pats, src->nPats, indexer
              // src->patString
#else
              error
#endif
              );
    /* LOG_RETURN_VOID(); */
}

static XP_Bool
firstWord( DictIter* iter, XP_Bool log )
{
    if ( log ) {
        LOG_FUNC();
    }
    array_edge* top = dict_getTopEdge( iter->dict );
    XP_Bool success = !!top;
    if ( success ) {
        // iter->nPatMatches = 0;
        PatMatch match;
        success = nextPeerMatch( iter, &top, &match, log );
        if ( success ) {
            iter->nEdges = 0;
            pushEdge( iter, top, &match );
            // iter->edges[0] = top;
            if ( ACCEPT_ITER( iter, log ) ) {
                XP_ASSERT(0);       /* should be impossible */
            } else {
                success = nextWord( iter, log );
            }
        }
    }
    return success;
}

static XP_Bool
addTilePats( ParseState* ps, const PatDesc* pd )
{
    XP_Bool success = XP_TRUE;
    XP_Bool anyOrderOk = pd->anyOrderOk;
    PatElem elem = { .typ = CHILD,
                     .minMatched = 1,
                     .maxMatched = 1,
    };
    for ( int ii = 0; success && ii < pd->nTiles; ++ii ) {
#ifdef MULTI_SET
        ++elem.u.child.tiles.cnts[pd->tiles[ii]];
#else
        elem.u.child.tiles |= 1 << pd->tiles[ii];
#endif
        if ( !anyOrderOk ) {
            success = ps->elemIndex < VSIZE(ps->elems);
            if ( success ) {
                success = PatErrNone == addElem( ps, &elem );
#ifdef MULTI_SET
                XP_MEMSET( &elem.u.child.tiles, 0, sizeof(elem.u.child.tiles) );
#else
                elem.u.child.tiles = 0;
#endif
            }
        }
    }
    if ( anyOrderOk ) {
        elem.u.child.flags |= FLAG_SINGLE;
        elem.minMatched = elem.maxMatched = pd->nTiles;
        success = PatErrNone == addElem( ps, &elem );
    }
    LOG_RETURNF( "%s", boolToStr(success) );
    return success;
}

static void
addWildcard( ParseState* ps )
{
    PatElem elem = { .typ = CHILD,
                     .minMatched = 0,
                     .maxMatched = 1000,
    };
#ifdef MULTI_SET
    elem.u.child.tiles.cnts[ps->blankVal] = 1;
#else
    elem.u.child.tiles = ps->blankMask;
#endif

#ifdef DEBUG
    PatErr err =
#endif
        addElem( ps, &elem );
    XP_ASSERT( err == PatErrNone );
}

static void
copyParsedPat( const DictionaryCtxt* XP_UNUSED_DBG(dict), Pat* pat,
               ParseState* ps, const XP_UCHAR* patStr )
{
    if ( !!patStr ) {
        pat->patString = copyString( dict->mpool, patStr );
    }

    size_t size = ps->elemIndex * sizeof(pat->patElems[0]);
    if ( 0 < size ) {
        XP_LOGFF( "pat elems size: %zu", size );
        pat->patElems = XP_MALLOC( dict->mpool, size );
        XP_MEMCPY( pat->patElems, ps->elems, size );
        pat->nPatElems = ps->elemIndex;
    }
}

enum { STARTS_WITH, CONTAINS, ENDS_WITH, N_SEGS };

DictIter*
di_makeIter( const DictionaryCtxt* dict, XWEnv xwe, const DIMinMax* minmax,
             const XP_UCHAR** strPats, XP_U16 nPats,
             const PatDesc* tilePats, XP_U16 XP_UNUSED_DBG(nTilePats) )
{
    XP_ASSERT( 0 == nPats || !tilePats ); /* Can't both be non-null */
    DictIter* iter = NULL;

    XP_U16 nUsed = 0;
    Pat pats[MAX_PATS] = {{0}};

    ParseState ps;

    XP_Bool success = XP_TRUE;
    if ( 0 < nPats ) {
        for ( int ii = 0; success && ii < nPats; ++ii ) {
            initPS( &ps, dict );
            success = compilePat( &ps, strPats[ii] );
            if ( success ) {
                copyParsedPat( dict, &pats[nUsed++], &ps, strPats[ii] );
            }
        }
    } else if ( !!tilePats ) {
        XP_ASSERT( N_SEGS == nTilePats );
        for ( int ii = STARTS_WITH; success && ii < N_SEGS; ++ii ) {
            const PatDesc* ta = &tilePats[ii];
            if ( 0 < ta->nTiles ) {
                initPS( &ps, dict );
                if ( ii != STARTS_WITH ) {
                    addWildcard( &ps );
                }
                success = addTilePats( &ps, ta );
                if ( success ) {
                    if ( ii != ENDS_WITH ) {
                        addWildcard( &ps );
                    }
                    copyParsedPat( dict, &pats[nUsed++], &ps, NULL );
                }
            }
        }
    }
    if ( success ) {
        XP_LOGFF( "making iter of size %zu", sizeof(*iter) );
        iter = XP_CALLOC( dict->mpool, sizeof(*iter) );
        initIter( iter, dict_ref( dict, xwe ), minmax, pats, nUsed, NULL );
    }
    return iter;
}

void
di_freeIter( DictIter* iter, XWEnv xwe )
{
    for ( int ii = 0; ii < iter->nPats; ++ii ) {
        XP_FREEP( iter->dict->mpool, &iter->pats[ii].patElems );
    }
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = iter->dict->mpool;
#endif
    dict_unref( iter->dict, xwe );
    XP_FREE( mpool, iter );
}

#ifdef XWFEATURE_TESTPATSTR
typedef struct _FindState {
    int timesCalled;
    Tile tiles[32];
    int nTiles;
} FindState;

static XP_Bool
onFoundTilesSM( void* closure, const Tile* tiles, int len )
{
    FindState* fs = (FindState*)closure;
    ++fs->timesCalled;
    fs->nTiles = len;
    XP_ASSERT( len * sizeof(fs->tiles[0]) < VSIZE(fs->tiles) );
    XP_MEMCPY( fs->tiles, tiles, len * sizeof(fs->tiles[0]) );
    return XP_TRUE;
}

#ifdef DEBUG
static void logPats( const DictIter* iter )
{
    for ( int ii = 0; ii < iter->nPats; ++ii ) {
        const Pat* pat = &iter->pats[ii];
        for ( int jj = 0; jj < pat->nPatElems; ++jj ) {
            const PatElem* elem = &pat->patElems[jj];
            XP_UCHAR elemBuf[64];
            PrintState prs = { .iter = iter, .buf = elemBuf, .bufLen = VSIZE(elemBuf), };
            formatElem( &prs, elem );
            if ( 1 == iter->nPats ) {
                XP_LOGFF( "elems[%d]: %s", jj, elemBuf );
            } else {
                XP_LOGFF( "pats[%d]/elems[%d]: %s", ii, jj, elemBuf );
            }
        }
    }
}
#else
# define logPats( iter )
#endif

XP_Bool
di_stringMatches( DictIter* iter, const XP_UCHAR* str )
{
    LOG_FUNC();
    FindState fs = {0};
    dict_tilesForString( iter->dict, str, 0, onFoundTilesSM, &fs );
    XP_ASSERT( 1 == fs.timesCalled );

    logPats( iter );

    array_edge_old tmps[fs.nTiles];
    XP_MEMSET( &tmps[0], 0, VSIZE(tmps) * sizeof(tmps[0]) );

    XP_Bool matched = XP_TRUE;
    for ( int ii = 0; matched && ii < fs.nTiles; ++ii ) {
        PatMatch match = {0};
        array_edge_old* tmp = &tmps[ii];
        tmp->bits = fs.tiles[ii];
        array_edge* fakeEdge = (array_edge*)tmp;
        matched = HAS_MATCH( iter, fakeEdge, &match, XP_TRUE );
        if ( !matched ) {
            break;
        }
        pushEdge( iter, fakeEdge, &match );
    }
    if ( matched ) {
        matched = MATCH_FINISHED( iter, XP_TRUE );
    }
    LOG_RETURNF( "%s", boolToStr(matched) );
    return matched;
} /* di_stringMatches */
#endif

static XP_U32
countWordsIn( DictIter* iter, LengthsArray* lens )
{
    if ( NULL != lens ) {
        XP_MEMSET( lens, 0, sizeof(*lens) );
    }

    XP_U32 count = 0;
    for ( XP_Bool ok = firstWord( iter, XP_FALSE );
          ok;
          ok = nextWord( iter, XP_FALSE) ) {
        ++count;

        if ( NULL != lens ) {
            ++lens->lens[iter->nEdges];
        }
    }
    return count;
}

XP_U32
di_countWords( const DictIter* iter, LengthsArray* lens )
{
    /* LOG_FUNC(); */
    DictIter counter;
    initIterFrom( &counter, iter, NULL );

    XP_U32 result = countWordsIn( &counter, lens );
    /* LOG_RETURNF( "%d", result ); */
    return result;
}

#define GUARD_VALUE 0x12345678
#define ASSERT_INITED( iter ) XP_ASSERT( (iter)->guard == GUARD_VALUE )

static void
initIter( DictIter* iter, const DictionaryCtxt* dict, const DIMinMax* minmax,
          const Pat* pats, XP_U16 nPats, Indexer* indexer )
{
    XP_MEMSET( iter, 0, sizeof(*iter) );
    iter->dict = dict;
    iter->indexer = indexer;
#ifdef MULTI_SET
    iter->blankVal = dict_getBlankTile( dict );
#else
    iter->blankMask = ((TileSet)1) << dict_getBlankTile( dict );
#endif
#ifdef DEBUG
    iter->guard = GUARD_VALUE;
#endif
#ifdef XWFEATURE_WALKDICT_FILTER
    if ( !!minmax ) {
        iter->min = XP_MAX(2, minmax->min);
        iter->max = minmax->max;
    }
    if ( iter->max < 2 || MAX_COLS_DICT < iter->max ) {
        iter->max = MAX_COLS_DICT;
    }
    if ( iter->min < 2 || iter->max < iter->min ) {
        iter->min = 2;
    }

    XP_ASSERT( nPats <= MAX_PATS );
    if ( 0 < nPats ) {
        XP_MEMCPY( iter->pats, pats, nPats * sizeof(pats[0]) );
    }
    iter->nPats = nPats;

    iter->nWords = countWordsIn( iter, NULL );
    /* XP_UCHAR buf[128]; */
    /* printPat( iter, buf, VSIZE(buf) ); */
    /* XP_LOGFF( "%s => %s", strPat, buf ); */
#else
    error;
#endif
}

static DictPosition
placeWordClose( DictIter* iter, const DictPosition position, XP_U16 depth,
                const IndexData* data )
{
    XP_S16 index = -1;
    for ( XP_S16 low = 0, high = data->count - 1; ; ) {
        if ( low > high ) {
            break;
        }
        index = low + ( (high - low) / 2);
        if ( position < data->indices[index] ) {
            high = index - 1;
        } else if ( data->indices[index+1] <= position) {
            low = index + 1;
        } else {
            break;
        }
    }

    /* Now we have the index immediately below the position we want.  But we
       may be better off starting with the next if it's closer.  The last
       index is a special case since we use lastWord rather than a prefix to
       init */
    if ( ( index + 1 < data->count ) 
         && (data->indices[index + 1] - position) 
         < (position - data->indices[index]) ) {
        ++index;
    }
    if ( !findWordStartsWith( iter, &data->prefixes[depth*index], depth, XP_FALSE ) ) {
        XP_ASSERT(0);
    }
    return data->indices[index];
} /* placeWordClose */

static void
iterToString( const DictIter* iter, XP_UCHAR* buf, XP_U16 buflen,
              const XP_UCHAR* delim )
{
    XP_U16 ii;
    XP_U16 nEdges = iter->nEdges;
    Tile tiles[nEdges];
    for ( ii = 0; ii < nEdges; ++ii ) {
        tiles[ii] = EDGETILE( iter->dict, iter->stack[ii].edge );
    }
    (void)dict_tilesToString( iter->dict, tiles, nEdges, buf, buflen, delim );
}

typedef struct _IndexState {
    const DictIter* iter;
    IndexData* data;
    XP_U16 depth;
    XP_U32 nWordsSeen;
    Tile* curPrefix;
    const Tile* lastPrefix;
} IndexState;

/* Each time we're called, check if the first <depth> tiles are a new
 * pattern. If so, increment the counter and record the position.
 */
static void
tryIndex( void* closure )
{
    IndexState* is = (IndexState*)closure;
    const XP_U16 depth = is->depth;
    IndexData* data = is->data;
    const DictIter* iter = is->iter;

    XP_ASSERT( iter->nEdges >= depth ); /* won't happen now */
    XP_ASSERT( iter->min <= iter->nEdges && iter->nEdges <= iter->max );

    if ( NULL == is->curPrefix ) { /* first time through */
        is->curPrefix = data->prefixes - depth;
    }

    /* We have to accumulate prefix somewhere. Might as well be where we'll
       save it if it's new. Should be no danger */
    Tile* nextPrefix = is->curPrefix + depth;
    if ( nextPrefix < is->lastPrefix ) {
        for ( int ii = 0; ii < depth; ++ii ) {
            nextPrefix[ii] = EDGETILE( iter->dict, iter->stack[ii].edge );
        }

        if ( 0 == data->count || 0 != XP_MEMCMP( is->curPrefix, nextPrefix, depth )) {
            /* It's new. Point at it */
            is->curPrefix = nextPrefix;
            data->indices[data->count] = is->nWordsSeen;
#ifdef DEBUG
            /* XP_UCHAR buf[depth * 3]; */
            /* (void)dict_tilesToString( iter->dict, nextPrefix, depth, buf, */
            /*                           VSIZE(buf), NULL ); */
            /* XP_LOGFF( "set position[%d]: %s: %d", data->count, buf, */
            /*           data->indices[data->count] ); */
#endif
            ++data->count;
        }
        ++is->nWordsSeen;
    } else {
        XP_LOGFF( "out of space" );
    }
}

void
di_makeIndex( const DictIter* iter, XP_U16 depth, IndexData* data )
{
    LOG_FUNC();
    ASSERT_INITED( iter );
    const DictionaryCtxt* dict = iter->dict;
    DI_ASSERT( depth < MAX_COLS_DICT );
    XP_U16 ii, needCount;
    const XP_U16 nFaces = dict_numTileFaces( dict );
    XP_U16 nNonBlankFaces = nFaces;
    XP_Bool hasBlank = dict_hasBlankTile( dict );
    if ( hasBlank ) {
        --nNonBlankFaces;
    }
    // this is just needCount = pow(nNonBlankFaces,depth)
    for ( ii = 1, needCount = nNonBlankFaces; ii < depth; ++ii ) {
        needCount *= nNonBlankFaces;
    }
    DI_ASSERT( needCount <= data->count );

    DictIter tmpIter;

    IndexState is = {
        .iter = &tmpIter,
        .data = data,
        .nWordsSeen = 0,
        .depth = depth,
        .curPrefix = NULL,
        .lastPrefix = data->prefixes + (depth * data->count * sizeof(data->prefixes[0])),
    };
    Indexer indexer = {
        .proc = tryIndex,
        .closure = &is,
    };

    data->count = 0;
    initIterFrom( &tmpIter, iter, &indexer );

#ifdef DI_DEBUG
    DictPosition pos;
    for ( pos = 1; pos < data->count; ++pos ) {
        DI_ASSERT( data->indices[pos-1] < data->indices[pos] );
    }
#endif
    LOG_RETURN_VOID();
} /* di_makeIndex */

XP_Bool
di_firstWord( DictIter* iter )
{
    ASSERT_INITED( iter );
    XP_Bool success = firstWord( iter, XP_FALSE );
    if ( success ) {
        iter->position = 0;
    }

    return success;
}

XP_Bool
di_getNextWord( DictIter* iter )
{
    ASSERT_INITED( iter );
    XP_Bool success = nextWord( iter, XP_FALSE );
    if ( success ) {
        ++iter->position;
    }
    return success;
}

XP_Bool
di_lastWord( DictIter* iter )
{
    const XP_Bool log = XP_FALSE;

    ASSERT_INITED( iter );
    while ( 0 < iter->nEdges ) {
        popEdge( iter );
    }

    pushLastEdges( iter, dict_getTopEdge( iter->dict ), log );

    XP_Bool success = ACCEPT_ITER( iter, log )
        && iter->min <= iter->nEdges
        && iter->nEdges <= iter->max;
    if ( !success ) {
        success = prevWord( iter, log );
    }
    if ( success ) {
        iter->position = iter->nWords - 1;
    }

    return success;
}

XP_Bool
di_getPrevWord( DictIter* iter )
{
    ASSERT_INITED( iter );
    XP_Bool success = prevWord( iter, XP_FALSE );
    if ( success ) {
        --iter->position;
    }
    return success;
}

/* If we start without an initialized word, init it to be closer to what's
   sought.  OR if we're father than necessary from what's sought, start over
   at the closer end.  Then move as many steps as necessary to reach it. */
XP_Bool
di_getNthWord( DictIter* iter, XWEnv xwe, DictPosition position, XP_U16 depth,
               const IndexData* data )
{
    /* XP_LOGFF( "(position=%d, depth=%d, data=%p)", position, depth, data ); */
    ASSERT_INITED( iter );
    const DictionaryCtxt* dict = iter->dict;
    XP_U32 wordCount;
    XP_Bool validWord = 0 < iter->nEdges;
    if ( validWord ) {        /* uninitialized */
        wordCount = iter->nWords;
        DI_ASSERT( wordCount == di_countWords( iter, NULL ) );
    } else {
        wordCount = dict_getWordCount( dict, xwe );
    }
    XP_Bool success = position < wordCount;
    if ( success ) {
        /* super common cases first */
        success = XP_FALSE;
        if ( validWord ) {
            if ( iter->position == position ) {
                success = XP_TRUE;
                /* do nothing; we're done */
            } else if ( iter->position == position - 1 ) {
                success = di_getNextWord( iter );
            } else if ( iter->position == position + 1 ) {
                success = di_getPrevWord( iter );
            }
        }

        if ( !success ) {
            XP_U32 wordIndex;
            if ( !!data && !!data->prefixes && !!data->indices ) {
                wordIndex = placeWordClose( iter, position, depth, data );
            } else {
                wordCount /= 2;             /* mid-point */

                /* If word's inited but farther from target than either
                   endpoint, better to start with an endpoint */
                if ( validWord && 
                     XP_ABS( position - iter->position ) > wordCount ) {
                    validWord = XP_FALSE;
                }

                if ( !validWord ) {
                    if ( position >= wordCount ) {
                        di_lastWord( iter );
                    } else {
                        di_firstWord( iter );
                    }
                }
                wordIndex = iter->position;
            }

            XP_Bool (*finder)( DictIter* iter, XP_Bool log ) = NULL;/* stupid compiler */
            XP_U32 repeats = 0;
            if ( wordIndex < position ) {
                finder = nextWord;
                repeats = position - wordIndex;
            } else if ( wordIndex > position ) {
                finder = prevWord;
                repeats = wordIndex - position;
            }
            while ( repeats-- ) {
                if ( !(*finder)( iter, XP_FALSE ) ) {
                    XP_ASSERT(0);
                    break;      /* prevents crash on release builds? */
                }
            }

            iter->position = position;
            success = XP_TRUE;
        }
    }
    return success;
} /* di_getNthWord */

XP_U32
di_getNWords( const DictIter* iter )
{
    return iter->nWords;
}

void
di_getMinMax( const DictIter* iter, XP_U16* min, XP_U16* max )
{
    *min = iter->min;
    *max = iter->max;
}

void
di_wordToString( const DictIter* iter, XP_UCHAR* buf, XP_U16 buflen,
                 const XP_UCHAR* delim )
{
    ASSERT_INITED( iter );
    iterToString( iter, buf, buflen, delim );
#ifdef DEBUG
    // If there's no delim, debug string should be same
    if ( !delim || '\0' == *delim ) {
        XP_ASSERT( 0 == XP_STRCMP( buf, iter->curWord ) );
    }
#endif
}

DictPosition
di_getPosition( const DictIter* iter )
{
    ASSERT_INITED( iter );
    return iter->position;
}

#ifdef CPLUS
}
#endif
#endif /* XWFEATURE_WALKDICT */
