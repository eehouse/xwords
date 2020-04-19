/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
/* 
 * Copyright 2001 - 2017 by Eric House (xwords@eehouse.org).  All rights
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
#include "utilwrapper.h"
#include "anddict.h"
#include "andutils.h"
#include "paths.h"
#include "LocalizedStrIncludes.h"
#include "dbgutil.h"

#define MAX_QUANTITY_STRS 4

typedef struct _AndDUtil {
    XW_DUtilCtxt dutil;
    EnvThreadInfo* ti;
    JNIUtilCtxt* jniutil;
    jobject jdutil;  /* global ref to object implementing XW_DUtilCtxt */
    XP_UCHAR* userStrings[N_AND_USER_STRINGS];
    XP_U32 userStringsBits;
#ifdef XWFEATURE_DEVID
    XP_UCHAR* devIDStorage;
#endif
} AndDUtil;

typedef struct _TimerStorage {
    XWTimerProc proc;
    void* closure;
} TimerStorage;

typedef struct _AndUtil {
    XW_UtilCtxt util;
    EnvThreadInfo* ti;
    jobject jutil;  /* global ref to object implementing XW_UtilCtxt */
    TimerStorage timerStorage[NUM_TIMERS_PLUS_ONE];
} AndUtil;

#ifndef XWFEATURE_STANDALONE_ONLY
static XWStreamCtxt*
and_util_makeStreamFromAddr( XW_UtilCtxt* uc, XP_PlayerAddr channelNo )
{
#ifdef DEBUG
    AndUtil* util = (AndUtil*)uc;
#endif
    AndGameGlobals* globals = (AndGameGlobals*)uc->closure;
    XWStreamCtxt* stream = and_empty_stream( MPPARM(util->util.mpool)
                                             globals );
    stream_setAddress( stream, channelNo );
    stream_setOnCloseProc( stream, and_send_on_close );
    return stream;
}
#endif

#define UTIL_CBK_HEADER(nam,sig)                                        \
    AndUtil* util = (AndUtil*)uc;                                       \
    JNIEnv* env = ENVFORME( util->ti );                                 \
    if ( NULL != util->jutil ) {                                        \
        jmethodID mid = getMethodID( env, util->jutil, nam, sig )

#define UTIL_CBK_TAIL()                                                 \
    } else {                                                            \
        XP_LOGF( "%s: skipping call into java because jutil==NULL",     \
                 __func__ );                                            \
    }

#define DUTIL_CBK_HEADER(nam,sig)                                       \
    AndDUtil* dutil = (AndDUtil*)duc;                                   \
    JNIEnv* env = ENVFORME( dutil->ti );                                \
    if ( NULL != dutil->jdutil ) {                                      \
        jmethodID mid = getMethodID( env, dutil->jdutil, nam, sig )

#define DUTIL_CBK_TAIL() UTIL_CBK_TAIL()
    
static XWBonusType and_util_getSquareBonus( XW_UtilCtxt* XP_UNUSED(uc), 
                                            XP_U16 boardSize,
                                            XP_U16 col, XP_U16 row )
{
#define BONUS_DIM 8
    static const int s_buttsBoard[BONUS_DIM][BONUS_DIM] = {
        { BONUS_TRIPLE_WORD,  BONUS_NONE,         BONUS_NONE,BONUS_DOUBLE_LETTER,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_TRIPLE_WORD },
        { BONUS_NONE,         BONUS_DOUBLE_WORD,  BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_TRIPLE_LETTER,BONUS_NONE,BONUS_NONE },

        { BONUS_NONE,         BONUS_NONE,         BONUS_DOUBLE_WORD,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_LETTER,BONUS_NONE },
        { BONUS_DOUBLE_LETTER,BONUS_NONE,         BONUS_NONE,BONUS_DOUBLE_WORD,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_LETTER },
                            
        { BONUS_NONE,         BONUS_NONE,         BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_WORD,BONUS_NONE,BONUS_NONE,BONUS_NONE },
        { BONUS_NONE,         BONUS_TRIPLE_LETTER,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_TRIPLE_LETTER,BONUS_NONE,BONUS_NONE },
                            
        { BONUS_NONE,         BONUS_NONE,         BONUS_DOUBLE_LETTER,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_LETTER,BONUS_NONE },
        { BONUS_TRIPLE_WORD,  BONUS_NONE,         BONUS_NONE,BONUS_DOUBLE_LETTER,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_WORD },
    }; /* buttsBoard */

    int half = boardSize / 2;
    if ( col > half ) { col = (half*2) - col; }
    if ( row > half ) { row = (half*2) - row; }
    XP_ASSERT( col < BONUS_DIM && row < BONUS_DIM );
    return s_buttsBoard[row][col];
#undef BONUS_DIM
}

static void
and_util_userError( XW_UtilCtxt* uc, UtilErrID id )
{
    UTIL_CBK_HEADER( "userError", "(I)V" );
    (*env)->CallVoidMethod( env, util->jutil, mid, id );
    if ((*env)->ExceptionOccurred(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        XP_LOGF( "exception found" );
    }
    UTIL_CBK_TAIL();
}

static void
and_util_notifyMove( XW_UtilCtxt* uc, XWStreamCtxt* stream )
{
    UTIL_CBK_HEADER("notifyMove", "(Ljava/lang/String;)V" );

    jstring jstr = NULL;
    if ( NULL != stream ) {
        jstr = streamToJString( env, stream );
    }
    (*env)->CallVoidMethod( env, util->jutil, mid, jstr );
    deleteLocalRef( env, jstr );
    UTIL_CBK_TAIL();
}

static void
and_util_notifyTrade( XW_UtilCtxt* uc, const XP_UCHAR** tiles, XP_U16 nTiles )
{
    UTIL_CBK_HEADER("notifyTrade", "([Ljava/lang/String;)V" );
    jobjectArray jtiles = makeStringArray( env, nTiles, tiles );
    (*env)->CallVoidMethod( env, util->jutil, mid, jtiles );
    deleteLocalRef( env, jtiles );
    UTIL_CBK_TAIL();
}

static void
and_util_notifyPickTileBlank( XW_UtilCtxt* uc, XP_U16 playerNum,
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
and_util_informNeedPickTiles( XW_UtilCtxt* uc, XP_Bool isInitial,
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
and_util_informNeedPassword( XW_UtilCtxt* uc, XP_U16 player,
                             const XP_UCHAR* name )
{
    UTIL_CBK_HEADER("informNeedPassword", "(ILjava/lang/String;)V" );

    jstring jname = (*env)->NewStringUTF( env, name );
    (*env)->CallVoidMethod( env, util->jutil, mid, player, jname );
    deleteLocalRef( env, jname );
    UTIL_CBK_TAIL();
}

static void
and_util_trayHiddenChange(XW_UtilCtxt* uc, XW_TrayVisState newState,
                          XP_U16 nVisibleRows )
{
}

static void
and_util_yOffsetChange(XW_UtilCtxt* uc, XP_U16 maxOffset, 
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
and_util_turnChanged( XW_UtilCtxt* uc, XP_S16 turn )
{
    UTIL_CBK_HEADER( "turnChanged", "(I)V" );
    (*env)->CallVoidMethod( env, util->jutil, mid, turn );
    UTIL_CBK_TAIL();
}
#endif

static void
and_util_informMove( XW_UtilCtxt* uc, XP_S16 turn, XWStreamCtxt* expl, 
                     XWStreamCtxt* words )
{
    UTIL_CBK_HEADER( "informMove", "(ILjava/lang/String;Ljava/lang/String;)V" );
    jstring jexpl = streamToJString( env, expl );
    jstring jwords = !!words ? streamToJString( env, words ) : NULL;
    (*env)->CallVoidMethod( env, util->jutil, mid, turn, jexpl, jwords );
    deleteLocalRefs( env, jexpl, jwords, DELETE_NO_REF );
    UTIL_CBK_TAIL();
}

static void
and_util_notifyDupStatus( XW_UtilCtxt* uc, XP_Bool amHost, const XP_UCHAR* msg )
{
    UTIL_CBK_HEADER( "notifyDupStatus", "(ZLjava/lang/String;)V" );
    jstring jmsg = (*env)->NewStringUTF( env, msg );
    (*env)->CallVoidMethod( env, util->jutil, mid, amHost, jmsg );
    deleteLocalRefs( env, jmsg, DELETE_NO_REF );
    UTIL_CBK_TAIL();
}

static void
and_util_informUndo( XW_UtilCtxt* uc )
{
    UTIL_CBK_HEADER( "informUndo", "()V" );
    (*env)->CallVoidMethod( env, util->jutil, mid );
    UTIL_CBK_TAIL();
}

static void
and_util_informNetDict( XW_UtilCtxt* uc, XP_LangCode lang,
                        const XP_UCHAR* oldName,
                        const XP_UCHAR* newName, const XP_UCHAR* newSum,
                        XWPhoniesChoice phoniesAction )
{
    LOG_FUNC();
    UTIL_CBK_HEADER( "informNetDict", 
                     "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;L"
                     PKG_PATH("jni/CurGameInfo$XWPhoniesChoice") ";)V" );
    jstring jnew = (*env)->NewStringUTF( env, newName );
    jstring jsum = (*env)->NewStringUTF( env, newSum );
    jstring jold = (*env)->NewStringUTF( env, oldName );
    jobject jphon = intToJEnum( env, phoniesAction, 
                                PKG_PATH("jni/CurGameInfo$XWPhoniesChoice") );

    (*env)->CallVoidMethod( env, util->jutil, mid, lang, jold, jnew, jsum, 
                            jphon );
    deleteLocalRefs( env, jnew, jold, jsum, jphon, DELETE_NO_REF );

    UTIL_CBK_TAIL();
}

static void
and_util_notifyGameOver( XW_UtilCtxt* uc, XP_S16 XP_UNUSED(quitter) )
{
    UTIL_CBK_HEADER( "notifyGameOver", "()V" );
    (*env)->CallVoidMethod( env, util->jutil, mid );
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

static XP_Bool
and_util_engineProgressCallback( XW_UtilCtxt* uc )
{
    XP_Bool result = XP_FALSE;
    UTIL_CBK_HEADER("engineProgressCallback","()Z" );
    result = (*env)->CallBooleanMethod( env, util->jutil, mid );
    UTIL_CBK_TAIL();
    return result;
}

/* This is added for java, not part of the util api */
bool
utilTimerFired( XW_UtilCtxt* uc, XWTimerReason why, int handle )
{
    bool handled = false;
    AndUtil* util = (AndUtil*)uc;
    TimerStorage* timerStorage = &util->timerStorage[why];
    if ( handle == (int)timerStorage ) {
        XWTimerProc proc = timerStorage->proc;
        if ( !!proc ) {
            handled = (*proc)( timerStorage->closure, why );
        } else {
            XP_LOGF( "%s(why=%d): ERROR: no proc set", __func__, why );
        }
    } else {
        XP_LOGF( "%s: mismatch: handle=%d; timerStorage=%d", __func__,
                 handle, (int)timerStorage );
    }
    return handled;
}

static void
and_util_setTimer( XW_UtilCtxt* uc, XWTimerReason why, XP_U16 when,
                   XWTimerProc proc, void* closure )
{
    UTIL_CBK_HEADER("setTimer", "(III)V" );

    XP_ASSERT( why < VSIZE(util->timerStorage) );
    TimerStorage* storage = &util->timerStorage[why];
    storage->proc = proc;
    storage->closure = closure;
    (*env)->CallVoidMethod( env, util->jutil, mid,
                            why, when, (int)storage );
    UTIL_CBK_TAIL();
}

static void
and_util_clearTimer( XW_UtilCtxt* uc, XWTimerReason why )
{
    UTIL_CBK_HEADER("clearTimer", "(I)V" );
    (*env)->CallVoidMethod( env, util->jutil, mid, why );
    UTIL_CBK_TAIL();
}


static void
and_util_requestTime( XW_UtilCtxt* uc )
{
    UTIL_CBK_HEADER("requestTime", "()V" );
    (*env)->CallVoidMethod( env, util->jutil, mid );
    UTIL_CBK_TAIL();
}

static XP_Bool
and_util_altKeyDown( XW_UtilCtxt* uc )
{
    LOG_FUNC();
    return XP_FALSE;
}

XP_U32
and_dutil_getCurSeconds( XW_DUtilCtxt* duc )
{
    AndDUtil* anddutil = (AndDUtil*)duc;
    XP_U32 curSeconds = getCurSeconds( ENVFORME( anddutil->ti ) );
    /* struct timeval tv; */
    /* gettimeofday( &tv, NULL ); */
    /* XP_LOGF( "%s: %d vs %d", __func__, (int)tv.tv_sec, (int)curSeconds ); */
    return curSeconds;
}

static DictionaryCtxt* 
and_util_makeEmptyDict( XW_UtilCtxt* uc )
{
#ifdef STUBBED_DICT
    XP_ASSERT(0);
#else
    AndGameGlobals* globals = (AndGameGlobals*)uc->closure;
    AndUtil* andutil = (AndUtil*)uc;
    DictionaryCtxt* result =  
        and_dictionary_make_empty( MPPARM( ((AndUtil*)uc)->util.mpool )
                                   ENVFORME( andutil->ti ), globals->jniutil );
    return dict_ref( result );
#endif
}

static const XP_UCHAR*
and_dutil_getUserString( XW_DUtilCtxt* duc, XP_U16 stringCode )
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
and_dutil_getUserQuantityString( XW_DUtilCtxt* duc, XP_U16 stringCode, XP_U16 quantity )
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
            XP_LOGF( "%s: found empty slot %d for %s", __func__, indx, jchars );
            break;
        } else if ( 0 == XP_STRCMP( jchars, ptrs[indx] ) ) {
            XP_LOGF( "%s: found %s at slot %d", __func__, jchars, indx );
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
and_dutil_storePtr( XW_DUtilCtxt* duc, const XP_UCHAR* key,
                    const void* data, XP_U16 len )
{
    DUTIL_CBK_HEADER( "store", "(Ljava/lang/String;[B)V" );

    jbyteArray jdata = makeByteArray( env, len, data );
    jstring jkey = (*env)->NewStringUTF( env, key );

    (*env)->CallVoidMethod( env, dutil->jdutil, mid, jkey, jdata );

    deleteLocalRefs( env, jdata, jkey, DELETE_NO_REF );

    DUTIL_CBK_TAIL();
}

static void
and_dutil_storeStream( XW_DUtilCtxt* duc, const XP_UCHAR* key,
                       XWStreamCtxt* stream )
{
    const void* ptr = stream_getPtr( stream );
    XP_U16 len = stream_getSize( stream );

    and_dutil_storePtr( duc, key, ptr, len );
}

static jbyteArray
loadToByteArray( XW_DUtilCtxt* duc, const XP_UCHAR* key )
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
and_dutil_loadPtr( XW_DUtilCtxt* duc, const XP_UCHAR* key,
                   void* data, XP_U16* lenp )
{
    AndDUtil* dutil = (AndDUtil*)duc;
    JNIEnv* env = ENVFORME( dutil->ti );
    jbyteArray jvalue = loadToByteArray( duc, key );
    if ( jvalue != NULL ) {
        jsize len = (*env)->GetArrayLength( env, jvalue );
        if ( len <= *lenp ) {
            jbyte* jelems = (*env)->GetByteArrayElements( env, jvalue, NULL );
            XP_MEMCPY( data, jelems, len );
            (*env)->ReleaseByteArrayElements( env, jvalue, jelems, 0 );
        }
        *lenp = len;
        deleteLocalRef( env, jvalue );
    }
}

static void
and_dutil_loadStream( XW_DUtilCtxt* duc, const XP_UCHAR* key, XWStreamCtxt* stream )
{
    AndDUtil* dutil = (AndDUtil*)duc;
    JNIEnv* env = ENVFORME( dutil->ti );
    jbyteArray jvalue = loadToByteArray( duc, key );
    if ( jvalue != NULL ) {
        jbyte* jelems = (*env)->GetByteArrayElements( env, jvalue, NULL );
        jsize len = (*env)->GetArrayLength( env, jvalue );
        stream_putBytes( stream, jelems, len );
        (*env)->ReleaseByteArrayElements( env, jvalue, jelems, 0 );
        deleteLocalRef( env, jvalue );
    }
}

static void
and_util_notifyIllegalWords( XW_UtilCtxt* uc, BadWordInfo* bwi,
                             XP_U16 turn, XP_Bool turnLost )
{
    UTIL_CBK_HEADER("notifyIllegalWords",
                    "(Ljava/lang/String;[Ljava/lang/String;IZ)V" );
    XP_ASSERT( bwi->nWords > 0 );
    if ( bwi->nWords > 0 ) {
        jobjectArray jwords = makeStringArray( env, bwi->nWords, 
                                               (const XP_UCHAR**)bwi->words );
        XP_ASSERT( !!bwi->dictName );
        jstring jname = (*env)->NewStringUTF( env, bwi->dictName );
        (*env)->CallVoidMethod( env, util->jutil, mid,
                                jname, jwords, turn, turnLost );
        deleteLocalRefs( env, jwords, jname, DELETE_NO_REF );
    }
    UTIL_CBK_TAIL();
}

#ifdef XWFEATURE_CHAT
static void
and_util_showChat( XW_UtilCtxt* uc, const XP_UCHAR* msg, XP_S16 from, XP_U32 timestamp )
{
    UTIL_CBK_HEADER( "showChat", "(Ljava/lang/String;ILjava/lang/String;I)V" );
    jstring jname = NULL;
    if ( 0 <= from ) {
        LocalPlayer* lp = &uc->gameInfo->players[from];
        XP_ASSERT( !lp->isLocal );
        jname = (*env)->NewStringUTF( env, lp->name );
    }

    jstring jmsg = (*env)->NewStringUTF( env, msg );
    (*env)->CallVoidMethod( env, util->jutil, mid, jmsg, from, jname, timestamp );
    deleteLocalRefs( env, jmsg, jname, DELETE_NO_REF );
    UTIL_CBK_TAIL();
}
#endif

static void
and_util_remSelected(XW_UtilCtxt* uc)
{
    UTIL_CBK_HEADER("remSelected", "()V" );
    (*env)->CallVoidMethod( env, util->jutil, mid );
    UTIL_CBK_TAIL();
}

static void
and_util_timerSelected( XW_UtilCtxt* uc, XP_Bool inDuplicateMode, XP_Bool canPause )
{
    UTIL_CBK_HEADER("timerSelected", "(ZZ)V" );
    (*env)->CallVoidMethod( env, util->jutil, mid, inDuplicateMode, canPause );
    UTIL_CBK_TAIL();
}

static void
and_util_formatPauseHistory( XW_UtilCtxt* uc, XWStreamCtxt* stream,
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

#ifndef XWFEATURE_MINIWIN
static void
and_util_bonusSquareHeld( XW_UtilCtxt* uc, XWBonusType bonus )
{
    UTIL_CBK_HEADER( "bonusSquareHeld", "(I)V" );
    (*env)->CallVoidMethod( env, util->jutil, mid, bonus );
    UTIL_CBK_TAIL();
}

static void
and_util_playerScoreHeld( XW_UtilCtxt* uc, XP_U16 player )
{
    UTIL_CBK_HEADER( "playerScoreHeld", "(I)V" );
    (*env)->CallVoidMethod( env, util->jutil, mid, player );
    UTIL_CBK_TAIL();
}
#endif

#ifdef XWFEATURE_SMS
static XP_Bool
and_dutil_phoneNumbersSame( XW_DUtilCtxt* duc, const XP_UCHAR* p1,
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
and_util_cellSquareHeld( XW_UtilCtxt* uc, XWStreamCtxt* words )
{
    if ( NULL != words ) {
        UTIL_CBK_HEADER( "cellSquareHeld", "(Ljava/lang/String;)V" );
        jstring jwords = streamToJString( env, words );
        (*env)->CallVoidMethod( env, util->jutil, mid, jwords );
        deleteLocalRef( env, jwords );
        UTIL_CBK_TAIL();
    }
}
#endif

#ifndef XWFEATURE_STANDALONE_ONLY

static void
and_util_informMissing( XW_UtilCtxt* uc, XP_Bool isServer, 
                        const CommsAddrRec* addr, XP_U16 nDevs, XP_U16 nMissing)
{
    UTIL_CBK_HEADER( "informMissing", 
                     "(ZL" PKG_PATH("jni/CommsAddrRec$CommsConnTypeSet") ";II)V" );
    jobject jtypset = NULL;
    if ( !!addr ) {
        jtypset = addrTypesToJ( env, addr );
    }
    (*env)->CallVoidMethod( env, util->jutil, mid, isServer, jtypset, nDevs,
                            nMissing );
    deleteLocalRef( env, jtypset );
    UTIL_CBK_TAIL();
}

static void
and_util_addrChange( XW_UtilCtxt* uc, const CommsAddrRec* oldAddr,
                     const CommsAddrRec* newAddr )
{
    // LOG_FUNC();
}

static void
and_util_setIsServer( XW_UtilCtxt* uc, XP_Bool isServer )
{
    /* Change both the C and Java structs, which need to stay in sync */
    uc->gameInfo->serverRole = isServer? SERVER_ISSERVER : SERVER_ISCLIENT;
    UTIL_CBK_HEADER( "setIsServer", "(Z)V" );
    (*env)->CallVoidMethod( env, util->jutil, mid, isServer );
    UTIL_CBK_TAIL();
}

static void
and_util_informWordBlocked( XW_UtilCtxt* uc )
{
    UTIL_CBK_HEADER( "informWordBlocked", "()V" );
    (*env)->CallVoidMethod( env, util->jutil, mid );
    UTIL_CBK_TAIL();
}

#ifdef XWFEATURE_DEVID
static const XP_UCHAR*
and_dutil_getDevID( XW_DUtilCtxt* duc, DevIDType* typ )
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
            XP_LOGF( "%s: already have matching devID", __func__ );
        } else {
            XP_LOGF( "%s: allocating storage for devID", __func__ );
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
    deleteLocalRef( env, jbarr );
    DUTIL_CBK_TAIL();
    return result;
}

static void
and_dutil_deviceRegistered( XW_DUtilCtxt* duc, DevIDType typ,
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
#endif  /* XWFEATURE_DEVID */

#endif

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

static XW_DUtilCtxt*
and_util_getDevUtilCtxt( XW_UtilCtxt* uc )
{
    AndGameGlobals* globals = (AndGameGlobals*)uc->closure;
    XP_ASSERT( !!globals->dutil );
    return globals->dutil;
}

#ifdef COMMS_CHECKSUM
static XP_UCHAR*
and_dutil_md5sum( XW_DUtilCtxt* duc, const XP_U8* ptr, XP_U16 len )
{
    AndDUtil* dutil = (AndDUtil*)duc;
    JNIEnv* env = ENVFORME( dutil->ti );
    struct JNIUtilCtxt* jniutil = dutil->jniutil;
    jstring jsum = and_util_getMD5SumForBytes( jniutil, ptr, len );
    XP_UCHAR* result = getStringCopy( MPPARM(duc->mpool) env, jsum );
    deleteLocalRef( env, jsum );
    return result;
}
#endif

static void
and_dutil_notifyPause( XW_DUtilCtxt* duc, XP_U32 gameID, DupPauseType pauseTyp,
                       XP_U16 pauser, const XP_UCHAR* name,
                       const XP_UCHAR* msg )
{
    DUTIL_CBK_HEADER( "notifyPause", "(IIILjava/lang/String;Ljava/lang/String;)V" );
    jstring jname = (*env)->NewStringUTF( env, name );
    jstring jmsg = (*env)->NewStringUTF( env, msg );
    (*env)->CallVoidMethod( env, dutil->jdutil, mid, gameID, pauseTyp, pauser,
                            jname, jmsg );
    deleteLocalRefs( env, jname, jmsg, DELETE_NO_REF );
    DUTIL_CBK_TAIL();
}

static void
and_dutil_onDupTimerChanged( XW_DUtilCtxt* duc, XP_U32 gameID,
                             XP_U32 oldVal, XP_U32 newVal )
{
    DUTIL_CBK_HEADER( "onDupTimerChanged", "(III)V" );
    (*env)->CallVoidMethod( env, dutil->jdutil, mid, gameID, oldVal, newVal );
    DUTIL_CBK_TAIL();
}

XW_UtilCtxt*
makeUtil( MPFORMAL EnvThreadInfo* ti, jobject jutil, CurGameInfo* gi,
          AndGameGlobals* closure )
{
    AndUtil* util = (AndUtil*)XP_CALLOC( mpool, sizeof(*util) );
    UtilVtable* vtable = (UtilVtable*)XP_CALLOC( mpool, sizeof(*vtable) );
    util->ti = ti;
    JNIEnv* env = ENVFORME( util->ti );
    if ( NULL != jutil ) {
        util->jutil = (*env)->NewGlobalRef( env, jutil );
    }
    util->util.vtable = vtable;
    MPASSIGN( util->util.mpool, mpool );
    util->util.closure = closure;
    util->util.gameInfo = gi;

#define SET_PROC(nam) vtable->m_util_##nam = and_util_##nam

#ifndef XWFEATURE_STANDALONE_ONLY
    SET_PROC(makeStreamFromAddr);
#endif
    SET_PROC(getSquareBonus);
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
    SET_PROC(informMove);
    SET_PROC(notifyDupStatus);
    SET_PROC(informUndo);
    SET_PROC(informNetDict);
    SET_PROC(notifyGameOver);
#ifdef XWFEATURE_HILITECELL
    SET_PROC(hiliteCell);
#endif
    SET_PROC(engineProgressCallback);
    SET_PROC(setTimer);
    SET_PROC(clearTimer);
    SET_PROC(requestTime);
    SET_PROC(altKeyDown);
    SET_PROC(makeEmptyDict);
    SET_PROC(notifyIllegalWords);
#ifdef XWFEATURE_CHAT
    SET_PROC(showChat);
#endif
    SET_PROC(remSelected);
    SET_PROC(timerSelected);
    SET_PROC(formatPauseHistory);

#ifndef XWFEATURE_MINIWIN
    SET_PROC(bonusSquareHeld);
    SET_PROC(playerScoreHeld);
#endif

#ifdef XWFEATURE_BOARDWORDS
    SET_PROC(cellSquareHeld);
#endif

#ifndef XWFEATURE_STANDALONE_ONLY
    SET_PROC(informMissing);
    SET_PROC(addrChange);
    SET_PROC(setIsServer);
#endif
#ifdef XWFEATURE_SEARCHLIMIT
    SET_PROC(getTraySearchLimits);
#endif
#ifdef SHOW_PROGRESS
    SET_PROC(engineStarting);
    SET_PROC(engineStopping);
#endif

    SET_PROC(getDevUtilCtxt);
    SET_PROC(informWordBlocked);

#undef SET_PROC
    assertTableFull( vtable, sizeof(*vtable), "util" );
    return (XW_UtilCtxt*)util;
} /* makeUtil */

void
destroyUtil( XW_UtilCtxt** utilc )
{
    AndUtil* util = (AndUtil*)*utilc;
    JNIEnv* env = ENVFORME( util->ti );

    if ( NULL != util->jutil ) {
        (*env)->DeleteGlobalRef( env, util->jutil );
    }
    XP_FREE( util->util.mpool, util->util.vtable );
    XP_FREE( util->util.mpool, util );
    *utilc = NULL;
}

XW_DUtilCtxt*
makeDUtil( MPFORMAL EnvThreadInfo* ti, jobject jdutil, VTableMgr* vtMgr,
           JNIUtilCtxt* jniutil, void* closure )
{
    AndDUtil* dutil = (AndDUtil*)XP_CALLOC( mpool, sizeof(*dutil) );
    dutil->ti = ti;
    dutil->jniutil = jniutil;
    dutil->dutil.closure = closure;
    dutil->dutil.vtMgr = vtMgr;

    if ( NULL != jdutil ) {
        JNIEnv* env = ENVFORME( ti );
        dutil->jdutil = (*env)->NewGlobalRef( env, jdutil );
    }

    MPASSIGN( dutil->dutil.mpool, mpool );

    DUtilVtable* vtable = &dutil->dutil.vtable;
#define SET_DPROC(nam) vtable->m_dutil_##nam = and_dutil_##nam
    SET_DPROC(getCurSeconds);
    SET_DPROC(getUserString);
    SET_DPROC(getUserQuantityString);
    SET_DPROC(storeStream);
    SET_DPROC(loadStream);
    SET_DPROC(storePtr);
    SET_DPROC(loadPtr);
# ifdef XWFEATURE_DEVID
    SET_DPROC(getDevID);
    SET_DPROC(deviceRegistered);
# endif
#ifdef XWFEATURE_SMS
    SET_DPROC(phoneNumbersSame);
#endif
#ifdef COMMS_CHECKSUM
    SET_DPROC(md5sum);
#endif
    SET_DPROC(notifyPause);
    SET_DPROC(onDupTimerChanged);

#undef SET_DPROC

    assertTableFull( vtable, sizeof(*vtable), "dutil" );
    return &dutil->dutil;
}

void
destroyDUtil( XW_DUtilCtxt** dutilp )
{
    AndDUtil* dutil = (AndDUtil*)*dutilp;
    JNIEnv* env = ENVFORME( dutil->ti );
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
