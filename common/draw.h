/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2007 by Eric House (xwords@eehouse.org).  All rights reserved.
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


typedef XP_Bool (*LastScoreCallback)( void* closure, XP_S16 player,
                                      XP_UCHAR* expl, XP_U16* explLen );

typedef enum {
    CELL_NONE = 0x00
    , CELL_ISBLANK = 0x01
    , CELL_HIGHLIGHT = 0x02
    , CELL_ISSTAR = 0x04
    , CELL_ISCURSOR = 0x08
    , CELL_ISEMPTY = 0x10       /* of a tray tile slot */
    , CELL_VALHIDDEN = 0x20     /* show letter only, not value */
    , CELL_DRAGSRC = 0x40       /* where drag originated */
    , CELL_DRAGCUR = 0x80       /* where drag is now */
    , CELL_ALL = 0xFF
} CellFlags;

typedef struct DrawScoreInfo {
    LastScoreCallback lsc;
    void* lscClosure;
    const XP_UCHAR* name;
    XP_U16 playerNum;
    XP_S16 totalScore;
    XP_S16 nTilesLeft;   /* < 0 means don't use */
    CellFlags flags;
    XP_Bool isTurn;
    XP_Bool selected;
    XP_Bool isRemote;
    XP_Bool isRobot;
} DrawScoreInfo;

enum HINT_ATTS { HINT_BORDER_NONE = 0,
                 HINT_BORDER_LEFT = 1,
                 HINT_BORDER_RIGHT = 2,
                 HINT_BORDER_TOP = 4,
                 HINT_BORDER_BOTTOM = 8,
                 HINT_BORDER_CENTER = 0x10
};
typedef XP_UCHAR HintAtts;
#define HINT_BORDER_EDGE \
    (HINT_BORDER_LEFT|HINT_BORDER_RIGHT|HINT_BORDER_TOP|HINT_BORDER_BOTTOM)


/* Platform-supplied draw functions are either staticly linked, or called via
 * a vtable.  If you want static linking, define DRAW_LINK_DIRECT via a -D
 * option to your compiler, and use the DRAW_FUNC_NAME macro to make your
 * names match what's declared here.  Otherwise, if DRAW_LINK_DIRECT is not
 * defined, you need to create and populate a vtable.  See any of the existing
 * platforms' draw implementations for examples.
 *
 * As to how to choose, static linking makes the binary a tiny bit smaller,
 * but vtables give more flexibilty.  For example, Palm uses them to support
 * both black-and-white and color screens, while linux on linux separate
 * vtable are created to allow a runtime choice between gtk and ncurses
 * drawing.
 */
#ifdef DRAW_LINK_DIRECT
# define DRAW_FUNC_NAME(name)   linked##_draw_##name
# define DRAW_VTABLE_NAME DRAW_FUNC_NAME
#else
# define DRAW_VTABLE_NAME(name) (*m_draw_ ## name) 
#endif

#ifdef DRAW_LINK_DIRECT
typedef void DrawCtxVTable;
#else
typedef struct DrawCtxVTable {
#endif

#ifdef DRAW_WITH_PRIMITIVES
    void DRAW_VTABLE_NAME(setClip)( DrawCtx* dctx, const XP_Rect* newClip, 
                                    const XP_Rect* oldClip );
    void DRAW_VTABLE_NAME(frameRect)( DrawCtx* dctx, const XP_Rect* rect );
    void DRAW_VTABLE_NAME(invertRect)( DrawCtx* dctx, const XP_Rect* rect );
    void DRAW_VTABLE_NAME(drawString)( DrawCtx* dctx, const XP_UCHAR* str, 
                                       XP_U16 x, XP_U16 y );
    void DRAW_VTABLE_NAME(drawBitmap)( DrawCtx* dctx, const XP_Bitmap bm, 
                                       XP_U16 x, XP_U16 y );
    void DRAW_VTABLE_NAME(measureText)( DrawCtx* dctx, const XP_UCHAR* buf, 
                                        XP_U16* widthP, XP_U16* heightP );
#endif

    void DRAW_VTABLE_NAME(destroyCtxt) ( DrawCtx* dctx );

    void DRAW_VTABLE_NAME(dictChanged)( DrawCtx* dctx,
                                        const DictionaryCtxt* dict );

    XP_Bool DRAW_VTABLE_NAME(boardBegin) ( DrawCtx* dctx, 
                                           const XP_Rect* rect, 
                                           DrawFocusState dfs );
    void DRAW_VTABLE_NAME(objFinished)( DrawCtx* dctx, BoardObjectType typ, 
                                        const XP_Rect* rect, 
                                        DrawFocusState dfs );

    /* rect is not const: set by callee */
    XP_Bool DRAW_VTABLE_NAME(vertScrollBoard) (DrawCtx* dctx, XP_Rect* rect, 
                                               XP_S16 dist, DrawFocusState dfs );

    XP_Bool DRAW_VTABLE_NAME(trayBegin) ( DrawCtx* dctx, const XP_Rect* rect, 
                                          XP_U16 owner, 
                                          DrawFocusState dfs );
    void DRAW_VTABLE_NAME(measureRemText) ( DrawCtx* dctx, const XP_Rect* r, 
                                            XP_S16 nTilesLeft, 
                                            XP_U16* width, XP_U16* height );
    void DRAW_VTABLE_NAME(drawRemText) (DrawCtx* dctx, const XP_Rect* rInner,
                                        const XP_Rect* rOuter, 
                                        XP_S16 nTilesLeft, XP_Bool focussed );

    void DRAW_VTABLE_NAME(scoreBegin) ( DrawCtx* dctx, const XP_Rect* rect, 
                                        XP_U16 numPlayers, 
                                        const XP_S16* const scores,
                                        XP_S16 remCount, DrawFocusState dfs );
    void DRAW_VTABLE_NAME(measureScoreText) ( DrawCtx* dctx, 
                                              const XP_Rect* r, 
                                              const DrawScoreInfo* dsi,
                                              XP_U16* width, XP_U16* height );
    void DRAW_VTABLE_NAME(score_drawPlayer) ( DrawCtx* dctx,
                                              const XP_Rect* rInner, 
                                              const XP_Rect* rOuter, 
                                              const DrawScoreInfo* dsi );

    void DRAW_VTABLE_NAME(score_pendingScore) ( DrawCtx* dctx, 
                                                const XP_Rect* rect, 
                                                XP_S16 score, 
                                                XP_U16 playerNum,
                                                CellFlags flags );

    void DRAW_VTABLE_NAME(drawTimer) ( DrawCtx* dctx, const XP_Rect* rect, 
                                       XP_U16 player, XP_S16 secondsLeft );

    XP_Bool DRAW_VTABLE_NAME(drawCell) ( DrawCtx* dctx, const XP_Rect* rect, 
                                         /* at least one of these two will be
                                            null */
                                         const XP_UCHAR* text, 
                                         const XP_Bitmaps* bitmaps,
                                         Tile tile,
                                         XP_S16 owner, /* -1 means don't use */
                                         XWBonusType bonus, HintAtts hintAtts,
                                         CellFlags flags );

    void DRAW_VTABLE_NAME(invertCell) ( DrawCtx* dctx, const XP_Rect* rect );

    void DRAW_VTABLE_NAME(drawTile) ( DrawCtx* dctx, const XP_Rect* rect, 
                                      /* at least 1 of these two will be null*/
                                      const XP_UCHAR* text, 
                                      const XP_Bitmaps* bitmaps,
                                      XP_U16 val, CellFlags flags );
#ifdef POINTER_SUPPORT
    void DRAW_VTABLE_NAME(drawTileMidDrag) ( DrawCtx* dctx, const XP_Rect* rect, 
                                      /* at least 1 of these two will be null*/
                                             const XP_UCHAR* text, 
                                             const XP_Bitmaps* bitmaps,
                                             XP_U16 val, XP_U16 owner, 
                                             CellFlags flags );
#endif
    void DRAW_VTABLE_NAME(drawTileBack) ( DrawCtx* dctx, const XP_Rect* rect,
                                          CellFlags flags );
    void DRAW_VTABLE_NAME(drawTrayDivider) ( DrawCtx* dctx, const XP_Rect* rect, 
                                             CellFlags flags );

    void DRAW_VTABLE_NAME(clearRect) ( DrawCtx* dctx, const XP_Rect* rect );

    void DRAW_VTABLE_NAME(drawBoardArrow) ( DrawCtx* dctx, 
                                            const XP_Rect* rect, 
                                            XWBonusType bonus, XP_Bool vert,
                                            HintAtts hintAtts,
                                            CellFlags flags);
    const XP_UCHAR* DRAW_VTABLE_NAME(getMiniWText) ( DrawCtx* dctx, 
                                                     XWMiniTextType textHint );
    void DRAW_VTABLE_NAME(measureMiniWText) ( DrawCtx* dctx, const XP_UCHAR* textP, 
                                              XP_U16* width, XP_U16* height );
    void DRAW_VTABLE_NAME(drawMiniWindow)( DrawCtx* dctx, const XP_UCHAR* text,
                                           const XP_Rect* rect, void** closure );
#ifndef DRAW_LINK_DIRECT
} DrawCtxVTable; /*  */
#endif

struct DrawCtx {
    DrawCtxVTable* vtable;
};

/* Franklin's compiler is too old to support __VA_ARGS__... */
#ifdef DRAW_LINK_DIRECT
# define CALL_DRAW_NAME0(name,dc) linked##_draw_##name(dc)
# define CALL_DRAW_NAME1(name,dc,p1)    linked##_draw_##name(dc,(p1))
# define CALL_DRAW_NAME2(name,dc,p1,p2) \
   linked##_draw_##name(dc,(p1),(p2))
# define CALL_DRAW_NAME3(name,dc,p1,p2,p3) \
   linked##_draw_##name(dc,(p1),(p2),(p3))
# define CALL_DRAW_NAME4(name,dc,p1,p2,p3,p4) \
   linked##_draw_##name(dc,(p1),(p2),(p3),(p4))
# define CALL_DRAW_NAME5(name,dc,p1,p2,p3,p4,p5) \
   linked##_draw_##name(dc,(p1),(p2),(p3),(p4),(p5))
# define CALL_DRAW_NAME6(name,dc,p1,p2,p3,p4,p5,p6) \
   linked##_draw_##name(dc,(p1),(p2),(p3),(p4),(p5),(p6))
# define CALL_DRAW_NAME8(name,dc,p1,p2,p3,p4,p5,p6,p7,p8) \
   linked##_draw_##name(dc,(p1),(p2),(p3),(p4),(p5),(p6),(p7),(p8))
# define CALL_DRAW_NAME10(name,dc,p1,p2,p3,p4,p5,p6,p7,p8,p9,p10) \
   linked##_draw_##name(dc,(p1),(p2),(p3),(p4),(p5),(p6),(p7),\
   (p8),(p9),(p10))
#else
# define CALL_DRAW_NAME0(name,dc)    ((dc)->vtable->m_draw_##name)(dc)
# define CALL_DRAW_NAME1(name,dc,p1)    ((dc)->vtable->m_draw_##name)(dc,(p1))
# define CALL_DRAW_NAME2(name,dc,p1,p2) \
   ((dc)->vtable->m_draw_##name)(dc,(p1),(p2))
# define CALL_DRAW_NAME3(name,dc,p1,p2,p3) \
   ((dc)->vtable->m_draw_##name)(dc,(p1),(p2),(p3))
# define CALL_DRAW_NAME4(name,dc,p1,p2,p3,p4) \
   ((dc)->vtable->m_draw_##name)(dc,(p1),(p2),(p3),(p4))
# define CALL_DRAW_NAME5(name,dc,p1,p2,p3,p4,p5) \
   ((dc)->vtable->m_draw_##name)(dc,(p1),(p2),(p3),(p4),(p5))
# define CALL_DRAW_NAME6(name,dc,p1,p2,p3,p4,p5,p6) \
   ((dc)->vtable->m_draw_##name)(dc,(p1),(p2),(p3),(p4),(p5),(p6))
# define CALL_DRAW_NAME8(name,dc,p1,p2,p3,p4,p5,p6,p7,p8) \
   ((dc)->vtable->m_draw_##name)(dc,(p1),(p2),(p3),(p4),(p5),(p6),(p7),\
   (p8))
# define CALL_DRAW_NAME10(name,dc,p1,p2,p3,p4,p5,p6,p7,p8,p9,p10) \
   ((dc)->vtable->m_draw_##name)(dc,(p1),(p2),(p3),(p4),(p5),(p6),(p7),\
   (p8),(p9),(p10))
#endif

#define draw_destroyCtxt(dc) CALL_DRAW_NAME0(destroyCtxt, dc)
#define draw_dictChanged( dc, d ) CALL_DRAW_NAME1(dictChanged, (dc), (d))
#define draw_boardBegin( dc,r,f ) CALL_DRAW_NAME2(boardBegin, dc, r,f)
#define draw_objFinished( dc, t, r, d ) CALL_DRAW_NAME3(objFinished, (dc), (t), (r), (d))
#define draw_trayBegin( dc, r, o, f ) CALL_DRAW_NAME3(trayBegin,dc, r, o, f)
#define draw_vertScrollBoard( dc, r, d, f ) \
    CALL_DRAW_NAME3(vertScrollBoard, (dc),(r),(d),(f))
#define draw_scoreBegin( dc, r, t, s, c, f ) \
    CALL_DRAW_NAME5( scoreBegin,(dc), (r), (t), (s), (c), (f))
#define draw_measureRemText( dc, r, n, wp, hp ) \
    CALL_DRAW_NAME4(measureRemText, (dc), (r), (n), (wp), (hp) )
#define draw_drawRemText( dc, ri, ro, n, f )                  \
    CALL_DRAW_NAME4(drawRemText, (dc), (ri), (ro), (n), (f) )
#define draw_measureScoreText(dc,r,dsi,wp,hp) \
    CALL_DRAW_NAME4(measureScoreText,(dc),(r),(dsi),(wp),(hp))
#define draw_score_drawPlayer(dc, ri, ro, dsi) \
    CALL_DRAW_NAME3(score_drawPlayer,(dc),(ri),(ro),(dsi))
#define draw_score_pendingScore(dc, r, s, p, f ) \
    CALL_DRAW_NAME4(score_pendingScore,(dc), (r), (s), (p), (f))
#define draw_drawTimer( dc, r, plyr, sec ) \
    CALL_DRAW_NAME3(drawTimer,(dc),(r),(plyr),(sec))
#define draw_drawCell( dc, rect, txt, bmap, t, o, bon, hi, f ) \
    CALL_DRAW_NAME8(drawCell,(dc),(rect),(txt),(bmap),(t),(o),(bon),(hi),\
    (f))
#define draw_invertCell( dc, rect ) CALL_DRAW_NAME1(invertCell,(dc),(rect))
#define draw_drawTile( dc, rect, text, bmp, val, hil ) \
    CALL_DRAW_NAME5(drawTile,(dc),(rect),(text),(bmp),(val),(hil))
#ifdef POINTER_SUPPORT
#define draw_drawTileMidDrag( dc, rect, text, bmp, val, ownr, hil )      \
    CALL_DRAW_NAME6(drawTileMidDrag,(dc),(rect),(text),(bmp),(val),(ownr),(hil))
#endif  /* POINTER_SUPPORT */
#define draw_drawTileBack( dc, rect, f ) \
    CALL_DRAW_NAME2(drawTileBack, (dc), (rect), (f) )
#define draw_drawTrayDivider( dc, rect, s ) \
    CALL_DRAW_NAME2(drawTrayDivider,(dc),(rect), (s))
#define draw_clearRect( dc, rect ) CALL_DRAW_NAME1(clearRect,(dc),(rect))
#define draw_drawBoardArrow( dc, r, b, v, h, f ) \
    CALL_DRAW_NAME5(drawBoardArrow,(dc),(r),(b), (v), (h), (f))

#define draw_getMiniWText( dc, b ) CALL_DRAW_NAME1(getMiniWText, (dc),(b) )
#define draw_measureMiniWText( dc, t, wp, hp) \
    CALL_DRAW_NAME3(measureMiniWText, (dc),(t), (wp), (hp) )
#define draw_drawMiniWindow( dc, t, r, c ) \
    CALL_DRAW_NAME3(drawMiniWindow, (dc), (t), (r), (c) )

#ifdef DRAW_WITH_PRIMITIVES
# define draw_setClip( dc, rn, ro ) CALL_DRAW_NAME2(setClip, (dc), (rn), (ro))
# define draw_frameRect( dc, r ) CALL_DRAW_NAME1(frameRect, (dc), (r) )
# define draw_invertRect( dc, r ) CALL_DRAW_NAME1(invertCell, (dc), (r) )
# define draw_drawString( dc, s, x, y) \
    CALL_DRAW_NAME3(drawString, (dc), (s), (x), (y) )
# define draw_drawBitmap( dc, bm, x, y ) \
    CALL_DRAW_NAME3(drawBitmap, (dc), (bm), (x), (y) )
# define draw_measureText( dc, t, wp, hp ) \
    CALL_DRAW_NAME3(measureText, (dc), (t), (wp), (hp) )

void InitDrawDefaults( DrawCtxVTable* vtable );
#endif /* DRAW_WITH_PRIMITIVES */


#endif
