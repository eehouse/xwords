/* -*-mode: C; compile-command: "../../scripts/ndkbuild.sh"; -*- */
/*
 * Copyright © 2009-2010 by Eric House (xwords@eehouse.org).  All
 * rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>

#include "anddict.h"
#include "xptypes.h"
#include "dictnry.h"
#include "dictnryp.h"
#include "strutils.h"
#include "andutils.h"
#include "utilwrapper.h"

typedef struct _AndDictionaryCtxt {
    DictionaryCtxt super;
    JNIUtilCtxt* jniutil;
    JNIEnv *env;
    XP_U8* bytes;
} AndDictionaryCtxt;

static void splitFaces_via_java( JNIEnv* env, AndDictionaryCtxt* ctxt, 
                                 const XP_U8* ptr, 
                                 int nFaceBytes, int nFaces, XP_Bool isUTF8 );

void
dict_splitFaces( DictionaryCtxt* dict, const XP_U8* bytes,
                 XP_U16 nBytes, XP_U16 nFaces )
{
    AndDictionaryCtxt* ctxt = (AndDictionaryCtxt*)dict;
    splitFaces_via_java( ctxt->env, ctxt, bytes, nBytes, nFaces, 
                         dict->isUTF8 );
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
andMakeBitmap( AndDictionaryCtxt* ctxt, XP_U8** ptrp )
{
    XP_U8* ptr = *ptrp;
    XP_U8 nCols = *ptr++;
    jobject bitmap = NULL;

    if ( nCols > 0 ) {
        XP_U8 nRows = *ptr++;
#ifdef DROP_BITMAPS
        ptr += ((nRows*nCols)+7) / 8;
#else
        XP_U8 srcByte = 0;
        XP_U8 nBits;
        XP_U16 ii;

        jboolean* colors = (jboolean*)XP_CALLOC( ctxt->super.mpool, 
                                                 nCols * nRows * sizeof(*colors) );
        jboolean* next = colors;

        nBits = nRows * nCols;
        for ( ii = 0; ii < nBits; ++ii ) {
            XP_U8 srcBitIndex = ii % 8;
            XP_U8 srcMask;

            if ( srcBitIndex == 0 ) {
                srcByte = *ptr++;
            }

            srcMask = 1 << (7 - srcBitIndex);
            XP_ASSERT( next < (colors + (nRows * nCols)) );
            *next++ = ((srcByte & srcMask) == 0) ? JNI_FALSE : JNI_TRUE;
        }

        JNIEnv* env = ctxt->env;
        bitmap = and_util_makeJBitmap( ctxt->jniutil, nCols, nRows, colors );
        (void)(*env)->NewGlobalRef( env, bitmap );
        (*env)->DeleteLocalRef( env, bitmap );
        XP_FREE( ctxt->super.mpool, colors );
#endif
    }

    *ptrp = ptr;
    return (XP_Bitmap)bitmap;
} /* andMakeBitmap */

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
            XP_ASSERT( *facep < nSpecials ); /* firing */
            texts[(int)*facep] = text;

            bitmaps[(int)*facep].largeBM = andMakeBitmap( ctxt, &ptr );
            bitmaps[(int)*facep].smallBM = andMakeBitmap( ctxt, &ptr );
        }
    }

    ctxt->super.chars = texts;
    ctxt->super.bitmaps = bitmaps;

    *ptrp = ptr;
} /* andLoadSpecialData */

/** Android doesn't include iconv for C code to use, so we'll have java do it.
 * Cons up a string with all the tile faces (skipping the specials to make
 * things easier) and have java return an array of strings.  Then load one at
 * a time into the expected null-separated format.
 */
static void
splitFaces_via_java( JNIEnv* env, AndDictionaryCtxt* ctxt, const XP_U8* ptr, 
                     int nFaceBytes, int nFaces, XP_Bool isUTF8 )
{
    XP_UCHAR facesBuf[nFaces*4]; /* seems a reasonable upper bound... */
    int indx = 0;
    int offsets[nFaces];
    int nBytes;
    int ii;

    jobject jstrarr = and_util_splitFaces( ctxt->jniutil, ptr, nFaceBytes,
                                           isUTF8 );
    XP_ASSERT( (*env)->GetArrayLength( env, jstrarr ) == nFaces );

    for ( ii = 0; ii < nFaces; ++ii ) {
        jobject jstr = (*env)->GetObjectArrayElement( env, jstrarr, ii );
        offsets[ii] = indx;
        nBytes = (*env)->GetStringUTFLength( env, jstr );

        const char* bytes = (*env)->GetStringUTFChars( env, jstr, NULL );
        char* end;
        long numval = strtol( bytes, &end, 10 );
        if ( end > bytes ) {
            XP_ASSERT( numval < 32 );
            nBytes = 1;
            facesBuf[indx] = (XP_UCHAR)numval;
        } else {
            XP_MEMCPY( &facesBuf[indx], bytes, nBytes );
        }
        (*env)->ReleaseStringUTFChars( env, jstr, bytes );
        (*env)->DeleteLocalRef( env, jstr );

        indx += nBytes;
        facesBuf[indx++] = '\0';
        XP_ASSERT( indx < VSIZE(facesBuf) );
    }
    (*env)->DeleteLocalRef( env, jstrarr );

    XP_UCHAR* faces = (XP_UCHAR*)XP_CALLOC( ctxt->super.mpool, indx );
    const XP_UCHAR** ptrs = (const XP_UCHAR**)
        XP_CALLOC( ctxt->super.mpool, nFaces * sizeof(ptrs[0]));

    XP_MEMCPY( faces, facesBuf, indx );
    for ( ii = 0; ii < nFaces; ++ii ) {
        ptrs[ii] = &faces[offsets[ii]];
    }

    XP_ASSERT( !ctxt->super.faces );
    ctxt->super.faces = faces;
    XP_ASSERT( !ctxt->super.facePtrs );
    ctxt->super.facePtrs = ptrs;
} /* splitFaces_via_java */

static void
parseDict( AndDictionaryCtxt* ctxt, XP_U8* ptr, XP_U32 dictLength )
{
    while( !!ptr ) {           /* lets us break.... */
        XP_U32 offset;
        XP_U16 nFaces, numFaceBytes = 0;
        XP_U16 i;
        XP_U16 flags;
        void* mappedBase = (void*)ptr;
        XP_U8 nodeSize;
        XP_Bool isUTF8 = XP_FALSE;

        flags = n_ptr_tohs( &ptr );

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

        if ( isUTF8 ) {
            numFaceBytes = (XP_U16)(*ptr++);
        }
        nFaces = (XP_U16)(*ptr++);
        if ( nFaces > 64 ) {
            break;
        }

        ctxt->super.nodeSize = nodeSize;

        if ( !isUTF8 ) {
            numFaceBytes = nFaces * 2;
        }

        ctxt->super.nFaces = (XP_U8)nFaces;
        ctxt->super.isUTF8 = isUTF8;

        if ( isUTF8 ) {
            splitFaces_via_java( ctxt->env, ctxt, ptr, numFaceBytes, nFaces,
                                 XP_TRUE );
            ptr += numFaceBytes;
        } else {
            XP_U8 tmp[nFaces*4]; /* should be enough... */
            XP_U16 nBytes = 0;
            XP_U16 ii;
            /* Need to translate from iso-8859-n to utf8 */
            for ( ii = 0; ii < nFaces; ++ii ) {
                XP_UCHAR ch = ptr[1];

                ptr += 2;

                tmp[nBytes] = ch;
                nBytes += 1;
            }
            XP_ASSERT( nFaces == nBytes );
            splitFaces_via_java( ctxt->env, ctxt, tmp, nBytes, nFaces, 
                                 XP_FALSE );
        }

        ctxt->super.is_4_byte = (ctxt->super.nodeSize == 4);

        ctxt->super.countsAndValues = 
            (XP_U8*)XP_MALLOC(ctxt->super.mpool, nFaces*2);

        ptr += 2;		/* skip xloc header */
        for ( i = 0; i < nFaces*2; i += 2 ) {
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

        break;              /* exit phony while loop */
    }
}

static void
and_dictionary_destroy( DictionaryCtxt* dict )
{
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
        JNIEnv* env = ctxt->env;
        for ( ii = 0; ii < nSpecials; ++ii ) {
            jobject bitmap = ctxt->super.bitmaps[ii].largeBM;
            if ( !!bitmap ) {
                (*env)->DeleteGlobalRef( env, bitmap );
            }
        }
        XP_FREE( ctxt->super.mpool, ctxt->super.bitmaps );
    }

    XP_FREE( ctxt->super.mpool, ctxt->super.faces );
    XP_FREE( ctxt->super.mpool, ctxt->super.facePtrs );
    XP_FREE( ctxt->super.mpool, ctxt->super.countsAndValues );
    XP_FREE( ctxt->super.mpool, ctxt->super.name );

    XP_FREE( ctxt->super.mpool, ctxt->bytes );
    XP_FREE( ctxt->super.mpool, ctxt );
}

jobject
and_dictionary_getChars( JNIEnv* env, DictionaryCtxt* dict )
{
    XP_ASSERT( env == ((AndDictionaryCtxt*)dict)->env );

    /* This is cheating: specials will be rep'd as 1,2, etc.  But as long as
       java code wants to ignore them anyway that's ok.  Otherwise need to
       use dict_tilesToString() */
    XP_U16 nFaces = dict_numTileFaces( dict );
    jobject jstrs = makeStringArray( env, nFaces, dict->facePtrs );
    return jstrs;
}

DictionaryCtxt* 
and_dictionary_make_empty( MPFORMAL JNIEnv* env, JNIUtilCtxt* jniutil )
{
    AndDictionaryCtxt* anddict
        = (AndDictionaryCtxt*)XP_CALLOC( mpool, sizeof( *anddict ) );
    anddict->env = env;
    anddict->jniutil = jniutil;
    dict_super_init( (DictionaryCtxt*)anddict );
    MPASSIGN( anddict->super.mpool, mpool );
    return (DictionaryCtxt*)anddict;
}

DictionaryCtxt* 
makeDict( MPFORMAL JNIEnv *env, JNIUtilCtxt* jniutil, jbyteArray jbytes,
          jstring jname )
{
    AndDictionaryCtxt* anddict = NULL;

    jsize len = (*env)->GetArrayLength( env, jbytes );

    XP_U8* localBytes = XP_MALLOC( mpool, len );
    jbyte* src = (*env)->GetByteArrayElements( env, jbytes, NULL );
    XP_MEMCPY( localBytes, src, len );
    (*env)->ReleaseByteArrayElements( env, jbytes, src, 0 );

    anddict = (AndDictionaryCtxt*)
        and_dictionary_make_empty( MPPARM(mpool) env, jniutil );
    anddict->super.destructor = and_dictionary_destroy;
    /* anddict->super.func_dict_getShortName = and_dict_getShortName; */

    anddict->bytes = localBytes;

    parseDict( anddict, localBytes, len );
    setBlankTile( &anddict->super );

    /* copy the name */
    len = 1 + (*env)->GetStringUTFLength( env, jname );
    const char* chars = (*env)->GetStringUTFChars( env, jname, NULL );
    anddict->super.name = XP_MALLOC( mpool, len );
    XP_MEMCPY( anddict->super.name, chars, len );
    (*env)->ReleaseStringUTFChars( env, jname, chars );
    
    return (DictionaryCtxt*)anddict;
}
