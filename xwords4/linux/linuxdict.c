/* -*- compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 1997 - 2013 by Eric House (xwords@eehouse.org).  All rights
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
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
/* #include <prc.h> */

#include "comtypes.h"
#include "dictnryp.h"
#include "linuxmain.h"
#include "strutils.h"
#include "linuxutl.h"
#include "dictmgr.h"

typedef struct DictStart {
    XP_U32 numNodes;
    /*    XP_U32 indexStart; */
    array_edge* array;
} DictStart;

typedef struct LinuxDictionaryCtxt {
    DictionaryCtxt super;
    XP_U8* dictBase;
    size_t dictLength;
    XP_Bool useMMap;
} LinuxDictionaryCtxt;


/************************ Prototypes ***********************/
static XP_Bool initFromDictFile( LinuxDictionaryCtxt* dctx, 
                                 const LaunchParams* params,
                                 const char* fileName );
static void linux_dictionary_destroy( DictionaryCtxt* dict, XWEnv xwe );
static const XP_UCHAR* linux_dict_getShortName( const DictionaryCtxt* dict );

/*****************************************************************************
 *
 ****************************************************************************/
DictionaryCtxt* 
linux_dictionary_make( MPFORMAL XWEnv xwe, const LaunchParams* params,
                       const char* dictFileName, XP_Bool useMMap )
{
    LinuxDictionaryCtxt* result = NULL;
    if ( !!dictFileName ) {
        /* dmgr_get increments ref count before returning! */
        result = (LinuxDictionaryCtxt*)dmgr_get( params->dictMgr, xwe, dictFileName );
    }
    if ( !result ) {
        result = (LinuxDictionaryCtxt*)XP_CALLOC(mpool, sizeof(*result));

        dict_super_init( MPPARM(mpool) &result->super );
        MPASSIGN( result->super.mpool, mpool );
        result->super.destructor = linux_dictionary_destroy;

        result->useMMap = useMMap;

        if ( !!dictFileName ) {
            XP_Bool success = initFromDictFile( result, params, dictFileName );
            if ( success ) {
                result->super.func_dict_getShortName = linux_dict_getShortName;
                setBlankTile( &result->super );
            } else {
                XP_ASSERT( 0 ); /* gonna crash anyway */
                XP_FREE( mpool, result );
                result = NULL;
            }

            dmgr_put( params->dictMgr, xwe, dictFileName, &result->super );
        } else {
            XP_LOGF( "%s(): no file name!!", __func__ );
        }
        (void)dict_ref( &result->super, xwe );
    }

    return &result->super;
} /* gtk_dictionary_make */

void
computeChecksum( DictionaryCtxt* XP_UNUSED(dctx), XWEnv XP_UNUSED(xwe),
                 const XP_U8* ptr, XP_U32 len, XP_UCHAR* out )
{
    gchar* checksum = g_compute_checksum_for_data( G_CHECKSUM_MD5, ptr, len );
    XP_MEMCPY( out, checksum, XP_STRLEN(checksum) + 1 );
    g_free( checksum );
}

void
dict_splitFaces( DictionaryCtxt* dict, XWEnv XP_UNUSED(xwe), const XP_U8* utf8,
                 XP_U16 nBytes, XP_U16 nFaces )
{
    XP_UCHAR* faces = XP_MALLOC( dict->mpool, nBytes + nFaces );
    const XP_UCHAR** ptrs = XP_MALLOC( dict->mpool, nFaces * sizeof(ptrs[0]));
    XP_Bool isUTF8 = dict->isUTF8;
    XP_UCHAR* next = faces;
    const gchar* bytesIn = (const gchar*)utf8;
    const gchar* bytesEnd = bytesIn + nBytes;

    for ( int ii = 0; ii < nFaces; ++ii ) {
        ptrs[ii] = next;
        if ( isUTF8 ) {
            for ( ; ; ) {
                gchar* cp = g_utf8_offset_to_pointer( bytesIn, 1 );
                size_t len = cp - bytesIn;
                XP_MEMCPY( next, bytesIn, len );
                next += len;
                bytesIn += len;
                if ( bytesIn >= bytesEnd || SYNONYM_DELIM != bytesIn[0] ) {
                    break;
                }
                ++bytesIn;        /* skip delimiter */
                *next++ = '\0';
            }
        } else {
            XP_ASSERT( 0 == *bytesIn );
            ++bytesIn;            /* skip empty */
            *next++ = *bytesIn++;
        }
        XP_ASSERT( next < faces + nFaces + nBytes );
        *next++ = '\0';
    }
    XP_ASSERT( !dict->faces );
    dict->faces = faces;
    dict->facesEnd = faces + nFaces + nBytes;
    XP_ASSERT( !dict->facePtrs );
    dict->facePtrs = ptrs;
} /* dict_splitFaces */

static XP_Bool
initFromDictFile( LinuxDictionaryCtxt* dctx, const LaunchParams* params, 
                  const char* fileName )
{
    XP_Bool formatOk = XP_TRUE;
    size_t dictLength;
    XP_U32 topOffset;
    char path[256];

    if ( file_exists( fileName ) ) {
        snprintf( path, VSIZE(path), "%s", fileName );
    } else if ( !getDictPath( params, fileName, path, VSIZE(path) ) ) {
        XP_LOGF( "%s: path=%s", __func__, path );
        goto closeAndExit;
    }
    struct stat statbuf;
    if ( 0 != stat( path, &statbuf ) || 0 == statbuf.st_size ) {
        goto closeAndExit;
    }
    dctx->dictLength = statbuf.st_size;

    {
        FILE* dictF = fopen( path, "r" );
        XP_ASSERT( !!dictF );
        if ( dctx->useMMap ) {
            dctx->dictBase = mmap( NULL, dctx->dictLength, PROT_READ, 
                                   MAP_PRIVATE, fileno(dictF), 0 );
        } else {
            dctx->dictBase = XP_MALLOC( dctx->super.mpool, dctx->dictLength );
            if ( dctx->dictLength != fread( dctx->dictBase, 1, 
                                            dctx->dictLength, dictF ) ) {
                XP_ASSERT( 0 );
            }
        }
        fclose( dictF );
    }

    const XP_U8* ptr = dctx->dictBase;
    const XP_U8* end = ptr + dctx->dictLength;
    formatOk = parseCommon( &dctx->super, NULL_XWE, &ptr, end );
        /* && loadSpecialData( &dctx->super, &ptr, end ); */

    if ( formatOk ) {
        size_t curPos = ptr - dctx->dictBase;
        dictLength = dctx->dictLength - curPos;

        if ( dictLength > 0 ) {
            memcpy( &topOffset, ptr, sizeof(topOffset) );
            /* it's in big-endian order */
            topOffset = ntohl(topOffset);
            dictLength -= sizeof(topOffset); /* first four bytes are offset */
            ptr += sizeof(topOffset);
        }

        XP_U32 numEdges;
        if ( dictLength > 0 ) {
            numEdges = dictLength / dctx->super.nodeSize;
#ifdef DEBUG
            XP_ASSERT( (dictLength % dctx->super.nodeSize) == 0 );
            dctx->super.numEdges = numEdges;
#endif
            dctx->super.base = (array_edge*)ptr;

            dctx->super.topEdge = dctx->super.base + topOffset;
        } else {
            dctx->super.base = NULL;
            dctx->super.topEdge = NULL;
            numEdges = 0;
        }

        dctx->super.name = copyString( dctx->super.mpool, fileName );

        if ( ! checkSanity( &dctx->super, numEdges ) ) {
            goto closeAndExit;
        }
    }
    goto ok;

 closeAndExit:
    formatOk = XP_FALSE;
 ok:

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
    XP_FREEP( ctxt->super.mpool, &ctxt->super.chars );
    XP_FREEP( ctxt->super.mpool, &ctxt->super.charEnds );
} /* freeSpecials */

static void
linux_dictionary_destroy( DictionaryCtxt* dict, XWEnv XP_UNUSED(xwe) )
{
    LinuxDictionaryCtxt* ctxt = (LinuxDictionaryCtxt*)dict;

    freeSpecials( ctxt );

    if ( !!ctxt->dictBase ) {
        if ( ctxt->useMMap ) {
            (void)munmap( ctxt->dictBase, ctxt->dictLength );
        } else {
            XP_FREE( dict->mpool, ctxt->dictBase );
        }
    }

    /* super's destructor should do this!!!! */
    XP_FREEP( dict->mpool, &ctxt->super.desc );
    XP_FREEP( dict->mpool, &ctxt->super.md5Sum );
    XP_FREEP( dict->mpool, &ctxt->super.countsAndValues );
    XP_FREEP( dict->mpool, &ctxt->super.faces );
    XP_FREEP( dict->mpool, &ctxt->super.facePtrs );
    XP_FREEP( dict->mpool, &ctxt->super.name );
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
/* static XP_Bool */
/* initFromDictFile( LinuxDictionaryCtxt* dctx, const char* fileName ) */
/* { */
/*     short i; */
/*     unsigned short* dataP; */
/*     unsigned nRecs; */
/*     prc_record_t* prect; */

/*     prc_t* pt = prcopen( fileName, PRC_OPEN_READ ); */
/*     dctx->pt = pt;		/\* remember so we can close it later *\/ */

/*     nRecs = prcgetnrecords( pt ); */

/*     /\* record 0 holds a struct whose 5th byte is the record num of the first */
/*        dawg record. 1 and 2 hold tile data.  Let's assume 3 is the first dawg */
/*        record for now. *\/ */

/*     prect = prcgetrecord( pt, 1 ); */
/*     dctx->super.numFaces = prect->datalen; /\* one char per byte *\/ */
/*     dctx->super.faces = malloc( prect->datalen ); */
/*     memcpy( dctx->super.faces, prect->data, prect->datalen ); */
    
/*     dctx->super.counts = malloc( dctx->super.numFaces ); */
/*     dctx->super.values = malloc( dctx->super.numFaces ); */

/*     prect = prcgetrecord( pt, 2 ); */
/*     dataP = (unsigned short*)prect->data + 1;	/\* skip the xloc header *\/ */

/*     for ( ii = 0; ii < dctx->super.numFaces; ++ii ) { */
/*         unsigned short byt = *dataP++; */
/*         dctx->super.values[ii] = byt >> 8; */
/*         dctx->super.counts[ii] = byt & 0xFF; */
/*         if ( dctx->super.values[ii] == 0 ) { */
/*             dctx->super.counts[ii] = 4; /\* 4 blanks :-) *\/ */
/*         } */
/*     } */

/*     dctx->numStarts = nRecs - RECS_BEFORE_DAWG; */
/*     dctx->starts = XP_MALLOC( dctx->numStarts * sizeof(*dctx->starts) ); */

/*     for ( i = 0/\* , offset = 0 *\/; i < dctx->numStarts; ++i ) { */
/*         prect = prcgetrecord( pt, i + RECS_BEFORE_DAWG ); */
/*         dctx->starts[i].numNodes = prect->datalen / 3; */
/*         dctx->starts[i].array = (array_edge*)prect->data; */

/*         XP_ASSERT( (prect->datalen % 3) == 0 ); */
/*     } */
/* } /\* initFromDictFile *\/ */

void
linux_dictionary_destroy( DictionaryCtxt* dict )
{
    LinuxDictionaryCtxt* ctxt = (LinuxDictionaryCtxt*)dict;
    prcclose( ctxt->pt );
}

#endif /* CLIENT_ONLY */

