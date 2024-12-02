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

enum { 
    JCACHE_RECT0
    ,JCACHE_RECT1
#ifdef XWFEATURE_SCOREONEPASS
    ,JCACHE_DSIS
    ,JCACHE_RECTS
#else
    ,JCACHE_DSI
#endif
    ,JCACHE_COUNT
};

typedef struct _ObjCacheElem {
    jobject obj;
#ifdef DEBUG
    pthread_t owner;
#endif
} ObjCacheElem;

typedef struct _AndDraw {
    DrawCtxVTable* vtable;
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
    ObjCacheElem jCache[JCACHE_COUNT];
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
makeJRect( AndDraw* draw, JNIEnv* env, const int indx, const XP_Rect* rect )
{
    ObjCacheElem* elem = &draw->jCache[indx];
    jobject robj = elem->obj;
#ifdef DEBUG
    if ( CHECKOUT_MARKER == robj ) {
        XP_LOGFF( "Error: cache obj[%d] taken by %lX (I am %lX)",
                  indx, elem->owner, pthread_self() );
    }
#endif
    int right = rect->left + rect->width;
    int bottom = rect->top + rect->height;

    if ( !robj ) {
        robj = makeObject(env, "android/graphics/Rect", "(IIII)V",
                          rect->left, rect->top, right, bottom );
                 
        elem->obj = (*env)->NewGlobalRef( env, robj );
        deleteLocalRef( env, robj );
        robj = elem->obj;
    } else {
        setInt( env, robj, "left", rect->left );
        setInt( env, robj, "top", rect->top );
        setInt( env, robj, "right", right );
        setInt( env, robj, "bottom", bottom );
    }

#ifdef DEBUG
    elem->obj = CHECKOUT_MARKER;
    elem->owner = pthread_self();
#endif
    return robj;
} /* makeJRect */

#ifdef DEBUG
static void
returnJRect( AndDraw* draw, const int indx, jobject used )
{
    ObjCacheElem* elem = &draw->jCache[indx];
    if ( CHECKOUT_MARKER != elem->obj ) { /* failed!! */
        XP_LOGFF( "ERROR: obj[%d] already in use by thread: %lX. "
                  "Me: %lX", indx, elem->owner, pthread_self() );
    }
    elem->obj = used;
}
#else
# define returnJRect( draw, indx, used )
#endif

#ifdef XWFEATURE_SCOREONEPASS
static void
readJRect( JNIEnv* env, XP_Rect* rect, jobject jrect )
{
    rect->left = getInt( env, jrect, "left" );
    rect->top = getInt( env, jrect, "top" );
    rect->width = getInt( env, jrect, "right" ) - rect->left;
    rect->height = getInt( env, jrect, "bottom" ) - rect->top;
}

static jobject
makeJRects( AndDraw* draw, XWEnv xwe, int indx, XP_U16 nPlayers, const XP_Rect rects[] )
{
    JNIEnv* env = xwe;
    ObjCacheElem* elem = &draw->jCache[indx];
    jobject jrects = elem->obj;
    if ( !jrects ) {
        jclass rclass = (*env)->FindClass( env, "android/graphics/Rect");
        jrects = (*env)->NewObjectArray( env, nPlayers, rclass, NULL );
        elem->obj = (*env)->NewGlobalRef( env, jrects );
        jrects = elem->obj;

        jmethodID initId = (*env)->GetMethodID( env, rclass, "<init>", 
                                                "()V" );

        for ( int ii = 0; ii < nPlayers; ++ii ) {
            jobject jrect = (*env)->NewObject( env, rclass, initId );
            (*env)->SetObjectArrayElement( env, jrects, ii, jrect );
            deleteLocalRef( env, jrect );
        }

        deleteLocalRefs( env, rclass, jrects, DELETE_NO_REF );
    }

    if ( NULL != rects ) {
        XP_ASSERT(0);
        /* for ( ii = 0; ii < nPlayers; ++ii ) { */
        /*     jobject jrect = (*env)->GetObjectArrayElement( env, jrects, ii ); */
        /*     writeJRect( env, jrect, &rects[ii] ); */
        /* } */
    }

    return jrects;
}

static jobject
makeDSIs( AndDraw* draw, XWEnv xwe, const int indx, XP_U16 nPlayers,
          const DrawScoreInfo dsis[] )
{
    JNIEnv* env = xwe;
    ObjCacheElem* elem = &draw->jCache[indx];
    jobject dsiobjs = elem->obj;

    if ( !dsiobjs ) {
        jclass clas = (*env)->FindClass( env, PKG_PATH(DSI_PATH) );
        dsiobjs = (*env)->NewObjectArray( env, nPlayers, clas, NULL );
        elem->obj = (*env)->NewGlobalRef( env, dsiobjs );
        deleteLocalRef( env, dsiobjs );
        dsiobjs = elem->obj;

        jmethodID initId = (*env)->GetMethodID( env, clas, "<init>", "()V" );
        for ( int ii = 0; ii < nPlayers; ++ii ) {
            jobject dsiobj = (*env)->NewObject( env, clas, initId );
            (*env)->SetObjectArrayElement( env, dsiobjs, ii, dsiobj );
            deleteLocalRef( env, dsiobj );
        }

        deleteLocalRef( env, clas );
    }

    for ( int ii = 0; ii < nPlayers; ++ii ) {
        jobject dsiobj = (*env)->GetObjectArrayElement( env, dsiobjs, ii );
        const DrawScoreInfo* dsi = &dsis[ii];

        setInt( env, dsiobj, "playerNum", dsi->playerNum );
        setInt( env, dsiobj, "totalScore", dsi->totalScore );
        setInt( env, dsiobj, "nTilesLeft", dsi->nTilesLeft );
        setInt( env, dsiobj, "flags", dsi->flags );
        setBool( env, dsiobj, "isTurn", dsi->isTurn );
        setBool( env, dsiobj, "selected", dsi->selected );
        setBool( env, dsiobj, "isRemote", dsi->isRemote );
        setBool( env, dsiobj, "isRobot", dsi->isRobot );
        setString( env, dsiobj, "name", dsi->name );
    }
    return dsiobjs;
}

#else

static jobject
makeDSI( AndDraw* draw, XWEnv xwe, const int indx, const DrawScoreInfo* dsi )
{
    JNIEnv* env = xwe;
    ObjCacheElem* elem = &draw->jCache[indx];
    jobject dsiobj = elem->obj;

    if ( !dsiobj ) {
        dsiobj = makeObjectEmptyConstr( env, PKG_PATH(DSI_PATH) );

        elem->obj = (*env)->NewGlobalRef( env, dsiobj );
        deleteLocalRef( env, dsiobj );
        dsiobj = elem->obj;
    }

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
#endif

#define DRAW_CBK_HEADER(nam,sig) {                              \
    JNIEnv* env = xwe;                                          \
    AndDraw* draw = (AndDraw*)dctx;                             \
    AND_DRAW_START(draw, env);                                  \
    ASSERT_ENV( draw->ti, env );                                \
    XP_ASSERT( !!draw->jdraw );                                 \
    jmethodID mid = getMethodID( xwe, draw->jdraw, nam, sig )   \

#define DRAW_CBK_HEADER_END() }                 \
    AND_DRAW_END();                             \

static XP_Bool
and_draw_scoreBegin( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                     XP_U16 numPlayers, const XP_S16* const scores,
                     XP_S16 remCount, DrawFocusState XP_UNUSED(dfs) )
{
    jboolean result;
    DRAW_CBK_HEADER("scoreBegin", "(Landroid/graphics/Rect;I[II)Z" );

    jint jarr[numPlayers];

    for ( int ii = 0; ii < numPlayers; ++ii ) {
        jarr[ii] = scores[ii];
    }
    jintArray jscores = makeIntArray( env, numPlayers, jarr, sizeof(jarr[0]) );
    jobject jrect = makeJRect( draw, env, JCACHE_RECT0, rect );

    result = (*env)->CallBooleanMethod( env, draw->jdraw, mid, 
                                        jrect, numPlayers, jscores, remCount );

    returnJRect( draw, JCACHE_RECT0, jrect );
    deleteLocalRef( env, jscores );
    DRAW_CBK_HEADER_END();
    return result;
}

#ifdef XWFEATURE_SCOREONEPASS
static XP_Bool
and_draw_drawRemText( DrawCtx* dctx, XP_S16 nTilesLeft, 
                      XP_Bool focussed, XP_Rect* rect )
{
    DRAW_CBK_HEADER("drawRemText", "(IZLandroid/graphics/Rect;)Z" );

    jobject jrect = makeJRect( draw, env, JCACHE_RECT0, rect );
    jboolean result = (*env)->CallBooleanMethod( env, draw->jdraw, mid, 
                                                 nTilesLeft, focussed, jrect );
    if ( result ) {
        readJRect( env, rect, jrect );
    }
    returnJRect( draw, JCACHE_RECT0, jrect );
    DRAW_CBK_HEADER_END();
    return result;
}

static void
and_draw_score_drawPlayers( DrawCtx* dctx, XWEnv xwe, const XP_Rect* scoreRect,
                            XP_U16 nPlayers, DrawScoreInfo playerData[], 
                            XP_Rect playerRects[] )
{
    DRAW_CBK_HEADER("score_drawPlayers", "(Landroid/graphics/Rect;"
                    "[L" PKG_PATH(DSI_PATH) ";"
                    "[Landroid/graphics/Rect;)V" );

    jobject jrect = makeJRect( draw, env, JCACHE_RECT0, scoreRect );
    jobject jdsis = makeDSIs( draw, xwe, JCACHE_DSIS, nPlayers, playerData );
    jobject jrects = makeJRects( draw, env, JCACHE_RECTS, nPlayers, NULL );
    (*env)->CallVoidMethod( env, draw->jdraw, mid, jrect, jdsis, jrects );

    for ( int ii = 0; ii < nPlayers; ++ii ) {
        jobject jrect = (*env)->GetObjectArrayElement( env, jrects, ii );
        readJRect( env, &playerRects[ii], jrect );
    }
    returnJRect( draw, JCACHE_RECT0, jrect );
    DRAW_CBK_HEADER_END();
}

#else

static XP_Bool
and_draw_measureRemText( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                         XP_S16 nTilesLeft, 
                         XP_U16* width, XP_U16* height )
{
    jboolean result;
    DRAW_CBK_HEADER("measureRemText", "(Landroid/graphics/Rect;I[I[I)Z" );

    jintArray widthArray = (*env)->NewIntArray( env, 1 );
    jintArray heightArray = (*env)->NewIntArray( env, 1 );
    jobject jrect = makeJRect( draw, xwe, JCACHE_RECT0, rect );

    result = (*env)->CallBooleanMethod( env, draw->jdraw, mid, jrect,
                                        nTilesLeft, widthArray,
                                        heightArray );
    if ( result ) {
        int tmp;
        getIntsFromArray( env, &tmp, widthArray, 1, true );
        *width = tmp;
        getIntsFromArray( env, &tmp, heightArray, 1, true );
        *height = tmp;
    }
    returnJRect( draw, JCACHE_RECT0, jrect );
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

    jobject jrinner = makeJRect( draw, env, JCACHE_RECT0, rInner );
    jobject jrouter = makeJRect( draw, env, JCACHE_RECT1, rOuter );

    (*env)->CallVoidMethod( env, draw->jdraw, mid, jrinner, jrouter, 
                            nTilesLeft, focussed );
    returnJRect( draw, JCACHE_RECT0, jrinner );
    returnJRect( draw, JCACHE_RECT1, jrouter );
    DRAW_CBK_HEADER_END();
}

static void
and_draw_measureScoreText( DrawCtx* dctx, XWEnv xwe, const XP_Rect* r,
                           const DrawScoreInfo* dsi,
                           XP_U16* width, XP_U16* height )
{
    DRAW_CBK_HEADER("measureScoreText", 
                    "(Landroid/graphics/Rect;L"
                    PKG_PATH(DSI_PATH) ";[I[I)V" );

    jobject jrect = makeJRect( draw, env, JCACHE_RECT0, r );
    jobject jdsi = makeDSI( draw, xwe, JCACHE_DSI, dsi );

    jintArray widthArray = (*env)->NewIntArray( env, 1 );
    jintArray heightArray = (*env)->NewIntArray( env, 1 );

    (*env)->CallVoidMethod( env, draw->jdraw, mid, jrect, jdsi,
                            widthArray, heightArray );
    returnJRect( draw, JCACHE_RECT0, jrect );

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

    jobject jrinner = makeJRect( draw, xwe, JCACHE_RECT0, rInner );
    jobject jrouter = makeJRect( draw, xwe, JCACHE_RECT1, rOuter );
    jobject jdsi = makeDSI( draw, xwe, JCACHE_DSI, dsi );

    (*env)->CallVoidMethod( env, draw->jdraw, mid, jrinner, jrouter, gotPct, 
                            jdsi );
    returnJRect( draw, JCACHE_RECT0, jrinner );
    returnJRect( draw, JCACHE_RECT1, jrouter );
    DRAW_CBK_HEADER_END();
} /* and_draw_score_drawPlayer */
#endif

static void
and_draw_drawTimer( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect, XP_U16 player,
                    XP_S16 secondsLeft, XP_Bool inDuplicateMode )
{
    if ( rect->width == 0 ) {
        XP_LOGFF( "exiting b/c rect empty" );
    } else {
        DRAW_CBK_HEADER("drawTimer", "(Landroid/graphics/Rect;IIZ)V" );

        jobject jrect = makeJRect( draw, xwe, JCACHE_RECT0, rect );
        (*env)->CallVoidMethod( env, draw->jdraw, mid, 
                                jrect, player, secondsLeft, inDuplicateMode );
        returnJRect( draw, JCACHE_RECT0, jrect );
        DRAW_CBK_HEADER_END();
    }
}

/* Not used on android yet */
static XP_Bool and_draw_beginDraw( DrawCtx* dctx, XWEnv xwe )
{
    AND_DRAW_START(dctx, xwe);
    AND_DRAW_END();
    return XP_TRUE;
}
static void and_draw_endDraw( DrawCtx* XP_UNUSED(dctx), XWEnv XP_UNUSED(xwe) ) {}

static XP_Bool
and_draw_boardBegin( DrawCtx* dctx, XWEnv xwe, const XP_Rect* XP_UNUSED(rect),
                     XP_U16 XP_UNUSED(cellWidth), XP_U16 XP_UNUSED(cellHeight),
                     DrawFocusState XP_UNUSED(dfs), TileValueType tvType )
{
    AND_DRAW_START(dctx, xwe);
    JNIEnv* env = xwe;
    AndDraw* draw = (AndDraw*)dctx;

    jobject jTvType = intToJEnum( env, tvType, PKG_PATH(TVT_PATH) );
    draw->jTvType = (*env)->NewGlobalRef( env, jTvType );
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

    jobject jrect = makeJRect( draw, xwe, JCACHE_RECT0, rect );
    jstring jtext = NULL;
    if ( !!text ) {
        if ( 0 == strcmp( "_", text ) ) {
            text = "?";
        }
        jtext = (*env)->NewStringUTF( env, text );
    }

    result = (*env)->CallBooleanMethod( env, draw->jdraw, mid, jrect,
                                        jtext, tile, value, owner, bonus,
                                        flags, draw->jTvType );
    returnJRect( draw, JCACHE_RECT0, jrect );
    deleteLocalRef( env, jtext );

    DRAW_CBK_HEADER_END();
    return result;
}

static void
and_draw_drawBoardArrow( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                         XWBonusType bonus,XP_Bool vert, HintAtts hintAtts,
                         CellFlags flags )
{
    DRAW_CBK_HEADER("drawBoardArrow", "(Landroid/graphics/Rect;IZII)V" );

    jobject jrect = makeJRect( draw, xwe, JCACHE_RECT0, rect );
    (*env)->CallVoidMethod( env, draw->jdraw, mid, 
                            jrect, bonus, vert, hintAtts, flags );
    returnJRect( draw, JCACHE_RECT0, jrect );
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
    jboolean result;
    DRAW_CBK_HEADER( "trayBegin", "(Landroid/graphics/Rect;II)Z" );

    jobject jrect = makeJRect( draw, xwe, JCACHE_RECT0, rect );

    result = (*env)->CallBooleanMethod( env, draw->jdraw, mid, jrect, owner, score );
    returnJRect( draw, JCACHE_RECT0, jrect );
    DRAW_CBK_HEADER_END();
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
    jobject jrect = makeJRect( draw, xwe, JCACHE_RECT0, rect );
    jstring jtext = NULL;
    if ( !!text ) {
        jtext = (*env)->NewStringUTF( env, text );
    }

    result = (*env)->CallBooleanMethod( env, draw->jdraw, mid, 
                                        jrect, jtext, val, flags );
    returnJRect( draw, JCACHE_RECT0, jrect );
    deleteLocalRef( env, jtext );
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

    jobject jrect = makeJRect( draw, xwe, JCACHE_RECT0, rect );
    jstring jtext = NULL;
    if ( !!text ) {
        jtext = (*env)->NewStringUTF( env, text );
    }

    result = (*env)->CallBooleanMethod( env, draw->jdraw, mid, 
                                        jrect, jtext, val, owner, flags );
    returnJRect( draw, JCACHE_RECT0, jrect );
    deleteLocalRef( env, jtext );
    DRAW_CBK_HEADER_END();
    return result;
}

static XP_Bool
and_draw_drawTileBack( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect, CellFlags flags )
{
    XP_Bool result;
    DRAW_CBK_HEADER( "drawTileBack", "(Landroid/graphics/Rect;I)Z" );

    jobject jrect = makeJRect( draw, xwe, JCACHE_RECT0, rect );

    result = (*env)->CallBooleanMethod( env, draw->jdraw, mid, jrect, flags );
    returnJRect( draw, JCACHE_RECT0, jrect );
    DRAW_CBK_HEADER_END();
    return result;
}

static void
and_draw_drawTrayDivider( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect, CellFlags flags )
{
    DRAW_CBK_HEADER( "drawTrayDivider", "(Landroid/graphics/Rect;I)V" );

    jobject jrect = makeJRect( draw, xwe, JCACHE_RECT0, rect );

    (*env)->CallVoidMethod( env, draw->jdraw, mid, 
                            jrect, flags );
    returnJRect( draw, JCACHE_RECT0, jrect );
    DRAW_CBK_HEADER_END();
}

static void
and_draw_score_pendingScore( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                             XP_S16 score, XP_U16 playerNum,
                             XP_Bool curTurn, CellFlags flags )
{
    DRAW_CBK_HEADER( "score_pendingScore", "(Landroid/graphics/Rect;IIZI)V" );

    jobject jrect = makeJRect( draw, xwe, JCACHE_RECT0, rect );

    (*env)->CallVoidMethod( env, draw->jdraw, mid, 
                            jrect, score, playerNum, curTurn, flags );
    returnJRect( draw, JCACHE_RECT0, jrect );
    DRAW_CBK_HEADER_END();
}

static void
and_draw_objFinished( DrawCtx* dctx, XWEnv xwe, BoardObjectType typ,
                      const XP_Rect* rect, 
                      DrawFocusState XP_UNUSED(dfs) )
{
#ifndef XWFEATURE_SCOREONEPASS
    DRAW_CBK_HEADER( "objFinished", "(ILandroid/graphics/Rect;)V" );

    jobject jrect = makeJRect( draw, xwe, JCACHE_RECT0, rect );
    (*env)->CallVoidMethod( env, draw->jdraw, mid, 
                            (jint)typ, jrect );
    returnJRect( draw, JCACHE_RECT0, jrect );

    if ( OBJ_BOARD == typ ) {
        deleteGlobalRef( env, draw->jTvType );
    }
    DRAW_CBK_HEADER_END();
#endif
}

static void
and_draw_dictChanged( DrawCtx* dctx, XWEnv xwe, XP_S16 playerNum,
                      const DictionaryCtxt* dict )
{
    AndDraw* draw = (AndDraw*)dctx;
    if ( !!dict && !!draw->jdraw ) {
        XP_LOGFF( "(dict=%p/%s); code=%x", dict, dict_getName(dict), andDictID(dict) );
        const XP_UCHAR* isoCode = NULL;   /* A null dict means no-lang */
        if ( NULL != dict ) {
            isoCode = dict_getISOCode( dict );
        }
        /* Don't bother sending repeats. */
        if ( 0 != XP_STRCMP( isoCode, draw->curISOCode ) ) {
            XP_STRNCPY( draw->curISOCode, isoCode, VSIZE(draw->curISOCode) );

            DRAW_CBK_HEADER( "dictChanged", "(J)V" );

            /* /\* create a DictWrapper object -- if the API changes to require it *\/ */
            /* jclass rclass = (*env)->FindClass( env, PKG_PATH("jni/XwJNI$DictWrapper") ); */
            /* // http://stackoverflow.com/questions/7260376/how-to-create-an-object-with-jni */
            /* const char* sig = "(L" PKG_PATH("jni/XwJNI") ";I)V"; */
            /* jmethodID initId = (*env)->GetMethodID( env, rclass, "<init>", sig ); */
            /* jobject jdict = (*env)->NewObject( env, rclass, initId, (int)dict ); */

            (*env)->CallVoidMethod( env, draw->jdraw, mid, (jlong)dict );
            DRAW_CBK_HEADER_END();
        }
    }
}

#ifdef XWFEATURE_MINIWIN
static const XP_UCHAR* 
and_draw_getMiniWText( DrawCtx* dctx, XWEnv xwe, XWMiniTextType textHint )
{
    DRAW_CBK_HEADER( "getMiniWText", "(I)Ljava/lang/String;" );
    jstring jstr = (*env)->CallObjectMethod( env, draw->jdraw, mid,
                                             textHint );
    const char* str = (*env)->GetStringUTFChars( env, jstr, NULL );
    snprintf( draw->miniTextBuf, VSIZE(draw->miniTextBuf), "%s", str );
    (*env)->ReleaseStringUTFChars( env, jstr, str );
    deleteLocalRef( env, jstr );
    DRAW_CBK_HEADER_END();
    return draw->miniTextBuf;
}

static void
and_draw_measureMiniWText( DrawCtx* dctx, XWEnv xwe, const XP_UCHAR* textP,
                           XP_U16* width, XP_U16* height )
{
    DRAW_CBK_HEADER( "measureMiniWText", "(Ljava/lang/String;[I[I)V" );

    jintArray widthArray = (*env)->NewIntArray( env, 1 );
    jintArray heightArray = (*env)->NewIntArray( env, 1 );
    jstring jstr = (*env)->NewStringUTF( env, textP );

    (*env)->CallVoidMethod( env, draw->jdraw, mid, 
                            jstr, widthArray, heightArray );

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
    jobject jrect = makeJRect( draw, xwe, JCACHE_RECT0, rect );

    (*env)->CallVoidMethod( env, draw->jdraw, mid, 
                            jstr, jrect );
    returnJRect( draw, JCACHE_RECT0, jrect );
    deleteLocalRef( env, jstr );
    DRAW_CBK_HEADER_END();
}
#endif

static XP_Bool
draw_doNothing( DrawCtx* dctx, XWEnv xwe, ... )
{
    LOG_FUNC();
    return XP_FALSE;
} /* draw_doNothing */

DrawCtx* 
makeDraw( MPFORMAL JNIEnv* env,
#ifdef MAP_THREAD_TO_ENV
          EnvThreadInfo* ti,
#endif
          jobject jdraw )
{
    AndDraw* draw = (AndDraw*)XP_CALLOC( mpool, sizeof(*draw) );
#ifdef MAP_THREAD_TO_ENV
    draw->ti = ti;
#endif
#ifdef DEBUG
    // draw->creator.thread = pthread_self();
    // draw->creator.env = env;
#endif
    draw->vtable = XP_MALLOC( mpool, sizeof(*draw->vtable) );
    if ( NULL != jdraw ) {
        draw->jdraw = (*env)->NewGlobalRef( env, jdraw );
    }
    MPASSIGN( draw->mpool, mpool );

    for ( int ii = 0; ii < sizeof(*draw->vtable)/sizeof(void*); ++ii ) {
        ((void**)(draw->vtable))[ii] = draw_doNothing;
    }

#define SET_PROC(nam) draw->vtable->m_draw_##nam = and_draw_##nam
    SET_PROC(beginDraw);
    SET_PROC(endDraw);
    SET_PROC(boardBegin);
    SET_PROC(scoreBegin);
#ifdef XWFEATURE_SCOREONEPASS
    SET_PROC(score_drawPlayers);
    SET_PROC(drawRemText);
#else
    SET_PROC(measureScoreText);
    SET_PROC(score_drawPlayer);
    SET_PROC(measureRemText);
    SET_PROC(drawRemText);
#endif
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

#undef SET_PROC
    return (DrawCtx*)draw;
}

static void
deleteGlobalRef( JNIEnv* env, jobject jobj )
{
    if ( !!jobj ) {
        (*env)->DeleteGlobalRef( env, jobj );
    }
}

void
destroyDraw( DrawCtx** dctx, JNIEnv* env )
{
    if ( !!*dctx ) {
        AndDraw* draw = (AndDraw*)*dctx;
        deleteGlobalRef( env, draw->jdraw );

        for ( int ii = 0; ii < JCACHE_COUNT; ++ii ) {
            deleteGlobalRef( env, draw->jCache[ii].obj );
        }

        XP_FREE( draw->mpool, draw->vtable );
        XP_FREE( draw->mpool, draw );
        *dctx = NULL;
    }
}
