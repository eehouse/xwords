/* -*-mode: C; compile-command: "../../scripts/ndkbuild.sh"; -*- */
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
            = (*env)->FindClass(env, "org/eehouse/android/xw4/jni/CommsAddrRec");
        XP_ASSERT( !!clazz );
        jmethodID mid = getMethodID( env, clazz, "<init>", "()V" );
        XP_ASSERT( !!mid );

        jaddr = (*env)->NewObject( env, clazz, mid );
        XP_ASSERT( !!jaddr );

        setJAddrRec( env, jaddr, addr );
    
        (*env)->DeleteLocalRef( env, clazz );
    }
    return jaddr;
}

static XP_S16
and_xport_send( const XP_U8* buf, XP_U16 len, const CommsAddrRec* addr,
                void* closure )
{
    jint result = -1;
    LOG_FUNC();
    AndTransportProcs* aprocs = (AndTransportProcs*)closure;
    if ( NULL != aprocs->jxport ) {
        JNIEnv* env = *aprocs->envp;
        const char* sig = "([BLorg/eehouse/android/xw4/jni/CommsAddrRec;)I";
        jmethodID mid = getMethodID( env, aprocs->jxport, "transportSend", sig );

        jbyteArray jbytes = makeByteArray( env, len, (jbyte*)buf );
        jobject jaddr = makeJAddr( env, addr );

        result = (*env)->CallIntMethod( env, aprocs->jxport, mid, 
                                        jbytes, jaddr );

        if ( NULL != jaddr ) {
            (*env)->DeleteLocalRef( env, jaddr );
        }
        (*env)->DeleteLocalRef( env, jbytes );
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
        const char* sig = "(Lorg/eehouse/android/xw4/jni/"
            "TransportProcs$CommsRelayState;)V";
        jmethodID mid = getMethodID( env, aprocs->jxport, "relayStatus", sig );

        jobject jenum = intToJEnum( env, newState, "org/eehouse/android/xw4/jni/"
                                    "TransportProcs$CommsRelayState" );
        (*env)->CallVoidMethod( env, aprocs->jxport, mid, jenum );
        (*env)->DeleteLocalRef( env, jenum );
    }
}

static void
and_xport_relayConnd( void* closure, XP_Bool allHere, XP_U16 nMissing )
{
    AndTransportProcs* aprocs = (AndTransportProcs*)closure;
    if ( NULL != aprocs->jxport ) {
        JNIEnv* env = *aprocs->envp;
        const char* sig = "(ZI)V";
        jmethodID mid = getMethodID( env, aprocs->jxport, "relayConnd", sig );

        (*env)->CallVoidMethod( env, aprocs->jxport, mid, 
                                allHere, nMissing );
    }
}

static void
and_xport_relayError( void* closure, XWREASON relayErr )
{
    AndTransportProcs* aprocs = (AndTransportProcs*)closure;
    if ( NULL != aprocs->jxport ) {
        JNIEnv* env = *aprocs->envp;
        jmethodID mid;
        const char* sig = "(Lorg/eehouse/android/xw4/jni/"
            "TransportProcs$XWRELAY_ERROR;)V";
        mid = getMethodID( env, aprocs->jxport, "relayErrorProc", sig );

        jobject jenum = intToJEnum( env, relayErr, "org/eehouse/android/xw4/jni/"
                                    "TransportProcs$XWRELAY_ERROR" );
        (*env)->CallVoidMethod( env, aprocs->jxport, mid, jenum );

        (*env)->DeleteLocalRef( env, jenum );
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
    XP_ASSERT( aprocs->jxport == jxport );
    aprocs->envp = envp;
    MPASSIGN( aprocs->mpool, mpool );

    aprocs->tp.send = and_xport_send;
    aprocs->tp.rstatus = and_xport_relayStatus;
    aprocs->tp.rconnd = and_xport_relayConnd;
    aprocs->tp.rerror = and_xport_relayError;
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
