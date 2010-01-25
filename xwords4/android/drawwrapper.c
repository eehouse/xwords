/* -*-mode: C; compile-command: "cd XWords4; ../scripts/ndkbuild.sh"; -*- */
/* 
 * Copyright 2001-2009 by Eric House (xwords@eehouse.org).  All rights
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

typedef struct _AndDraw {
    DrawCtxVTable* vtable;
    JNIEnv** env;
    jobject j_draw;             /* global ref; free it! */
    MPSLOT
} AndDraw;

static jobject
makeJRect( JNIEnv *env, const XP_Rect* rect )
{
    jclass rclass = (*env)->FindClass( env, "android/graphics/Rect");
    jmethodID initId = (*env)->GetMethodID( env, rclass, "<init>", "(IIII)V" );
    jobject robj = (*env)->NewObject( env, rclass, initId, rect->left, rect->top,
                                      rect->left + rect->width,
                                      rect->top + rect->height );
#ifdef DEBUG
    int test;
    XP_ASSERT( getInt( env, robj, "left", &test ) && (test == rect->left) );
    XP_ASSERT( getInt( env, robj, "top", &test ) && (test == rect->top) );
    XP_ASSERT( getInt( env, robj, "right", &test )
               && (test == rect->left + rect->width ) );
    XP_ASSERT( getInt( env, robj, "bottom", &test ) 
               && (test == rect->top + rect->height ) );
#endif

    (*env)->DeleteLocalRef( env, rclass );
    return robj;
} /* makeJRect */

static void
copyJRect( JNIEnv* env, XP_Rect* dest, jobject jrect )
{
    int tmp;
    getInt( env, jrect, "left", &tmp );
    dest->left = tmp;
    getInt( env, jrect, "top", &tmp ); 
    dest->top = tmp;
    getInt( env, jrect, "right", &tmp ); 
    dest->width = tmp - dest->left;
    getInt( env, jrect, "bottom", &tmp ); 
    dest->height = tmp - dest->top;
}

static jobject
makeDSI( JNIEnv* env, const DrawScoreInfo* dsi )
{
    jclass rclass = (*env)->FindClass( env, "org/eehouse/android/xw4/jni/DrawScoreInfo");
    jmethodID initId = (*env)->GetMethodID( env, rclass, "<init>", "()V" );
    jobject dsiobj = (*env)->NewObject( env, rclass, initId );

    /* public String name; */
    /* public int playerNum; */
    /* public int totalScore; */
    /* public int nTilesLeft;   /\* < 0 means don't use *\/ */
    /* public int flags;        // was CellFlags; use CELL_ constants above */
    /* public boolean isTurn; */
    /* public boolean selected; */
    /* public boolean isRemote; */
    /* public boolean isRobot; */


    bool success = setInt( env, dsiobj, "playerNum", dsi->playerNum )
        && setInt( env, dsiobj, "totalScore", dsi->totalScore )
        && setInt( env, dsiobj, "nTilesLeft", dsi->nTilesLeft )
        && setInt( env, dsiobj, "flags", dsi->flags )
        && setBool( env, dsiobj, "isTurn", dsi->isTurn )
        && setBool( env, dsiobj, "selected", dsi->selected )
        && setBool( env, dsiobj, "isRemote", dsi->isRemote )
        && setBool( env, dsiobj, "isRobot", dsi->isRobot )
        && setString( env, dsiobj, "name", dsi->name )
        ;
    
    (*env)->DeleteLocalRef( env, rclass );

    return dsiobj;
}

#define DRAW_CBK_HEADER(nam,sig)                                \
    AndDraw* draw = (AndDraw*)dctx;                             \
    JNIEnv* env = *draw->env;                                   \
    jmethodID mid = getMethodID( env, draw->j_draw, nam, sig );

static void 
and_draw_scoreBegin( DrawCtx* dctx, const XP_Rect* rect, 
                     XP_U16 numPlayers, 
                     const XP_S16* const scores,
                     XP_S16 remCount, DrawFocusState dfs )
{
    DRAW_CBK_HEADER("scoreBegin", "(Landroid/graphics/Rect;I[III)V" );

    jint jarr[numPlayers];
    int ii;
    for ( ii = 0; ii < numPlayers; ++ii ) {
        jarr[ii] = scores[ii];
    }
    jintArray jscores = makeIntArray( env, numPlayers, jarr );
    jobject jrect = makeJRect( env, rect );

    (*env)->CallVoidMethod( env, draw->j_draw, mid, 
                            jrect, numPlayers, jscores, remCount, dfs );

    (*env)->DeleteLocalRef( env, jscores );
    (*env)->DeleteLocalRef( env, jrect );
}

static void 
and_draw_measureRemText( DrawCtx* dctx, const XP_Rect* r, 
                         XP_S16 nTilesLeft, 
                         XP_U16* width, XP_U16* height )
{
    DRAW_CBK_HEADER("measureRemText", "(Landroid/graphics/Rect;I[I[I)V" );
    
    jintArray widthArray = makeIntArray( env, 1, NULL );
    jintArray heightArray = makeIntArray( env, 1, NULL );
    jobject jrect = makeJRect( env, r );

    (*env)->CallVoidMethod( env, draw->j_draw, mid, jrect, nTilesLeft, widthArray,
                            heightArray );

    (*env)->DeleteLocalRef( env, jrect );  

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

    jobject jrect = makeJRect( env, r );
    jobject jdsi = makeDSI( env, dsi );

    jintArray widthArray = makeIntArray( env, 1, NULL );
    jintArray heightArray = makeIntArray( env, 1, NULL );

    (*env)->CallVoidMethod( env, draw->j_draw, mid, jrect, jdsi,
                            widthArray, heightArray );

    (*env)->DeleteLocalRef( env, jrect );
    (*env)->DeleteLocalRef( env, jdsi );

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

    jobject jrinner = makeJRect( env, rInner );
    jobject jrouter = makeJRect( env, rOuter );

    (*env)->CallVoidMethod( env, draw->j_draw, mid, jrinner, jrouter, nTilesLeft,
                            focussed );

    (*env)->DeleteLocalRef( env, jrinner );
    (*env)->DeleteLocalRef( env, jrouter );
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

    jobject jrinner = makeJRect( env, rInner );
    jobject jrouter = makeJRect( env, rOuter );
    jobject jdsi = makeDSI( env, dsi );

    (*env)->CallVoidMethod( env, draw->j_draw, mid, jrinner, jrouter, jdsi );

    (*env)->DeleteLocalRef( env, jrinner);
    (*env)->DeleteLocalRef( env, jrouter );
    (*env)->DeleteLocalRef( env, jdsi );
} /* and_draw_score_drawPlayer */

static XP_Bool
and_draw_boardBegin( DrawCtx* dctx, const XP_Rect* rect, DrawFocusState dfs )
{
    return XP_TRUE;
}

static XP_Bool 
and_draw_drawCell( DrawCtx* dctx, const XP_Rect* rect, const XP_UCHAR* text, 
                   const XP_Bitmaps* bitmaps, Tile tile, XP_S16 owner,
                   XWBonusType bonus, HintAtts hintAtts, CellFlags flags )
{
    DRAW_CBK_HEADER("drawCell", "(Landroid/graphics/Rect;Ljava/lang/String;"
                    "[Landroid/graphics/drawable/BitmapDrawable;IIIII)Z" );
    jobject jrect = makeJRect( env, rect );
    jstring jtext = NULL;
    if ( !!text ) {
        jtext = (*env)->NewStringUTF( env, text );
    }

    jobjectArray jbitmaps = !!bitmaps ? makeBitmapsArray( env, bitmaps ) : NULL;
    jboolean result = (*env)->CallBooleanMethod( env, draw->j_draw, mid, 
                                                 jrect, jtext, jbitmaps, tile,
                                                 owner, bonus, hintAtts, 
                                                 flags );
    (*env)->DeleteLocalRef( env, jrect );
    if ( !!jtext ) {
        (*env)->DeleteLocalRef( env, jtext );
    }
    if ( !!jbitmaps ) {
        (*env)->DeleteLocalRef( env, jbitmaps );
    }

    return result;
}

static void
and_draw_drawBoardArrow(DrawCtx* dctx, const XP_Rect* rect, XWBonusType bonus, 
                        XP_Bool vert, HintAtts hintAtts, CellFlags flags )
{
    DRAW_CBK_HEADER("drawBoardArrow", "(Landroid/graphics/Rect;IZII)V" );

    jobject jrect = makeJRect( env, rect );
    (*env)->CallVoidMethod( env, draw->j_draw, mid, 
                            jrect, bonus, vert, hintAtts, flags );
    (*env)->DeleteLocalRef( env, jrect );
}

static XP_Bool
and_draw_vertScrollBoard( DrawCtx* dctx, XP_Rect* rect, XP_S16 dist,
                          DrawFocusState dfs )
{
    DRAW_CBK_HEADER( "vertScrollBoard", "(Landroid/graphics/Rect;II)Z" );

    jobject jrect = makeJRect( env, rect );
    jboolean result = (*env)->CallBooleanMethod( env, draw->j_draw, mid, 
                                                 jrect, dist, dfs );
    copyJRect( env, rect, jrect );    
    (*env)->DeleteLocalRef( env, jrect );

    return result;
}

static XP_Bool
and_draw_trayBegin( DrawCtx* dctx, const XP_Rect* rect, XP_U16 owner, 
                    DrawFocusState dfs )
{
    DRAW_CBK_HEADER( "trayBegin", "(Landroid/graphics/Rect;II)Z" );

    jobject jrect = makeJRect( env, rect );

    jboolean result = (*env)->CallBooleanMethod( env, draw->j_draw, mid, 
                                                 jrect, owner, (jint)dfs );

    (*env)->DeleteLocalRef( env, jrect );

    return XP_TRUE;
}

static void
and_draw_drawTile( DrawCtx* dctx, const XP_Rect* rect, const XP_UCHAR* text, 
                   const XP_Bitmaps* bitmaps, XP_U16 val, CellFlags flags )
{
    DRAW_CBK_HEADER( "drawTile", "(Landroid/graphics/Rect;Ljava/lang/String;"
                     "[Landroid/graphics/drawable/BitmapDrawable;II)V" );
    jobject jrect = makeJRect( env, rect );
    jstring jtext = NULL;
    if ( !!text ) {
        jtext = (*env)->NewStringUTF( env, text );
    }

    jobjectArray jbitmaps = makeBitmapsArray( env, bitmaps );
    (*env)->CallVoidMethod( env, draw->j_draw, mid, 
                            jrect, jtext, jbitmaps, val, flags );

    (*env)->DeleteLocalRef( env, jrect );
    if ( !!jtext ) {
        (*env)->DeleteLocalRef( env, jtext );
    }
    if ( !!jbitmaps ) {
        (*env)->DeleteLocalRef( env, jbitmaps );
    }
}

static void
and_draw_drawTileMidDrag( DrawCtx* dctx, const XP_Rect* rect, 
                          const XP_UCHAR* text, const XP_Bitmaps* bitmaps,
                          XP_U16 val, XP_U16 owner, CellFlags flags )
{
    DRAW_CBK_HEADER( "drawTileMidDrag", "(Landroid/graphics/Rect;Ljava/lang/String;"
                     "[Landroid/graphics/drawable/BitmapDrawable;III)V" );

    jobject jrect = makeJRect( env, rect );
    jstring jtext = NULL;
    if ( !!text ) {
        jtext = (*env)->NewStringUTF( env, text );
    }

    jobjectArray jbitmaps = makeBitmapsArray( env, bitmaps );
    (*env)->CallVoidMethod( env, draw->j_draw, mid, 
                            jrect, jtext, jbitmaps, val, owner, flags );

    (*env)->DeleteLocalRef( env, jrect );
    if ( !!jtext ) {
        (*env)->DeleteLocalRef( env, jtext );
    }
    if ( !!jbitmaps ) {
        (*env)->DeleteLocalRef( env, jbitmaps );
    }
}

static void 
and_draw_drawTileBack( DrawCtx* dctx, const XP_Rect* rect, CellFlags flags )
{
    DRAW_CBK_HEADER( "drawTileBack", "(Landroid/graphics/Rect;I)V" );

    jobject jrect = makeJRect( env, rect );

    (*env)->CallVoidMethod( env, draw->j_draw, mid, 
                            jrect, flags );

    (*env)->DeleteLocalRef( env, jrect );
}

static void
and_draw_drawTrayDivider( DrawCtx* dctx, const XP_Rect* rect, CellFlags flags )
{
    DRAW_CBK_HEADER( "drawTrayDivider", "(Landroid/graphics/Rect;I)V" );

    jobject jrect = makeJRect( env, rect );

    (*env)->CallVoidMethod( env, draw->j_draw, mid, 
                            jrect, flags );

    (*env)->DeleteLocalRef( env, jrect );
}

static void
and_draw_score_pendingScore( DrawCtx* dctx, const XP_Rect* rect, 
                             XP_S16 score, XP_U16 playerNum,
                             CellFlags flags )
{
    DRAW_CBK_HEADER( "score_pendingScore", "(Landroid/graphics/Rect;III)V" );

    jobject jrect = makeJRect( env, rect );

    (*env)->CallVoidMethod( env, draw->j_draw, mid, 
                            jrect, score, playerNum, flags );

    (*env)->DeleteLocalRef( env, jrect );
}

static void
and_draw_objFinished( DrawCtx* dctx, BoardObjectType typ, 
                      const XP_Rect* rect, 
                      DrawFocusState dfs )
{
    DRAW_CBK_HEADER( "objFinished", "(ILandroid/graphics/Rect;I)V" );

    jobject jrect = makeJRect( env, rect );
    (*env)->CallVoidMethod( env, draw->j_draw, mid, 
                            (jint)typ, jrect, (jint)dfs );
    (*env)->DeleteLocalRef( env, jrect );
}

static void
and_draw_dictChanged( DrawCtx* dctx, const DictionaryCtxt* dict )
{
    LOG_FUNC();
}

static const XP_UCHAR* 
and_draw_getMiniWText( DrawCtx* dctx, XWMiniTextType textHint )
{
    LOG_FUNC();
    return "hi";
}

static void
and_draw_measureMiniWText( DrawCtx* dctx, const XP_UCHAR* textP, 
                           XP_U16* width, XP_U16* height )
{
    LOG_FUNC();
}

static void 
and_draw_drawMiniWindow( DrawCtx* dctx, const XP_UCHAR* text,
                         const XP_Rect* rect, void** closure )
{
    LOG_FUNC();
}

static XP_Bool
draw_doNothing( DrawCtx* dctx, ... )
{
    LOG_FUNC();
    return XP_FALSE;
} /* draw_doNothing */

DrawCtx* 
makeDraw( MPFORMAL JNIEnv** envp, jobject j_draw )
{
    AndDraw* draw = (AndDraw*)XP_CALLOC( mpool, sizeof(*draw) );
    JNIEnv* env = *envp;
    draw->vtable = XP_MALLOC( mpool, sizeof(*draw->vtable) );
    draw->j_draw = (*env)->NewGlobalRef( env, j_draw );
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
destroyDraw( DrawCtx* dctx )
{
    AndDraw* draw = (AndDraw*)dctx;
    JNIEnv* env = *draw->env;
    (*env)->DeleteGlobalRef( env, draw->j_draw );
    XP_FREE( draw->mpool, draw->vtable );
    XP_FREE( draw->mpool, draw );
}
