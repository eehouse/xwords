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

#include "drawwrapper.h"
#include "andutils.h"

enum { 
    JCACHE_RECT0
    ,JCACHE_RECT1
    ,JCACHE_DSI
    ,JCACHE_COUNT
};

typedef struct _AndDraw {
    DrawCtxVTable* vtable;
    JNIEnv** env;
    jobject jdraw;             /* global ref; free it! */
    XP_LangCode curLang;
    jobject jCache[JCACHE_COUNT];
    XP_UCHAR miniTextBuf[128];
    MPSLOT
} AndDraw;

static jobject
makeJRect( AndDraw* draw, int indx, const XP_Rect* rect )
{
    JNIEnv* env = *draw->env;
    jobject robj = draw->jCache[indx];
    int right = rect->left + rect->width;
    int bottom = rect->top + rect->height;

    if ( !robj ) {
        jclass rclass = (*env)->FindClass( env, "android/graphics/Rect");
        jmethodID initId = (*env)->GetMethodID( env, rclass, "<init>", 
                                                "(IIII)V" );
        robj = (*env)->NewObject( env, rclass, initId, rect->left, rect->top,
                                  right, bottom );
                 
        (*env)->DeleteLocalRef( env, rclass );
        draw->jCache[indx] = robj;

        robj = (*env)->NewGlobalRef( env, robj );
        (*env)->DeleteLocalRef( env, robj );
    } else {
        setInt( env, robj, "left", rect->left );
        setInt( env, robj, "top", rect->top );
        setInt( env, robj, "right", right );
        setInt( env, robj, "bottom", bottom );
    }

    return robj;
} /* makeJRect */

static jobject
makeDSI( AndDraw* draw, int indx, const DrawScoreInfo* dsi )
{
    JNIEnv* env = *draw->env;
    jobject dsiobj = draw->jCache[indx];

    if ( !dsiobj ) {
        jclass rclass = (*env)->FindClass( env, "org/eehouse/android/"
                                           "xw4/jni/DrawScoreInfo");
        jmethodID initId = (*env)->GetMethodID( env, rclass, "<init>", "()V" );
        dsiobj = (*env)->NewObject( env, rclass, initId );
        (*env)->DeleteLocalRef( env, rclass );

        dsiobj = (*env)->NewGlobalRef( env, dsiobj );
        (*env)->DeleteLocalRef( env, dsiobj );
        draw->jCache[indx] = dsiobj;
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

#define DRAW_CBK_HEADER(nam,sig)                                \
    AndDraw* draw = (AndDraw*)dctx;                             \
    JNIEnv* env = *draw->env;                                   \
    XP_ASSERT( !!draw->jdraw );                                 \
    jmethodID mid = getMethodID( env, draw->jdraw, nam, sig );

static XP_Bool
and_draw_scoreBegin( DrawCtx* dctx, const XP_Rect* rect, 
                     XP_U16 numPlayers, 
                     const XP_S16* const scores,
                     XP_S16 remCount, DrawFocusState dfs )
{
    jboolean result;
    DRAW_CBK_HEADER("scoreBegin", "(Landroid/graphics/Rect;I[III)Z" );

    jint jarr[numPlayers];
    int ii;
    for ( ii = 0; ii < numPlayers; ++ii ) {
        jarr[ii] = scores[ii];
    }
    jintArray jscores = makeIntArray( env, numPlayers, jarr );
    jobject jrect = makeJRect( draw, JCACHE_RECT0, rect );

    result = (*env)->CallBooleanMethod( env, draw->jdraw, mid, 
                                        jrect, numPlayers, jscores, remCount, 
                                        dfs );

    (*env)->DeleteLocalRef( env, jscores );
    return result;
}

static void 
and_draw_measureRemText( DrawCtx* dctx, const XP_Rect* r, 
                         XP_S16 nTilesLeft, 
                         XP_U16* width, XP_U16* height )
{
    DRAW_CBK_HEADER("measureRemText", "(Landroid/graphics/Rect;I[I[I)V" );
    
    jintArray widthArray = makeIntArray( env, 1, NULL );
    jintArray heightArray = makeIntArray( env, 1, NULL );
    jobject jrect = makeJRect( draw, JCACHE_RECT0, r );

    (*env)->CallVoidMethod( env, draw->jdraw, mid, jrect, nTilesLeft, 
                            widthArray, heightArray );

    *width = getIntFromArray( env, widthArray, true );
    *height = getIntFromArray( env, heightArray, true );
} /* and_draw_measureRemText */

static void
and_draw_measureScoreText( DrawCtx* dctx, 
                           const XP_Rect* r, 
                           const DrawScoreInfo* dsi,
                           XP_U16* width, XP_U16* height )
{
    DRAW_CBK_HEADER("measureScoreText", 
                    "(Landroid/graphics/Rect;Lorg/eehouse/android/"
                    "xw4/jni/DrawScoreInfo;[I[I)V" );

    jobject jrect = makeJRect( draw, JCACHE_RECT0, r );
    jobject jdsi = makeDSI( draw, JCACHE_DSI, dsi );

    jintArray widthArray = makeIntArray( env, 1, NULL );
    jintArray heightArray = makeIntArray( env, 1, NULL );

    (*env)->CallVoidMethod( env, draw->jdraw, mid, jrect, jdsi,
                            widthArray, heightArray );

    *width = getIntFromArray( env, widthArray, true );
    *height = getIntFromArray( env, heightArray, true );
} /* and_draw_measureScoreText */

static void
and_draw_drawRemText( DrawCtx* dctx, const XP_Rect* rInner,
                      const XP_Rect* rOuter, 
                      XP_S16 nTilesLeft, XP_Bool focussed )
{
    DRAW_CBK_HEADER("drawRemText",
                    "(Landroid/graphics/Rect;Landroid/graphics/Rect;IZ)V" );

    jobject jrinner = makeJRect( draw, JCACHE_RECT0, rInner );
    jobject jrouter = makeJRect( draw, JCACHE_RECT1, rOuter );

    (*env)->CallVoidMethod( env, draw->jdraw, mid, jrinner, jrouter, 
                            nTilesLeft, focussed );
}

static void
and_draw_score_drawPlayer( DrawCtx* dctx,
                           const XP_Rect* rInner, 
                           const XP_Rect* rOuter, 
                           const DrawScoreInfo* dsi )
{
    DRAW_CBK_HEADER("score_drawPlayer", 
                    "(Landroid/graphics/Rect;Landroid/graphics/Rect;"
                    "Lorg/eehouse/android/xw4/jni/DrawScoreInfo;)V" );

    jobject jrinner = makeJRect( draw, JCACHE_RECT0, rInner );
    jobject jrouter = makeJRect( draw, JCACHE_RECT1, rOuter );
    jobject jdsi = makeDSI( draw, JCACHE_DSI, dsi );

    (*env)->CallVoidMethod( env, draw->jdraw, mid, jrinner, jrouter, jdsi );
} /* and_draw_score_drawPlayer */

static void
and_draw_drawTimer( DrawCtx* dctx, const XP_Rect* rect, XP_U16 player, 
                    XP_S16 secondsLeft )
{
    if ( rect->width == 0 ) {
        XP_LOGF( "%s: exiting b/c rect empty", __func__ );
    } else {
        DRAW_CBK_HEADER("drawTimer", "(Landroid/graphics/Rect;II)V" );

        jobject jrect = makeJRect( draw, JCACHE_RECT0, rect );
        (*env)->CallVoidMethod( env, draw->jdraw, mid, 
                                jrect, player, secondsLeft );
    }
}

static XP_Bool
and_draw_boardBegin( DrawCtx* dctx, const XP_Rect* rect, 
                     XP_U16 cellWidth, XP_U16 cellHeight, DrawFocusState dfs )
{
    DRAW_CBK_HEADER( "boardBegin", "(Landroid/graphics/Rect;III)Z" );

    jobject jrect = makeJRect( draw, JCACHE_RECT0, rect );

    jboolean result = (*env)->CallBooleanMethod( env, draw->jdraw, mid, 
                                                 jrect, cellWidth, cellHeight,
                                                 (jint)dfs );
    return result;
}

static XP_Bool 
and_draw_drawCell( DrawCtx* dctx, const XP_Rect* rect, const XP_UCHAR* text, 
                   const XP_Bitmaps* bitmaps, Tile tile, XP_S16 owner,
                   XWBonusType bonus, HintAtts hintAtts, CellFlags flags )
{
    DRAW_CBK_HEADER("drawCell",
                    "(Landroid/graphics/Rect;Ljava/lang/String;IIIII)Z" );
    jobject jrect = makeJRect( draw, JCACHE_RECT0, rect );
    jstring jtext = NULL;
    if ( !!text ) {
        if ( 0 == strcmp( "_", text ) ) {
            text = "?";
        }
        jtext = (*env)->NewStringUTF( env, text );
    }

    jboolean result = (*env)->CallBooleanMethod( env, draw->jdraw, mid, 
                                                 jrect, jtext, tile,
                                                 owner, bonus, hintAtts, 
                                                 flags );
    if ( !!jtext ) {
        (*env)->DeleteLocalRef( env, jtext );
    }

    return result;
}

static void
and_draw_drawBoardArrow(DrawCtx* dctx, const XP_Rect* rect, XWBonusType bonus, 
                        XP_Bool vert, HintAtts hintAtts, CellFlags flags )
{
    DRAW_CBK_HEADER("drawBoardArrow", "(Landroid/graphics/Rect;IZII)V" );

    jobject jrect = makeJRect( draw, JCACHE_RECT0, rect );
    (*env)->CallVoidMethod( env, draw->jdraw, mid, 
                            jrect, bonus, vert, hintAtts, flags );
}

static XP_Bool
and_draw_vertScrollBoard( DrawCtx* XP_UNUSED(dctx), XP_Rect* XP_UNUSED(rect), 
                          XP_S16 XP_UNUSED(dist), DrawFocusState XP_UNUSED(dfs) )
{
    /* Scrolling a bitmap in-place isn't any faster than drawing every cell
       anew so no point in calling into java. */
    return XP_FALSE;
}

static XP_Bool
and_draw_trayBegin( DrawCtx* dctx, const XP_Rect* rect, XP_U16 owner, 
                    DrawFocusState dfs )
{
    DRAW_CBK_HEADER( "trayBegin", "(Landroid/graphics/Rect;II)Z" );

    jobject jrect = makeJRect( draw, JCACHE_RECT0, rect );

    jboolean result = (*env)->CallBooleanMethod( env, draw->jdraw, mid, 
                                                 jrect, owner, (jint)dfs );

    return result;
}

static void
and_draw_drawTile( DrawCtx* dctx, const XP_Rect* rect, const XP_UCHAR* text, 
                   const XP_Bitmaps* bitmaps, XP_U16 val, CellFlags flags )
{
    DRAW_CBK_HEADER( "drawTile",
                     "(Landroid/graphics/Rect;Ljava/lang/String;II)V" );
    jobject jrect = makeJRect( draw, JCACHE_RECT0, rect );
    jstring jtext = NULL;
    if ( !!text ) {
        jtext = (*env)->NewStringUTF( env, text );
    }

    (*env)->CallVoidMethod( env, draw->jdraw, mid, 
                            jrect, jtext, val, flags );

    if ( !!jtext ) {
        (*env)->DeleteLocalRef( env, jtext );
    }
}

static void
and_draw_drawTileMidDrag( DrawCtx* dctx, const XP_Rect* rect, 
                          const XP_UCHAR* text, const XP_Bitmaps* bitmaps,
                          XP_U16 val, XP_U16 owner, CellFlags flags )
{
    DRAW_CBK_HEADER( "drawTileMidDrag", 
                     "(Landroid/graphics/Rect;Ljava/lang/String;III)V" );

    jobject jrect = makeJRect( draw, JCACHE_RECT0, rect );
    jstring jtext = NULL;
    if ( !!text ) {
        jtext = (*env)->NewStringUTF( env, text );
    }

    (*env)->CallVoidMethod( env, draw->jdraw, mid, 
                            jrect, jtext, val, owner, flags );

    if ( !!jtext ) {
        (*env)->DeleteLocalRef( env, jtext );
    }
}

static void 
and_draw_drawTileBack( DrawCtx* dctx, const XP_Rect* rect, CellFlags flags )
{
    DRAW_CBK_HEADER( "drawTileBack", "(Landroid/graphics/Rect;I)V" );

    jobject jrect = makeJRect( draw, JCACHE_RECT0, rect );

    (*env)->CallVoidMethod( env, draw->jdraw, mid, 
                            jrect, flags );
}

static void
and_draw_drawTrayDivider( DrawCtx* dctx, const XP_Rect* rect, CellFlags flags )
{
    DRAW_CBK_HEADER( "drawTrayDivider", "(Landroid/graphics/Rect;I)V" );

    jobject jrect = makeJRect( draw, JCACHE_RECT0, rect );

    (*env)->CallVoidMethod( env, draw->jdraw, mid, 
                            jrect, flags );
}

static void
and_draw_score_pendingScore( DrawCtx* dctx, const XP_Rect* rect, 
                             XP_S16 score, XP_U16 playerNum,
                             CellFlags flags )
{
    DRAW_CBK_HEADER( "score_pendingScore", "(Landroid/graphics/Rect;III)V" );

    jobject jrect = makeJRect( draw, JCACHE_RECT0, rect );

    (*env)->CallVoidMethod( env, draw->jdraw, mid, 
                            jrect, score, playerNum, flags );
}

static void
and_draw_objFinished( DrawCtx* dctx, BoardObjectType typ, 
                      const XP_Rect* rect, 
                      DrawFocusState dfs )
{
    DRAW_CBK_HEADER( "objFinished", "(ILandroid/graphics/Rect;I)V" );

    jobject jrect = makeJRect( draw, JCACHE_RECT0, rect );
    (*env)->CallVoidMethod( env, draw->jdraw, mid, 
                            (jint)typ, jrect, (jint)dfs );
}

static void
and_draw_dictChanged( DrawCtx* dctx, XP_S16 playerNum, 
                      const DictionaryCtxt* dict )
{
    AndDraw* draw = (AndDraw*)dctx;
    if ( NULL != draw->jdraw ) {
        XP_LangCode code = 0;   /* A null dict means no-lang */
        if ( NULL != dict ) {
            code = dict_getLangCode( dict );
        }
        /* Don't bother sending repeats. */
        if ( code != draw->curLang ) {
            draw->curLang = code;

            DRAW_CBK_HEADER( "dictChanged", "(I)V" );
            (*env)->CallVoidMethod( env, draw->jdraw, mid, (jint)dict );
        }
    }
}

static const XP_UCHAR* 
and_draw_getMiniWText( DrawCtx* dctx, XWMiniTextType textHint )
{
    DRAW_CBK_HEADER( "getMiniWText", "(I)Ljava/lang/String;" );
    jstring jstr = (*env)->CallObjectMethod( env, draw->jdraw, mid,
                                            textHint );
    const char* str = (*env)->GetStringUTFChars( env, jstr, NULL );
    snprintf( draw->miniTextBuf, VSIZE(draw->miniTextBuf), "%s", str );
    (*env)->ReleaseStringUTFChars( env, jstr, str );
    (*env)->DeleteLocalRef( env, jstr );
    return draw->miniTextBuf;
}

static void
and_draw_measureMiniWText( DrawCtx* dctx, const XP_UCHAR* textP, 
                           XP_U16* width, XP_U16* height )
{
    DRAW_CBK_HEADER( "measureMiniWText", "(Ljava/lang/String;[I[I)V" );

    jintArray widthArray = makeIntArray( env, 1, NULL );
    jintArray heightArray = makeIntArray( env, 1, NULL );
    jstring jstr = (*env)->NewStringUTF( env, textP );

    (*env)->CallVoidMethod( env, draw->jdraw, mid, 
                            jstr, widthArray, heightArray );

    (*env)->DeleteLocalRef( env, jstr );
    *width = getIntFromArray( env, widthArray, true );
    *height = getIntFromArray( env, heightArray, true );
}

static void 
and_draw_drawMiniWindow( DrawCtx* dctx, const XP_UCHAR* text,
                         const XP_Rect* rect, void** closure )
{
    DRAW_CBK_HEADER( "drawMiniWindow",
                     "(Ljava/lang/String;Landroid/graphics/Rect;)V" );

    jstring jstr = (*env)->NewStringUTF( env, text );
    jobject jrect = makeJRect( draw, JCACHE_RECT0, rect );

    (*env)->CallVoidMethod( env, draw->jdraw, mid, 
                            jstr, jrect );

    (*env)->DeleteLocalRef( env, jstr );
}

static XP_Bool
draw_doNothing( DrawCtx* dctx, ... )
{
    LOG_FUNC();
    return XP_FALSE;
} /* draw_doNothing */

DrawCtx* 
makeDraw( MPFORMAL JNIEnv** envp, jobject jdraw )
{
    AndDraw* draw = (AndDraw*)XP_CALLOC( mpool, sizeof(*draw) );
    JNIEnv* env = *envp;
    draw->vtable = XP_MALLOC( mpool, sizeof(*draw->vtable) );
    if ( NULL != jdraw ) {
        draw->jdraw = (*env)->NewGlobalRef( env, jdraw );
    }
    draw->env = envp;
    MPASSIGN( draw->mpool, mpool );

    int ii;
    for ( ii = 0; ii < sizeof(*draw->vtable)/4; ++ii ) {
        ((void**)(draw->vtable))[ii] = draw_doNothing;
    }

#define SET_PROC(nam) draw->vtable->m_draw_##nam = and_draw_##nam
    SET_PROC(boardBegin);
    SET_PROC(scoreBegin);
    SET_PROC(measureRemText);
    SET_PROC(measureScoreText);
    SET_PROC(drawRemText);
    SET_PROC(score_drawPlayer);
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

    SET_PROC(getMiniWText);
    SET_PROC(measureMiniWText);
    SET_PROC(drawMiniWindow);

#undef SET_PROC
    return (DrawCtx*)draw;
}

void
destroyDraw( DrawCtx** dctx )
{
    AndDraw* draw = (AndDraw*)*dctx;
    JNIEnv* env = *draw->env;
    if ( NULL != draw->jdraw ) {
        (*env)->DeleteGlobalRef( env, draw->jdraw );
    }

    int ii;
    for ( ii = 0; ii < JCACHE_COUNT; ++ii ) {
        jobject jobj = draw->jCache[ii];
        if ( !!jobj ) {
            (*env)->DeleteGlobalRef( env, jobj );
        }
    }

    XP_FREE( draw->mpool, draw->vtable );
    XP_FREE( draw->mpool, draw );
    *dctx = NULL;
}
