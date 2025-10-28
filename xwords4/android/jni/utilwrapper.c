/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/* 
 * Copyright 2001 - 2023 by Eric House (xwords@eehouse.org).  All rights
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
#include <sys/time.h>

#include <jni.h>

#include "comtypes.h"
#include "device.h"
#include "utilwrapper.h"
#include "anddict.h"
#include "andutils.h"
#include "paths.h"
#include "LocalizedStrIncludes.h"
#include "dbgutil.h"
#include "nli.h"
#include "strutils.h"
#include "drawwrapper.h"

#define MAX_QUANTITY_STRS 4

typedef struct _AndDUtil {
    XW_DUtilCtxt dutil;
    JNIUtilCtxt* jniutil;
    jobject jdutil;  /* global ref to object implementing XW_DUtilCtxt */
    XP_UCHAR* userStrings[N_AND_USER_STRINGS];
    XP_U32 userStringsBits;
#ifdef MAP_THREAD_TO_ENV
    EnvThreadInfo* ti;
#endif
#ifdef XWFEATURE_DEVID
    XP_UCHAR* devIDStorage;
#endif
} AndDUtil;

typedef struct _TimerStorage {
    UtilTimerProc proc;
    void* closure;
} TimerStorage;

typedef struct _AndUtil {
    XW_UtilCtxt super;
    /* Cache these so they can be reused */
    DrawCtx* draw;
#ifdef MAP_THREAD_TO_ENV
    EnvThreadInfo* ti;
#endif
    jobject jutil;  /* global ref to object implementing XW_UtilCtxt */
    TimerStorage timerStorage[NUM_TIMERS_PLUS_ONE];
} AndUtil;

static void
and_util_destroy( XW_UtilCtxt* uc, XWEnv xwe )
{
    LOG_FUNC();
    AndUtil* autil = (AndUtil*)uc;
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = uc->_mpool;
#endif

    if ( NULL != autil->jutil ) {
        (*xwe)->DeleteGlobalRef( xwe, autil->jutil );
    }
    XP_FREE( mpool, uc->vtable );
    util_super_cleanup( uc, xwe );
    XP_FREE( mpool, autil );
    LOG_RETURN_VOID();
}

#define UTIL_CBK_HEADER(nam,sig)                                        \
    AndUtil* util = (AndUtil*)uc;                                       \
    JNIEnv* env = xwe;                                                  \
    ASSERT_ENV(util->ti, env);                                          \
    if ( NULL != util->jutil ) {                                        \
        jmethodID mid = getMethodID( env, util->jutil, nam, sig )

#define UTIL_CBK_TAIL()                                                 \
    } else {                                                            \
        XP_LOGFF( "skipping call into java because jutil==NULL" );      \
        XP_ASSERT(0); /* no longer happens? */                          \
    }

#define DUTIL_CBK_HEADER(nam,sig)                                       \
    AndDUtil* dutil = (AndDUtil*)duc;                                   \
    JNIEnv* env = xwe;                                                  \
    ASSERT_ENV(dutil->ti, env);                                         \
    if ( NULL != dutil->jdutil ) {                                      \
        jmethodID mid = getMethodID( env, dutil->jdutil, nam, sig )

#define DUTIL_CBK_TAIL() UTIL_CBK_TAIL()

static void
and_util_userError( XW_UtilCtxt* uc, XWEnv xwe, UtilErrID id )
{
    UTIL_CBK_HEADER( "userError", "(I)V" );
    (*env)->CallVoidMethod( env, util->jutil, mid, id );
    if ((*env)->ExceptionOccurred(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        XP_LOGFF( "exception found" );
    }
    UTIL_CBK_TAIL();
}

static void
and_util_notifyMove( XW_UtilCtxt* uc, XWEnv xwe, XWStreamCtxt* stream )
{
    UTIL_CBK_HEADER("notifyMove", "(Ljava/lang/String;)V" );

    jstring jstr = NULL;
    if ( NULL != stream ) {
        jstr = streamToJString( env, stream, XP_FALSE );
    }
    (*env)->CallVoidMethod( env, util->jutil, mid, jstr );
    deleteLocalRef( env, jstr );
    UTIL_CBK_TAIL();
}

static void
and_util_notifyTrade( XW_UtilCtxt* uc, XWEnv xwe, const XP_UCHAR** tiles, XP_U16 nTiles )
{
    UTIL_CBK_HEADER("notifyTrade", "([Ljava/lang/String;)V" );
    jobjectArray jtiles = makeStringArray( env, nTiles, tiles );
    (*env)->CallVoidMethod( env, util->jutil, mid, jtiles );
    deleteLocalRef( env, jtiles );
    UTIL_CBK_TAIL();
}

static void
and_util_notifyPickTileBlank( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 playerNum,
                              XP_U16 col, XP_U16 row,
                              const XP_UCHAR** tileFaces, XP_U16 nTiles )
{
    UTIL_CBK_HEADER("notifyPickTileBlank", "(III[Ljava/lang/String;)V" );

    jobject jtexts = makeStringArray( env, nTiles, tileFaces );

    (*env)->CallVoidMethod( env, util->jutil, mid, playerNum, col, row, jtexts );

    deleteLocalRef( env, jtexts );
    UTIL_CBK_TAIL();
}

static void
and_util_informNeedPickTiles( XW_UtilCtxt* uc, XWEnv xwe, XP_Bool isInitial,
                              XP_U16 player, XP_U16 nToPick,
                              XP_U16 nFaces, const XP_UCHAR** faces,
                              const XP_U16* counts )
{
    UTIL_CBK_HEADER("informNeedPickTiles",
                    "(ZII[Ljava/lang/String;[I)V" );
    jobject jtexts = makeStringArray( env, nFaces, faces );
    jobject jcounts = makeIntArray( env, nFaces, counts, sizeof(counts[0]) );

    (*env)->CallVoidMethod( env, util->jutil, mid, isInitial, player,
                            nToPick, jtexts, jcounts );

    deleteLocalRefs( env, jtexts, jcounts, DELETE_NO_REF );
    UTIL_CBK_TAIL();
} /* and_util_informNeedPickTiles */

static void
and_util_informNeedPassword( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 player,
                             const XP_UCHAR* name )
{
    UTIL_CBK_HEADER("informNeedPassword", "(ILjava/lang/String;)V" );

    jstring jname = (*env)->NewStringUTF( env, name );
    (*env)->CallVoidMethod( env, util->jutil, mid, player, jname );
    deleteLocalRef( env, jname );
    UTIL_CBK_TAIL();
}

static void
and_util_trayHiddenChange(XW_UtilCtxt* uc, XWEnv xwe,
                          XW_TrayVisState newState,
                          XP_U16 nVisibleRows )
{
}

static void
and_util_yOffsetChange(XW_UtilCtxt* uc, XWEnv xwe, XP_U16 maxOffset,
                       XP_U16 oldOffset, XP_U16 newOffset )
{
#if 0
    AndUtil* util = (AndUtil*)uc;
    JNIEnv* env = *util->env;
    const char* sig = "(III)V";
    jmethodID mid = getMethodID( env, util->jutil, "yOffsetChange", sig );

    (*env)->CallVoidMethod( env, util->jutil, mid, 
                            maxOffset, oldOffset, newOffset );
#endif
}

#ifdef XWFEATURE_TURNCHANGENOTIFY
static void
and_util_turnChanged( XW_UtilCtxt* uc, XWEnv xwe, XP_S16 turn )
{
    UTIL_CBK_HEADER( "turnChanged", "(I)V" );
    (*env)->CallVoidMethod( env, util->jutil, mid, turn );
    UTIL_CBK_TAIL();
}
#endif

static void
and_util_notifyDupStatus( XW_UtilCtxt* uc, XWEnv xwe, XP_Bool amHost, const XP_UCHAR* msg )
{
    UTIL_CBK_HEADER( "notifyDupStatus", "(ZLjava/lang/String;)V" );
    jstring jmsg = (*env)->NewStringUTF( env, msg );
    (*env)->CallVoidMethod( env, util->jutil, mid, amHost, jmsg );
    deleteLocalRefs( env, jmsg, DELETE_NO_REF );
    UTIL_CBK_TAIL();
}

static void
and_util_informUndo( XW_UtilCtxt* uc, XWEnv xwe )
{
    UTIL_CBK_HEADER( "informUndo", "()V" );
    (*env)->CallVoidMethod( env, util->jutil, mid );
    UTIL_CBK_TAIL();
}

static void
and_util_informNetDict( XW_UtilCtxt* uc, XWEnv xwe, const XP_UCHAR* isoCodeStr,
                        const XP_UCHAR* oldName, const XP_UCHAR* newName,
                        const XP_UCHAR* newSum, XWPhoniesChoice phoniesAction )
{
    LOG_FUNC();
    UTIL_CBK_HEADER( "informNetDict", 
                     "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
                     "Ljava/lang/String;L"
                     PKG_PATH("jni/CurGameInfo$XWPhoniesChoice") ";)V" );
    jstring jIsoCode = (*env)->NewStringUTF( env, isoCodeStr );
    jstring jnew = (*env)->NewStringUTF( env, newName );
    jstring jsum = (*env)->NewStringUTF( env, newSum );
    jstring jold = (*env)->NewStringUTF( env, oldName );
    jobject jphon = intToJEnum( env, phoniesAction, 
                                PKG_PATH("jni/CurGameInfo$XWPhoniesChoice") );

    (*env)->CallVoidMethod( env, util->jutil, mid, jIsoCode, jold, jnew, jsum,
                            jphon );
    deleteLocalRefs( env, jnew, jold, jsum, jphon, jIsoCode, DELETE_NO_REF );

    UTIL_CBK_TAIL();
}

#ifdef XWFEATURE_HILITECELL
static XP_Bool
and_util_hiliteCell( XW_UtilCtxt* uc, XP_U16 col, XP_U16 row )
{
    /* don't log; this is getting called a lot */
    return XP_TRUE;             /* means keep going */
}
#endif

#ifdef XWFEATURE_STOP_ENGINE
static XP_Bool
and_util_stopEngineProgress( XW_UtilCtxt* uc, XWEnv xwe )
{
    XP_Bool result = XP_FALSE;
    UTIL_CBK_HEADER("stopEngineProgress","()Z" );
    result = (*env)->CallBooleanMethod( env, util->jutil, mid );
    UTIL_CBK_TAIL();
    return result;
}
#endif

static XP_Bool
and_util_altKeyDown( XW_UtilCtxt* uc, XWEnv xwe )
{
    LOG_FUNC();
    return XP_FALSE;
}

XP_U32
and_dutil_getCurSeconds( XW_DUtilCtxt* XP_UNUSED(duc), XWEnv xwe )
{
    XP_U32 curSeconds = getCurSeconds( xwe );
    return curSeconds;
}

static DrawCtx*
and_dutil_getThumbDraw( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr )
{
    DrawCtx* result = NULL;
    DUTIL_CBK_HEADER("getThumbDraw", "(I)Ljava/lang/Object;" );
    const CurGameInfo* gi = gr_getGI(duc, gr, xwe);
    jobject jdraw = (*env)->CallObjectMethod( env, dutil->jdutil, mid,
                                              gi->boardSize );
    result = makeDraw( xwe, jdraw, DT_THUMB );
    UTIL_CBK_TAIL();
    return result;
}

static const XP_UCHAR*
and_dutil_getUserString( XW_DUtilCtxt* duc, XWEnv xwe, XP_U16 stringCode )
{
    XP_UCHAR* result = "";
    DUTIL_CBK_HEADER("getUserString", "(I)Ljava/lang/String;" );
    int index = stringCode - 1; /* see LocalizedStrIncludes.h */
    XP_ASSERT( index < VSIZE( dutil->userStrings ) );

    XP_ASSERT( 0 == (dutil->userStringsBits & (1 << index)) );

    if ( ! dutil->userStrings[index] ) {
        jstring jresult = (*env)->CallObjectMethod( env, dutil->jdutil, mid,
                                                    stringCode );
        jsize len = (*env)->GetStringUTFLength( env, jresult );
        XP_UCHAR* buf = XP_MALLOC( dutil->dutil.mpool, len + 1 );

        const char* jchars = (*env)->GetStringUTFChars( env, jresult, NULL );
        XP_MEMCPY( buf, jchars, len );
        buf[len] = '\0';
        (*env)->ReleaseStringUTFChars( env, jresult, jchars );
        deleteLocalRef( env, jresult );
        dutil->userStrings[index] = buf;
    }

    result = dutil->userStrings[index];
    DUTIL_CBK_TAIL();
    return result;
}

static const XP_UCHAR*
and_dutil_getUserQuantityString( XW_DUtilCtxt* duc, XWEnv xwe,
                                 XP_U16 stringCode, XP_U16 quantity )
{
    XP_UCHAR* result = "";
    DUTIL_CBK_HEADER("getUserQuantityString", "(II)Ljava/lang/String;" );
    int index = stringCode - 1; /* see LocalizedStrIncludes.h */
    XP_ASSERT( index < VSIZE( dutil->userStrings ) );
    XP_UCHAR** ptrs;

    dutil->userStringsBits |= 1 << index;
    ptrs = (XP_UCHAR**)dutil->userStrings[index];
    if ( !ptrs ) {
        ptrs = (XP_UCHAR**)XP_CALLOC( dutil->dutil.mpool, MAX_QUANTITY_STRS * sizeof(*ptrs) );
        dutil->userStrings[index] = (XP_UCHAR*)ptrs;
    }

    jstring jresult = (*env)->CallObjectMethod( env, dutil->jdutil, mid,
                                                stringCode, quantity );
    const char* jchars = (*env)->GetStringUTFChars( env, jresult, NULL );
    int indx = 0;
    for ( ; indx < MAX_QUANTITY_STRS; ++indx ) {
        if ( !ptrs[indx] ) {
            XP_LOGFF( "found empty slot %d for %s", indx, jchars );
            break;
        } else if ( 0 == XP_STRCMP( jchars, ptrs[indx] ) ) {
            XP_LOGFF( "found %s at slot %d", jchars, indx );
            break;
        }
    }

    if ( !ptrs[indx] ) {
        XP_ASSERT( indx < MAX_QUANTITY_STRS );
        jsize len = (*env)->GetStringUTFLength( env, jresult );
        XP_UCHAR* buf = XP_MALLOC( dutil->dutil.mpool, len + 1 );
        XP_MEMCPY( buf, jchars, len );
        buf[len] = '\0';
        ptrs[indx] = buf;
    }

    (*env)->ReleaseStringUTFChars( env, jresult, jchars );
    deleteLocalRef( env, jresult );

    result = ptrs[indx];
    DUTIL_CBK_TAIL();
    return result;
}

static void
and_dutil_storePtr( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                    const void* data, XP_U32 len )
{
    XP_LOGFF( "(key: %s; len: %d)", key, len );
    DUTIL_CBK_HEADER( "store", "(Ljava/lang/String;[B)V" );

    jbyteArray jdata = makeByteArray( env, len, data );
    jstring jkey = (*env)->NewStringUTF( env, key );

    (*env)->CallVoidMethod( env, dutil->jdutil, mid, jkey, jdata );

    deleteLocalRefs( env, jdata, jkey, DELETE_NO_REF );

    DUTIL_CBK_TAIL();
    LOG_RETURN_VOID();
}

static jbyteArray
loadToByteArray( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key )
{
    jbyteArray result = NULL;
    DUTIL_CBK_HEADER( "load", "(Ljava/lang/String;)[B");

    jstring jkey = (*env)->NewStringUTF( env, key );
    result = (*env)->CallObjectMethod( env, dutil->jdutil, mid, jkey );
    deleteLocalRef( env, jkey );
    DUTIL_CBK_TAIL();
    return result;
}

static void
and_dutil_loadPtr( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                   void* data, XP_U32* lenp )
{
    JNIEnv* env = xwe;
    jbyteArray jvalue = loadToByteArray( duc, env, key );
    jsize len = 0;
    if ( jvalue != NULL ) {
        len = (*env)->GetArrayLength( env, jvalue );
        XP_LOGFF( "got %d bytes from storage", len );
        if ( len <= *lenp ) {
            jbyte* jelems = (*env)->GetByteArrayElements( env, jvalue, NULL );
            XP_MEMCPY( data, jelems, len );
            (*env)->ReleaseByteArrayElements( env, jvalue, jelems, 0 );
        }
        deleteLocalRef( env, jvalue );
    }
    XP_LOGFF( "(key: %s, data: %p) => len: %d)", key, data, len );
    *lenp = len;
}

static void
and_dutil_removeStored( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key )
{
    DUTIL_CBK_HEADER( "removeStored", "(Ljava/lang/String;)V");
    jstring jkey = (*env)->NewStringUTF( env, key );
    (*env)->CallVoidMethod( env, dutil->jdutil, mid, jkey );
    deleteLocalRef( env, jkey );
    DUTIL_CBK_TAIL();
}

static void
and_dutil_getKeysLike( XW_DUtilCtxt* duc, XWEnv xwe,
                       const XP_UCHAR* pattern, OnGotKey proc,
                       void* closure )
{
    LOG_FUNC();
    DUTIL_CBK_HEADER( "getKeysLike", "(Ljava/lang/String;)[Ljava/lang/String;");

    jstring jpat = (*env)->NewStringUTF( env, pattern );
    jobject jkeys = (*env)->CallObjectMethod( env, dutil->jdutil, mid, jpat );

    jsize len = (*env)->GetArrayLength( env, jkeys );
    for ( int ii = 0; ii < len; ++ii ) {
        jobject jkey = (*env)->GetObjectArrayElement( env, jkeys, ii );

        const char* jchars = (*env)->GetStringUTFChars( env, jkey, NULL );
        (*proc)(jchars, closure, xwe);
        (*env)->ReleaseStringUTFChars( env, jkey, jchars );

        deleteLocalRef( env, jkey );
    }
    deleteLocalRefs( env, jkeys, jpat, DELETE_NO_REF );
    DUTIL_CBK_TAIL();
}

#ifdef XWFEATURE_DEVICE
static void
and_dutil_forEach( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* keys[],
                   OnOneProc proc, void* closure )
{
    XP_ASSERT(0);
}

/* static void */
/* and_dutil_remove( XW_DUtilCtxt* duc, const XP_UCHAR* keys[] ) */
/* { */
/*     XP_ASSERT(0); */
/* } */
#endif

static void
and_util_notifyIllegalWords( XW_UtilCtxt* uc, XWEnv xwe,
                             const BadWordInfo* bwi,
                             const XP_UCHAR* dictName,
                             XP_U16 turn, XP_Bool turnLost,
                             XP_U32 badWordsKey )
{
    UTIL_CBK_HEADER("notifyIllegalWords",
                    "(Ljava/lang/String;[Ljava/lang/String;IZI)V" );
    XP_ASSERT( bwi->nWords > 0 );
    if ( bwi->nWords > 0 ) {
        jobjectArray jwords = makeStringArray( env, bwi->nWords, 
                                               (const XP_UCHAR**)bwi->words );
        XP_ASSERT( !!dictName );
        jstring jname = (*env)->NewStringUTF( env, dictName );
        (*env)->CallVoidMethod( env, util->jutil, mid,
                                jname, jwords, turn, turnLost, badWordsKey );
        deleteLocalRefs( env, jwords, jname, DELETE_NO_REF );
    }
    UTIL_CBK_TAIL();
}

static XP_Bool
and_util_showChat( XW_UtilCtxt* uc, XWEnv xwe, const XP_UCHAR* msg,
                   XP_S16 fromIndx, XP_U32 timestamp )
{
    XP_Bool result = XP_FALSE;
    UTIL_CBK_HEADER( "showChat", "(Ljava/lang/String;II)Z" );

    jstring jmsg = (*env)->NewStringUTF( env, msg );
    result = (*env)->CallBooleanMethod( env, util->jutil, mid, jmsg, fromIndx,
                                        timestamp );
    deleteLocalRef( env, jmsg );

    UTIL_CBK_TAIL();
    return result;
}

static void
and_util_remSelected( XW_UtilCtxt* uc, XWEnv xwe )
{
    UTIL_CBK_HEADER("remSelected", "()V" );
    (*env)->CallVoidMethod( env, util->jutil, mid );
    UTIL_CBK_TAIL();
}

static void
and_util_timerSelected( XW_UtilCtxt* uc, XWEnv xwe, XP_Bool inDuplicateMode, XP_Bool canPause )
{
    UTIL_CBK_HEADER("timerSelected", "(ZZ)V" );
    (*env)->CallVoidMethod( env, util->jutil, mid, inDuplicateMode, canPause );
    UTIL_CBK_TAIL();
}

static void
and_util_formatPauseHistory( XW_UtilCtxt* uc, XWEnv xwe, XWStreamCtxt* stream,
                             DupPauseType typ, XP_S16 turn,
                             XP_U32 secsPrev, XP_U32 secsCur,
                             const XP_UCHAR* msg )
{
    UTIL_CBK_HEADER( "formatPauseHistory",
                     "(IIIILjava/lang/String;)Ljava/lang/String;" );
    jstring jmsg = !! msg ? (*env)->NewStringUTF( env, msg ) : NULL;

    jstring jresult = (*env)->CallObjectMethod( env, util->jutil, mid, typ,
                                                turn, secsPrev, secsCur, jmsg );

    const char* jchars = (*env)->GetStringUTFChars( env, jresult, NULL );
    stream_catString( stream, jchars );
    (*env)->ReleaseStringUTFChars( env, jresult, jchars );
    deleteLocalRefs( env, jresult, jmsg, DELETE_NO_REF );
    UTIL_CBK_TAIL();
}

static void
and_util_dictGone( XW_UtilCtxt* uc, XWEnv xwe, const XP_UCHAR* dictName )
{
    UTIL_CBK_HEADER( "dictGone", "(Ljava/lang/String;)V" );
    jstring jname = (*env)->NewStringUTF( env, dictName );
    (*env)->CallVoidMethod( env, util->jutil, mid, jname );
    deleteLocalRef( env, jname );
    UTIL_CBK_TAIL();
}

#ifndef XWFEATURE_MINIWIN
static void
and_util_bonusSquareHeld( XW_UtilCtxt* uc, XWEnv xwe, XWBonusType bonus )
{
    UTIL_CBK_HEADER( "bonusSquareHeld", "(I)V" );
    (*env)->CallVoidMethod( env, util->jutil, mid, bonus );
    UTIL_CBK_TAIL();
}

static void
and_util_playerScoreHeld( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 player )
{
    UTIL_CBK_HEADER( "playerScoreHeld", "(I)V" );
    (*env)->CallVoidMethod( env, util->jutil, mid, player );
    UTIL_CBK_TAIL();
}
#endif

#ifdef XWFEATURE_SMS
static XP_Bool
and_dutil_phoneNumbersSame( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* p1,
                            const XP_UCHAR* p2 )
{
    XP_Bool same = 0 == strcmp( p1, p2 );
    if ( !same ) {
        DUTIL_CBK_HEADER( "phoneNumbersSame",
                          "(Ljava/lang/String;Ljava/lang/String;)Z" );
        jstring js1 = (*env)->NewStringUTF( env, p1 );
        jstring js2 = (*env)->NewStringUTF( env, p2 );
        same = (*env)->CallBooleanMethod( env, dutil->jdutil, mid, js1, js2 );
        deleteLocalRefs( env, js1, js2, DELETE_NO_REF );
        DUTIL_CBK_TAIL();
    }
    return same;
}
#endif

#ifdef XWFEATURE_BOARDWORDS
static void
and_util_cellSquareHeld( XW_UtilCtxt* uc, XWEnv xwe, XWStreamCtxt* words )
{
    if ( NULL != words ) {
        UTIL_CBK_HEADER( "cellSquareHeld", "(Ljava/lang/String;)V" );
        jstring jwords = streamToJString( env, words, XP_FALSE );
        (*env)->CallVoidMethod( env, util->jutil, mid, jwords );
        deleteLocalRef( env, jwords );
        UTIL_CBK_TAIL();
    }
}
#endif

/* static void */
/* and_util_informMissing( XW_UtilCtxt* uc, XWEnv xwe, XP_Bool isServer, */
/*                         const CommsAddrRec* hostAddr, */
/*                         const CommsAddrRec* selfAddr, XP_U16 nDevs, */
/*                         XP_U16 nMissing, XP_U16 nInvited, XP_Bool fromRematch ) */
/* { */
/*     UTIL_CBK_HEADER( "informMissing",  */
/*                      "(ZL" PKG_PATH("jni/CommsAddrRec") ";" */
/*                      "L" PKG_PATH("jni/CommsAddrRec$CommsConnTypeSet") ";IIIZ)V" ); */
/*     jobject jHostAddr = NULL; */
/*     if ( !!hostAddr ) { */
/*         jHostAddr = makeJAddr( env, hostAddr ); */
/*     } */

/*     jobject jtypset = NULL; */
/*     if ( !!selfAddr ) { */
/*         jtypset = addrTypesToJ( env, selfAddr ); */
/*     } */
/*     (*env)->CallVoidMethod( env, util->jutil, mid, isServer, jHostAddr, */
/*                             jtypset, nDevs, nMissing, nInvited, fromRematch ); */
/*     deleteLocalRefs( env, jHostAddr, jtypset, DELETE_NO_REF ); */
/*     UTIL_CBK_TAIL(); */
/* } */

static void
and_util_informWordsBlocked( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 nBadWords,
                             XWStreamCtxt* words, const XP_UCHAR* dict )
{
    UTIL_CBK_HEADER( "informWordsBlocked", "(ILjava/lang/String;Ljava/lang/String;)V" );
    jstring jwords = streamToJString( env, words, XP_FALSE );
    jstring jdict = (*env)->NewStringUTF( env, dict );
    (*env)->CallVoidMethod( env, util->jutil, mid, nBadWords, jwords, jdict );
    deleteLocalRefs( env, jwords, jdict, DELETE_NO_REF );
    UTIL_CBK_TAIL();
}

/* static void */
/* and_util_getInviteeName( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 plyrNum, */
/*                          XP_UCHAR* buf, XP_U16* bufLen ) */
/* { */
/*     XP_SNPRINTF( buf, *bufLen, "InvitEE %d", plyrNum ); */
/*     /\* UTIL_CBK_HEADER( "getInviteeName", "(I)Ljava/lang/String;" ); *\/ */
/*     /\* jstring jresult = (*env)->CallObjectMethod( env, util->jutil, mid, plyrNum ); *\/ */
/*     /\* if ( NULL != jresult ) { *\/ */
/*     /\*     jsize len = (*env)->GetStringUTFLength( env, jresult ); *\/ */
/*     /\*     if ( len < *bufLen ) { *\/ */
/*     /\*         const char* jchars = (*env)->GetStringUTFChars( env, jresult, NULL ); *\/ */
/*     /\*         XP_STRCAT( buf, jchars ); *\/ */
/*     /\*         (*env)->ReleaseStringUTFChars( env, jresult, jchars ); *\/ */
/*     /\*         *bufLen = len; *\/ */
/*     /\*     } else { *\/ */
/*     /\*         *bufLen = 0; *\/ */
/*     /\*     } *\/ */
/*     /\*     deleteLocalRef( env, jresult ); *\/ */
/*     /\* } *\/ */
/*     /\* UTIL_CBK_TAIL(); *\/ */
/* } */

#if defined XWFEATURE_DEVID && defined XWFEATURE_RELAY
static const XP_UCHAR*
and_dutil_getDevID( XW_DUtilCtxt* duc, XWEnv xwe, DevIDType* typ )
{
    const XP_UCHAR* result = NULL;
    *typ = ID_TYPE_NONE;
    DUTIL_CBK_HEADER( "getDevID", "([B)Ljava/lang/String;" );
    jbyteArray jbarr = makeByteArray( env, 1, NULL );
    jstring jresult = (*env)->CallObjectMethod( env, dutil->jdutil, mid, jbarr );
    if ( NULL != jresult ) {
        const char* jchars = (*env)->GetStringUTFChars( env, jresult, NULL );
        jsize len = (*env)->GetStringUTFLength( env, jresult );
        if ( NULL != dutil->devIDStorage
             && 0 == XP_MEMCMP( dutil->devIDStorage, jchars, len ) ) {
            XP_LOGFF( "already have matching devID" );
        } else {
            XP_LOGFF( "allocating storage for devID" );
            XP_FREEP( dutil->dutil.mpool, &dutil->devIDStorage );
            dutil->devIDStorage = XP_MALLOC( dutil->dutil.mpool, len + 1 );
            XP_MEMCPY( dutil->devIDStorage, jchars, len );
            dutil->devIDStorage[len] = '\0';
        }
        (*env)->ReleaseStringUTFChars( env, jresult, jchars );
        result = (const XP_UCHAR*)dutil->devIDStorage;

        jbyte* elems = (*env)->GetByteArrayElements( env, jbarr, NULL );
        *typ = (DevIDType)elems[0];
        (*env)->ReleaseByteArrayElements( env, jbarr, elems, 0 );
    }
    deleteLocalRefs( env, jbarr, jresult, DELETE_NO_REF );
    DUTIL_CBK_TAIL();
    return result;
}

static void
and_dutil_deviceRegistered( XW_DUtilCtxt* duc, XWEnv xwe, DevIDType typ,
                            const XP_UCHAR* idRelay )
{
    DUTIL_CBK_HEADER( "deviceRegistered",
                      "(L" PKG_PATH("jni/DUtilCtxt$DevIDType") ";Ljava/lang/String;)V" );
    jstring jstr = (*env)->NewStringUTF( env, idRelay );
    jobject jtyp = intToJEnum( env, typ, 
                               PKG_PATH("jni/DUtilCtxt$DevIDType") );
    (*env)->CallVoidMethod( env, dutil->jdutil, mid, jtyp, jstr );
    deleteLocalRefs( env, jstr, jtyp, DELETE_NO_REF );
    DUTIL_CBK_TAIL();
}
#endif  /* XWFEATURE_DEVID && XWFEATURE_RELAY */

#ifdef XWFEATURE_SEARCHLIMIT
static XP_Bool
and_util_getTraySearchLimits(XW_UtilCtxt* uc, XP_U16* min, XP_U16* max )
{
    LOG_FUNC();
    foobar;                     /* this should not be compiling */
}

#endif

#ifdef SHOW_PROGRESS
static void
and_util_engineStarting( XW_UtilCtxt* uc, XP_U16 nBlanks )
{
    UTIL_CBK_HEADER("engineStarting", "(I)V" );
    (*env)->CallVoidMethod( env, util->jutil, mid, nBlanks );
    UTIL_CBK_TAIL();
}

static void
and_util_engineStopping( XW_UtilCtxt* uc )
{
    UTIL_CBK_HEADER("engineStopping", "()V" );
    (*env)->CallVoidMethod( env, util->jutil, mid );
    UTIL_CBK_TAIL();
}
#endif

/* static XW_DUtilCtxt* */
/* and_util_getDevUtilCtxt( XW_UtilCtxt* uc, XWEnv xwe ) */
/* { */
/*     AndGameGlobals* globals = (AndGameGlobals*)uc->closure; */
/*     XP_ASSERT( !!globals->dutil ); */
/*     return globals->dutil; */
/* } */

static void
and_dutil_md5sum( XW_DUtilCtxt* duc, XWEnv xwe, const XP_U8* ptr, XP_U32 len,
                  Md5SumBuf* sb )
{
    /* will be signed in the java world, and negative is bad! */
    XP_ASSERT( (0x80000000 & len) == 0 );
    AndDUtil* dutil = (AndDUtil*)duc;
    JNIEnv* env = xwe;
    struct JNIUtilCtxt* jniutil = dutil->jniutil;
    jstring jsum = and_util_getMD5SumForBytes( jniutil, env, ptr, len );

    if ( !!jsum ) {
        const char* jchars = (*env)->GetStringUTFChars( env, jsum, NULL );
        (void)XP_SNPRINTF( sb->buf, VSIZE(sb->buf), "%s", jchars );
        (*env)->ReleaseStringUTFChars( env, jsum, jchars );
        deleteLocalRef( env, jsum );
    } else {
        sb->buf[0] = '\0';
    }

}

static void
and_dutil_setTimer( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 when, TimerKey key )
{
    // XP_LOGFF( "(key=%d)", key );
    DUTIL_CBK_HEADER( "setTimer", "(II)V" );
    (*env)->CallVoidMethod( env, dutil->jdutil, mid, when, key );
    DUTIL_CBK_TAIL();
}

static void
and_dutil_clearTimer( XW_DUtilCtxt* duc, XWEnv xwe, TimerKey key )
{
    DUTIL_CBK_HEADER( "clearTimer", "(I)V" );
    (*env)->CallVoidMethod( env, dutil->jdutil, mid, key );
    DUTIL_CBK_TAIL();
}

static void
and_dutil_getUsername( XW_DUtilCtxt* duc, XWEnv xwe, XP_U16 num,
                       XP_Bool isLocal, XP_Bool isRobot,
                       XP_UCHAR* buf, XP_U16* lenp )
{
    DUTIL_CBK_HEADER( "getUsername", "(IZZ)Ljava/lang/String;" );

    jstring jresult = (*env)->CallObjectMethod( env, dutil->jdutil, mid,
                                                num, isLocal, isRobot );
    jsize len = (*env)->GetStringUTFLength( env, jresult );
    if ( len < *lenp ) {
        const char* jchars = (*env)->GetStringUTFChars( env, jresult, NULL );
        *lenp = XP_SNPRINTF( buf, len+1, "%s", jchars );
        (*env)->ReleaseStringUTFChars( env, jresult, jchars );
        deleteLocalRef( env, jresult );
    }
    DUTIL_CBK_TAIL();
}

static void
and_dutil_getSelfAddr( XW_DUtilCtxt* duc, XWEnv xwe, CommsAddrRec* addr )
{
    DUTIL_CBK_HEADER( "getSelfAddr", "()L" PKG_PATH("jni/CommsAddrRec") ";" );
    jobject jaddr = (*env)->CallObjectMethod( env, dutil->jdutil, mid );
    getJAddrRec( env, addr, jaddr );
    deleteLocalRef( env, jaddr );
    DUTIL_CBK_TAIL();
}

static void
and_dutil_notifyPause( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr,
                       DupPauseType pauseTyp, XP_U16 pauser,
                       const XP_UCHAR* name, const XP_UCHAR* msg )
{
    DUTIL_CBK_HEADER( "notifyPause",
                      "(JIILjava/lang/String;Ljava/lang/String;)V" );
    jstring jname = (*env)->NewStringUTF( env, name );
    jstring jmsg = (*env)->NewStringUTF( env, msg );
    (*env)->CallVoidMethod( env, dutil->jdutil, mid, gr, pauseTyp, pauser,
                            jname, jmsg );
    deleteLocalRefs( env, jname, jmsg, DELETE_NO_REF );
    DUTIL_CBK_TAIL();
}

static void
and_dutil_informMove( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr, XP_S16 turn,
                      XWStreamCtxt* expl, XWStreamCtxt* words )
{
    DUTIL_CBK_HEADER( "informMove",
                      "(JILjava/lang/String;Ljava/lang/String;)V" );
    jstring jexpl = streamToJString( env, expl, XP_FALSE );
    jstring jwords = !!words ? streamToJString( env, words, XP_FALSE ) : NULL;
    (*env)->CallVoidMethod( env, dutil->jdutil, mid, gr, turn, jexpl, jwords );
    deleteLocalRefs( env, jexpl, jwords, DELETE_NO_REF );
    DUTIL_CBK_TAIL();
}

static void
and_dutil_notifyGameOver( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr,
                          XP_S16 XP_UNUSED(quitter) )
{
    DUTIL_CBK_HEADER( "notifyGameOver", "(J)V" );
    (*env)->CallVoidMethod( env, dutil->jdutil, mid, gr );
    DUTIL_CBK_TAIL();
}

static void
and_dutil_onDupTimerChanged( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr,
                             XP_U32 oldVal, XP_U32 newVal )
{
    DUTIL_CBK_HEADER( "onDupTimerChanged", "(JII)V" );
    (*env)->CallVoidMethod( env, dutil->jdutil, mid, gr, oldVal, newVal );
    DUTIL_CBK_TAIL();
}

static void
and_dutil_onGroupChanged( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp,
                          GroupChangeEvents flags )
{
    DUTIL_CBK_HEADER( "onGroupChanged", "(II)V" );
    (*env)->CallVoidMethod( env, dutil->jdutil, mid, grp, flags );
    DUTIL_CBK_TAIL();
}

static void
and_dutil_onCtrlReceived( XW_DUtilCtxt* duc, XWEnv xwe, const XP_U8* buf, XP_U16 len )
{
    DUTIL_CBK_HEADER( "onCtrlReceived", "([B)V" );
    jbyteArray jmsg = makeByteArray( env, len, (jbyte*)buf );
    (*env)->CallVoidMethod( env, dutil->jdutil, mid, jmsg );
    deleteLocalRef( env, jmsg );
    DUTIL_CBK_TAIL();
}

static void
and_dutil_onGameGoneReceived( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 gameID,
                              const CommsAddrRec* from )
{
    DUTIL_CBK_HEADER( "onGameGoneReceived",
                      "(IL" PKG_PATH("jni/CommsAddrRec") ";)V" );
    jobject jaddr = makeJAddr( env, from );
    (*env)->CallVoidMethod( env, dutil->jdutil, mid, gameID, jaddr );

    deleteLocalRefs( env, jaddr, DELETE_NO_REF );
    DUTIL_CBK_TAIL();
}

static void
and_dutil_sendViaWeb( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 resultKey,
                      const XP_UCHAR* api, const cJSON* params )
{
    DUTIL_CBK_HEADER( "sendViaWeb", "(ILjava/lang/String;Ljava/lang/String;)V" );
    char* pstr = cJSON_PrintUnformatted( params );
    jstring jParams = (*env)->NewStringUTF( env, pstr );
    jstring jApi = (*env)->NewStringUTF( env, api );
    (*env)->CallVoidMethod( env, dutil->jdutil, mid, resultKey, jApi, jParams );
    deleteLocalRefs( env, jApi, jParams, DELETE_NO_REF );
    free( pstr );
    DUTIL_CBK_TAIL();
}

const DictionaryCtxt*
and_dutil_makeDict( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* dictName )
{
    DictionaryCtxt* dict = NULL;
    XP_LOGFF( "(name: %s)", dictName );
    DUTIL_CBK_HEADER( "getDictPath",
                      "(Ljava/lang/String;[Ljava/lang/String;[[B)V" );

    jstring jname = (*env)->NewStringUTF( env, dictName );

    jobjectArray jstrs = makeStringArray( env, 1, NULL );
    jobjectArray jbytes = makeByteArrayArray( env, 1 );

    (*env)->CallVoidMethod( env, dutil->jdutil, mid, jname, jstrs, jbytes );

    jstring jpath = (*env)->GetObjectArrayElement( env, jstrs, 0 );
    jbyteArray jdata = (*env)->GetObjectArrayElement( env, jbytes, 0 );

    if ( !!jpath || !!jdata ) {
        dict = makeDict( MPPARM(duc->mpool) xwe,
                         TI_IF( ((AndDUtil*)duc)->ti )
                         ((AndDUtil*)duc)->jniutil,
                         jname, jdata, jpath, NULL, false );
    }
    deleteLocalRefs( env, jname, jstrs, jbytes, jdata, jpath, DELETE_NO_REF );
    DUTIL_CBK_TAIL();
    return dict;
}

static DictionaryCtxt*
and_dutil_makeEmptyDict( XW_DUtilCtxt* duc, XWEnv xwe )
{
#ifdef STUBBED_DICT
    XP_ASSERT(0);
#else
    AndDUtil* dutil = (AndDUtil*)duc;
    JNIUtilCtxt* jniutil = dutil->jniutil;
    DictionaryCtxt* result =
        and_dictionary_make_empty( MPPARM(duc->mpool) jniutil );
    return (DictionaryCtxt*)dict_ref( result );
#endif
}

static cJSON*
and_dutil_getRegValues( XW_DUtilCtxt* duc, XWEnv xwe )
{
    cJSON* result = NULL;
    DUTIL_CBK_HEADER( "getRegValues", "()Ljava/lang/String;" );
    jstring jresult = (*env)->CallObjectMethod( env, dutil->jdutil, mid );
    const char* jchars = (*env)->GetStringUTFChars( env, jresult, NULL );
    result = cJSON_Parse( jchars );
    (*env)->ReleaseStringUTFChars( env, jresult, jchars );
    deleteLocalRef( env, jresult );
    DUTIL_CBK_TAIL();
    return result;
}

static void
and_dutil_missingDictAdded( XW_DUtilCtxt* duc, XWEnv xwe,
                            GameRef gr, const XP_UCHAR* dictName )
{
    XP_LOGFF( "(" GR_FMT ", %s)", gr, dictName );
    XP_LOGFF( "(dictName: %s)", dictName );
    DUTIL_CBK_HEADER( "missingDictAdded", "(JLjava/lang/String;)V" );
    XP_LOGFF( "(dictName: %s)", dictName );
    jstring jname = (*env)->NewStringUTF( env, dictName );
    (*env)->CallVoidMethod( env, dutil->jdutil, mid, gr, jname );
    deleteLocalRef( env, jname );

    DUTIL_CBK_TAIL();
    LOG_RETURN_VOID();
}

static void
and_dutil_dictGone( XW_DUtilCtxt* duc, XWEnv xwe,
                    GameRef gr, const XP_UCHAR* dictName )
{
    XP_LOGFF( "(dictName: %s)", dictName );
    DUTIL_CBK_HEADER( "dictGone", "(JLjava/lang/String;)V" );
    XP_LOGFF( "(dictName: %s)", dictName );
    jstring jname = (*env)->NewStringUTF( env, dictName );
    (*env)->CallVoidMethod( env, dutil->jdutil, mid, gr, jname );
    deleteLocalRef( env, jname );

    DUTIL_CBK_TAIL();
    LOG_RETURN_VOID();
}

static void
and_dutil_startMQTTListener( XW_DUtilCtxt* duc, XWEnv xwe,
                             const MQTTDevID* devID,
                             const XP_UCHAR** topics, XP_U8 qos )
{
    LOG_FUNC();
    DUTIL_CBK_HEADER( "startMQTTListener", "("
                      "Ljava/lang/String;"
                      "[Ljava/lang/String;"
                      "I)V" );
    XP_UCHAR buf[64];
    formatMQTTDevID( devID, buf, VSIZE(buf) );
    jstring jdevID = (*env)->NewStringUTF( env, buf );

    XP_U16 nTopics = 0;
    for ( int ii = 0; ; ++ii ) {
        if (!topics[ii]) { break; }
        ++nTopics;
    }
    jobjectArray jTopics = makeStringArray( env, nTopics, topics );

    (*env)->CallVoidMethod( env, dutil->jdutil, mid, jdevID, jTopics, qos );
    deleteLocalRefs( env, jdevID, jTopics, DELETE_NO_REF );
    DUTIL_CBK_TAIL();
}

static XP_S16
and_dutil_sendViaMQTT( XW_DUtilCtxt* duc, XWEnv xwe,
                       const XP_UCHAR* topic, const XP_U8* buf,
                       XP_U16 len, XP_U8 qos )
{
    XP_LOGFF( "(topic: %s)", topic );
    DUTIL_CBK_HEADER( "sendViaMQTT", "("
                      "Ljava/lang/String;"
                      "[B"
                      "I)V" );
    jbyteArray jmsg = makeByteArray( env, len, (jbyte*)buf );
    jstring jtopic = (*env)->NewStringUTF( env, topic );

    (*env)->CallVoidMethod( env, dutil->jdutil, mid, jtopic, jmsg, qos );

    deleteLocalRefs( env, jmsg, jtopic, DELETE_NO_REF );
    DUTIL_CBK_TAIL();
    return -1;
}

static XP_S16
and_dutil_sendViaBT( XW_DUtilCtxt* duc, XWEnv xwe,
                     const XP_U8* buf, XP_U16 len,
                     const XP_UCHAR* hostName,
                     const XP_BtAddrStr* btAddr )
{
    XP_LOGFF( "sending %d bytes to %s", len, hostName );
    DUTIL_CBK_HEADER( "sendViaBT", "("
                      "[B"
                      "Ljava/lang/String;"
                      "Ljava/lang/String;"
                      ")V" );
    jbyteArray jmsg = makeByteArray( env, len, (jbyte*)buf );
    jstring jname = (*env)->NewStringUTF( env, hostName );
    jstring jaddr = (*env)->NewStringUTF( env, btAddr->chars );

    (*env)->CallVoidMethod( env, dutil->jdutil, mid, jmsg, jname, jaddr );

    deleteLocalRefs( env, jmsg, jname, jaddr, DELETE_NO_REF );
    DUTIL_CBK_TAIL();
    return -1;
}

static XP_S16
and_dutil_sendViaNBS( XW_DUtilCtxt* duc, XWEnv xwe, const XP_U8* buf,
                      XP_U16 len, const XP_UCHAR* phone, XP_U16 port )
{
    LOG_FUNC();
    DUTIL_CBK_HEADER( "sendViaNBS", "("
                      "[B"
                      "Ljava/lang/String;"
                      "I)V" );
    jbyteArray jmsg = makeByteArray( env, len, (jbyte*)buf );
    jstring jphone = (*env)->NewStringUTF( env, phone );
    (*env)->CallVoidMethod( env, dutil->jdutil, mid, jmsg, jphone, port );
    deleteLocalRefs( env, jmsg, jphone, DELETE_NO_REF );
    DUTIL_CBK_TAIL();
    return -1;
}

static void
and_dutil_onKnownPlayersChange( XW_DUtilCtxt* duc, XWEnv xwe )
{
    DUTIL_CBK_HEADER( "onKnownPlayersChange", "()V" );
    (*env)->CallVoidMethod( env, dutil->jdutil, mid );
    DUTIL_CBK_TAIL();
}

static void
and_dutil_onGameChanged( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr, GameChangeEvents gces )
{
    // XP_LOGFF( "(gces=%x)", gces );
    DUTIL_CBK_HEADER( "onGameChanged", "(JI)V" );
    (*env)->CallVoidMethod( env, dutil->jdutil, mid, gr, gces );
    DUTIL_CBK_TAIL();
}

static void
and_dutil_getCommonPrefs( XW_DUtilCtxt* duc, XWEnv xwe, CommonPrefs* cp )
{
    DUTIL_CBK_HEADER( "getCommonPrefs", "()L" PKG_PATH("jni/CommonPrefs") ";" );
    jobject jcp = (*env)->CallObjectMethod( env, dutil->jdutil, mid );
    loadCommonPrefs( env, cp, jcp );
    deleteLocalRef( env, jcp );
    DUTIL_CBK_TAIL();
}

XW_UtilCtxt*
makeUtil( MPFORMAL JNIEnv* env, jobject jutil, const CurGameInfo* gi,
          XW_DUtilCtxt* dutil, GameRef gr )
{
    AndUtil* autil = (AndUtil*)XP_CALLOC( mpool, sizeof(*autil) );
    XW_UtilCtxt* super = &autil->super;

    UtilVtable* vtable = (UtilVtable*)XP_CALLOC( mpool, sizeof(*vtable) );
    super->vtable = vtable;
    autil->jutil = (*env)->NewGlobalRef( env, jutil );

    util_super_init( MPPARM(mpool) super, gi, dutil, gr, and_util_destroy );

#define SET_PROC(nam) vtable->m_util_##nam = and_util_##nam
    SET_PROC(userError);
    SET_PROC(notifyMove);
    SET_PROC(notifyTrade);
    SET_PROC(notifyPickTileBlank);
    SET_PROC(informNeedPickTiles);
    SET_PROC(informNeedPassword);
    SET_PROC(trayHiddenChange);
    SET_PROC(yOffsetChange);
#ifdef XWFEATURE_TURNCHANGENOTIFY
    SET_PROC(turnChanged);
#endif
    SET_PROC(notifyDupStatus);
    SET_PROC(informUndo);
    SET_PROC(informNetDict);
#ifdef XWFEATURE_HILITECELL
    SET_PROC(hiliteCell);
#endif
#ifdef XWFEATURE_STOP_ENGINE
    SET_PROC(stopEngineProgress);
#endif
    SET_PROC(altKeyDown);
    SET_PROC(notifyIllegalWords);
    SET_PROC(showChat);
    SET_PROC(remSelected);
    SET_PROC(timerSelected);
    SET_PROC(formatPauseHistory);
    SET_PROC(dictGone);

#ifndef XWFEATURE_MINIWIN
    SET_PROC(bonusSquareHeld);
    SET_PROC(playerScoreHeld);
#endif

#ifdef XWFEATURE_BOARDWORDS
    SET_PROC(cellSquareHeld);
#endif

    /* SET_PROC(informMissing); */
#ifdef XWFEATURE_SEARCHLIMIT
    SET_PROC(getTraySearchLimits);
#endif
#ifdef SHOW_PROGRESS
    SET_PROC(engineStarting);
    SET_PROC(engineStopping);
#endif

    // SET_PROC(getDevUtilCtxt);
    SET_PROC(informWordsBlocked);
    // SET_PROC(getInviteeName);
#undef SET_PROC

    assertTableFull( vtable, sizeof(*vtable), "util" );
    return super;
} /* makeUtil */

XW_DUtilCtxt*
makeDUtil( MPFORMAL JNIEnv* env,
#ifdef MAP_THREAD_TO_ENV
           EnvThreadInfo* ti,
#endif
           jobject jdutil, VTableMgr* vtMgr,
           JNIUtilCtxt* jniutil, void* closure )
{
    AndDUtil* dutil = (AndDUtil*)XP_CALLOC( mpool, sizeof(*dutil) );
    XW_DUtilCtxt* super = &dutil->dutil;

#ifdef MAP_THREAD_TO_ENV
    dutil->ti = ti;
#endif
    dutil->jniutil = jniutil;
    dutil->dutil.closure = closure;
    dutil->dutil.vtMgr = vtMgr;

    if ( NULL != jdutil ) {
        dutil->jdutil = (*env)->NewGlobalRef( env, jdutil );
    }

    DUtilVtable* vtable = &dutil->dutil.vtable;
#define SET_DPROC(nam) vtable->m_dutil_##nam = and_dutil_##nam
    SET_DPROC(getCurSeconds);
    SET_DPROC(getThumbDraw);
    SET_DPROC(getUserString);
    SET_DPROC(getUserQuantityString);
    SET_DPROC(storePtr);
    SET_DPROC(loadPtr);
    SET_DPROC(removeStored);
    SET_DPROC(getKeysLike);
# ifdef XWFEATURE_DEVICE
    SET_DPROC(forEach);
    // SET_DPROC(remove);
# endif

#ifdef XWFEATURE_SMS
    SET_DPROC(phoneNumbersSame);
#endif
    SET_DPROC(md5sum);
    SET_DPROC(setTimer);
    SET_DPROC(clearTimer);

    SET_DPROC(getUsername);
    SET_DPROC(getSelfAddr);
    SET_DPROC(notifyPause);
    SET_DPROC(informMove);
    SET_DPROC(notifyGameOver);
    SET_DPROC(onDupTimerChanged);
    SET_DPROC(onGroupChanged);
    SET_DPROC(onCtrlReceived);

    SET_DPROC(onGameGoneReceived);
    SET_DPROC(sendViaWeb);
    SET_DPROC(makeDict);
    SET_DPROC(makeEmptyDict);
    SET_DPROC(getRegValues);
    SET_DPROC(missingDictAdded);
    SET_DPROC(dictGone);
    SET_DPROC(startMQTTListener);
    SET_DPROC(sendViaMQTT);
    SET_DPROC(sendViaBT);
    SET_DPROC(sendViaNBS);
    SET_DPROC(onKnownPlayersChange);
    SET_DPROC(onGameChanged);
    SET_DPROC(getCommonPrefs);

#undef SET_DPROC

    /* This must happen after the vtable is inited!!! */
    dutil_super_init( MPPARM(mpool) super, env );
    assertTableFull( vtable, sizeof(*vtable), "dutil" );

    dvc_init( super, env );

    return super;
}

void
destroyDUtil( XW_DUtilCtxt** dutilp, JNIEnv* env )
{
    AndDUtil* dutil = (AndDUtil*)*dutilp;
    if ( NULL != dutil->jdutil ) {
        (*env)->DeleteGlobalRef( env, dutil->jdutil );
    }

    for ( int ii = 0; ii < VSIZE(dutil->userStrings); ++ii ) {
        XP_UCHAR* ptr = dutil->userStrings[ii];
        if ( NULL != ptr ) {
            if ( 0 == (dutil->userStringsBits & (1 << ii)) ) {
                XP_FREE( dutil->dutil.mpool, ptr );
            } else {
                XP_UCHAR** ptrs = (XP_UCHAR**)ptr;
                for ( int jj = 0; jj < MAX_QUANTITY_STRS; ++jj ) {
                    ptr = ptrs[jj];
                    if ( !!ptr ) {
                        XP_FREE( dutil->dutil.mpool, ptr );
                    }
                }
                XP_FREE( dutil->dutil.mpool, ptrs );
            }
        }
    }
#ifdef XWFEATURE_DEVID
    XP_FREEP( dutil->dutil.mpool, &dutil->devIDStorage );
#endif
}
