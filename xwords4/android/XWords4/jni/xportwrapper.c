/* -*-mode: C; compile-command: "cd ..; ../scripts/ndkbuild.sh -j3"; -*- */
/* 
 * Copyright 2001-2010 by Eric House (xwords@eehouse.org).  All rights
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

#include "xportwrapper.h"
#include "andutils.h"
#include "paths.h"

typedef struct _AndTransportProcs {
    TransportProcs tp;
    JNIEnv** envp;
    jobject jxport;
    MPSLOT
} AndTransportProcs;

static jobject
makeJAddr( JNIEnv* env, const CommsAddrRec* addr )
{
    jobject jaddr = NULL;
    if ( NULL != addr ) {
        jclass clazz
            = (*env)->FindClass(env, PKG_PATH("jni/CommsAddrRec") );
        XP_ASSERT( !!clazz );
        jmethodID mid = getMethodID( env, clazz, "<init>", "()V" );
        XP_ASSERT( !!mid );

        jaddr = (*env)->NewObject( env, clazz, mid );
        XP_ASSERT( !!jaddr );

        setJAddrRec( env, jaddr, addr );
    
        deleteLocalRef( env, clazz );
    }
    return jaddr;
}

static XP_U32
and_xport_getFlags( void* closure )
{
    jint result = 0;
    AndTransportProcs* aprocs = (AndTransportProcs*)closure;
    if ( NULL != aprocs->jxport ) {
        JNIEnv* env = *aprocs->envp;
        const char* sig = "()I";
        jmethodID mid = getMethodID( env, aprocs->jxport, "getFlags", sig );

        result = (*env)->CallIntMethod( env, aprocs->jxport, mid );
    }
    return result;
}

static XP_S16
and_xport_send( const XP_U8* buf, XP_U16 len, const CommsAddrRec* addr,
                XP_U32 gameID, void* closure )
{
    jint result = -1;
    LOG_FUNC();
    AndTransportProcs* aprocs = (AndTransportProcs*)closure;
    if ( NULL != aprocs->jxport ) {
        JNIEnv* env = *aprocs->envp;
        const char* sig = "([BL" PKG_PATH("jni/CommsAddrRec") ";I)I";
        jmethodID mid = getMethodID( env, aprocs->jxport, "transportSend", sig );

        jbyteArray jbytes = makeByteArray( env, len, (jbyte*)buf );
        jobject jaddr = makeJAddr( env, addr );

        result = (*env)->CallIntMethod( env, aprocs->jxport, mid, 
                                        jbytes, jaddr, gameID );
        deleteLocalRefs( env, jaddr, jbytes, DELETE_NO_REF );
    }
    LOG_RETURNF( "%d", result );
    return result;
}


static void
and_xport_relayStatus( void* closure, CommsRelayState newState )
{
    AndTransportProcs* aprocs = (AndTransportProcs*)closure;
    if ( NULL != aprocs->jxport ) {
        JNIEnv* env = *aprocs->envp;
        const char* sig = "(L" PKG_PATH("jni/TransportProcs$CommsRelayState") ";)V";
        jmethodID mid = getMethodID( env, aprocs->jxport, "relayStatus", sig );

        jobject jenum = intToJEnum( env, newState, 
                                    PKG_PATH("jni/TransportProcs$CommsRelayState") );
        (*env)->CallVoidMethod( env, aprocs->jxport, mid, jenum );
        deleteLocalRef( env, jenum );
    }
}

static void
and_xport_relayConnd( void* closure, XP_UCHAR* const room, XP_Bool reconnect, 
                      XP_U16 devOrder, XP_Bool allHere, XP_U16 nMissing )
{
    AndTransportProcs* aprocs = (AndTransportProcs*)closure;
    if ( NULL != aprocs->jxport ) {
        JNIEnv* env = *aprocs->envp;
        const char* sig = "(Ljava/lang/String;IZI)V";
        jmethodID mid = getMethodID( env, aprocs->jxport, "relayConnd", sig );

        jstring str = (*env)->NewStringUTF( env, room );
        (*env)->CallVoidMethod( env, aprocs->jxport, mid, 
                                str, devOrder, allHere, nMissing );
        deleteLocalRef( env, str );
    }
}

static XP_Bool 
and_xport_sendNoConn( const XP_U8* buf, XP_U16 len,
                      const XP_UCHAR* relayID, void* closure )
{
    jboolean result = false;
    AndTransportProcs* aprocs = (AndTransportProcs*)closure;
    if ( NULL != aprocs && NULL != aprocs->jxport ) {
        JNIEnv* env = *aprocs->envp;

        const char* sig = "([BLjava/lang/String;)Z";
        jmethodID mid = getMethodID( env, aprocs->jxport, 
                                     "relayNoConnProc", sig );
        jbyteArray jbytes = makeByteArray( env, len, (jbyte*)buf );
        jstring str = (*env)->NewStringUTF( env, relayID );
        result = (*env)->CallBooleanMethod( env, aprocs->jxport, mid, 
                                            jbytes, str );
        deleteLocalRefs( env, jbytes, str, DELETE_NO_REF );
    }
    return result;
}

static void
and_xport_relayError( void* closure, XWREASON relayErr )
{
    AndTransportProcs* aprocs = (AndTransportProcs*)closure;
    if ( NULL != aprocs->jxport ) {
        JNIEnv* env = *aprocs->envp;
        jmethodID mid;
        const char* sig = 
            "(L" PKG_PATH("jni/TransportProcs$XWRELAY_ERROR") ";)V";
        mid = getMethodID( env, aprocs->jxport, "relayErrorProc", sig );

        jobject jenum = intToJEnum( env, relayErr, 
                                    PKG_PATH("jni/TransportProcs$XWRELAY_ERROR") );
        (*env)->CallVoidMethod( env, aprocs->jxport, mid, jenum );

        deleteLocalRef( env, jenum );
    }
}

TransportProcs*
makeXportProcs( MPFORMAL JNIEnv** envp, jobject jxport )
{
    AndTransportProcs* aprocs = NULL;

    JNIEnv* env = *envp;
    aprocs = (AndTransportProcs*)XP_CALLOC( mpool, sizeof(*aprocs) );
    if ( NULL != jxport ) {
        aprocs->jxport = (*env)->NewGlobalRef( env, jxport );
    }
    aprocs->envp = envp;
    MPASSIGN( aprocs->mpool, mpool );

#ifdef COMMS_XPORT_FLAGSPROC
    aprocs->tp.getFlags = and_xport_getFlags;
#endif
    aprocs->tp.send = and_xport_send;
    aprocs->tp.rstatus = and_xport_relayStatus;
    aprocs->tp.rconnd = and_xport_relayConnd;
    aprocs->tp.rerror = and_xport_relayError;
    aprocs->tp.sendNoConn = and_xport_sendNoConn;
    aprocs->tp.closure = aprocs;

    return (TransportProcs*)aprocs;
}

void
destroyXportProcs( TransportProcs** xport )
{
    AndTransportProcs* aprocs = (AndTransportProcs*)*xport;
    JNIEnv* env = *aprocs->envp;
    if ( NULL != aprocs->jxport ) {
        (*env)->DeleteGlobalRef( env, aprocs->jxport );
    }

    XP_FREE( aprocs->mpool, aprocs );
    *xport = NULL;
}
