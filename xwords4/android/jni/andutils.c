/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright © 2009 - 2023 by Eric House (xwords@eehouse.org).  All rights
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

#include <sys/time.h>
#include <time.h>

#include "andutils.h"
#include "paths.h"

#include "comtypes.h"
#include "device.h"
#include "xwstream.h"
#include "strutils.h"
#include "dbgutil.h"

void
and_assert( const char* test, int line, const char* file, const char* func )
{
    RAW_LOG( "assertion \"%s\" failed: line %d in %s() in %s",
             test, line, func, file );
    XP_LOGF( "assertion \"%s\" failed: line %d in %s() in %s",
             test, line, func, file );

    /* give log a chance to write before __android_log_assert() kills the
       process */
    struct timespec req = {
        .tv_nsec = 400000000, /* 4/10 second */
    };
    nanosleep( &req, NULL );

    __android_log_assert( test, "ASSERT", "line %d in %s() in %s",
                          line, func, file  );
}

#ifdef __LITTLE_ENDIAN
XP_U32
and_ntohl(XP_U32 ll)
{
    XP_U32 result = 0L;
    for ( int ii = 0; ii < 4; ++ii ) {
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

jfieldID
getFieldID( JNIEnv* env, jobject obj, const char* fieldName, const char* fieldSig )
{
    jclass cls = (*env)->GetObjectClass( env, obj );
    XP_ASSERT( !!cls );
    jfieldID fid = (*env)->GetFieldID( env, cls, fieldName, fieldSig );
    XP_ASSERT( !!fid );
    deleteLocalRef( env, cls );
    return fid;
}

int
getInt( JNIEnv* env, jobject obj, const char* name )
{
    jfieldID fid = getFieldID( env, obj, name, "I");
    XP_ASSERT( !!fid );
    int result = (*env)->GetIntField( env, obj, fid );
    return result;
}

void
getInts( JNIEnv* env, void* cobj, jobject jobj, const SetInfo* sis, XP_U16 nSis )
{
    for ( int ii = 0; ii < nSis; ++ii ) {
        const SetInfo* si = &sis[ii];
        uint8_t* ptr = ((uint8_t*)cobj) + si->offset;
        int val = getInt( env, jobj, si->name );
        switch( si->siz ) {
        case 4:
            *(uint32_t*)ptr = val;
            break;
        case 2:
            *(uint16_t*)ptr = val;
            break;
        case 1:
            *ptr = val;
            break;
        }
        /* XP_LOGF( "%s: wrote int %s of size %d with val %d at offset %d", */
        /*          __func__, si->name, si->siz, val, si->offset ); */
    }
}

void
setInt( JNIEnv* env, jobject obj, const char* name, int value )
{
    jfieldID fid = getFieldID( env, obj, name, "I");
    if ( !fid ) {
        XP_LOGFF( "getFieldID(%s) failed", name );
        XP_ASSERT(0);
    }
    (*env)->SetIntField( env, obj, fid, value );
}

void
setInts( JNIEnv* env, jobject jobj, void* cobj, const SetInfo* sis, XP_U16 nSis )
{
    for ( int ii = 0; ii < nSis; ++ii ) {
        const SetInfo* si = &sis[ii];
        uint8_t* ptr = ((uint8_t*)cobj) + si->offset;
        int val;
        switch( si->siz ) {
        case 4:
            val = *(uint32_t*)ptr;
            break;
        case 2:
            val = *(uint16_t*)ptr;
            break;
        case 1:
            val = *ptr;
            break;
        default:
            val = 0;
            XP_ASSERT(0);
        }
        setInt( env, jobj, si->name, val );
        /* XP_LOGFF( "read int %s of size %d with val %d/0x%x from offset %d", */
        /*           si->name, si->siz, val, val, si->offset ); */
    }
}

bool
setBool( JNIEnv* env, jobject obj, const char* name, bool value )
{
    bool success = false;
    jfieldID fid = getFieldID( env, obj, name, "Z" );
    if ( 0 != fid ) {
        (*env)->SetBooleanField( env, obj, fid, value );
        success = true;
    }

    return success;
}

void
setBools( JNIEnv* env, jobject jobj, void* cobj, const SetInfo* sis, XP_U16 nSis )
{
    for ( int ii = 0; ii < nSis; ++ii ) {
        const SetInfo* si = &sis[ii];
        XP_Bool val = *(XP_Bool*)(((uint8_t*)cobj)+si->offset);
        setBool( env, jobj, si->name, val );
        /* XP_LOGF( "%s: read bool %s with val %d from offset %d", __func__, */
        /*          si->name, val, si->offset ); */
    }
}

bool
setString( JNIEnv* env, jobject container, const char* fieldName, const XP_UCHAR* value )
{
    // XP_LOGFF( "(fieldName=%s, val=%s)", fieldName, value );
    bool success = false;
    /* jfieldID fid = getFieldID( env, obj, name, "Ljava/lang/String;" ); */

    jstring str = (*env)->NewStringUTF( env, value );
    setObjectField( env, container, fieldName, "Ljava/lang/String;", str );
    success = true;

#ifdef DEBUG
    XP_UCHAR buf[1024];
    getString( env, container, fieldName, buf, VSIZE(buf) );
    XP_ASSERT( !value || 0 == XP_STRCMP( buf, value ) );
#endif

    // XP_LOGFF( "(%s, %s) => %s", fieldName, value, boolToStr(success) );
    return success;
}

void
getStrings( JNIEnv* env, void* cobj, jobject jobj, const SetInfo* sis, XP_U16 nSis )
{
    for ( int ii = 0; ii < nSis; ++ii ) {
        const SetInfo* si = &sis[ii];
        XP_UCHAR* buf = (XP_UCHAR*)(((uint8_t*)cobj) + si->offset);
        getString( env, jobj, si->name, buf, si->siz );
    }
}

void
setStrings( JNIEnv* env, jobject jobj, void* cobj, const SetInfo* sis, XP_U16 nSis )
{
    for ( int ii = 0; ii < nSis; ++ii ) {
        const SetInfo* si = &sis[ii];
        // XP_LOGF( "calling setString(%s)", si->name );
        XP_UCHAR* val = (XP_UCHAR*)(((uint8_t*)cobj) + si->offset);
        setString( env, jobj, si->name, val );
    }
}

void
getString( JNIEnv* env, jobject container, const char* name, XP_UCHAR* buf,
           int bufLen )
{
    jstring jstr = getObjectField( env, container, name, "Ljava/lang/String;" );
    jsize len = 0;
    if ( !!jstr ) {             /* might be null */
        len = (*env)->GetStringUTFLength( env, jstr );
        XP_ASSERT( len < bufLen );
        const char* chars = (*env)->GetStringUTFChars( env, jstr, NULL );
        XP_MEMCPY( buf, chars, len );
        (*env)->ReleaseStringUTFChars( env, jstr, chars );
        deleteLocalRef( env, jstr );
    }
    buf[len] = '\0';
    // XP_LOGFF( "(field: %s) => '%s'", name, buf );
}

XP_UCHAR* 
getStringCopy( MPFORMAL JNIEnv* env, jstring jstr )
{
    XP_UCHAR* result = NULL;
    if ( NULL != jstr ) {
        const char* chars = (*env)->GetStringUTFChars( env, jstr, NULL );
        result = copyString( mpool, chars );
        (*env)->ReleaseStringUTFChars( env, jstr, chars );
    }
    return result;
}

static jobject
getObjectFieldWithFID( JNIEnv* env, jobject obj, const char* fieldName,
                       const char* sig, jfieldID* fidp )
{
    jfieldID fid = getFieldID( env, obj, fieldName, sig );
    XP_ASSERT( !!fid );
    if ( !!fidp ) {
        *fidp = fid;
    }
    jobject result = (*env)->GetObjectField( env, obj, fid );
    return result;
}

jobject
getObjectField( JNIEnv* env, jobject container, const char* name, const char* sig )
{
    return getObjectFieldWithFID( env, container, name, sig, NULL );
}

void
setObjectField( JNIEnv* env, jobject obj, const char* name, const char* sig,
                jobject val )
{
    jfieldID fid = getFieldID( env, obj, name, sig );
    XP_ASSERT( !!fid );
    (*env)->SetObjectField( env, obj, fid, val );
    deleteLocalRef( env, val );
}

bool
getBool( JNIEnv* env, jobject obj, const char* name )
{
    bool result;
    jfieldID fid = getFieldID( env, obj, name, "Z");
    XP_ASSERT( !!fid );
    result = (*env)->GetBooleanField( env, obj, fid );
    return result;
}

void
getBools( JNIEnv* env, void* cobj, jobject jobj, const SetInfo* sis, XP_U16 nSis )
{
    for ( int ii = 0; ii < nSis; ++ii ) {
        const SetInfo* si = &sis[ii];
        XP_Bool val = getBool( env, jobj, si->name );
        *(XP_Bool*)(((uint8_t*)cobj)+si->offset) = val;
        /* XP_LOGF( "%s: wrote bool %s with val %d at offset %d", __func__,  */
        /*          si->name, val, si->offset ); */
    }
}

jlongArray
makeLongArray( JNIEnv* env, int count, const jlong* vals )
{
    jlongArray array = (*env)->NewLongArray( env, count );
    XP_ASSERT( !!array );
    jlong* elems = (*env)->GetLongArrayElements( env, array, NULL );
    XP_ASSERT( !!elems );
    for ( int ii = 0; ii < count; ++ii ) {
        elems[ii] = vals[ii];
    }
    (*env)->ReleaseLongArrayElements( env, array, elems, 0 );
    return array;
}

jintArray
makeIntArray( JNIEnv* env, int count, const void* vals, size_t elemSize )
{
    jintArray array = (*env)->NewIntArray( env, count );
    XP_ASSERT( !!array );
    jint* elems = (*env)->GetIntArrayElements( env, array, NULL );
    XP_ASSERT( !!elems );
    jint elem;
    for ( int ii = 0; ii < count; ++ii ) {
        switch( elemSize ) {
        case sizeof(XP_U32):
            elem = *(XP_U32*)vals;
            break;
        case sizeof(XP_U16):
            elem = *(XP_U16*)vals;
            break;
        case sizeof(XP_U8):
            elem = *(XP_U8*)vals;
            break;
        default:
            XP_ASSERT(0);
            break;
        }
        vals += elemSize;
        elems[ii] = elem;
    }
    (*env)->ReleaseIntArrayElements( env, array, elems, 0 );
    return array;
}

void
setIntArray( JNIEnv* env, jobject jowner, const char* fieldName,
             int count, const void* vals, size_t elemSize )
{
    jintArray jarr = makeIntArray( env, count, vals, elemSize );
    setObjectField( env, jowner, fieldName, "[I", jarr );
}

jbyteArray
makeByteArray( JNIEnv* env, int siz, const jbyte* vals )
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

jbyteArray
streamToBArray( JNIEnv* env, XWStreamCtxt* stream )
{
    int nBytes = strm_getSize( stream );
    jbyteArray result = (*env)->NewByteArray( env, nBytes );
    jbyte* jelems = (*env)->GetByteArrayElements( env, result, NULL );
    strm_getBytes( stream, jelems, nBytes );
    (*env)->ReleaseByteArrayElements( env, result, jelems, 0 );
    return result;
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
makeBooleanArray( JNIEnv* env, int siz, const jboolean* vals )
{
    jbooleanArray array = (*env)->NewBooleanArray( env, siz );
    XP_ASSERT( !!array );
    if ( !!vals ) {
        setBoolArray( env, array, siz, vals );
    }
    return array;
}

int
getIntsFromArray( JNIEnv* env, int dest[], jintArray arr, int count, bool del )
{
    {
        jsize len = (*env)->GetArrayLength( env, arr );
        if ( len < count ) {
            count = len;
        }
    }
    jint* ints = (*env)->GetIntArrayElements(env, arr, 0);
    for ( int ii = 0; ii < count; ++ii ) {
        dest[ii] = ints[ii];
    }
    (*env)->ReleaseIntArrayElements( env, arr, ints, 0 );
    if ( del ) {
        deleteLocalRef( env, arr );
    }
    return count;
}

void
setIntInArray( JNIEnv* env, jintArray arr, int index, int val )
{
    jint* ints = (*env)->GetIntArrayElements( env, arr, 0 );
#ifdef DEBUG
    jsize len = (*env)->GetArrayLength( env, arr );
    XP_ASSERT( len > index );
#endif
    ints[index] = val;
    (*env)->ReleaseIntArrayElements( env, arr, ints, 0 );
}

void
addStrToList( JNIEnv* env, jobject list, const XP_UCHAR* str )
{
    jmethodID mid = getMethodID( env, list, "add", "(Ljava/lang/Object;)Z" );

    jstring jstr = (*env)->NewStringUTF( env, str );
    (*env)->CallBooleanMethod( env, list, mid, jstr );
    deleteLocalRef( env, jstr );
}

jobjectArray
makeStringArray( JNIEnv* env, const int count, const XP_UCHAR* const* vals )
{
    jobjectArray jarray;
    {
        jclass clas = (*env)->FindClass(env, "java/lang/String");
        jstring empty = (*env)->NewStringUTF( env, "" );
        jarray = (*env)->NewObjectArray( env, count, clas, empty );
        deleteLocalRefs( env, clas, empty, DELETE_NO_REF );
    }

    for ( int ii = 0; !!vals && ii < count; ++ii ) {
        jstring jstr = (*env)->NewStringUTF( env, vals[ii] );
        (*env)->SetObjectArrayElement( env, jarray, ii, jstr );
        deleteLocalRef( env, jstr );
    }

    return jarray;
}

void
setStringArray( JNIEnv* env, jobject jowner, const char* ownerField,
                int count, const XP_UCHAR** vals )
{
    jobjectArray jaddrs = makeStringArray( env, count, vals );
    setObjectField( env, jowner, ownerField, "[Ljava/lang/String;", jaddrs );
}

jobjectArray
makeByteArrayArray( JNIEnv* env, int siz )
{
    jclass clas = (*env)->FindClass( env, "[B" );
    jobjectArray result = (*env)->NewObjectArray( env, siz, clas, NULL );
    deleteLocalRef( env, clas );
    return result;
}

jstring
streamToJString( JNIEnv* env, XWStreamCtxt* stream, XP_Bool destroy )
{
    int len = strm_getSize( stream );
    XP_UCHAR buf[1 + len];
    strm_getBytes( stream, buf, len );
    buf[len] = '\0';

    jstring jstr = (*env)->NewStringUTF( env, buf );

    if ( destroy ) {
        strm_destroy( stream );
    }

    return jstr;
}

jmethodID
getMethodID( JNIEnv* env, jobject obj, const char* proc, const char* sig )
{
    XP_ASSERT( !!env );
    jclass cls = (*env)->GetObjectClass( env, obj );
    XP_ASSERT( !!cls );
#ifdef DEBUG
    char buf[128] = {};
    getClassName( env, obj, buf, sizeof(buf) );
#endif
    jmethodID mid = (*env)->GetMethodID( env, cls, proc, sig );
    if ( !mid ) {
        XP_LOGFF( "no mid for proc %s, sig %s in object of class %s",
                  proc, sig, buf );
    }
    XP_ASSERT( !!mid );
    deleteLocalRef( env, cls );
    return mid;
}

void
setTypeSetFieldIn( JNIEnv* env, const CommsAddrRec* addr, jobject jTarget, 
                   const char* fieldName )
{
    jobject jtypset = conTypesToJ( env, addr->_conTypes );
    XP_ASSERT( !!jtypset );
    setObjectField( env, jTarget, fieldName,
                    "L" PKG_PATH("jni/CommsAddrRec$CommsConnTypeSet") ";",
                    jtypset );
}

jobject
makeObject( JNIEnv* env, const char* className, const char* initSig, ... )
{
    jclass clazz = (*env)->FindClass( env, className );
    XP_ASSERT( !!clazz );
    jmethodID mid = (*env)->GetMethodID( env, clazz, "<init>", initSig );
    XP_ASSERT( !!mid );

    va_list ap;
    va_start( ap, initSig );
    jobject result = (*env)->NewObjectV( env, clazz, mid, ap );
    va_end( ap );

    deleteLocalRef( env, clazz );
    return result;
}

jobject
makeObjectEmptyConstr( JNIEnv* env, const char* className )
{
    return makeObject( env, className, "()V" );
}

jobject
makeJSummaryRec( JNIEnv* env, jobject jsummary, const GameSummary* gs,
                 const CurGameInfo* gi )
{
    setInt( env, jsummary, "nMoves", gs->nMoves );
    setBool( env, jsummary, "gameOver", gs->gameOver );
    setBool( env, jsummary, "collapsed", gs->collapsed );
    setBool( env, jsummary, "quashed", gs->quashed );
    setInt( env, jsummary, "turn", gs->turn );
    setBool( env, jsummary, "turnIsLocal", gs->turnIsLocal );
    setBool( env, jsummary, "canRematch", gs->canRematch );
    setBool( env, jsummary, "hasChat", gs->hasChat );
    setInt( env, jsummary, "lastMoveTime", gs->lastMoveTime );
    setInt( env, jsummary, "dupTimerExpires", gs->dupTimerExpires );

    setInt( env, jsummary, "missingPlayers", gs->missingPlayers );
    setInt( env, jsummary, "nPacketsPending", gs->nPacketsPending );

    setInt( env, jsummary, "nMissing", gs->nMissing );
    setInt( env, jsummary, "nInvited", gs->nInvited );

    setInt( env, jsummary, "nPlayers", gi->nPlayers );
    intToJenumField( env, jsummary, gi->deviceRole, "deviceRole",
                     PKG_PATH("jni/CurGameInfo$DeviceRole") );

    jintArray jscores = makeIntArray( env, gi->nPlayers, gs->scores.arr,
                                      sizeof(gs->scores.arr[0]) );
    setObjectField( env, jsummary, "scores", "[I", jscores );

    setInt( env, jsummary, "gameID", gi->gameID );

    // isoCode = gi.isoCode()
    
    return jsummary;
}

jobject
makeJSummary( JNIEnv* env, const GameSummary* gs, const CurGameInfo* gi )
{
    XP_LOGFF( "here" );
    jobject jsum = makeObjectEmptyConstr( env, PKG_PATH("jni/GameSummary") );
    return makeJSummaryRec( env, jsum, gs, gi );
}

jobject
makeJAddr( JNIEnv* env, const CommsAddrRec* addr )
{
    jobject jaddr = NULL;
    if ( NULL != addr ) {
        jaddr = makeObjectEmptyConstr( env, PKG_PATH("jni/CommsAddrRec") );
        setJAddrRec( env, jaddr, addr );
    }
    return jaddr;
}

/* Copy C object data into Java object */
jobject
setJAddrRec( JNIEnv* env, jobject jaddr, const CommsAddrRec* addr )
{
    XP_ASSERT( !!addr );
    setTypeSetFieldIn( env, addr, jaddr, "conTypes" );

    CommsConnType typ;
    for ( XP_U32 st = 0; addr_iter( addr, &typ, &st ); ) {
        switch ( typ ) {
        case COMMS_CONN_NONE:
            break;
        case COMMS_CONN_RELAY:
#ifdef XWFEATURE_RELAY
            setInt( env, jaddr, "ip_relay_port", addr->u.ip_relay.port );
            setString( env, jaddr, "ip_relay_hostName", addr->u.ip_relay.hostName );
            setString( env, jaddr, "ip_relay_invite", addr->u.ip_relay.invite );
            setBool( env, jaddr, "ip_relay_seeksPublicRoom",
                     addr->u.ip_relay.seeksPublicRoom );
            setBool( env, jaddr, "ip_relay_advertiseRoom",
                     addr->u.ip_relay.advertiseRoom );
#endif
            break;
        case COMMS_CONN_SMS:
            setString( env, jaddr, "sms_phone", addr->u.sms.phone );
            setInt( env, jaddr, "sms_port", addr->u.sms.port );
            break;
        case COMMS_CONN_BT:
            setString( env, jaddr, "bt_hostName", addr->u.bt.hostName );
            setString( env, jaddr, "bt_btAddr", addr->u.bt.btAddr.chars );
            break;
        case COMMS_CONN_P2P:
            setString( env, jaddr, "p2p_addr", addr->u.p2p.mac_addr );
            break;
        case COMMS_CONN_NFC:
            break;
        case COMMS_CONN_MQTT: {
            XP_UCHAR buf[32];
            formatMQTTDevID( &addr->u.mqtt.devID, buf, VSIZE(buf) );
            setString( env, jaddr, "mqtt_devID", buf );
        }
            break;
        default:
            XP_ASSERT(0);
        }
    }
    return jaddr;
}

jobject
conTypesToJ( JNIEnv* env, ConnTypeSetBits types )
{
    jobject result =
        makeObjectEmptyConstr( env, PKG_PATH("jni/CommsAddrRec$CommsConnTypeSet") );
    XP_ASSERT( !!result );

    jmethodID mid = getMethodID( env, result, "add",
                                  "(Ljava/lang/Object;)Z" );
    XP_ASSERT( !!mid );
    CommsConnType typ;
    for ( XP_U32 st = 0; types_iter( types, &typ, &st ); ) {
        jobject jtyp = intToJEnum( env, typ, 
                                   PKG_PATH("jni/CommsAddrRec$CommsConnType") );
        XP_ASSERT( !!jtyp );
        (*env)->CallBooleanMethod( env, result, mid, jtyp );
        deleteLocalRef( env, jtyp );
    }
    return result;
}

jobjectArray
makeAddrArray( JNIEnv* env, XP_U16 count, const CommsAddrRec* addrs )
{
    jclass clas = (*env)->FindClass( env, PKG_PATH("jni/CommsAddrRec") );
    jobjectArray result = (*env)->NewObjectArray( env, count, clas, NULL );
    for ( int ii = 0; ii < count; ++ii ) {
        jobject jaddr = makeJAddr( env, &addrs[ii] );
        (*env)->SetObjectArrayElement( env, result, ii, jaddr );
        deleteLocalRef( env, jaddr );
    }
    deleteLocalRef( env, clas );
    return result;
}

ConnTypeSetBits
getTypesFromSet( JNIEnv* env, jobject jtypeset )
{
    XP_ASSERT( !!jtypeset );
    ConnTypeSetBits result = 0;

    jmethodID mid = getMethodID( env, jtypeset, "getTypes", 
                                 "()[L" PKG_PATH("jni/CommsAddrRec$CommsConnType;") );
    XP_ASSERT( !!mid );
    jobject jtypesarr = (*env)->CallObjectMethod( env, jtypeset, mid );
    XP_ASSERT( !!jtypesarr );
    jsize len = (*env)->GetArrayLength( env, jtypesarr );
    for ( int ii = 0; ii < len; ++ii ) {
        jobject jtype = (*env)->GetObjectArrayElement( env, jtypesarr, ii );
        jint asInt = jEnumToInt( env, jtype );
        deleteLocalRef( env, jtype );
        CommsConnType typ = (CommsConnType)asInt;

        XP_LOGFF( "adding type %s", ConnType2Str( typ ) );
        types_addType( &result, typ );
    }

    deleteLocalRef( env, jtypesarr );

    LOG_RETURNF( "0x%x", result );
    return result;
}

/* Writes a java version of CommsAddrRec into a C one */
CommsAddrRec
getJAddrRec( JNIEnv* env, jobject jaddr )
{
    CommsAddrRec addr = {};
    /* Iterate over types in the set in jaddr, and for each call
       addr_addType() and then copy in the types. */
    jobject jtypeset = getObjectField( env, jaddr, "conTypes",
                                       "L" PKG_PATH("jni/CommsAddrRec$CommsConnTypeSet") ";" );
    XP_ASSERT( !!jtypeset );
    ConnTypeSetBits conTypes = getTypesFromSet( env, jtypeset );
    CommsConnType typ;
    for ( XP_U32 state = 0; types_iter( conTypes, &typ, &state ); ) {
        addr_addType( &addr, typ );

        switch ( typ ) {
        case COMMS_CONN_RELAY:
#ifdef XWFEATURE_RELAY
            addr.u.ip_relay.port = getInt( env, jaddr, "ip_relay_port" );
            getString( env, jaddr, "ip_relay_hostName", addr.u.ip_relay.hostName,
                       VSIZE(addr.u.ip_relay.hostName) );
            getString( env, jaddr, "ip_relay_invite", addr.u.ip_relay.invite,
                       VSIZE(addr.u.ip_relay.invite) );
            addr.u.ip_relay.seeksPublicRoom =
                getBool( env, jaddr, "ip_relay_seeksPublicRoom" );
            addr.u.ip_relay.advertiseRoom =
                getBool( env, jaddr, "ip_relay_advertiseRoom" );

#endif
            break;
        case COMMS_CONN_SMS:
            getString( env, jaddr, "sms_phone", addr.u.sms.phone,
                       VSIZE(addr.u.sms.phone) );
            // XP_LOGF( "%s: got SMS; phone=%s", __func__, addr.u.sms.phone );
            addr.u.sms.port = getInt( env, jaddr, "sms_port" );
            break;
        case COMMS_CONN_BT:
            getString( env, jaddr, "bt_hostName", addr.u.bt.hostName,
                       VSIZE(addr.u.bt.hostName) );
            getString( env, jaddr, "bt_btAddr", addr.u.bt.btAddr.chars,
                       VSIZE(addr.u.bt.btAddr.chars) );
            break;
        case COMMS_CONN_P2P:
            getString( env, jaddr, "p2p_addr", addr.u.p2p.mac_addr,
                       VSIZE(addr.u.p2p.mac_addr) );
            break;
        case COMMS_CONN_NFC:
            break;
        case COMMS_CONN_MQTT: {
            XP_UCHAR buf[32];
            getString( env, jaddr, "mqtt_devID", buf, VSIZE(buf) );
            sscanf( buf, MQTTDevID_FMT, &addr.u.mqtt.devID );
        }
            break;
        default:
            XP_ASSERT(0);
        }
    }
    deleteLocalRef( env, jtypeset );
    return addr;
}

jint
jenumFieldToInt( JNIEnv* env, jobject j_gi, const char* field, 
                 const char* fieldSig )
{
    char sig[128];
    snprintf( sig, sizeof(sig), "L%s;", fieldSig );
    jobject jenum = getObjectField( env, j_gi, field, sig );
    XP_ASSERT( !!jenum );
    jint result = jEnumToInt( env, jenum );

    deleteLocalRef( env, jenum );
    return result;
}

void
intToJenumField( JNIEnv* env, jobject jobj, int val, const char* field, 
                 const char* fieldSig )
{
    char buf[128];
    snprintf( buf, sizeof(buf), "L%s;", fieldSig );

    jfieldID fid;
    jobject jenum = getObjectFieldWithFID( env, jobj, field, buf, &fid );
    if ( !jenum ) {       /* won't exist in new object */
        jenum = makeObjectEmptyConstr( env, fieldSig );
        XP_ASSERT( !!jenum );
        (*env)->SetObjectField( env, jobj, fid, jenum );
    }

    jobject jval = intToJEnum( env, val, fieldSig );
    XP_ASSERT( !!jval );
    (*env)->SetObjectField( env, jobj, fid, jval );
    deleteLocalRef( env, jval );
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

    deleteLocalRefs( env, jvalues, clazz, DELETE_NO_REF );
    return jenum;
} /* intToJEnum */

jint
jEnumToInt( JNIEnv* env, jobject jenum )
{
    jmethodID mid = getMethodID( env, jenum, "ordinal", "()I" );
    XP_ASSERT( !!mid );
    return (*env)->CallIntMethod( env, jenum, mid );
}

static const SetInfo nli_ints[] = {
    ARR_MEMBER( NetLaunchInfo, _conTypes ),
    ARR_MEMBER( NetLaunchInfo, forceChannel ),
    ARR_MEMBER( NetLaunchInfo, nPlayersT ),
    ARR_MEMBER( NetLaunchInfo, nPlayersH ),
    ARR_MEMBER( NetLaunchInfo, gameID ),
    ARR_MEMBER( NetLaunchInfo, osVers ),
};

static const SetInfo nli_bools[] = {
    ARR_MEMBER( NetLaunchInfo, isGSM ),
    ARR_MEMBER( NetLaunchInfo, remotesAreRobots ),
};

static const SetInfo nli_strs[] = {
    ARR_MEMBER( NetLaunchInfo, dict ),
    ARR_MEMBER( NetLaunchInfo, isoCodeStr ),
    ARR_MEMBER( NetLaunchInfo, gameName ),
#ifdef XWFEATURE_RELAY
    ARR_MEMBER( NetLaunchInfo, room ),
#endif
    ARR_MEMBER( NetLaunchInfo, btName ),
    ARR_MEMBER( NetLaunchInfo, btAddress ),
    ARR_MEMBER( NetLaunchInfo, phone ),
    ARR_MEMBER( NetLaunchInfo, inviteID ),
    ARR_MEMBER( NetLaunchInfo, mqttDevID ),
};

void
loadNLI( JNIEnv* env, NetLaunchInfo* nli, jobject jnli )
{
    XP_MEMSET( nli, 0, sizeof(*nli) );
    getInts( env, (void*)nli, jnli, AANDS(nli_ints) );
    getBools( env, (void*)nli, jnli, AANDS(nli_bools) );
    getStrings( env, (void*)nli, jnli, AANDS(nli_strs) );
}

void
setNLI( JNIEnv* env, jobject jnli, const NetLaunchInfo* nli )
{
    setInts( env, jnli, (void*)nli, AANDS(nli_ints) );
    setBools( env, jnli, (void*)nli, AANDS(nli_bools) );
    setStrings( env, jnli, (void*)nli, AANDS(nli_strs) );
}

XWStreamCtxt*
and_tmp_stream( XW_DUtilCtxt* dutil )
{
    XWStreamCtxt* stream = dvc_makeStream( dutil );
    return stream;
}

XP_U32
getCurSeconds( JNIEnv* env )
{
    jclass clazz = (*env)->FindClass( env, PKG_PATH("Utils") );
    XP_ASSERT( !!clazz );
    jmethodID mid = (*env)->GetStaticMethodID( env, clazz,
                                               "getCurSeconds", "()J" );
    jlong result = (*env)->CallStaticLongMethod( env, clazz, mid );

    deleteLocalRef( env, clazz );
    return result;
}

void deleteLocalRef( JNIEnv* env, jobject jobj )
{
    if ( NULL != jobj ) {
        (*env)->DeleteLocalRef( env, jobj );
    }
}

void
deleteLocalRefs( JNIEnv* env, ... )
{
    va_list ap;
    va_start( ap, env );
    for ( ; ; ) {
        jobject jnext = va_arg( ap, jobject );
        if ( DELETE_NO_REF == jnext ) {
            break;
        }
        deleteLocalRef( env, jnext );
    }
    va_end( ap );
}

/* Passed to device methods related to MQTT messages */
void
msgAndTopicProc( void* closure, const XP_UCHAR* topic,
                 const XP_U8* msgBuf, XP_U16 msgLen, XP_U8 qos )
{
    MTPData* mtp = (MTPData*)closure;
    JNIEnv* env = mtp->env;

    if ( VSIZE(mtp->topics) <= mtp->count ) {
        XP_LOGFF( "exausted space for topics; dropping" );
    } else {
        const XP_UCHAR* ptr = &mtp->storage[mtp->offset];
        size_t siz = XP_SNPRINTF( (char*)ptr, VSIZE(mtp->storage) - mtp->offset,
                                  "%s", topic );
        if ( siz >= VSIZE(mtp->storage) - mtp->offset ) {
            XP_LOGFF( "exausted space for data; dropping" );
        } else {
            mtp->topics[mtp->count] = ptr;
            mtp->offset += 1 + XP_STRLEN(ptr);

            mtp->jPackets[mtp->count] = makeByteArray( env, msgLen, (const jbyte*)msgBuf );

            ++mtp->count;
            XP_LOGFFV( "mtp->count now: %d", mtp->count );

            XP_ASSERT( mtp->qos == 0 || mtp->qos == qos );
            mtp->qos = qos;
        }
    }
}

jobject
wrapResults( MTPData* mtp )
{
    JNIEnv* env = mtp->env;
    jobject result =
        makeObjectEmptyConstr( env, PKG_PATH("jni/Device$TopicsAndPackets"));

    setInt( env, result, "qos", mtp->qos );

    jobjectArray jTopics = makeStringArray( env, mtp->count, mtp->topics );
    setObjectField( env, result, "topics", "[Ljava/lang/String;", jTopics );

    jobjectArray jPackets = makeByteArrayArray( env, mtp->count );
    for ( int ii = 0; ii < mtp->count; ++ii ) {
        (*env)->SetObjectArrayElement( env, jPackets, ii, mtp->jPackets[ii] );
        deleteLocalRef( env, mtp->jPackets[ii] );
    }
    setObjectField( env, result, "packets", "[[B", jPackets );

    return result;
}

void
loadCommonPrefs( JNIEnv* env, CommonPrefs* cp, jobject j_cp )
{
    XP_ASSERT( !!j_cp );
    cp->showBoardArrow = getBool( env, j_cp, "showBoardArrow" );
    cp->showRobotScores = getBool( env, j_cp, "showRobotScores" );
    cp->hideTileValues = getBool( env, j_cp, "hideTileValues" );
    cp->skipCommitConfirm = getBool( env, j_cp, "skipCommitConfirm" );
    cp->showColors = getBool( env, j_cp, "showColors" );
    cp->sortNewTiles = getBool( env, j_cp, "sortNewTiles" );
    cp->allowPeek = getBool( env, j_cp, "allowPeek" );
    cp->skipMQTTAdd = getBool( env, j_cp, "skipMQTTAdd" );
#ifdef XWFEATURE_CROSSHAIRS
    cp->hideCrosshairs = getBool( env, j_cp, "hideCrosshairs" );
#endif
    cp->tvType = jenumFieldToInt( env, j_cp, "tvType",
                                  PKG_PATH("jni/CommonPrefs$TileValueType"));
}

#ifdef DEBUG

/* A bunch of threads are generating log statements. */
static void
passToJava( const char* tag, const char* msg )
{
    JNIEnv* env = waitEnvFromGlobals();
    if ( !!env ) {
        jstring jtag = (*env)->NewStringUTF( env, tag );
        jstring jbuf = (*env)->NewStringUTF( env, msg );
        jclass clazz = (*env)->FindClass( env, PKG_PATH("Log") );
        XP_ASSERT( !!clazz );
        jmethodID mid = (*env)->GetStaticMethodID( env, clazz, "store",
                                                   "(Ljava/lang/String;Ljava/lang/String;)V" );
        (*env)->CallStaticVoidMethod( env, clazz, mid, jtag, jbuf );
        deleteLocalRefs( env, clazz, jtag, jbuf, DELETE_NO_REF );
    } else {
        // RAW_LOG( "env is NULL; dropping" );
    }
}

static void
debugf( const char* format, va_list ap )
{
    char buf[1024];
    int len;
    struct tm* timp;
    struct timeval tv;
    struct timezone tz;

    gettimeofday( &tv, &tz );
    timp = localtime( &tv.tv_sec );

    len = snprintf( buf, sizeof(buf), "%.2d:%.2d:%.2d: ", 
                    timp->tm_hour, timp->tm_min, timp->tm_sec );
    if ( len < sizeof(buf) ) {
        vsnprintf( buf + len, sizeof(buf)-len, format, ap );
    }

    const char* tag =
# if defined VARIANT_xw4GPlay || defined VARIANT_xw4fdroid || defined VARIANT_xw4Foss
                               "xw4"
# elif defined VARIANT_xw4d || defined VARIANT_xw4dGPlay
                               "x4bg"
# elif defined VARIANT_xw4dup || defined VARIANT_xw4dupGPlay
                               "x4du"
# else
      ERROR
# endif
        ;

    (void)__android_log_write( ANDROID_LOG_DEBUG, tag, buf );

    passToJava( tag, buf );
}

void
raw_log( const char* func, const char* fmt, ... )
{
    char buf[1024];
    int len = snprintf( buf, VSIZE(buf) - 1, "in %s(): %s", func, fmt );
    buf[len] = '\0';

    va_list ap;
    va_start( ap, fmt );
    char buf2[1024];
    len = vsnprintf( buf2, VSIZE(buf2) - 1, buf, ap );
    va_end( ap );

    (void)__android_log_write( ANDROID_LOG_DEBUG, "raw", buf2 );
}

void
android_debugf( const char* format, ... )
{
    va_list ap;
    va_start( ap, format );
    debugf( format, ap );
    va_end(ap);
}

void
android_debugff(const char* func, const char* file, int line, const char* fmt, ...)
{
    char buf[256];
    const char* shortPath = 1 + strrchr(file, '/');
    if ( !shortPath ) {
        shortPath = file;
    }
    snprintf( buf, sizeof(buf), "%s.%d:%s(): %s", shortPath, line, func, fmt );

    va_list ap;
    va_start( ap, fmt );
    debugf( buf, ap );
    va_end( ap );
}

void
android_gid_debugff( const XP_U32 gid, const char* func, const char* file,
                     int line, const char* fmt, ...)
{
    char buf[256];
    const char* shortPath = 1 + strrchr(file, '/');
    if ( !shortPath ) {
        shortPath = file;
    }
    snprintf( buf, sizeof(buf), "<%08X> %s.%d:%s(): %s", gid, shortPath, line, func, fmt );

    va_list ap;
    va_start( ap, fmt );
    debugf( buf, ap );
    va_end( ap );
}

/* Print an object's class name into buffer.
 *
 * NOTE: this must be called in advance of any jni error, because methods on
 * env can't be called once there's an exception pending.
 */
#ifdef DEBUG
char*
getClassName( JNIEnv* env, jobject obj, char* out, int outLen )
{
    XP_ASSERT( !!obj );
    jclass cls1 = (*env)->GetObjectClass( env, obj );

    // First get the class object
    jmethodID mid = (*env)->GetMethodID( env, cls1, "getClass",
                                         "()Ljava/lang/Class;" );
    jobject clsObj = (*env)->CallObjectMethod( env, obj, mid );

    // Now get the class object's class descriptor
    jclass cls2 = (*env)->GetObjectClass( env, clsObj );
    // Find the getName() method on the class object
    mid = (*env)->GetMethodID( env, cls2, "getName", "()Ljava/lang/String;" );

    // Call the getName() to get a jstring object back
    jstring strObj = (jstring)(*env)->CallObjectMethod( env, clsObj, mid );

    jint slen = (*env)->GetStringUTFLength( env, strObj );
    if ( slen < outLen ) {
        (*env)->GetStringUTFRegion( env, strObj, 0, slen, out );
        out[slen] = '\0';
    } else {
        out[0] = '\0';
    }
    deleteLocalRefs( env, clsObj, cls1, cls2, strObj, DELETE_NO_REF );
    return out;
}
#endif
#endif

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

#ifndef MEM_DEBUG
void
and_freep( void** ptrp )
{
    if ( !!*ptrp ) {
        free( *ptrp );
        *ptrp = NULL;
    }
}
#endif
