/* -*-mode: C; compile-command: "../../scripts/ndkbuild.sh"; -*- */

#include "andutils.h"

#include "comtypes.h"
#include "xwstream.h"

void
and_assert( const char* test, int line, const char* file, const char* func )
{
    XP_LOGF( "assertion \"%s\" failed: line %d in %s() in %s",
             test, line, file, func );
    __android_log_assert( test, "ASSERT", "line %d in %s() in %s",
                          line, file, func  );
}

#ifdef __LITTLE_ENDIAN
XP_U32
and_ntohl(XP_U32 ll)
{
    XP_U32 result = 0L;
    int ii;
    for ( ii = 0; ii < 4; ++ii ) {
        result <<= 8;
        result |= ll & 0x000000FF;
        ll >>= 8;
    }

    return result;
}

XP_U16
and_ntohs( XP_U16 ss )
{
    XP_U16 result;
    result = ss << 8;
    result |= ss >> 8;
    return result;
}

XP_U32
and_htonl( XP_U32 ll )
{
    return and_ntohl( ll );
}


XP_U16
and_htons( XP_U16 ss ) 
{
    return and_ntohs( ss );
}
#else
error error error
#endif

bool
getInt( JNIEnv* env, jobject obj, const char* name, int* result )
{
    bool success = false;
    jclass cls = (*env)->GetObjectClass( env, obj );
    jfieldID fid = (*env)->GetFieldID( env, cls, name, "I");
    XP_ASSERT( !!fid );
    *result = (*env)->GetIntField( env, obj, fid );
    success = true;
    (*env)->DeleteLocalRef( env, cls );
    return success;
}

bool
setInt( JNIEnv* env, jobject obj, const char* name, int value )
{
    bool success = false;
    jclass cls = (*env)->GetObjectClass( env, obj );
    jfieldID fid = (*env)->GetFieldID( env, cls, name, "I");
    XP_ASSERT( !!fid );
    (*env)->SetIntField( env, obj, fid, value );
    success = true;
    (*env)->DeleteLocalRef( env, cls );
    return success;
}

bool
setBool( JNIEnv* env, jobject obj, const char* name, bool value )
{
    bool success = false;
    jclass cls = (*env)->GetObjectClass( env, obj );
    jfieldID fid = (*env)->GetFieldID( env, cls, name, "Z");
    if ( 0 != fid ) {
        (*env)->SetBooleanField( env, obj, fid, value );
        success = true;
    }
    (*env)->DeleteLocalRef( env, cls );

    return success;
}

bool
setString( JNIEnv* env, jobject obj, const char* name, const XP_UCHAR* value )
{
    bool success = false;
    jclass cls = (*env)->GetObjectClass( env, obj );
    jfieldID fid = (*env)->GetFieldID( env, cls, name, "Ljava/lang/String;" );
    (*env)->DeleteLocalRef( env, cls );

    if ( 0 != fid ) {
        jstring str = (*env)->NewStringUTF( env, value );
        (*env)->SetObjectField( env, obj, fid, str );
        success = true;
        (*env)->DeleteLocalRef( env, str );
    }

    return success;
}

bool
getString( JNIEnv* env, jobject obj, const char* name, XP_UCHAR* buf,
           int bufLen )
{
    jclass cls = (*env)->GetObjectClass( env, obj );
    XP_ASSERT( !!cls );
    jfieldID fid = (*env)->GetFieldID( env, cls, name, "Ljava/lang/String;" );
    XP_ASSERT( !!fid );
    jstring jstr = (*env)->GetObjectField( env, obj, fid );
    jsize len = 0;
    if ( !!jstr ) {             /* might be null */
        len = (*env)->GetStringUTFLength( env, jstr );
        XP_ASSERT( len < bufLen );
        const char* chars = (*env)->GetStringUTFChars( env, jstr, NULL );
        XP_MEMCPY( buf, chars, len );
        (*env)->ReleaseStringUTFChars( env, jstr, chars );
        (*env)->DeleteLocalRef( env, jstr );
    }
    buf[len] = '\0';

    (*env)->DeleteLocalRef( env, cls );
}

bool
getObject( JNIEnv* env, jobject obj, const char* name, const char* sig,
           jobject* ret )
{
    jclass cls = (*env)->GetObjectClass( env, obj );
    XP_ASSERT( !!cls );
    jfieldID fid = (*env)->GetFieldID( env, cls, name, sig );
    XP_ASSERT( !!fid );         /* failed */
    *ret = (*env)->GetObjectField( env, obj, fid );
    XP_ASSERT( !!*ret );

    (*env)->DeleteLocalRef( env, cls );
    return true;
}

/* return false on failure, e.g. exception raised */
bool
getBool( JNIEnv* env, jobject obj, const char* name, XP_Bool* result )
{
    bool success = false;
    jclass cls = (*env)->GetObjectClass( env, obj );
    XP_ASSERT( !!cls );
    jfieldID fid = (*env)->GetFieldID( env, cls, name, "Z");
    XP_ASSERT( !!fid );
    if ( 0 != fid ) {
        *result = (*env)->GetBooleanField( env, obj, fid );
        success = true;
    }
    (*env)->DeleteLocalRef( env, cls );
    XP_ASSERT( success );
    return success;
}

jintArray
makeIntArray( JNIEnv *env, int siz, const jint* vals )
{
    jintArray array = (*env)->NewIntArray( env, siz );
    XP_ASSERT( !!array );
    if ( !!vals ) {
        jint* elems = (*env)->GetIntArrayElements( env, array, NULL );
        XP_ASSERT( !!elems );
        XP_MEMCPY( elems, vals, siz * sizeof(*elems) );
        (*env)->ReleaseIntArrayElements( env, array, elems, 0 );
    }
    return array;
}

jbyteArray
makeByteArray( JNIEnv *env, int siz, const jbyte* vals )
{
    jbyteArray array = (*env)->NewByteArray( env, siz );
    XP_ASSERT( !!array );
    if ( !!vals ) {
        jbyte* elems = (*env)->GetByteArrayElements( env, array, NULL );
        XP_ASSERT( !!elems );
        XP_MEMCPY( elems, vals, siz * sizeof(*elems) );
        (*env)->ReleaseByteArrayElements( env, array, elems, 0 );
    }
    return array;
}

jbooleanArray
makeBooleanArray( JNIEnv *env, int siz, const jboolean* vals )
{
    jbooleanArray array = (*env)->NewBooleanArray( env, siz );
    XP_ASSERT( !!array );
    if ( !!vals ) {
        jboolean* elems = (*env)->GetBooleanArrayElements( env, array, NULL );
        XP_ASSERT( !!elems );
        XP_MEMCPY( elems, vals, siz * sizeof(*elems) );
        (*env)->ReleaseBooleanArrayElements( env, array, elems, 0 );
    }
    return array;
}

int
getIntFromArray( JNIEnv* env, jintArray arr, bool del )
{
    jint* ints = (*env)->GetIntArrayElements(env, arr, 0);
    int result = ints[0];
    (*env)->ReleaseIntArrayElements( env, arr, ints, 0);
    if ( del ) {
        (*env)->DeleteLocalRef( env, arr );  
    }
    return result;
}

jobjectArray
makeStringArray( JNIEnv *env, int siz, const XP_UCHAR** vals )
{
    jclass clas = (*env)->FindClass(env, "java/lang/String");
    jstring empty = (*env)->NewStringUTF( env, "" );
    jobjectArray jarray = (*env)->NewObjectArray( env, siz, clas, empty );
    (*env)->DeleteLocalRef( env, clas );
    (*env)->DeleteLocalRef( env, empty );

    int ii;
    for ( ii = 0; ii < siz; ++ii ) {    
        jstring jstr = (*env)->NewStringUTF( env, vals[ii] );
        (*env)->SetObjectArrayElement( env, jarray, ii, jstr );
        (*env)->DeleteLocalRef( env, jstr );
    }

    return jarray;
}

jstring
streamToJString( MPFORMAL JNIEnv *env, XWStreamCtxt* stream )
{
    int len = stream_getSize( stream );
    XP_UCHAR* buf = XP_MALLOC( mpool, 1 + len );
    stream_getBytes( stream, buf, len );
    buf[len] = '\0';

    jstring jstr = (*env)->NewStringUTF( env, buf );

    XP_FREE( mpool, buf );
    return jstr;
}

jmethodID
getMethodID( JNIEnv* env, jobject obj, const char* proc, const char* sig )
{
    XP_ASSERT( !!env );
    jclass cls = (*env)->GetObjectClass( env, obj );
    XP_ASSERT( !!cls );
    jmethodID mid = (*env)->GetMethodID( env, cls, proc, sig );
    XP_ASSERT( !!mid );
    (*env)->DeleteLocalRef( env, cls );
    return mid;
}

jobjectArray
makeBitmapsArray( JNIEnv* env, const XP_Bitmaps* bitmaps )
{
    jobjectArray result = NULL;

    if ( !!bitmaps && bitmaps->nBitmaps > 0 ) {
        jclass clazz = (*env)->FindClass( env,
                                          "android/graphics/drawable/BitmapDrawable" );
        XP_ASSERT( !!clazz );
        result =  (*env)->NewObjectArray( env, bitmaps->nBitmaps, clazz, NULL );
        (*env)->DeleteLocalRef( env, clazz );

        int ii;
        for ( ii = 0; ii < bitmaps->nBitmaps; ++ii ) {
            (*env)->SetObjectArrayElement( env, result, ii, bitmaps->bmps[ii] );
        }
    }

    return result;
}
