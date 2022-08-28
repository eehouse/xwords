/* -*-mode: C; compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/* 
 * Copyright 2001 - 2020 by Eric House (xwords@eehouse.org).  All rights
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
#include "dbgutil.h"
#include "paths.h"

typedef struct _AndTransportProcs {
    TransportProcs tp;
#ifdef MAP_THREAD_TO_ENV
    EnvThreadInfo* ti;
#endif
    jobject jxport;
    MPSLOT
} AndTransportProcs;

static XP_U32
and_xport_getFlags( XWEnv xwe, void* closure )
{
    jint result = COMMS_XPORT_FLAGS_NONE;
    AndTransportProcs* aprocs = (AndTransportProcs*)closure;
    ASSERT_ENV( aprocs->ti, xwe );
    if ( NULL != aprocs->jxport ) {
        JNIEnv* env = xwe;
        const char* sig = "()I";
        jmethodID mid = getMethodID( env, aprocs->jxport, "getFlags", sig );

        result = (*env)->CallIntMethod( env, aprocs->jxport, mid );
    }
    return result;
}

#ifdef XWFEATURE_COMMS_INVITE
static XP_S16
and_xport_sendInvite( XWEnv xwe, const NetLaunchInfo* nli,
                      XP_U32 createdStamp,
                      const CommsAddrRec* addr, void* closure )
{
    XP_USE( xwe );
    XP_USE( nli );
    XP_USE( createdStamp );
    XP_USE( addr );
    XP_USE( closure );
    XP_ASSERT(0);
    return -1;
}
#endif

static XP_S16
and_xport_send( XWEnv xwe, const XP_U8* buf, XP_U16 len,
                const XP_UCHAR* msgNo, XP_U32 timestamp,
                const CommsAddrRec* addr, CommsConnType conType,
                XP_U32 gameID, void* closure )
{
    jint result = -1;
    LOG_FUNC();
    AndTransportProcs* aprocs = (AndTransportProcs*)closure;
    ASSERT_ENV( aprocs->ti, xwe );
    if ( NULL != aprocs->jxport ) {
        JNIEnv* env = xwe;
        const char* sig = "([BLjava/lang/String;L" PKG_PATH("jni/CommsAddrRec")
            ";L" PKG_PATH("jni/CommsAddrRec$CommsConnType") ";II)I";

        jmethodID mid = getMethodID( env, aprocs->jxport, "transportSend", sig );

        jbyteArray jbytes = makeByteArray( env, len, (jbyte*)buf );
        jobject jaddr = makeJAddr( env, addr );
        jobject jConType = 
            intToJEnum(env, conType, PKG_PATH("jni/CommsAddrRec$CommsConnType"));
        jstring jMsgNo = !!msgNo ? (*env)->NewStringUTF( env, msgNo ) : NULL;
        result = (*env)->CallIntMethod( env, aprocs->jxport, mid, 
                                        jbytes, jMsgNo, jaddr, jConType,
                                        gameID, timestamp );
        deleteLocalRefs( env, jaddr, jbytes, jMsgNo, jConType, DELETE_NO_REF );
    }

    if ( result < len ) {
        XP_LOGFF( "changing result %d to -1", result );
        result = -1;            /* signal failure. Not sure where 0's coming from */
    }

    LOG_RETURNF( "%d", result );
    return result;
}

static void
and_xport_countChanged( XWEnv xwe, void* closure, XP_U16 count )
{
    AndTransportProcs* aprocs = (AndTransportProcs*)closure;
    ASSERT_ENV( aprocs->ti, xwe );
    if ( NULL != aprocs && NULL != aprocs->jxport ) {
        JNIEnv* env = xwe;
        const char* sig = "(I)V";
        jmethodID mid = getMethodID( env, aprocs->jxport, "countChanged", sig );
        (*env)->CallVoidMethod( env, aprocs->jxport, mid, count );
    }
}

#ifdef XWFEATURE_RELAY
static void
and_xport_relayStatus( XWEnv XP_UNUSED(xwe), void* XP_UNUSED(closure),
                       CommsRelayState XP_UNUSED(newState) )
{
    XP_ASSERT(0);
}

static void
and_xport_relayConnd( XWEnv xwe, void* closure, XP_UCHAR* const room,
                      XP_Bool reconnect, XP_U16 devOrder, XP_Bool allHere,
                      XP_U16 nMissing )
{
    XP_ASSERT(0);
    AndTransportProcs* aprocs = (AndTransportProcs*)closure;
    ASSERT_ENV( aprocs->ti, xwe );
    if ( NULL != aprocs->jxport ) {
        JNIEnv* env = xwe;
        const char* sig = "(Ljava/lang/String;IZI)V";
        jmethodID mid = getMethodID( env, aprocs->jxport, "relayConnd", sig );

        jstring str = (*env)->NewStringUTF( env, room );
        (*env)->CallVoidMethod( env, aprocs->jxport, mid, 
                                str, devOrder, allHere, nMissing );
        deleteLocalRef( env, str );
    }
}

static XP_Bool 
and_xport_sendNoConn( XWEnv xwe, const XP_U8* buf, XP_U16 len,
                      const XP_UCHAR* msgNo, const XP_UCHAR* relayID,
                      void* closure )
{
    XP_ASSERT(0);
    jboolean result = false;
    AndTransportProcs* aprocs = (AndTransportProcs*)closure;
    ASSERT_ENV( aprocs->ti, xwe );
    if ( NULL != aprocs && NULL != aprocs->jxport ) {
        JNIEnv* env = xwe;

        const char* sig = "([BLjava/lang/String;Ljava/lang/String;)Z";
        jmethodID mid = getMethodID( env, aprocs->jxport, 
                                     "relayNoConnProc", sig );
        jbyteArray jbytes = makeByteArray( env, len, (jbyte*)buf );
        jstring jRelayID = (*env)->NewStringUTF( env, relayID );
        jstring jMsgNo = !!msgNo ? (*env)->NewStringUTF( env, msgNo ) : NULL;
        result = (*env)->CallBooleanMethod( env, aprocs->jxport, mid, 
                                            jbytes, jMsgNo, jRelayID );
        deleteLocalRefs( env, jbytes, jRelayID, jMsgNo, DELETE_NO_REF );
    }
    LOG_RETURNF( "%s", boolToStr(result) );
    return result;
}

static void
and_xport_relayError( XWEnv xwe, void* closure, XWREASON relayErr )
{
    XP_ASSERT(0);
    AndTransportProcs* aprocs = (AndTransportProcs*)closure;
    ASSERT_ENV( aprocs->ti, xwe );
    if ( NULL != aprocs->jxport ) {
        JNIEnv* env = xwe;
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
#endif

TransportProcs*
makeXportProcs( MPFORMAL JNIEnv* env,
#ifdef MAP_THREAD_TO_ENV
                EnvThreadInfo* ti,
#endif
                jobject jxport )
{
    AndTransportProcs* aprocs = NULL;

    aprocs = (AndTransportProcs*)XP_CALLOC( mpool, sizeof(*aprocs) );
#ifdef MAP_THREAD_TO_ENV
    aprocs->ti = ti;
#endif
    if ( NULL != jxport ) {
        aprocs->jxport = (*env)->NewGlobalRef( env, jxport );
    }
    MPASSIGN( aprocs->mpool, mpool );

#ifdef COMMS_XPORT_FLAGSPROC
    aprocs->tp.getFlags = and_xport_getFlags;
#endif
    aprocs->tp.sendMsg = and_xport_send;
#ifdef XWFEATURE_COMMS_INVITE
    aprocs->tp.sendInvt = and_xport_sendInvite;
#endif
#ifdef XWFEATURE_RELAY
    aprocs->tp.rstatus = and_xport_relayStatus;
    aprocs->tp.rconnd = and_xport_relayConnd;
    aprocs->tp.rerror = and_xport_relayError;
    aprocs->tp.sendNoConn = and_xport_sendNoConn;
#endif
    aprocs->tp.countChanged = and_xport_countChanged;
    aprocs->tp.closure = aprocs;

    return (TransportProcs*)aprocs;
}

void
destroyXportProcs( TransportProcs** xport, JNIEnv* env )
{
    AndTransportProcs* aprocs = (AndTransportProcs*)*xport;
    if ( NULL != aprocs->jxport ) {
        (*env)->DeleteGlobalRef( env, aprocs->jxport );
    }

    XP_FREE( aprocs->mpool, aprocs );
    *xport = NULL;
}
