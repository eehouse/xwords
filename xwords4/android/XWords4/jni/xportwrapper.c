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
    jobject j_xport;
    MPSLOT
} AndTransportProcs;


static XP_S16
and_xport_send( const XP_U8* buf, XP_U16 len, const CommsAddrRec* addr,
                void* closure )
{
    LOG_FUNC();
    AndTransportProcs* aprocs = (AndTransportProcs*)closure;
    JNIEnv* env = *aprocs->envp;
    const char* sig = "([BLorg/eehouse/android/xw4/jni/CommsAddrRec;)I";
    jmethodID mid = getMethodID( env, aprocs->j_xport, "transportSend", sig );

    jbyteArray jbytes = makeByteArray( env, len, buf );

    jint result = (*env)->CallIntMethod( env, aprocs->j_xport, mid, 
                                         jbytes, NULL );
    (*env)->DeleteLocalRef( env, jbytes );
    return result;
}


static void
and_xport_relayStatus( void* closure, CommsRelayState newState )
{
    LOG_FUNC();
}

static void
and_xport_relayConnd( void* closure, XP_Bool allHere, XP_U16 nMissing )
{
    LOG_FUNC();
}

static void
and_xport_relayError( void* closure, XWREASON relayErr )
{
    LOG_FUNC();
}

TransportProcs*
makeXportProcs( MPFORMAL JNIEnv** envp, jobject jxport )
{
    AndTransportProcs* aprocs = NULL;

    XP_LOGF( "%s: jxport=%p", __func__, jxport );

    if ( NULL != jxport ) {
        JNIEnv* env = *envp;
        aprocs = (AndTransportProcs*)XP_CALLOC( mpool, sizeof(*aprocs) );
        aprocs->j_xport = (*env)->NewGlobalRef( env, jxport );
        XP_ASSERT( aprocs->j_xport == jxport );
        aprocs->envp = envp;
        MPASSIGN( aprocs->mpool, mpool );

        aprocs->tp.send = and_xport_send;
        aprocs->tp.rstatus = and_xport_relayStatus;
        aprocs->tp.rconnd = and_xport_relayConnd;
        aprocs->tp.rerror = and_xport_relayError;
        aprocs->tp.closure = aprocs;
    }

    LOG_RETURNF( "%p", aprocs );
    return (TransportProcs*)aprocs;
}

void
destroyXportProcs( TransportProcs* xport )
{
    if ( !!xport ) {
        AndTransportProcs* aprocs = (AndTransportProcs*)xport;
        JNIEnv* env = *aprocs->envp;
        (*env)->DeleteGlobalRef( env, aprocs->j_xport );

        XP_FREE( aprocs->mpool, aprocs );
    }
}
