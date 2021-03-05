/* -*- compile-command: "cd ../wasm && make MEMDEBUG=TRUE install -j3"; -*- */
/*
 * Copyright 2021 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>

#include "wasmdict.h"
#include "dictnryp.h"
#include "strutils.h"
#include "dictmgr.h"

typedef struct _WasmDictionaryCtxt {
    DictionaryCtxt super;
    Globals* globals;
    size_t dictLength;
    XP_U8* dictBase;
    XP_Bool useMMap;
} WasmDictionaryCtxt;

static const XP_UCHAR*
getShortName( const DictionaryCtxt* dict )
{
    const XP_UCHAR* full = dict_getName( dict );
    const XP_UCHAR* ch = strchr( full, '/' );
    if ( !!ch ) {
        ++ch;
    } else {
        ch = full;
    }
    return ch;
}

static XP_Bool
initFromPtr( WasmDictionaryCtxt* dctx, const char* name,
             uint8_t* dictBase, size_t len )
{
    XP_Bool formatOk = XP_TRUE;
    size_t dictLength;
    XP_U32 topOffset;
    char path[256];

    dctx->dictLength = len;
    dctx->dictBase = dictBase;

    const XP_U8* ptr = dctx->dictBase;
    const XP_U8* end = ptr + dctx->dictLength;
    formatOk = parseCommon( &dctx->super, NULL, &ptr, end );
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

        dctx->super.name = copyString( dctx->super.mpool, name );

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
freeSpecials( WasmDictionaryCtxt* ctxt )
{
    XP_U16 nSpecials = 0;

    for ( XP_U16 ii = 0; ii < ctxt->super.nFaces; ++ii ) {
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
wasm_dictionary_destroy( DictionaryCtxt* dict, XWEnv xwe )
{
    WasmDictionaryCtxt* ctxt = (WasmDictionaryCtxt*)dict;

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
}

DictionaryCtxt* 
wasm_dictionary_make( Globals* globals, XWEnv xwe,
                      const char* name, uint8_t* base, size_t len )
{
    WasmDictionaryCtxt* result = (WasmDictionaryCtxt*)
        XP_CALLOC(globals->mpool, sizeof(*result));
    result->globals = globals;

    dict_super_init( MPPARM(globals->mpool) &result->super );
    result->super.destructor = wasm_dictionary_destroy;

    result->useMMap = false;

    XP_Bool success = initFromPtr( result, name, base, len );
    if ( success ) {
        result->super.func_dict_getShortName = getShortName;
        setBlankTile( &result->super );
    } else {
        XP_ASSERT( 0 ); /* gonna crash anyway */
        XP_FREE( globals->mpool, result );
        result = NULL;
    }
    (void)dict_ref( &result->super, xwe );

    LOG_RETURNF( "%p", &result->super );
    return &result->super;
}

void
dict_splitFaces( DictionaryCtxt* dict, XWEnv xwe, const XP_U8* utf8,
                 XP_U16 nBytes, XP_U16 nFaces )
{
    XP_UCHAR* faces = XP_MALLOC( dict->mpool, nBytes + nFaces );
    const XP_UCHAR** ptrs = XP_MALLOC( dict->mpool, nFaces * sizeof(ptrs[0]));
    XP_U16 ii;
    XP_Bool isUTF8 = dict->isUTF8;
    XP_UCHAR* next = faces;
    const XP_U8* bytesIn = utf8;
    const XP_U8* bytesEnd = bytesIn + nBytes;

    for ( ii = 0; ii < nFaces; ++ii ) {
        ptrs[ii] = next;
        if ( isUTF8 ) {
            for ( ; ; ) {
                const XP_U8* cp = bytesIn + 1; // g_utf8_offset_to_pointer( bytesIn, 1 );
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

    /* for ( int ii = 0; ii < nFaces; ++ii ) { */
    /*     XP_LOGFF( "face %d: %s", ii, dict->facePtrs[ii] ); */
    /* } */
} /* dict_splitFaces */

void
computeChecksum( DictionaryCtxt* dctx, XWEnv xwe, const XP_U8* ptr,
                 XP_U32 len, XP_UCHAR* out )
{
    *out = '\0';
}
