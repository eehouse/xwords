#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>

#include "wasmdict.h"
#include "strutils.h"

#define DICTNAME "assets_dir/CollegeEng_2to8.xwd"

typedef struct _WasmDictionaryCtxt {
    DictionaryCtxt super;
    size_t dictLength;
    XP_U8* dictBase;
    XP_Bool useMMap;
} WasmDictionaryCtxt;

static XP_Bool
initFromDictFile( WasmDictionaryCtxt* dctx, const char* path )
{
    LOG_FUNC();
    XP_Bool formatOk = XP_TRUE;
    XP_U32 topOffset;

    struct stat statbuf;
    int err = stat( DICTNAME, &statbuf );
    XP_LOGFF( "stat(%s) => %d; size: %d", DICTNAME, err, statbuf.st_size );
    
    if ( 0 == err && 0 != statbuf.st_size ) {
        /* do nothing */
    } else {
        XP_LOGF( "%s: path=%s", __func__, path );
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
    formatOk = parseCommon( &dctx->super, NULL, &ptr, end );
        /* && loadSpecialData( &dctx->super, &ptr, end ); */

    if ( formatOk ) {
        size_t curPos = ptr - dctx->dictBase;
        size_t dictLength = dctx->dictLength - curPos;

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

        dctx->super.name = copyString( dctx->super.mpool, path );

        if ( ! checkSanity( &dctx->super, numEdges ) ) {
            goto closeAndExit;
        }
    }
    goto ok;

 closeAndExit:
    formatOk = XP_FALSE;
 ok:

    LOG_RETURNF( "%d", formatOk );
    return formatOk;
} /* initFromDictFile */

void
dict_splitFaces( DictionaryCtxt* dict, XWEnv XP_UNUSED(xwe), const XP_U8* utf8,
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

DictionaryCtxt*
wasm_load_dict( MPFORMAL_NOCOMMA )
{
    LOG_FUNC();

    WasmDictionaryCtxt* wdctxt = XP_MALLOC( mpool, sizeof(*wdctxt) );
    dict_super_init( mpool, &wdctxt->super );
    wdctxt->useMMap = XP_TRUE;

    initFromDictFile( wdctxt, DICTNAME );
    
    return &wdctxt->super;
}
