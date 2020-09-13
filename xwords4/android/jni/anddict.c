/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright © 2009 - 2016 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "anddict.h"
#include "xptypes.h"
#include "dictnry.h"
#include "dictnryp.h"
#include "strutils.h"
#include "andutils.h"
#include "utilwrapper.h"

typedef struct _AndDictionaryCtxt {
    DictionaryCtxt super;
#ifdef MAP_THREAD_TO_ENV
    EnvThreadInfo* ti;
#endif
    JNIUtilCtxt* jniutil;
    off_t bytesSize;
    jbyte* bytes;
    jbyteArray byteArray;
#ifdef DEBUG
    uint32_t dbgid;
#endif
} AndDictionaryCtxt;

static void splitFaces_via_java( JNIEnv* env, AndDictionaryCtxt* ctxt, 
                                 const XP_U8* ptr, 
                                 int nFaceBytes, int nFaces, XP_Bool isUTF8 );

void
dict_splitFaces( DictionaryCtxt* dict, XWEnv xwe, const XP_U8* bytes,
                 XP_U16 nBytes, XP_U16 nFaces )
{
    AndDictionaryCtxt* ctxt = (AndDictionaryCtxt*)dict;
    ASSERT_ENV( ctxt->ti, xwe );
    splitFaces_via_java( xwe, ctxt, bytes, nBytes, nFaces,
                         dict->isUTF8 );
}

static XP_U32
n_ptr_tohl( XP_U8 const** inp )
{
    XP_U32 t;
    XP_MEMCPY( &t, *inp, sizeof(t) );

    *inp += sizeof(t);

    return XP_NTOHL(t);
} /* n_ptr_tohl */

/** Android doesn't include iconv for C code to use, so we'll have java do it.
 * Cons up a string with all the tile faces (skipping the specials to make
 * things easier) and have java return an array of strings.  Then load one at
 * a time into the expected null-separated format.
 */
static void
splitFaces_via_java( JNIEnv* env, AndDictionaryCtxt* ctxt, const XP_U8* ptr, 
                     int nFaceBytes, int nFaces, XP_Bool isUTF8 )
{
    XP_UCHAR facesBuf[nFaces*16]; /* seems a reasonable upper bound... */
    int indx = 0;
    int offsets[nFaces];
    int nBytes;

    jobject jstrarr = and_util_splitFaces( ctxt->jniutil, env, ptr, nFaceBytes,
                                           isUTF8 );
    XP_ASSERT( (*env)->GetArrayLength( env, jstrarr ) == nFaces );

    for ( int ii = 0; ii < nFaces; ++ii ) {
        jobject jstrs = (*env)->GetObjectArrayElement( env, jstrarr, ii );
        offsets[ii] = indx;
        int nAlternates = (*env)->GetArrayLength( env, jstrs );
        for ( int jj = 0; jj < nAlternates; ++jj ) {
            jobject jstr = (*env)->GetObjectArrayElement( env, jstrs, jj );
            nBytes = (*env)->GetStringUTFLength( env, jstr );

            const char* bytes = (*env)->GetStringUTFChars( env, jstr, NULL );
            char* end;
            long numval = strtol( bytes, &end, 10 );
            if ( end > bytes ) {
                XP_ASSERT( numval < 32 );
                XP_ASSERT( jj == 0 );
                nBytes = 1;
                facesBuf[indx] = (XP_UCHAR)numval;
            } else {
                XP_MEMCPY( &facesBuf[indx], bytes, nBytes );
            }
            (*env)->ReleaseStringUTFChars( env, jstr, bytes );
            deleteLocalRef( env, jstr );
            indx += nBytes;
            facesBuf[indx++] = '\0';
        }

        deleteLocalRef( env, jstrs );
        XP_ASSERT( indx < VSIZE(facesBuf) );
    }
    deleteLocalRef( env, jstrarr );

    XP_UCHAR* faces = (XP_UCHAR*)XP_CALLOC( ctxt->super.mpool, indx );
    const XP_UCHAR** ptrs = (const XP_UCHAR**)
        XP_CALLOC( ctxt->super.mpool, nFaces * sizeof(ptrs[0]));

    XP_MEMCPY( faces, facesBuf, indx );
    for ( int ii = 0; ii < nFaces; ++ii ) {
        ptrs[ii] = &faces[offsets[ii]];
    }

    XP_ASSERT( !ctxt->super.faces );
    ctxt->super.faces = faces;
    ctxt->super.facesEnd = faces + indx;
    XP_ASSERT( !ctxt->super.facePtrs );
    ctxt->super.facePtrs = ptrs;
} /* splitFaces_via_java */

void
computeChecksum( DictionaryCtxt* dctx, XWEnv xwe, const XP_U8* ptr,
                 XP_U32 len, XP_UCHAR* out )
{
    AndDictionaryCtxt* ctxt = (AndDictionaryCtxt*)dctx;
    JNIEnv* env = xwe;
    jstring jsum = and_util_getMD5SumForDict( ctxt->jniutil, env,
                                              ctxt->super.name, ptr, len );
    const char* sum = (*env)->GetStringUTFChars( env, jsum, NULL );
    XP_MEMCPY( out, sum, 1 + XP_STRLEN(sum) );
    (*env)->ReleaseStringUTFChars( env, jsum, sum );
    deleteLocalRef( env, jsum );
}

static XP_Bool
parseDict( AndDictionaryCtxt* ctxt, XWEnv xwe, XP_U8 const* ptr,
           XP_U32 dictLength, XP_U32* numEdges )
{
    XP_Bool success = XP_TRUE;
    XP_ASSERT( !!ptr );
    ASSERT_ENV( ctxt->ti, xwe );
    const XP_U8* end = ptr + dictLength;
    XP_U32 offset;
    void* mappedBase = (void*)ptr;

    if ( !parseCommon( &ctxt->super, xwe, &ptr, end ) ) {
        goto error;
    }

    dictLength -= ptr - (XP_U8*)mappedBase;
    if ( dictLength >= sizeof(offset) ) {
        CHECK_PTR( ptr, sizeof(offset), end, error );
        offset = n_ptr_tohl( &ptr );
        dictLength -= sizeof(offset);
        XP_ASSERT( dictLength % ctxt->super.nodeSize == 0 );
        *numEdges = dictLength / ctxt->super.nodeSize;
#ifdef DEBUG
        ctxt->super.numEdges = *numEdges;
#endif
    } else {
        offset = 0;
    }

    if ( dictLength > 0 ) {
        ctxt->super.base = (array_edge*)ptr;
        ctxt->super.topEdge = ctxt->super.base 
            + (offset * ctxt->super.nodeSize);
    } else {
        XP_ASSERT( !ctxt->super.topEdge );
        XP_ASSERT( !ctxt->super.base );
        ctxt->super.topEdge = (array_edge*)NULL;
        ctxt->super.base = (array_edge*)NULL;
    }

    setBlankTile( &ctxt->super );

    goto done;
 error:
    success = XP_FALSE;
 done:
    return success;
} /* parseDict */

static void
and_dictionary_destroy( DictionaryCtxt* dict, XWEnv xwe )
{
    AndDictionaryCtxt* ctxt = (AndDictionaryCtxt*)dict;
    ASSERT_ENV( ctxt->ti, xwe );
    XP_LOGF( "%s(dict=%p); code=%x", __func__, ctxt, ctxt->dbgid );
    XP_U16 nSpecials = countSpecials( &ctxt->super );
    JNIEnv* env = xwe;

    if ( !!ctxt->super.chars ) {
        for ( int ii = 0; ii < nSpecials; ++ii ) {
            XP_UCHAR* text = ctxt->super.chars[ii];
            if ( !!text ) {
                XP_FREE( ctxt->super.mpool, text );
            }
        }
        XP_FREE( ctxt->super.mpool, ctxt->super.chars );
    }
    XP_FREEP( ctxt->super.mpool, &ctxt->super.charEnds );

    if ( !!ctxt->super.bitmaps ) {
        for ( int ii = 0; ii < nSpecials; ++ii ) {
            jobject bitmap = ctxt->super.bitmaps[ii].largeBM;
            if ( !!bitmap ) {
                (*env)->DeleteGlobalRef( env, bitmap );
            }
        }
        XP_FREE( ctxt->super.mpool, ctxt->super.bitmaps );
    }

    XP_FREEP( ctxt->super.mpool, &ctxt->super.md5Sum );
    XP_FREEP( ctxt->super.mpool, &ctxt->super.desc );
    XP_FREEP( ctxt->super.mpool, &ctxt->super.faces );
    XP_FREEP( ctxt->super.mpool, &ctxt->super.facePtrs );
    XP_FREEP( ctxt->super.mpool, &ctxt->super.countsAndValues );
    XP_FREEP( ctxt->super.mpool, &ctxt->super.name );
    XP_FREEP( ctxt->super.mpool, &ctxt->super.langName );

    if ( NULL == ctxt->byteArray ) { /* mmap case */
#ifdef DEBUG
        int err = 
#endif
            munmap( ctxt->bytes, ctxt->bytesSize );
        XP_ASSERT( 0 == err );
    } else {
        (*env)->ReleaseByteArrayElements( env, ctxt->byteArray, ctxt->bytes, 0 );
        (*env)->DeleteGlobalRef( env, ctxt->byteArray );
    }
    XP_FREE( ctxt->super.mpool, ctxt );
} /* and_dictionary_destroy */

jobject
and_dictionary_getChars( JNIEnv* env, DictionaryCtxt* dict )
{
    /* XP_ASSERT( env == ((AndDictionaryCtxt*)dict)->env ); */
    /* The above is failing now that dictmgr reuses dicts across threads.  I
       think that's ok, that I didn't have a good reason for having this
       assert.  But bears watching... */

    /* This is cheating: specials will be rep'd as 1,2, etc.  But as long as
       java code wants to ignore them anyway that's ok.  Otherwise need to
       use dict_tilesToString() */
    XP_U16 nFaces = dict_numTileFaces( dict );
    jobject jstrs = makeStringArray( env, nFaces, dict->facePtrs );
    return jstrs;
}

DictionaryCtxt* 
and_dictionary_make_empty( MPFORMAL JNIUtilCtxt* jniutil )
{
    AndDictionaryCtxt* anddict
        = (AndDictionaryCtxt*)XP_CALLOC( mpool, sizeof( *anddict ) );
    anddict->jniutil = jniutil;
#ifdef DEBUG
    anddict->dbgid = rand();
#endif
    dict_super_init( (DictionaryCtxt*)anddict );
    MPASSIGN( anddict->super.mpool, mpool );

    LOG_RETURNF( "%p", anddict );
    return (DictionaryCtxt*)anddict;
}

void
makeDicts( MPFORMAL JNIEnv* env,
#ifdef MAP_THREAD_TO_ENV
           EnvThreadInfo* ti,
#endif
           DictMgrCtxt* dictMgr, JNIUtilCtxt* jniutil,
           DictionaryCtxt** dictp, PlayerDicts* dicts,
           jobjectArray jnames, jobjectArray jdicts, jobjectArray jpaths,
           jstring jlang )
{
    jsize len = (*env)->GetArrayLength( env, jdicts );
    XP_ASSERT( len == (*env)->GetArrayLength( env, jnames ) );

    for ( int ii = 0; ii <= VSIZE(dicts->dicts); ++ii ) {
        DictionaryCtxt* dict = NULL;
        if ( ii < len ) {
            jobject jdict = (*env)->GetObjectArrayElement( env, jdicts, ii );
            jstring jpath = jpaths == NULL ? 
                    NULL : (*env)->GetObjectArrayElement( env, jpaths, ii );
            if ( NULL != jdict || NULL != jpath ) { 
                jstring jname = (*env)->GetObjectArrayElement( env, jnames, ii );
                dict = makeDict( MPPARM(mpool) env, TI_IF(ti) dictMgr, jniutil,
                                 jname, jdict, jpath, jlang, false );
                XP_ASSERT( !!dict );
                deleteLocalRefs( env, jdict, jname, DELETE_NO_REF );
            }
            deleteLocalRef( env, jpath );
        }
        if ( 0 == ii ) {
            *dictp = dict;
        } else {
            XP_ASSERT( ii-1 < VSIZE( dicts->dicts ) );
            dicts->dicts[ii-1] = dict;
        }
    }
}

DictionaryCtxt* 
makeDict( MPFORMAL JNIEnv* env,
#ifdef MAP_THREAD_TO_ENV
          EnvThreadInfo* ti,
#endif
          DictMgrCtxt* dictMgr, JNIUtilCtxt* jniutil, jstring jname,
          jbyteArray jbytes, jstring jpath, jstring jlangname, jboolean check )
{
    jbyte* bytes = NULL;
    jbyteArray byteArray = NULL;
    off_t bytesSize = 0;

    const char* name = (*env)->GetStringUTFChars( env, jname, NULL );
    /* remember: dmgr_get calls dict_ref() */
    AndDictionaryCtxt* anddict = (AndDictionaryCtxt*)dmgr_get( dictMgr,
                                                               env, name );

    if ( NULL == anddict ) {
        if ( NULL == jpath ) {
            bytesSize = (*env)->GetArrayLength( env, jbytes );
            byteArray = (*env)->NewGlobalRef( env, jbytes );
            bytes = (*env)->GetByteArrayElements( env, byteArray, NULL );
        } else {
            const char* path = (*env)->GetStringUTFChars( env, jpath, NULL );

            struct stat statbuf;
            if ( 0 == stat( path, &statbuf ) && 0 < statbuf.st_size ) {
                int fd = open( path, O_RDONLY );
                if ( fd >= 0 ) {
                    void* ptr = mmap( NULL, statbuf.st_size, PROT_READ, 
                                      MAP_PRIVATE, fd, 0 );
                    close( fd );
                    if ( MAP_FAILED != ptr ) {
                        bytes = ptr;
                        bytesSize = statbuf.st_size;
                    }
                }
            }
            (*env)->ReleaseStringUTFChars( env, jpath, path );
        }

        if ( NULL != bytes ) {
            anddict = (AndDictionaryCtxt*)
                and_dictionary_make_empty( MPPARM(mpool) jniutil );
#ifdef MAP_THREAD_TO_ENV
            anddict->ti = ti;
#endif
            anddict->bytes = bytes;
            anddict->byteArray = byteArray;
            anddict->bytesSize = bytesSize;

            anddict->super.destructor = and_dictionary_destroy;

            /* copy the name */
            anddict->super.name = copyString( mpool, name );
            XP_LOGF( "%s(dict=%p); code=%x; name=%s", __func__, anddict, 
                     anddict->dbgid, anddict->super.name );
            anddict->super.langName = getStringCopy( MPPARM(mpool) 
                                                     env, jlangname );

            XP_U32 numEdges = 0;
            XP_Bool parses = parseDict( anddict, env, (XP_U8*)anddict->bytes,
                                        bytesSize, &numEdges );
            if ( !parses || (check && !checkSanity( &anddict->super, 
                                                    numEdges ) ) ) {
                and_dictionary_destroy( (DictionaryCtxt*)anddict, env );
                anddict = NULL;
            }
        }
        dmgr_put( dictMgr, env, name, &anddict->super );
        dict_ref( &anddict->super, env );
    }
    
    (*env)->ReleaseStringUTFChars( env, jname, name );

    return &anddict->super;
} /* makeDict */

#ifdef DEBUG
uint32_t
andDictID( const DictionaryCtxt* dict )
{
    const AndDictionaryCtxt* ctxt = (AndDictionaryCtxt*)dict;
    return ctxt->dbgid;
}
#endif
