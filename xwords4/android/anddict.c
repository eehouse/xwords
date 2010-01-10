
#include <jni.h>
#include <stdio.h>
#include <stdlib.h>

#include "anddict.h"
#include "xptypes.h"
#include "dictnry.h"
#include "strutils.h"

typedef struct _AndDictionaryCtxt {
    DictionaryCtxt super;
    JNIEnv *env;
    XP_U8* bytes;
} AndDictionaryCtxt;

void
dict_splitFaces( DictionaryCtxt* dict, const XP_U8* bytes,
                 XP_U16 nBytes, XP_U16 nFaces )
{
    LOG_FUNC();

    XP_UCHAR* faces = (XP_UCHAR*)XP_CALLOC( dict->mpool, nBytes + nFaces );
    XP_UCHAR** ptrs = (XP_UCHAR**)XP_CALLOC( dict->mpool, nFaces * sizeof(ptrs[0]));
    XP_U16 ii;
    XP_UCHAR* next = faces;

    /* now split; this will not work for utf8!!! */
    for ( ii = 0; ii < nFaces; ++ii ) {
        ptrs[ii] = next;
        *next++ = *bytes++;
        *next++ = 0;
    }

    XP_ASSERT( next == faces + nFaces + nBytes );
    XP_ASSERT( !dict->faces );
    dict->faces = faces;
    XP_ASSERT( !dict->facePtrs );
    dict->facePtrs = ptrs;
}

static XP_U32
n_ptr_tohl( XP_U8** inp )
{
    XP_U32 t;
    XP_MEMCPY( &t, *inp, sizeof(t) );

    *inp += sizeof(t);

    return XP_NTOHL(t);
} /* n_ptr_tohl */

static XP_U16
n_ptr_tohs( XP_U8** inp )
{
    XP_U16 t;
    XP_MEMCPY( &t, *inp, sizeof(t) );

    *inp += sizeof(t);

    return XP_NTOHS(t);
} /* n_ptr_tohs */

static XP_U16
andCountSpecials( AndDictionaryCtxt* ctxt )
{
    XP_U16 result = 0;
    XP_U16 ii;

    for ( ii = 0; ii < ctxt->super.nFaces; ++ii ) {
        if ( IS_SPECIAL( ctxt->super.facePtrs[ii][0] ) ) {
            ++result;
        }
    }

    return result;
} /* andCountSpecials */

static XP_Bitmap
andMakeBitmap( XP_U8** ptrp )
{
    XP_U8* ptr = *ptrp;
    XP_U8 nCols = *ptr++;
    XP_Bitmap bitmap = NULL;
    /* CEBitmapInfo* bitmap = (CEBitmapInfo*)NULL; */

    if ( nCols > 0 ) {
        XP_ASSERT(0);
#if 0
        XP_U8* dest;
        XP_U8* savedDest;
        XP_U8 nRows = *ptr++;
        XP_U16 rowBytes = (nCols+7) / 8;
        XP_U8 srcByte = 0;
        XP_U8 destByte = 0;
        XP_U8 nBits;
        XP_U16 i;

        bitmap = (CEBitmapInfo*)XP_CALLOC( ctxt->super.mpool, 
                                           sizeof(bitmap) );
        bitmap->nCols = nCols;
        bitmap->nRows = nRows;
        dest = XP_MALLOC( ctxt->super.mpool, rowBytes * nRows );
        bitmap->bits = savedDest = dest;

        nBits = nRows * nCols;
        for ( i = 0; i < nBits; ++i ) {
            XP_U8 srcBitIndex = i % 8;
            XP_U8 destBitIndex = (i % nCols) % 8;
            XP_U8 srcMask, bit;

            if ( srcBitIndex == 0 ) {
                srcByte = *ptr++;
            }

            srcMask = 1 << (7 - srcBitIndex);
            bit = (srcByte & srcMask) != 0;
            destByte |= bit << (7 - destBitIndex);

            /* we need to put the byte if we've filled it or if we're done
               with the row */
            if ( (destBitIndex==7) || ((i%nCols) == (nCols-1)) ) {
                *dest++ = destByte;
                destByte = 0;
            }
        }

/*         printBitmapData1( nCols, nRows, savedDest ); */
/*         printBitmapData2( nCols, nRows, savedDest ); */
#endif
    }

    *ptrp = ptr;
    return (XP_Bitmap)bitmap;
} /* andMakeBitmap */

static void
andDeleteBitmap( const AndDictionaryCtxt* XP_UNUSED_DBG(ctxt),
                 XP_Bitmap* bitmap )
{
    if ( !!bitmap ) {
        XP_ASSERT(0);
        /* CEBitmapInfo* bmi = (CEBitmapInfo*)bitmap; */
        /* XP_FREE( ctxt->super.mpool, bmi->bits ); */
        /* XP_FREE( ctxt->super.mpool, bmi ); */
    }
}

static void
andLoadSpecialData( AndDictionaryCtxt* ctxt, XP_U8** ptrp )
{
    XP_U16 nSpecials = andCountSpecials( ctxt );
    XP_U8* ptr = *ptrp;
    Tile ii;
    XP_UCHAR** texts;
    SpecialBitmaps* bitmaps;

    texts = (XP_UCHAR**)XP_MALLOC( ctxt->super.mpool, 
                                   nSpecials * sizeof(*texts) );
    bitmaps = (SpecialBitmaps*)
        XP_CALLOC( ctxt->super.mpool, nSpecials * sizeof(*bitmaps) );

    for ( ii = 0; ii < ctxt->super.nFaces; ++ii ) {
	
        const XP_UCHAR* facep = ctxt->super.facePtrs[(short)ii];
        if ( IS_SPECIAL(*facep) ) {
            /* get the string */
            XP_U8 txtlen = *ptr++;
            XP_UCHAR* text = (XP_UCHAR*)XP_MALLOC(ctxt->super.mpool, txtlen+1);
            XP_MEMCPY( text, ptr, txtlen );
            ptr += txtlen;
            text[txtlen] = '\0';
            XP_ASSERT( *facep < nSpecials );
            texts[(int)*facep] = text;

            bitmaps[(int)*facep].largeBM = andMakeBitmap( &ptr );
            bitmaps[(int)*facep].smallBM = andMakeBitmap( &ptr );
        }
    }

    ctxt->super.chars = texts;
    ctxt->super.bitmaps = bitmaps;

    *ptrp = ptr;
} /* andLoadSpecialData */

static void
parseDict( AndDictionaryCtxt* ctxt, XP_U8* ptr, XP_U32 dictLength )
{
    while( !!ptr ) {           /* lets us break.... */
        XP_U32 offset;
        XP_U16 numFaces, numFaceBytes = 0;
        XP_U16 i;
        XP_U16 flags;
        void* mappedBase = (void*)ptr;
        XP_U8 nodeSize;
        XP_Bool isUTF8 = XP_FALSE;

        flags = n_ptr_tohs( &ptr );

#ifdef NODE_CAN_4
        if ( flags == 0x0002 ) {
            nodeSize = 3;
        } else if ( flags == 0x0003 ) {
            nodeSize = 4;
        } else if ( flags == 0x0004 ) {
            isUTF8 = XP_TRUE;
            nodeSize = 3;
        } else if ( flags == 0x0005 ) {
            isUTF8 = XP_TRUE;
            nodeSize = 4;
        } else {
            break;          /* we want to return NULL */
        }
#else
        if( flags != 0x0001 ) {
            break;
        }
#endif
        if ( isUTF8 ) {
            numFaceBytes = (XP_U16)(*ptr++);
        }
        numFaces = (XP_U16)(*ptr++);
        if ( numFaces > 64 ) {
            break;
        }

        ctxt->super.nodeSize = nodeSize;

        if ( !isUTF8 ) {
            numFaceBytes = numFaces * 2;
        }

        ctxt->super.nFaces = (XP_U8)numFaces;
        ctxt->super.isUTF8 = isUTF8;

        if ( isUTF8 ) {
            XP_ASSERT(0);
            dict_splitFaces( &ctxt->super, ptr, numFaceBytes, numFaces );
            ptr += numFaceBytes;
        } else {
            XP_U8 tmp[numFaces*4]; /* should be enough... */
            XP_U16 nBytes = 0;
            XP_U16 ii;
            /* Need to translate from iso-8859-n to utf8 */
            for ( ii = 0; ii < numFaces; ++ii ) {
                XP_UCHAR ch = ptr[1];

                ptr += 2;

                tmp[nBytes] = ch;
                nBytes += 1;
            }
            dict_splitFaces( &ctxt->super, tmp, nBytes, numFaces );
        }

        ctxt->super.is_4_byte = (ctxt->super.nodeSize == 4);

        ctxt->super.countsAndValues = 
            (XP_U8*)XP_MALLOC(ctxt->super.mpool, numFaces*2);

        ptr += 2;		/* skip xloc header */
        for ( i = 0; i < numFaces*2; i += 2 ) {
            ctxt->super.countsAndValues[i] = *ptr++;
            ctxt->super.countsAndValues[i+1] = *ptr++;
        }

        andLoadSpecialData( ctxt, &ptr );

        dictLength -= ptr - (XP_U8*)mappedBase;
        if ( dictLength > sizeof(XP_U32) ) {
            offset = n_ptr_tohl( &ptr );
            dictLength -= sizeof(offset);
#ifdef NODE_CAN_4
            XP_ASSERT( dictLength % ctxt->super.nodeSize == 0 );
# ifdef DEBUG
            ctxt->super.numEdges = dictLength / ctxt->super.nodeSize;
# endif
#else
            XP_ASSERT( dictLength % 3 == 0 );
# ifdef DEBUG
            ctxt->super.numEdges = dictLength / 3;
# endif
#endif
        } else {
            offset = 0;
        }

        if ( dictLength > 0 ) {
            ctxt->super.base = (array_edge*)ptr;
#ifdef NODE_CAN_4
            ctxt->super.topEdge = ctxt->super.base 
                + (offset * ctxt->super.nodeSize);
#else
            ctxt->super.topEdge = ctxt->super.base + (offset * 3);
#endif
        } else {
            ctxt->super.topEdge = (array_edge*)NULL;
            ctxt->super.base = (array_edge*)NULL;
        }

        setBlankTile( &ctxt->super );

        ctxt->super.name = copyString(ctxt->super.mpool, "no name dict" );
        break;              /* exit phony while loop */
    }
}

static void
and_dictionary_destroy( DictionaryCtxt* dict )
{
    LOG_FUNC();
    AndDictionaryCtxt* ctxt = (AndDictionaryCtxt*)dict;
    XP_U16 nSpecials = andCountSpecials( ctxt );
    XP_U16 ii;

    if ( !!ctxt->super.chars ) {
        for ( ii = 0; ii < nSpecials; ++ii ) {
            XP_UCHAR* text = ctxt->super.chars[ii];
            if ( !!text ) {
                XP_FREE( ctxt->super.mpool, text );
            }
        }
        XP_FREE( ctxt->super.mpool, ctxt->super.chars );
    }
    if ( !!ctxt->super.bitmaps ) {
        for ( ii = 0; ii < nSpecials; ++ii ) {
            XP_ASSERT( !ctxt->super.bitmaps[ii].largeBM );
            XP_ASSERT( !ctxt->super.bitmaps[ii].smallBM );
             andDeleteBitmap( ctxt, ctxt->super.bitmaps[ii].largeBM );
             andDeleteBitmap( ctxt, ctxt->super.bitmaps[ii].smallBM );
        }
        XP_FREE( ctxt->super.mpool, ctxt->super.bitmaps );
    }

    XP_FREE( ctxt->super.mpool, ctxt->super.faces );
    XP_FREE( ctxt->super.mpool, ctxt->super.facePtrs );
    XP_FREE( ctxt->super.mpool, ctxt->super.countsAndValues );
    XP_FREE( ctxt->super.mpool, ctxt->super.name );

    XP_FREE( ctxt->super.mpool, ctxt->bytes );
    XP_FREE( ctxt->super.mpool, ctxt );

    LOG_RETURN_VOID();
}

DictionaryCtxt* 
makeDict( MPFORMAL JNIEnv *env, jbyteArray jbytes )
{
    XP_Bool formatOk = XP_TRUE;
    XP_Bool isUTF8 = XP_FALSE;
    XP_U16 charSize;
    AndDictionaryCtxt* anddict = NULL;

    jsize len = (*env)->GetArrayLength( env, jbytes );
    XP_LOGF( "%s: got %d bytes", __func__, len );

    XP_U8* localBytes = XP_MALLOC( mpool, len );
    jbyte* src = (*env)->GetByteArrayElements( env, jbytes, NULL );
    XP_MEMCPY( localBytes, src, len );
    (*env)->ReleaseByteArrayElements( env, jbytes, src, 0 );

    anddict = (AndDictionaryCtxt*)XP_CALLOC( mpool, sizeof( *anddict ) );
    dict_super_init( (DictionaryCtxt*)anddict );
    anddict->super.destructor = and_dictionary_destroy;
    /* anddict->super.func_dict_getShortName = and_dict_getShortName; */

    MPASSIGN(anddict->super.mpool, mpool);
    anddict->bytes = localBytes;
    anddict->env = env;
    
    parseDict( anddict, localBytes, len );
    setBlankTile( &anddict->super );

 err:
    return (DictionaryCtxt*)anddict;
}
