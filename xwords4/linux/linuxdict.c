/* -*-mode: C; fill-column: 78; c-basic-offset: 4; compile-command: "make MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 1997-2009 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef CLIENT_ONLY		/* there's an else in the middle!!! */

#include <stdio.h>
#include <stdlib.h>
/* #include <prc.h> */

#include "comtypes.h"
#include "dictnryp.h"
#include "linuxmain.h"
#include "strutils.h"

typedef struct DictStart {
    XP_U32 numNodes;
    /*    XP_U32 indexStart; */
    array_edge* array;
} DictStart;

typedef struct LinuxDictionaryCtxt {
    DictionaryCtxt super;
    /*     prc_t* pt; */
    /*     DictStart* starts; */
    /*     XP_U16 numStarts; */
} LinuxDictionaryCtxt;


/************************ Prototypes ***********************/
static XP_Bool initFromDictFile( LinuxDictionaryCtxt* dctx, 
                                 const char* fileName );
static void linux_dictionary_destroy( DictionaryCtxt* dict );
static const XP_UCHAR* linux_dict_getShortName( const DictionaryCtxt* dict );

/*****************************************************************************
 *
 ****************************************************************************/
DictionaryCtxt* 
linux_dictionary_make( MPFORMAL const char* dictFileName )
{
    LinuxDictionaryCtxt* result = 
        (LinuxDictionaryCtxt*)XP_MALLOC(mpool, sizeof(*result));
    XP_MEMSET( result, 0, sizeof(*result) );

    dict_super_init( (DictionaryCtxt*)result );
    MPASSIGN(result->super.mpool, mpool);

    if ( !!dictFileName ) {
        XP_Bool success = initFromDictFile( result, dictFileName );
        if ( success ) {
            result->super.destructor = linux_dictionary_destroy;
            result->super.func_dict_getShortName = linux_dict_getShortName;
            setBlankTile( &result->super );
        } else {
            XP_FREE( mpool, result );
            result = NULL;
        }
    }

    return (DictionaryCtxt*)result;
} /* gtk_dictionary_make */

static XP_U16
countSpecials( LinuxDictionaryCtxt* ctxt )
{
    XP_U16 result = 0;
    XP_U16 ii;

    for ( ii = 0; ii < ctxt->super.nFaces; ++ii ) {
        if ( IS_SPECIAL(ctxt->super.facePtrs[ii][0]) ) {
            ++result;
        }
    }

    return result;
} /* countSpecials */

static XP_Bitmap
skipBitmap( LinuxDictionaryCtxt* XP_UNUSED_DBG(ctxt), FILE* dictF )
{
    XP_U8 nCols, nRows, nBytes;
    LinuxBMStruct* lbs = NULL;
    
    if ( 1 == fread( &nCols, sizeof(nCols), 1, dictF )
         && nCols > 0 
         && 1 == fread( &nRows, sizeof(nRows), 1, dictF ) ) {

        nBytes = ((nRows * nCols) + 7) / 8;

        lbs = XP_MALLOC( ctxt->super.mpool, sizeof(*lbs) + nBytes );
        lbs->nRows = nRows;
        lbs->nCols = nCols;
        lbs->nBytes = nBytes;

        if ( 1 != fread( lbs + 1, nBytes, 1, dictF ) ) {
            XP_FREE( ctxt->super.mpool, lbs );
            lbs = NULL;
        }
    }

    return lbs;
} /* skipBitmap */

static void
skipBitmaps( LinuxDictionaryCtxt* ctxt, FILE* dictF )
{
    XP_U16 nSpecials;
    XP_UCHAR* text;
    XP_UCHAR** texts;
    SpecialBitmaps* bitmaps;
    Tile tile;

    nSpecials = countSpecials( ctxt );

    texts = (XP_UCHAR**)XP_MALLOC( ctxt->super.mpool, 
                                   nSpecials * sizeof(*texts) );
    bitmaps = (SpecialBitmaps*)XP_MALLOC( ctxt->super.mpool, 
                                          nSpecials * sizeof(*bitmaps) );
    XP_MEMSET( bitmaps, 0, nSpecials * sizeof(*bitmaps) );

    for ( tile = 0; tile < ctxt->super.nFaces; ++tile ) {
	
        XP_UCHAR* facep = ctxt->super.facePtrs[(short)tile];
        if ( IS_SPECIAL(*facep) ) {
            XP_U16 asIndex = (XP_U16)*facep;
            XP_U8 txtlen;
            XP_ASSERT( *facep < nSpecials );

            /* get the string */
            if ( 1 == fread( &txtlen, sizeof(txtlen), 1, dictF ) ) {
                text = (XP_UCHAR*)XP_MALLOC(ctxt->super.mpool, txtlen+1);
                if ( 1 == fread( text, txtlen, 1, dictF ) ) {
                    text[txtlen] = '\0';
                    texts[(XP_U16)*facep] = text;

                    XP_DEBUGF( "skipping bitmaps for " XP_S, texts[asIndex] );

                    bitmaps[asIndex].largeBM = skipBitmap( ctxt, dictF );
                    bitmaps[asIndex].smallBM = skipBitmap( ctxt, dictF );
                }
            }
        }
    }

    ctxt->super.chars = texts;
    ctxt->super.bitmaps = bitmaps;
} /* skipBitmaps */

void
dict_splitFaces( DictionaryCtxt* dict, const XP_U8* utf8,
                 XP_U16 nBytes, XP_U16 nFaces )
{
    XP_UCHAR* faces = XP_MALLOC( dict->mpool, nBytes + nFaces );
    XP_UCHAR** ptrs = XP_MALLOC( dict->mpool, nFaces * sizeof(ptrs[0]));
    XP_U16 ii;
    XP_Bool isUTF8 = dict->isUTF8;
    XP_UCHAR* next = faces;
    const gchar* bytes = (const gchar*)utf8;

    for ( ii = 0; ii < nFaces; ++ii ) {
        ptrs[ii] = next;
        if ( isUTF8 ) {
            gchar* cp = g_utf8_offset_to_pointer( bytes, 1 );
            XP_U16 len = cp - bytes;
            XP_MEMCPY( next, bytes, len );
            next += len;
            bytes += len;
        } else {
            XP_ASSERT( 0 == *bytes );
            ++bytes;            /* skip empty */
            *next++ = *bytes++;
        }
        XP_ASSERT( next < faces + nFaces + nBytes );
        *next++ = '\0';
    }
    XP_ASSERT( !dict->faces );
    dict->faces = faces;
    XP_ASSERT( !dict->facePtrs );
    dict->facePtrs = ptrs;
} /* dict_splitFaces */

static XP_Bool
initFromDictFile( LinuxDictionaryCtxt* dctx, const char* fileName )
{
    XP_Bool formatOk = XP_TRUE;
    XP_U8 numFaces, numFaceBytes;
    long curPos, dictLength;
    XP_U32 topOffset;
    FILE* dictF = fopen( fileName, "r" );
    unsigned short xloc;
    XP_U16 flags;
    XP_U16 facesSize;
    XP_U16 charSize;
    XP_Bool isUTF8 = XP_FALSE;

    XP_ASSERT( dictF );
    if ( 1 == fread( &flags, sizeof(flags), 1, dictF ) ) {
        flags = ntohs(flags);
        XP_DEBUGF( "flags=0x%x", flags );
#ifdef NODE_CAN_4
        if ( flags == 0x0001 ) {
            dctx->super.nodeSize = 3;
            charSize = 1;
            dctx->super.is_4_byte = XP_FALSE;
        } else if ( flags == 0x0002 ) {
            dctx->super.nodeSize = 3;
            charSize = 2;
            dctx->super.is_4_byte = XP_FALSE;
        } else if ( flags == 0x0003 ) {
            dctx->super.nodeSize = 4;
            charSize = 2;
            dctx->super.is_4_byte = XP_TRUE;
        } else if ( flags == 0x0004 ) {
            dctx->super.nodeSize = 3;
            dctx->super.isUTF8 = XP_TRUE;
            isUTF8 = XP_TRUE;
            dctx->super.is_4_byte = XP_FALSE;
        } else if ( flags == 0x0005 ) {
            dctx->super.nodeSize = 4;
            dctx->super.isUTF8 = XP_TRUE;
            isUTF8 = XP_TRUE;
            dctx->super.is_4_byte = XP_TRUE;
        } else {
            /* case I don't know how to deal with */
            formatOk = XP_FALSE;
            XP_ASSERT(0);
        }
    } else {
        XP_ASSERT(0);
    }
#else
    XP_ASSERT( flags == 0x0001 );
#endif

    if ( formatOk ) {
        if ( isUTF8 ) {
            if ( 1 != fread( &numFaceBytes, sizeof(numFaceBytes), 1, dictF ) ) {
                goto closeAndExit;
            }
        }
        if ( 1 != fread( &numFaces, sizeof(numFaces), 1, dictF ) ) {
            goto closeAndExit;
        }
        if ( !isUTF8 ) {
            numFaceBytes = numFaces * charSize;
        }

        dctx->super.nFaces = numFaces;

        dctx->super.countsAndValues = XP_MALLOC( dctx->super.mpool, 
                                                 numFaces*2 );
        facesSize = numFaceBytes;
        if ( !isUTF8 ) {
            facesSize /= 2;
        }

        XP_U8 tmp[numFaceBytes];
        if ( 1 != fread( tmp, numFaceBytes, 1, dictF ) ) {
            goto closeAndExit;
        }

        dict_splitFaces( &dctx->super, tmp, numFaceBytes, numFaces );

        if ( (1 != fread( &xloc, 2, 1, dictF ) )/* read in (dump) the xloc
                                                 header for now */
             || (1 != fread( dctx->super.countsAndValues, numFaces*2, 1, 
                             dictF ) ) ) {
            goto closeAndExit;
        }
    }

    if ( formatOk ) {
        skipBitmaps( dctx, dictF );

        curPos = ftell( dictF );
        fseek( dictF, 0L, SEEK_END );
        dictLength = ftell( dictF ) - curPos;
        fseek( dictF, curPos, SEEK_SET );

        if ( dictLength > 0 ) {
            if ( 1 != fread( &topOffset, sizeof(topOffset), 1, dictF ) ) {
                goto closeAndExit;
            }
            /* it's in big-endian order */
            topOffset = ntohl(topOffset);
            dictLength -= sizeof(topOffset); /* first four bytes are offset */
        }

        if ( dictLength > 0 ) {
#ifdef DEBUG
# ifdef NODE_CAN_4
            dctx->super.numEdges = dictLength / dctx->super.nodeSize;
            XP_ASSERT( (dictLength % dctx->super.nodeSize) == 0 );
# else
            dctx->super.numEdges = dictLength / 3;
            XP_ASSERT( (dictLength % 3) == 0 );
# endif
#endif

            dctx->super.base = (array_edge*)XP_MALLOC( dctx->super.mpool, 
                                                   dictLength );
            XP_ASSERT( !!dctx->super.base );
            if ( 1 != fread( dctx->super.base, dictLength, 1, dictF ) ) {
                goto closeAndExit;
            }

            dctx->super.topEdge = dctx->super.base + topOffset;
        } else {
            dctx->super.base = NULL;
            dctx->super.topEdge = NULL;
        }

        dctx->super.name = copyString( dctx->super.mpool, fileName );
    }
    goto ok;

 closeAndExit:
    formatOk = XP_FALSE;
 ok:
    fclose( dictF );
    return formatOk;
} /* initFromDictFile */

static void
freeSpecials( LinuxDictionaryCtxt* ctxt )
{
    XP_U16 nSpecials = 0;
    XP_U16 ii;

    for ( ii = 0; ii < ctxt->super.nFaces; ++ii ) {
        if ( IS_SPECIAL(ctxt->super.facePtrs[ii][0] ) ) {
            if ( !!ctxt->super.bitmaps ) {
                XP_Bitmap* bmp = ctxt->super.bitmaps[nSpecials].largeBM;
                if ( !!bmp ) {
                    XP_FREE( ctxt->super.mpool, bmp );
                }
                bmp = ctxt->super.bitmaps[nSpecials].smallBM;
                if ( !!bmp ) {
                    XP_FREE( ctxt->super.mpool, bmp );
                }
            }
            if ( !!ctxt->super.chars && !!ctxt->super.chars[nSpecials]) {
                XP_FREE( ctxt->super.mpool, ctxt->super.chars[nSpecials] );
            }
            ++nSpecials;
        }
    }
    if ( !!ctxt->super.bitmaps ) {
        XP_FREE( ctxt->super.mpool, ctxt->super.bitmaps );
    }
    if ( !!ctxt->super.chars ) {
        XP_FREE( ctxt->super.mpool, ctxt->super.chars );
    }
} /* freeSpecials */

static void
linux_dictionary_destroy( DictionaryCtxt* dict )
{
    LinuxDictionaryCtxt* ctxt = (LinuxDictionaryCtxt*)dict;

    freeSpecials( ctxt );

    if ( !!dict->topEdge ) {
        XP_FREE( dict->mpool, dict->topEdge );
    }

    XP_FREE( dict->mpool, ctxt->super.countsAndValues );
    XP_FREE( dict->mpool, ctxt->super.faces );
    XP_FREE( dict->mpool, ctxt->super.facePtrs );
    XP_FREE( dict->mpool, ctxt->super.name );
    XP_FREE( dict->mpool, ctxt );
} /* linux_dictionary_destroy */

static const XP_UCHAR*
linux_dict_getShortName( const DictionaryCtxt* dict )
{
    const XP_UCHAR* full = dict_getName( dict );
    const XP_UCHAR* c = strchr( full, '/' );
    if ( !!c ) {
        ++c;
    } else {
        c = full;
    }
    return c;
}

#else  /* CLIENT_ONLY *IS* defined */

/* initFromDictFile:
 * This guy reads in from a prc file, and probably hasn't worked in a year.
 */
#define RECS_BEFORE_DAWG 3	/* a hack */
static XP_Bool
initFromDictFile( LinuxDictionaryCtxt* dctx, const char* fileName )
{
    short i;
    unsigned short* dataP;
    unsigned nRecs;
    prc_record_t* prect;

    prc_t* pt = prcopen( fileName, PRC_OPEN_READ );
    dctx->pt = pt;		/* remember so we can close it later */

    nRecs = prcgetnrecords( pt );

    /* record 0 holds a struct whose 5th byte is the record num of the first
       dawg record. 1 and 2 hold tile data.  Let's assume 3 is the first dawg
       record for now. */

    prect = prcgetrecord( pt, 1 );
    dctx->super.numFaces = prect->datalen; /* one char per byte */
    dctx->super.faces = malloc( prect->datalen );
    memcpy( dctx->super.faces, prect->data, prect->datalen );
    
    dctx->super.counts = malloc( dctx->super.numFaces );
    dctx->super.values = malloc( dctx->super.numFaces );

    prect = prcgetrecord( pt, 2 );
    dataP = (unsigned short*)prect->data + 1;	/* skip the xloc header */

    for ( i = 0; i < dctx->super.numFaces; ++i ) {
        unsigned short byt = *dataP++;
        dctx->super.values[i] = byt >> 8;
        dctx->super.counts[i] = byt & 0xFF;
        if ( dctx->super.values[i] == 0 ) {
            dctx->super.counts[i] = 4; /* 4 blanks :-) */
        }
    }

    dctx->numStarts = nRecs - RECS_BEFORE_DAWG;
    dctx->starts = XP_MALLOC( dctx->numStarts * sizeof(*dctx->starts) );

    for ( i = 0/* , offset = 0 */; i < dctx->numStarts; ++i ) {
        prect = prcgetrecord( pt, i + RECS_BEFORE_DAWG );
        dctx->starts[i].numNodes = prect->datalen / 3;
        dctx->starts[i].array = (array_edge*)prect->data;

        XP_ASSERT( (prect->datalen % 3) == 0 );
    }
} /* initFromDictFile */

void
linux_dictionary_destroy( DictionaryCtxt* dict )
{
    LinuxDictionaryCtxt* ctxt = (LinuxDictionaryCtxt*)dict;
    prcclose( ctxt->pt );
}

#endif /* CLIENT_ONLY */

