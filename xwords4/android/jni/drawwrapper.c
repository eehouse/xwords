/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/* 
 * Copyright 2001-2023 by Eric House (xwords@eehouse.org).  All rights
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

#include "drawwrapper.h"
#include "andutils.h"
#include "anddict.h"
#include "paths.h"

typedef struct _ObjCacheElem {
    jobject obj;
#ifdef DEBUG
    pthread_t owner;
#endif
} ObjCacheElem;

typedef struct _AndDraw {
    DrawCtx super;
#ifdef MAP_THREAD_TO_ENV
    EnvThreadInfo* ti;
#endif
#ifdef DEBUG
    struct {
        pthread_t thread;
        JNIEnv* env;
    } creator;
#endif
    jobject jdraw;             /* global ref; free it! */
    XP_UCHAR curISOCode[MAX_ISO_CODE_LEN+1];
    jobject jTvType;
    XP_UCHAR miniTextBuf[128];
    MPSLOT
} AndDraw;

#define CHECKOUT_MARKER ((jobject)-1)
#define DSI_PATH "jni/DrawCtx$DrawScoreInfo"
#define TVT_PATH "jni/CommonPrefs$TileValueType"

/* There are concurrency problems with this cache, and I'm not sure a mutex
   will fix them. So for now I'm leaving this logging in place but
   disabled. Removing the cache and changing where the DrawCtxts are
   allocated are both too high-risk for now. */
#if 0
# define AND_DRAW_START(DCTX, ENV) {                            \
        pthread_t self = pthread_self();                        \
        pthread_t* other = &((AndDraw*)DCTX)->creator.thread;   \
        if ( !*other ) {                                        \
            /* do nothing */                                    \
        } else if ( self != *other ) {                          \
            XP_LOGFF( "unexpected thread change: "              \
                      "initial: %lX; current %lX",              \
                      *other, self );                           \
        }                                                       \
        *other = self;                                          \

/* if ( ENV != ((AndDraw*)dctx)->creator.env ) {           \ */
        /*     XP_LOGFF( "unexpected env change" );                \ */
        /* }                                                       \ */

# define  AND_DRAW_END() }
#else
# define AND_DRAW_START(DCTX, ENV) {
# define AND_DRAW_END() }
#endif

static void deleteGlobalRef( JNIEnv* env, jobject jobj );

static jobject
makeJRect( JNIEnv* env, const XP_Rect* rect )
{
    int right = rect->left + rect->width;
    int bottom = rect->top + rect->height;
    jobject jrect = makeObject(env, "android/graphics/Rect", "(IIII)V",
                               rect->left, rect->top, right, bottom );
    return jrect;
}

static jobject
makeDSI( XWEnv xwe, const DrawScoreInfo* dsi )
{
    JNIEnv* env = xwe;
    jobject dsiobj = makeObjectEmptyConstr( env, PKG_PATH(DSI_PATH) );

    setInt( env, dsiobj, "playerNum", dsi->playerNum );
    setInt( env, dsiobj, "totalScore", dsi->totalScore );
    setInt( env, dsiobj, "nTilesLeft", dsi->nTilesLeft );
    setInt( env, dsiobj, "flags", dsi->flags );
    setBool( env, dsiobj, "isTurn", dsi->isTurn );
    setBool( env, dsiobj, "selected", dsi->selected );
    setBool( env, dsiobj, "isRemote", dsi->isRemote );
    setBool( env, dsiobj, "isRobot", dsi->isRobot );
    setString( env, dsiobj, "name", dsi->name );

    return dsiobj;
}

#define DRAW_CBK_HEADER(nam,sig) {                              \
    JNIEnv* env = xwe;                                          \
    AndDraw* adraw = (AndDraw*)dctx;                            \
    AND_DRAW_START(adraw, env);                                 \
    ASSERT_ENV( adraw->ti, env );                               \
    XP_ASSERT( !!adraw->jdraw );                                \
    jmethodID mid = getMethodID( xwe, adraw->jdraw, nam, sig )  \

#define DRAW_CBK_HEADER_END() }                 \
    AND_DRAW_END();                             \

static XP_Bool
and_draw_scoreBegin( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                     XP_U16 numPlayers, const XP_S16* const scores,
                     XP_S16 remCount, DrawFocusState XP_UNUSED(dfs) )
{
    jboolean result = XP_FALSE;
    if ( DT_SCREEN == dctx->dt ) {
        DRAW_CBK_HEADER("scoreBegin", "(Landroid/graphics/Rect;I[II)Z" );
        jint jarr[numPlayers];

        for ( int ii = 0; ii < numPlayers; ++ii ) {
            jarr[ii] = scores[ii];
        }
        jintArray jscores = makeIntArray( env, numPlayers, jarr, sizeof(jarr[0]) );
        jobject jrect = makeJRect( env, rect );

        result = (*env)->CallBooleanMethod( env, adraw->jdraw, mid,
                                            jrect, numPlayers, jscores, remCount );

        deleteLocalRefs( env, jscores, jrect, DELETE_NO_REF );
        DRAW_CBK_HEADER_END();
    }
    return result;
}

static XP_Bool
and_draw_measureRemText( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                         XP_S16 nTilesLeft, 
                         XP_U16* width, XP_U16* height )
{
    jboolean result;
    DRAW_CBK_HEADER("measureRemText", "(Landroid/graphics/Rect;I[I[I)Z" );

    jintArray widthArray = (*env)->NewIntArray( env, 1 );
    jintArray heightArray = (*env)->NewIntArray( env, 1 );
    jobject jrect = makeJRect( xwe, rect );

    result = (*env)->CallBooleanMethod( env, adraw->jdraw, mid, jrect,
                                        nTilesLeft, widthArray,
                                        heightArray );
    if ( result ) {
        int tmp;
        getIntsFromArray( env, &tmp, widthArray, 1, true );
        *width = tmp;
        getIntsFromArray( env, &tmp, heightArray, 1, true );
        *height = tmp;
    }
    deleteLocalRef( env, jrect );
    DRAW_CBK_HEADER_END();
    return result;
} /* and_draw_measureRemText */

static void
and_draw_drawRemText( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rInner,
                      const XP_Rect* rOuter, 
                      XP_S16 nTilesLeft, XP_Bool focussed )
{
    DRAW_CBK_HEADER("drawRemText",
                    "(Landroid/graphics/Rect;Landroid/graphics/Rect;IZ)V" );

    jobject jrinner = makeJRect( env, rInner );
    jobject jrouter = makeJRect( env, rOuter );

    (*env)->CallVoidMethod( env, adraw->jdraw, mid, jrinner, jrouter,
                            nTilesLeft, focussed );
    deleteLocalRefs( env, jrinner, jrouter, DELETE_NO_REF );
    DRAW_CBK_HEADER_END();
}

static void
and_draw_measureScoreText( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                           const DrawScoreInfo* dsi,
                           XP_U16* width, XP_U16* height )
{
    DRAW_CBK_HEADER("measureScoreText", 
                    "(Landroid/graphics/Rect;L"
                    PKG_PATH(DSI_PATH) ";[I[I)V" );

    jobject jrect = makeJRect( env, rect );
    jobject jdsi = makeDSI( xwe, dsi );

    jintArray widthArray = (*env)->NewIntArray( env, 1 );
    jintArray heightArray = (*env)->NewIntArray( env, 1 );

    (*env)->CallVoidMethod( env, adraw->jdraw, mid, jrect, jdsi,
                            widthArray, heightArray );
    deleteLocalRefs( env, jrect, jdsi, DELETE_NO_REF );

    int tmp;
    getIntsFromArray( env, &tmp, widthArray, 1, true );
    *width = tmp;
    getIntsFromArray( env, &tmp, heightArray, 1, true );
    *height = tmp;
    DRAW_CBK_HEADER_END();
} /* and_draw_measureScoreText */

static void
and_draw_score_drawPlayer( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rInner,
                           const XP_Rect* rOuter, XP_U16 gotPct,
                           const DrawScoreInfo* dsi )
{
    DRAW_CBK_HEADER("score_drawPlayer", 
                    "(Landroid/graphics/Rect;Landroid/graphics/Rect;I"
                    "L" PKG_PATH(DSI_PATH) ";)V" );

    jobject jrinner = makeJRect( xwe, rInner );
    jobject jrouter = makeJRect( xwe, rOuter );
    jobject jdsi = makeDSI( xwe, dsi );

    (*env)->CallVoidMethod( env, adraw->jdraw, mid, jrinner, jrouter, gotPct,
                            jdsi );
    deleteLocalRefs( env, jrinner, jrouter, jdsi, DELETE_NO_REF );
    DRAW_CBK_HEADER_END();
} /* and_draw_score_drawPlayer */

static void
and_draw_drawTimer( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect, XP_U16 player,
                    XP_S16 secondsLeft, XP_Bool inDuplicateMode )
{
    if ( rect->width == 0 ) {
        XP_LOGFF( "exiting b/c rect empty" );
    } else {
        DRAW_CBK_HEADER("drawTimer", "(Landroid/graphics/Rect;IIZ)V" );

        jobject jrect = makeJRect( xwe, rect );
        (*env)->CallVoidMethod( env, adraw->jdraw, mid, jrect, player,
                                secondsLeft, inDuplicateMode );
        deleteLocalRef( env, jrect );
        DRAW_CBK_HEADER_END();
    }
}

static void
and_draw_destroy(DrawCtx* dctx, XWEnv xwe)
{
    AndDraw* adraw = (AndDraw*)dctx;
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = adraw->mpool;
#endif
    deleteGlobalRef( xwe, adraw->jdraw );

    XP_FREE( mpool, adraw->super.vtable );
    XP_FREE( mpool, adraw );
    mpool_destroy( mpool );
}

/* Return FALSE if jdraw is null: we can't draw, which is the point of this */
static XP_Bool
and_draw_beginDraw( DrawCtx* dctx, XWEnv xwe )
{
    AndDraw* adraw = (AndDraw*)dctx;
    jboolean jresult = !!adraw->jdraw;
    if ( jresult ) {
        DRAW_CBK_HEADER("beginDraw", "()Z" );
        jresult = (*env)->CallBooleanMethod( env, adraw->jdraw, mid );
        DRAW_CBK_HEADER_END();
    }
    return jresult;
}
static void
and_draw_endDraw( DrawCtx* dctx, XWEnv xwe )
{
    DRAW_CBK_HEADER("endDraw", "()V" );
    (*env)->CallVoidMethod( env, adraw->jdraw, mid );
    DRAW_CBK_HEADER_END();
}

static XP_Bool
and_draw_boardBegin( DrawCtx* dctx, XWEnv xwe, const XP_Rect* XP_UNUSED(rect),
                     XP_U16 XP_UNUSED(cellWidth), XP_U16 XP_UNUSED(cellHeight),
                     DrawFocusState XP_UNUSED(dfs), TileValueType tvType )
{
    AND_DRAW_START(dctx, xwe);
    JNIEnv* env = xwe;
    AndDraw* adraw = (AndDraw*)dctx;

    jobject jTvType = intToJEnum( env, tvType, PKG_PATH(TVT_PATH) );
    adraw->jTvType = (*env)->NewGlobalRef( env, jTvType );
    deleteLocalRef( env, jTvType );
    AND_DRAW_END();
    return XP_TRUE;
}

static XP_Bool 
and_draw_drawCell( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                   const XP_UCHAR* text, const XP_Bitmaps* bitmaps,
                   Tile tile, XP_U16 value,
                   XP_S16 owner, XWBonusType bonus, HintAtts hintAtts, 
                   CellFlags flags )
{
    jboolean result;
    DRAW_CBK_HEADER("drawCell",
                    "(Landroid/graphics/Rect;Ljava/lang/String;IIIII"
                    "L" PKG_PATH(TVT_PATH) ";)Z" );

    jobject jrect = makeJRect( xwe, rect );
    jstring jtext = NULL;
    if ( !!text ) {
        if ( 0 == strcmp( "_", text ) ) {
            text = "?";
        }
        jtext = (*env)->NewStringUTF( env, text );
    }

    result = (*env)->CallBooleanMethod( env, adraw->jdraw, mid, jrect,
                                        jtext, tile, value, owner, bonus,
                                        flags, adraw->jTvType );
    deleteLocalRefs( env, jtext, jrect, DELETE_NO_REF );

    DRAW_CBK_HEADER_END();
    return result;
}

static void
and_draw_drawBoardArrow( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                         XWBonusType bonus,XP_Bool vert, HintAtts hintAtts,
                         CellFlags flags )
{
    DRAW_CBK_HEADER("drawBoardArrow", "(Landroid/graphics/Rect;IZII)V" );

    jobject jrect = makeJRect( xwe, rect );
    (*env)->CallVoidMethod( env, adraw->jdraw, mid, jrect, bonus, vert,
                            hintAtts, flags );
    deleteLocalRef( env, jrect );
    DRAW_CBK_HEADER_END();
}

static XP_Bool
and_draw_vertScrollBoard( DrawCtx* dctx, XWEnv xwe,
                          XP_Rect* XP_UNUSED(rect), XP_S16 XP_UNUSED(dist),
                          DrawFocusState XP_UNUSED(dfs) )
{
    AND_DRAW_START(dctx, xwe);
    /* Scrolling a bitmap in-place isn't any faster than drawing every cell
       anew so no point in calling into java. */
    AND_DRAW_END();
    return XP_FALSE;
}

static XP_Bool
and_draw_trayBegin( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect, XP_U16 owner,
                    XP_S16 score, DrawFocusState XP_UNUSED(dfs) )
{
    jboolean result = XP_FALSE;
    if ( DT_SCREEN == dctx->dt ) {
        DRAW_CBK_HEADER( "trayBegin", "(Landroid/graphics/Rect;II)Z" );
        jobject jrect = makeJRect( xwe, rect );

        result = (*env)->CallBooleanMethod( env, adraw->jdraw, mid,
                                            jrect, owner, score );
        deleteLocalRef( env, jrect );
        DRAW_CBK_HEADER_END();
    }
    return result;
}

static XP_Bool
and_draw_drawTile( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                   const XP_UCHAR* text, const XP_Bitmaps* bitmaps,
                   XP_S16 val, CellFlags flags )
{
    XP_Bool result;
    DRAW_CBK_HEADER( "drawTile",
                     "(Landroid/graphics/Rect;Ljava/lang/String;II)Z" );
    jobject jrect = makeJRect( xwe, rect );
    jstring jtext = NULL;
    if ( !!text ) {
        jtext = (*env)->NewStringUTF( env, text );
    }

    result = (*env)->CallBooleanMethod( env, adraw->jdraw, mid,
                                        jrect, jtext, val, flags );
    deleteLocalRefs( env, jtext, jrect, DELETE_NO_REF );
    DRAW_CBK_HEADER_END();
    return result;
}

static XP_Bool
and_draw_drawTileMidDrag( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                          const XP_UCHAR* text, const XP_Bitmaps* bitmaps,
                          XP_U16 val, XP_U16 owner, CellFlags flags )
{
    XP_Bool result;
    DRAW_CBK_HEADER( "drawTileMidDrag", 
                     "(Landroid/graphics/Rect;Ljava/lang/String;III)Z" );

    jobject jrect = makeJRect( xwe, rect );
    jstring jtext = NULL;
    if ( !!text ) {
        jtext = (*env)->NewStringUTF( env, text );
    }

    result = (*env)->CallBooleanMethod( env, adraw->jdraw, mid,
                                        jrect, jtext, val, owner, flags );
    deleteLocalRefs( env, jtext, jrect, DELETE_NO_REF );
    DRAW_CBK_HEADER_END();
    return result;
}

static XP_Bool
and_draw_drawTileBack( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect, CellFlags flags )
{
    XP_Bool result;
    DRAW_CBK_HEADER( "drawTileBack", "(Landroid/graphics/Rect;I)Z" );

    jobject jrect = makeJRect( xwe, rect );

    result = (*env)->CallBooleanMethod( env, adraw->jdraw, mid, jrect, flags );
    deleteLocalRef( env, jrect );
    DRAW_CBK_HEADER_END();
    return result;
}

static void
and_draw_drawTrayDivider( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect, CellFlags flags )
{
    DRAW_CBK_HEADER( "drawTrayDivider", "(Landroid/graphics/Rect;I)V" );

    jobject jrect = makeJRect( xwe, rect );

    (*env)->CallVoidMethod( env, adraw->jdraw, mid, jrect, flags );
    deleteLocalRef( env, jrect );
    DRAW_CBK_HEADER_END();
}

static void
and_draw_score_pendingScore( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                             XP_S16 score, XP_U16 playerNum,
                             XP_Bool curTurn, CellFlags flags )
{
    DRAW_CBK_HEADER( "score_pendingScore", "(Landroid/graphics/Rect;IIZI)V" );

    jobject jrect = makeJRect( xwe, rect );

    (*env)->CallVoidMethod( env, adraw->jdraw, mid, jrect, score,
                            playerNum, curTurn, flags );
    deleteLocalRef( xwe, jrect );
    DRAW_CBK_HEADER_END();
}

static void
and_draw_objFinished( DrawCtx* dctx, XWEnv xwe, BoardObjectType typ,
                      const XP_Rect* rect, 
                      DrawFocusState XP_UNUSED(dfs) )
{
    DRAW_CBK_HEADER( "objFinished", "(ILandroid/graphics/Rect;)V" );

    jobject jrect = makeJRect( xwe, rect );
    (*env)->CallVoidMethod( env, adraw->jdraw, mid, (jint)typ, jrect );
    deleteLocalRef( xwe, jrect );

    if ( OBJ_BOARD == typ ) {
        deleteGlobalRef( env, adraw->jTvType );
    }
    DRAW_CBK_HEADER_END();
}

static void
and_draw_dictChanged( DrawCtx* dctx, XWEnv xwe, XP_S16 playerNum,
                      const DictionaryCtxt* dict )
{
    AndDraw* adraw = (AndDraw*)dctx;
    if ( !!dict && !!adraw->jdraw ) {
        XP_LOGFF( "(dict=%p/%s); code=%x", dict, dict_getName(dict), andDictID(dict) );
        const XP_UCHAR* isoCode = NULL;   /* A null dict means no-lang */
        if ( NULL != dict ) {
            isoCode = dict_getISOCode( dict );
        }
        /* Don't bother sending repeats. */
        if ( 0 != XP_STRCMP( isoCode, adraw->curISOCode ) ) {
            XP_STRNCPY( adraw->curISOCode, isoCode, VSIZE(adraw->curISOCode) );

            DRAW_CBK_HEADER( "dictChanged", "(J)V" );

            /* /\* create a DictWrapper object -- if the API changes to require it *\/ */
            /* jclass rclass = (*env)->FindClass( env, PKG_PATH("jni/XwJNI$DictWrapper") ); */
            /* // http://stackoverflow.com/questions/7260376/how-to-create-an-object-with-jni */
            /* const char* sig = "(L" PKG_PATH("jni/XwJNI") ";I)V"; */
            /* jmethodID initId = (*env)->GetMethodID( env, rclass, "<init>", sig ); */
            /* jobject jdict = (*env)->NewObject( env, rclass, initId, (int)dict ); */

            (*env)->CallVoidMethod( env, adraw->jdraw, mid, (jlong)dict );
            DRAW_CBK_HEADER_END();
        }
    }
}

#ifdef XWFEATURE_MINIWIN
static const XP_UCHAR* 
and_draw_getMiniWText( DrawCtx* dctx, XWEnv xwe, XWMiniTextType textHint )
{
    DRAW_CBK_HEADER( "getMiniWText", "(I)Ljava/lang/String;" );
    jstring jstr = (*env)->CallObjectMethod( env, adraw->jdraw, mid,
                                             textHint );
    const char* str = (*env)->GetStringUTFChars( env, jstr, NULL );
    snprintf( adraw->miniTextBuf, VSIZE(adraw->miniTextBuf), "%s", str );
    (*env)->ReleaseStringUTFChars( env, jstr, str );
    deleteLocalRef( env, jstr );
    DRAW_CBK_HEADER_END();
    return adraw->miniTextBuf;
}

static void
and_draw_measureMiniWText( DrawCtx* dctx, XWEnv xwe, const XP_UCHAR* textP,
                           XP_U16* width, XP_U16* height )
{
    DRAW_CBK_HEADER( "measureMiniWText", "(Ljava/lang/String;[I[I)V" );

    jintArray widthArray = (*env)->NewIntArray( env, 1 );
    jintArray heightArray = (*env)->NewIntArray( env, 1 );
    jstring jstr = (*env)->NewStringUTF( env, textP );

    (*env)->CallVoidMethod( env, adraw->jdraw, mid, jstr,
                            widthArray, heightArray );

    deleteLocalRef( env, jstr );

    int tmp;
    getIntsFromArray( env, &tmp, widthArray, 1, true );
    *width = tmp;
    getIntsFromArray( env, &tmp, heightArray, 1, true );
    *height = tmp;
    DRAW_CBK_HEADER_END();
}

static void 
and_draw_drawMiniWindow( DrawCtx* dctx, XWEnv xwe, const XP_UCHAR* text,
                         const XP_Rect* rect, void** closure )
{
    DRAW_CBK_HEADER( "drawMiniWindow",
                     "(Ljava/lang/String;Landroid/graphics/Rect;)V" );

    jstring jstr = (*env)->NewStringUTF( env, text );
    jobject jrect = makeJRect( xwe, rect );

    (*env)->CallVoidMethod( env, adraw->jdraw, mid, jstr, jrect );
    deleteLocalRefs( env, jstr, jrect, DELETE_NO_REF );
    DRAW_CBK_HEADER_END();
}
#endif

static XP_U16
and_draw_getThumbSize(DrawCtx* dctx, XWEnv xwe)
{
    XP_U16 result;
    LOG_FUNC();
    XP_ASSERT( DT_THUMB == dctx->dt );
    DRAW_CBK_HEADER( "getThumbSize", "()I" );
    result = (*env)->CallIntMethod( env, adraw->jdraw, mid );
    DRAW_CBK_HEADER_END();
    return result;
}

/* move me */
static void
fillStreamFromBA( JNIEnv* env, jbyteArray jstream, XWStreamCtxt* out )
{
    int len = (*env)->GetArrayLength( env, jstream );
    XP_LOGFF( "len: %d", len );
    jbyte* jelems = (*env)->GetByteArrayElements( env, jstream, NULL );
    stream_putBytes( out, jelems, len );
    (*env)->ReleaseByteArrayElements( env, jstream, jelems, 0 );
}

static void
and_draw_getThumbData(DrawCtx* dctx, XWEnv xwe, XWStreamCtxt* stream )
{
    LOG_FUNC();
    XP_ASSERT( DT_THUMB == dctx->dt );
    DRAW_CBK_HEADER( "getThumbData", "()[B" );
    jobject jarr = (*env)->CallObjectMethod( env, adraw->jdraw, mid );
    XP_LOGFF( "got %p from java", jarr );
    fillStreamFromBA( env, jarr, stream );
    deleteLocalRef( env, jarr );
    DRAW_CBK_HEADER_END();
}

static XP_Bool
draw_doNothing( DrawCtx* dctx, XWEnv xwe, ... )
{
    LOG_FUNC();
    return XP_FALSE;
} /* draw_doNothing */

DrawCtx* 
makeDraw( JNIEnv* env, jobject jdraw, DrawTarget dt )
{
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = mpool_make( NULL );
#endif
    AndDraw* adraw = (AndDraw*)XP_CALLOC( mpool, sizeof(*adraw) );
    DrawCtx* super = &adraw->super;
#ifdef MAP_THREAD_TO_ENV
    adraw->ti = ti;
#endif
#ifdef DEBUG
    // draw->creator.thread = pthread_self();
    // draw->creator.env = env;
#endif
    super->vtable = XP_MALLOC( mpool, sizeof(*super->vtable) );
    if ( NULL != jdraw ) {
        adraw->jdraw = (*env)->NewGlobalRef( env, jdraw );
    }
    MPASSIGN( adraw->mpool, mpool );

    for ( int ii = 0; ii < sizeof(super->vtable)/sizeof(void*); ++ii ) {
        ((void**)(super->vtable))[ii] = draw_doNothing;
    }

#define SET_PROC(nam) super->vtable->m_draw_##nam = and_draw_##nam
    SET_PROC(destroy);
    SET_PROC(beginDraw);
    SET_PROC(endDraw);
    SET_PROC(boardBegin);
    SET_PROC(scoreBegin);
    SET_PROC(measureScoreText);
    SET_PROC(score_drawPlayer);
    SET_PROC(measureRemText);
    SET_PROC(drawRemText);
    SET_PROC(drawTimer);

    SET_PROC(drawCell);
    SET_PROC(drawBoardArrow);
    SET_PROC(vertScrollBoard);

    SET_PROC(trayBegin);
    SET_PROC(drawTile);
    SET_PROC(drawTileMidDrag);
    SET_PROC(drawTileBack);
    SET_PROC(drawTrayDivider);
    SET_PROC(score_pendingScore);

    SET_PROC(objFinished);
    SET_PROC(dictChanged);

#ifdef XWFEATURE_MINIWIN
    SET_PROC(getMiniWText);
    SET_PROC(measureMiniWText);
    SET_PROC(drawMiniWindow);
#endif

    if ( DT_THUMB == dt ) {
        SET_PROC(getThumbSize);
        SET_PROC(getThumbData);
    }

    draw_super_init( super, dt );

#undef SET_PROC
    return super;
}

static void
deleteGlobalRef( JNIEnv* env, jobject jobj )
{
    if ( !!jobj ) {
        (*env)->DeleteGlobalRef( env, jobj );
    }
}
