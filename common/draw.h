/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2002 by Eric House (fixin@peak.org).  All rights reserved.
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

#ifndef _DRAW_H_
#define _DRAW_H_

#include "comtypes.h"
#include "xptypes.h"
#include "model.h"

/* typedef struct DrawCtx DrawCtx; */

typedef struct DrawScoreInfo {
    void* closure;
    XP_UCHAR* name;
    XP_S16 score;
    XP_S16 nTilesLeft;		   /* < 0 means don't use */
    XP_Bool isTurn;
    XP_Bool selected;
    XP_Bool isRemote;
    XP_Bool isRobot;
} DrawScoreInfo;

typedef struct DrawCtxVTable {

#ifdef DRAW_WITH_PRIMITIVES
    void (*m_draw_setClip)( DrawCtx* dctx, XP_Rect* newClip, 
                           XP_Rect* oldClip );
    void (*m_draw_frameRect)( DrawCtx* dctx, XP_Rect* rect );
    void (*m_draw_invertRect)( DrawCtx* dctx, XP_Rect* rect );
    void (*m_draw_drawString)( DrawCtx* dctx, XP_UCHAR* str, 
                               XP_U16 x, XP_U16 y );
    void (*m_draw_drawBitmap)( DrawCtx* dctx, XP_Bitmap bm, 
                               XP_U16 x, XP_U16 y );
    void (*m_draw_measureText)( DrawCtx* dctx, XP_UCHAR* buf, 
                                XP_U16* widthP, XP_U16* heightP );
#endif

    void (*m_draw_destroyCtxt)( DrawCtx* dctx );

    XP_Bool (*m_draw_boardBegin)( DrawCtx* dctx, XP_Rect* rect, 
                                  XP_Bool hasfocus );
    void (*m_draw_boardFinished)( DrawCtx* dctx );

    XP_Bool (*m_draw_vertScrollBoard)(DrawCtx* dctx, XP_Rect* rect, 
                                      XP_S16 dist );

    XP_Bool (*m_draw_trayBegin)( DrawCtx* dctx, XP_Rect* rect, 
                                 XP_U16 owner, XP_Bool hasfocus );
    void (*m_draw_trayFinished)( DrawCtx* dctx );

    void (*m_draw_measureRemText)( DrawCtx* dctx, XP_Rect* r, 
                                   XP_S16 nTilesLeft, 
                                   XP_U16* width, XP_U16* height );
    void (*m_draw_drawRemText)(DrawCtx* dctx, XP_Rect* rInner, 
                               XP_Rect* rOuter, XP_S16 nTilesLeft);

    void (*m_draw_scoreBegin)( DrawCtx* dctx, XP_Rect* rect, 
                               XP_U16 numPlayers, XP_Bool hasfocus );
    void (*m_draw_measureScoreText)( DrawCtx* dctx, XP_Rect* r, 
                                     DrawScoreInfo* dsi,
                                     XP_U16* width, XP_U16* height );
    void (*m_draw_score_drawPlayer)( DrawCtx* dctx, 
                                     XP_S16 playerNum, /* -1: don't use */
                                     XP_Rect* rInner, XP_Rect* rOuter, 
                                     DrawScoreInfo* dsi );

    void (*m_draw_score_pendingScore)( DrawCtx* dctx, XP_Rect* rect, 
                                       XP_S16 score, XP_U16 playerNum );

    void (*m_draw_scoreFinished)( DrawCtx* dctx );

    void (*m_draw_drawTimer)( DrawCtx* dctx, XP_Rect* rInner, XP_Rect* rOuter,
                              XP_U16 player, XP_S16 secondsLeft );

    XP_Bool (*m_draw_drawCell)( DrawCtx* dctx, XP_Rect* rect, 
                                /* at least one of these two will be null */
                               XP_UCHAR* text, XP_Bitmap bitmap,
                               XP_S16 owner, /* -1 means don't use */
                               XWBonusType bonus, XP_Bool isBlank, 
                               XP_Bool highlight, XP_Bool isStar);

    void (*m_draw_invertCell)( DrawCtx* dctx, XP_Rect* rect );

    void (*m_draw_drawTile)( DrawCtx* dctx, XP_Rect* rect, 
                             /* at least 1 of these two will be null*/
                             XP_UCHAR* text, XP_Bitmap bitmap,
                             XP_S16 val, XP_Bool highlighted );
    void (*m_draw_drawTileBack)( DrawCtx* dctx, XP_Rect* rect );
    void (*m_draw_drawTrayDivider)( DrawCtx* dctx, XP_Rect* rect, 
                                    XP_Bool selected );

    void (*m_draw_clearRect)( DrawCtx* dctx, XP_Rect* rect );

    void (*m_draw_drawBoardArrow)( DrawCtx* dctx, XP_Rect* rect, 
                                   XWBonusType bonus, XP_Bool vert );
#ifdef KEY_SUPPORT
    void (*m_draw_drawTrayCursor)( DrawCtx* dctx, XP_Rect* rect );
    void (*m_draw_drawBoardCursor)( DrawCtx* dctx, XP_Rect* rect );
#endif

    XP_UCHAR* (*m_draw_getMiniWText)( DrawCtx* dctx, 
                                      XWMiniTextType textHint );
    void (*m_draw_measureMiniWText)( DrawCtx* dctx, XP_UCHAR* textP, 
                                     XP_U16* width, XP_U16* height );
    void (*m_draw_drawMiniWindow)( DrawCtx* dctx, XP_UCHAR* text,
                                   XP_Rect* rect, void** closure );
    void (*m_draw_eraseMiniWindow)( DrawCtx* dctx, XP_Rect* rect,
                                    XP_Bool lastTime, void** closure,
                                    XP_Bool* invalUnder );

} DrawCtxVTable; /*  */

struct DrawCtx {
    DrawCtxVTable* vtable;
};

#define draw_destroyCtxt(dc) \
         (dc)->vtable->m_draw_destroyCtxt(dc)

#define draw_boardBegin( dc, r, f ) \
         (dc)->vtable->m_draw_boardBegin((dc), (r), (f))
#define draw_boardFinished( dc ) \
         (dc)->vtable->m_draw_boardFinished(dc)

#define draw_trayBegin( dc, r, o, f ) \
         (dc)->vtable->m_draw_trayBegin((dc), (r), (o), (f))
#define draw_trayFinished( dc ) \
         (dc)->vtable->m_draw_trayFinished(dc)

#define draw_vertScrollBoard( dc, r, d ) \
         (dc)->vtable->m_draw_vertScrollBoard((dc),(r),(d))

#define draw_scoreBegin( dc, r, t, f ) \
         (dc)->vtable->m_draw_scoreBegin((dc), (r), (t), (f))

#define draw_measureRemText( dc, r, n, wp, hp ) \
         (dc)->vtable->m_draw_measureRemText( (dc), (r), (n), (wp), (hp) )

#define draw_drawRemText( dc, ri, ro, n ) \
         (dc)->vtable->m_draw_drawRemText( (dc), (ri), (ro), (n) )

#define draw_measureScoreText(dc,r,dsi,wp,hp) \
         (dc)->vtable->m_draw_measureScoreText((dc),(r),(dsi),(wp),(hp))

#define draw_score_drawPlayer(dc, i, ri, ro, dsi) \
         (dc)->vtable->m_draw_score_drawPlayer((dc),(i),(ri),(ro),(dsi))

#define draw_score_pendingScore(dc, r, s, p ) \
         (dc)->vtable->m_draw_score_pendingScore((dc), (r), (s), (p))

#define draw_scoreFinished( dc ) \
         (dc)->vtable->m_draw_scoreFinished(dc)

#define draw_drawTimer( dc, ri, ro, plyr, sec ) \
         (dc)->vtable->m_draw_drawTimer((dc),(ri),(ro),(plyr),(sec))

/* #define draw_frameBoard( dc, rect ) \ */
/*          (dc)->vtable->m_draw_frameBoard((dc),(rect)) */
/* #define draw_frameTray( dc, rect ) (dc)->vtable->m_draw_frameTray((dc),(rect)) */

#define draw_drawCell( dc, rect, txt, bmap, o, bon, bl, h, s ) \
         (dc)->vtable->m_draw_drawCell((dc),(rect),(txt),(bmap),(o),(bon),\
            (bl),(h),(s))

#define draw_invertCell( dc, rect ) \
         (dc)->vtable->m_draw_invertCell((dc),(rect))

#define draw_drawTile( dc, rect, text, bmp, val, hil ) \
         (dc)->vtable->m_draw_drawTile((dc),(rect),(text),(bmp),(val),(hil))
#define draw_drawTileBack( dc, rect ) \
         (dc)->vtable->m_draw_drawTileBack( (dc), (rect) )
#define draw_drawTrayDivider( dc, rect, s ) \
         (dc)->vtable->m_draw_drawTrayDivider((dc),(rect), (s))

#define draw_clearRect( dc, rect ) (dc)->vtable->m_draw_clearRect((dc),(rect))

#define draw_drawBoardArrow( dc, r, b, v ) \
         (dc)->vtable->m_draw_drawBoardArrow((dc),(r),(b), (v))
#ifdef KEY_SUPPORT
# define draw_drawTrayCursor( dc, r ) \
         (dc)->vtable->m_draw_drawTrayCursor((dc),(r))
# define draw_drawBoardCursor( dc, r ) \
         (dc)->vtable->m_draw_drawBoardCursor((dc),(r))
#else
# define draw_drawTrayCursor( dc, r )
# define draw_drawBoardCursor( dc, r )
#endif

#define draw_getMiniWText( dc, b ) \
         (dc)->vtable->m_draw_getMiniWText( (dc),(b) )

#define draw_measureMiniWText( dc, t, wp, hp) \
         (dc)->vtable->m_draw_measureMiniWText( (dc),(t), (wp), (hp) )

#define  draw_drawMiniWindow( dc, t, r, c ) \
         (dc)->vtable->m_draw_drawMiniWindow( (dc), (t), (r), (c) )

#define  draw_eraseMiniWindow(dc, r, l, c, b) \
          (dc)->vtable->m_draw_eraseMiniWindow( (dc), (r), (l), (c), (b) )

#ifdef DRAW_WITH_PRIMITIVES
#define  draw_setClip( dc, rn, ro ) \
          (dc)->vtable->m_draw_setClip( (dc), (rn), (ro) )

#define    draw_frameRect( dc, r ) \
          (dc)->vtable->m_draw_frameRect( (dc), (r) )

#define    draw_invertRect( dc, r ) \
          (dc)->vtable->m_draw_invertCell( (dc), (r) )

#define    draw_drawString( dc, s, x, y ) \
           (dc)->vtable->m_draw_drawString( (dc), (s), (x), (y) )

#define    draw_drawBitmap( dc, bm, x, y ) \
           (dc)->vtable->m_draw_drawBitmap( (dc), (bm), (x), (y) )

#define    draw_measureText( dc, t, wp, hp ) \
           (dc)->vtable->m_draw_measureText( (dc), (t), (wp), (hp) )


void InitDrawDefaults( DrawCtxVTable* vtable );
#endif /* DRAW_WITH_PRIMITIVES */


#endif
