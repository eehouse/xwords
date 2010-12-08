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

int
getInt( JNIEnv* env, jobject obj, const char* name )
{
    jclass cls = (*env)->GetObjectClass( env, obj );
    XP_ASSERT( !!cls );
    jfieldID fid = (*env)->GetFieldID( env, cls, name, "I");
    XP_ASSERT( !!fid );
    int result = (*env)->GetIntField( env, obj, fid );
    (*env)->DeleteLocalRef( env, cls );
    return result;
}

void
setInt( JNIEnv* env, jobject obj, const char* name, int value )
{
    jclass cls = (*env)->GetObjectClass( env, obj );
    XP_ASSERT( !!cls );
    jfieldID fid = (*env)->GetFieldID( env, cls, name, "I");
    XP_ASSERT( !!fid );
    (*env)->SetIntField( env, obj, fid, value );
    (*env)->DeleteLocalRef( env, cls );
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

void
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
    XP_ASSERT( !!fid );
    *ret = (*env)->GetObjectField( env, obj, fid );
    XP_ASSERT( !!*ret );

    (*env)->DeleteLocalRef( env, cls );
    return true;
}

void
setObject( JNIEnv* env, jobject obj, const char* name, const char* sig,
           jobject val )
{
    jclass cls = (*env)->GetObjectClass( env, obj );
    XP_ASSERT( !!cls );
    jfieldID fid = (*env)->GetFieldID( env, cls, name, sig );
    XP_ASSERT( !!fid );
    (*env)->SetObjectField( env, obj, fid, val );

    (*env)->DeleteLocalRef( env, cls );
}

bool
getBool( JNIEnv* env, jobject obj, const char* name )
{
    bool result;
    jclass cls = (*env)->GetObjectClass( env, obj );
    XP_ASSERT( !!cls );
    jfieldID fid = (*env)->GetFieldID( env, cls, name, "Z");
    XP_ASSERT( !!fid );
    result = (*env)->GetBooleanField( env, obj, fid );
    (*env)->DeleteLocalRef( env, cls );
    return result;
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

void
setBoolArray( JNIEnv* env, jbooleanArray jarr, int count, 
              const jboolean* vals )
{
    jboolean* elems = (*env)->GetBooleanArrayElements( env, jarr, NULL );
    XP_ASSERT( !!elems );
    XP_MEMCPY( elems, vals, count * sizeof(*elems) );
    (*env)->ReleaseBooleanArrayElements( env, jarr, elems, 0 );
} 

jbooleanArray
makeBooleanArray( JNIEnv *env, int siz, const jboolean* vals )
{
    jbooleanArray array = (*env)->NewBooleanArray( env, siz );
    XP_ASSERT( !!array );
    if ( !!vals ) {
        setBoolArray( env, array, siz, vals );
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

void
setJAddrRec( JNIEnv* env, jobject jaddr, const CommsAddrRec* addr )
{
    XP_ASSERT( !!addr );
    intToJenumField( env, jaddr, addr->conType, "conType",
                     "org/eehouse/android/xw4/jni/CommsAddrRec$CommsConnType" );

    switch ( addr->conType ) {
    case COMMS_CONN_NONE:
        break;
    case COMMS_CONN_RELAY:
        setInt( env, jaddr, "ip_relay_port", addr->u.ip_relay.port );
        setString( env, jaddr, "ip_relay_hostName", addr->u.ip_relay.hostName );
        setString( env, jaddr, "ip_relay_invite", addr->u.ip_relay.invite );
        setBool( env, jaddr, "ip_relay_seeksPublicRoom",
                 addr->u.ip_relay.seeksPublicRoom );
        setBool( env, jaddr, "ip_relay_advertiseRoom",
                 addr->u.ip_relay.advertiseRoom );
        break;
    case COMMS_CONN_SMS:
        setString( env, jaddr, "sms_phone", addr->u.sms.phone );
        XP_LOGF( "%s: got SMS; phone=%s", __func__, addr->u.sms.phone );
        setInt( env, jaddr, "sms_port", addr->u.sms.port );
        break;
    case COMMS_CONN_BT:
    default:
        XP_ASSERT(0);
    }
}

void
getJAddrRec( JNIEnv* env, CommsAddrRec* addr, jobject jaddr )
{
    addr->conType = jenumFieldToInt( env, jaddr, "conType",
                                     "org/eehouse/android/xw4/jni/"
                                     "CommsAddrRec$CommsConnType" );

    switch ( addr->conType ) {
    case COMMS_CONN_NONE:
        break;
    case COMMS_CONN_RELAY:
        addr->u.ip_relay.port = getInt( env, jaddr, "ip_relay_port" );
        getString( env, jaddr, "ip_relay_hostName", addr->u.ip_relay.hostName,
                   VSIZE(addr->u.ip_relay.hostName) );
        getString( env, jaddr, "ip_relay_invite", addr->u.ip_relay.invite,
                   VSIZE(addr->u.ip_relay.invite) );
        addr->u.ip_relay.seeksPublicRoom =
            getBool( env, jaddr, "ip_relay_seeksPublicRoom" );
        addr->u.ip_relay.advertiseRoom =
            getBool( env, jaddr, "ip_relay_advertiseRoom" );

        break;
    case COMMS_CONN_SMS:
        getString( env, jaddr, "sms_phone", addr->u.sms.phone,
                   VSIZE(addr->u.sms.phone) );
        XP_LOGF( "%s: got SMS; phone=%s", __func__, addr->u.sms.phone );
        addr->u.sms.port = getInt( env, jaddr, "sms_port" );
        break;
    case COMMS_CONN_BT:
    default:
        XP_ASSERT(0);
    }
}

jint
jenumFieldToInt( JNIEnv* env, jobject j_gi, const char* field, 
                 const char* fieldSig )
{
    jclass clazz = (*env)->GetObjectClass( env, j_gi );
    XP_ASSERT( !!clazz );
    char sig[128];
    snprintf( sig, sizeof(sig), "L%s;", fieldSig );
    jfieldID fid = (*env)->GetFieldID( env, clazz, field, sig );
    XP_ASSERT( !!fid );
    jobject jenum = (*env)->GetObjectField( env, j_gi, fid );
    XP_ASSERT( !!jenum );
    jint result = jEnumToInt( env, jenum );

    (*env)->DeleteLocalRef( env, clazz );
    (*env)->DeleteLocalRef( env, jenum );
    return result;
}

void
intToJenumField( JNIEnv* env, jobject jobj, int val, const char* field, 
                 const char* fieldSig )
{
    jclass clazz = (*env)->GetObjectClass( env, jobj );
    XP_ASSERT( !!clazz );
    char buf[128];
    snprintf( buf, sizeof(buf), "L%s;", fieldSig );
    jfieldID fid = (*env)->GetFieldID( env, clazz, field, buf );
    XP_ASSERT( !!fid );         /* failed */
    (*env)->DeleteLocalRef( env, clazz );

    jobject jenum = (*env)->GetObjectField( env, jobj, fid );
    if ( !jenum ) {       /* won't exist in new object */
        jclass clazz = (*env)->FindClass( env, fieldSig );
        XP_ASSERT( !!clazz );
        jmethodID mid = getMethodID( env, clazz, "<init>", "()V" );
        XP_ASSERT( !!mid );
        jenum = (*env)->NewObject( env, clazz, mid );
        XP_ASSERT( !!jenum );
        (*env)->SetObjectField( env, jobj, fid, jenum );
        (*env)->DeleteLocalRef( env, clazz );
    }

    jobject jval = intToJEnum( env, val, fieldSig );
    XP_ASSERT( !!jval );
    (*env)->SetObjectField( env, jobj, fid, jval );
    (*env)->DeleteLocalRef( env, jval );
} /* intToJenumField */

/* Cons up a new enum instance and set its value */
jobject
intToJEnum( JNIEnv* env, int val, const char* enumSig )
{
    jobject jenum = NULL;
    jclass clazz = (*env)->FindClass( env, enumSig );
    XP_ASSERT( !!clazz );

    char buf[128];
    snprintf( buf, sizeof(buf), "()[L%s;", enumSig );
    jmethodID mid = (*env)->GetStaticMethodID( env, clazz, "values", buf );
    XP_ASSERT( !!mid );

    jobject jvalues = (*env)->CallStaticObjectMethod( env, clazz, mid );
    XP_ASSERT( !!jvalues );
    XP_ASSERT( val < (*env)->GetArrayLength( env, jvalues ) );
    /* get the value we want */
    jenum = (*env)->GetObjectArrayElement( env, jvalues, val );
    XP_ASSERT( !!jenum );

    (*env)->DeleteLocalRef( env, jvalues );
    (*env)->DeleteLocalRef( env, clazz );
    return jenum;
} /* intToJEnum */

jint
jEnumToInt( JNIEnv* env, jobject jenum )
{
    jmethodID mid = getMethodID( env, jenum, "ordinal", "()I" );
    XP_ASSERT( !!mid );
    return (*env)->CallIntMethod( env, jenum, mid );
}

XWStreamCtxt*
and_empty_stream( MPFORMAL AndGlobals* globals )
{
    XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) globals->vtMgr,
                                            globals, 0, NULL );
    return stream;
}

/* #ifdef DEBUG */
/* XP_U32 */
/* andy_rand( const char* caller ) */
/* { */
/*     XP_U32 result = rand(); */
/*     XP_LOGF( "%s: returning 0x%lx to %s", __func__, result, caller ); */
/*     LOG_RETURNF( "%lx", result ); */
/*     return result; */
/* } */
/* #endif */
