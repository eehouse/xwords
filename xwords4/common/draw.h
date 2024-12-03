/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2010 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _DRAW_H_
#define _DRAW_H_

#include "comtypes.h"
#include "xptypes.h"
#include "model.h"

/* typedef struct DrawCtx DrawCtx; */


typedef XP_Bool (*LastScoreCallback)(void* closure, XWEnv xwe,
                                     XP_S16 player, LastMoveInfo* lmi);

typedef enum {
    CELL_NONE = 0x00
    , CELL_ISBLANK = 0x01
    , CELL_RECENT = 0x02
    , CELL_ISSTAR = 0x04
    , CELL_ISCURSOR = 0x08
    , CELL_ISEMPTY = 0x10       /* of a tray tile slot */
    , CELL_VALHIDDEN = 0x20     /* show letter only, not value */
    , CELL_DRAGSRC = 0x40       /* where drag originated */
    , CELL_DRAGCUR = 0x80       /* where drag is now */
    , CELL_CROSSVERT = 0x100    /* vertical component of crosshair */
    , CELL_CROSSHOR = 0x200     /* horizontal component of crosshair */
    , CELL_PENDING = 0x400
    , CELL_ALL = 0x7FF
} CellFlags;

typedef struct _DrawScoreInfo {
    LastScoreCallback lsc;
    void* lscClosure;
    XP_UCHAR name[64];
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
#define HINT_BORDER_EDGE                                                \
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
 * both black-and-white and color screens, while on linux separate vtables are
 * created to allow a runtime choice between gtk and ncurses drawing.
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
    void DRAW_VTABLE_NAME(setClip)(DrawCtx* dctx, XWEnv xwe, XWEnv xwe, const XP_Rect* newClip,
                                   const XP_Rect* oldClip);
    void DRAW_VTABLE_NAME(frameRect)(DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect);
    void DRAW_VTABLE_NAME(invertRect)(DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect);
    void DRAW_VTABLE_NAME(drawString)(DrawCtx* dctx, XWEnv xwe, const XP_UCHAR* str,
                                      XP_U16 x, XP_U16 y);
    void DRAW_VTABLE_NAME(drawBitmap)(DrawCtx* dctx, XWEnv xwe, const XP_Bitmap bm,
                                      XP_U16 x, XP_U16 y);
    void DRAW_VTABLE_NAME(measureText)(DrawCtx* dctx, XWEnv xwe, const XP_UCHAR* buf,
                                       XP_U16* widthP, XP_U16* heightP);
#endif

    void DRAW_VTABLE_NAME(destroyCtxt) (DrawCtx* dctx, XWEnv xwe);

    void DRAW_VTABLE_NAME(dictChanged)(DrawCtx* dctx, XWEnv xwe, XP_S16 playerNum,
                                       const DictionaryCtxt* dict);

    XP_Bool DRAW_VTABLE_NAME(beginDraw) (DrawCtx* dctx, XWEnv xwe);
    void DRAW_VTABLE_NAME(endDraw) (DrawCtx* dctx, XWEnv xwe);

    XP_Bool DRAW_VTABLE_NAME(boardBegin) (DrawCtx* dctx, XWEnv xwe,
                                          const XP_Rect* rect, 
                                          XP_U16 hScale, XP_U16 vScale,
                                          DrawFocusState dfs, TileValueType tvType);
    void DRAW_VTABLE_NAME(objFinished)(DrawCtx* dctx, XWEnv xwe, BoardObjectType typ,
                                       const XP_Rect* rect, 
                                       DrawFocusState dfs);

    /* rect is not const: set by callee */
    XP_Bool DRAW_VTABLE_NAME(vertScrollBoard) (DrawCtx* dctx, XWEnv xwe, XP_Rect* rect,
                                               XP_S16 dist, DrawFocusState dfs);

    XP_Bool DRAW_VTABLE_NAME(trayBegin) (DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                                         XP_U16 owner, XP_S16 score,
                                         DrawFocusState dfs);

    XP_Bool DRAW_VTABLE_NAME(scoreBegin) (DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                                          XP_U16 numPlayers, 
                                          const XP_S16* const scores,
                                          XP_S16 remCount, DrawFocusState dfs);
#ifdef XWFEATURE_SCOREONEPASS
    XP_Bool DRAW_VTABLE_NAME(drawRemText) (DrawCtx* dctx, XWEnv xwe, XP_S16 nTilesLeft,
                                           XP_Bool focussed, XP_Rect* rect);
    void DRAW_VTABLE_NAME(score_drawPlayers)(DrawCtx* dctx, XWEnv xwe,
                                             const XP_Rect* scoreRect,
                                             XP_U16 nPlayers, 
                                             DrawScoreInfo playerData[],
                                             XP_Rect playerRects[]);
#else
    XP_Bool DRAW_VTABLE_NAME(measureRemText) (DrawCtx* dctx, XWEnv xwe, const XP_Rect* r,
                                              XP_S16 nTilesLeft,
                                              XP_U16* width, XP_U16* height);
    void DRAW_VTABLE_NAME(drawRemText) (DrawCtx* dctx, XWEnv xwe, const XP_Rect* rInner,
                                        const XP_Rect* rOuter,
                                        XP_S16 nTilesLeft, XP_Bool focussed);
    void DRAW_VTABLE_NAME(measureScoreText) (DrawCtx* dctx, XWEnv xwe,
                                             const XP_Rect* r, 
                                             const DrawScoreInfo* dsi,
                                             XP_U16* width, XP_U16* height);
    void DRAW_VTABLE_NAME(score_drawPlayer) (DrawCtx* dctx, XWEnv xwe,
                                             const XP_Rect* rInner, 
                                             const XP_Rect* rOuter, 
                                             XP_U16 gotPct, 
                                             const DrawScoreInfo* dsi);
#endif
    void DRAW_VTABLE_NAME(score_pendingScore) (DrawCtx* dctx, XWEnv xwe,
                                               const XP_Rect* rect,
                                               XP_S16 score,
                                               XP_U16 playerNum,
                                               XP_Bool curTurn,
                                               CellFlags flags);

    void DRAW_VTABLE_NAME(drawTimer) (DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                                      XP_U16 player, XP_S16 secondsLeft,
                                      XP_Bool turnDone);

    XP_Bool DRAW_VTABLE_NAME(drawCell) (DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                                        /* at least one of these two will be null */
                                        const XP_UCHAR* text, const XP_Bitmaps* bitmaps,
                                        Tile tile, XP_U16 value,
                                        XP_S16 owner, /* -1 means don't use */
                                        XWBonusType bonus, HintAtts hintAtts,
                                        CellFlags flags);

    void DRAW_VTABLE_NAME(invertCell) (DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect);

    XP_Bool DRAW_VTABLE_NAME(drawTile) (DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                                        /* at least 1 of these 2 will be
                                           null*/
                                        const XP_UCHAR* text,
                                        const XP_Bitmaps* bitmaps,
                                        XP_S16 val, CellFlags flags);
#ifdef POINTER_SUPPORT
    XP_Bool DRAW_VTABLE_NAME(drawTileMidDrag) (DrawCtx* dctx, XWEnv xwe,
                                               const XP_Rect* rect, 
                                               /* at least 1 of these 2 will
                                                  be null*/
                                               const XP_UCHAR* text, 
                                               const XP_Bitmaps* bitmaps,
                                               XP_U16 val, XP_U16 owner, 
                                               CellFlags flags);
#endif
    XP_Bool DRAW_VTABLE_NAME(drawTileBack) (DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                                            CellFlags flags);
    void DRAW_VTABLE_NAME(drawTrayDivider) (DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                                            CellFlags flags);

    void DRAW_VTABLE_NAME(clearRect) (DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect);

    void DRAW_VTABLE_NAME(drawBoardArrow) (DrawCtx* dctx, XWEnv xwe,
                                           const XP_Rect* rect, 
                                           XWBonusType bonus, XP_Bool vert,
                                           HintAtts hintAtts,
                                           CellFlags flags);
#ifdef XWFEATURE_MINIWIN
    const XP_UCHAR* DRAW_VTABLE_NAME(getMiniWText) (DrawCtx* dctx, XWEnv xwe,
                                                    XWMiniTextType textHint);
    void DRAW_VTABLE_NAME(measureMiniWText) (DrawCtx* dctx, XWEnv xwe, const XP_UCHAR* textP,
                                             XP_U16* width, XP_U16* height);
    void DRAW_VTABLE_NAME(drawMiniWindow)(DrawCtx* dctx, XWEnv xwe, const XP_UCHAR* text,
                                          const XP_Rect* rect, void** closure);
#endif
#ifndef DRAW_LINK_DIRECT
} DrawCtxVTable; /*  */
#endif

struct DrawCtx {
    DrawCtxVTable* vtable;
};

/* Franklin's compiler is too old to support __VA_ARGS__... */
# define DRAW_CALL(name, dc, ...) ((dc)->vtable->m_draw_##name)((dc), __VA_ARGS__)

#define draw_destroyCtxt(dc,...) DRAW_CALL(destroyCtxt, dc, __VA_ARGS__)
#define draw_dictChanged(dc, ...) DRAW_CALL(dictChanged, dc,__VA_ARGS__)
#define draw_beginDraw(dc,...) DRAW_CALL(beginDraw, dc, __VA_ARGS__)
#define draw_endDraw(dc, ...) DRAW_CALL(endDraw, dc, __VA_ARGS__)
#define draw_boardBegin(dc, ...) DRAW_CALL(boardBegin, dc, __VA_ARGS__)

#define draw_objFinished(dc, ...)      DRAW_CALL(objFinished, dc, __VA_ARGS__)
#define draw_trayBegin(dc, ...)       DRAW_CALL(trayBegin, dc, __VA_ARGS__)
#define draw_vertScrollBoard(dc, ...) DRAW_CALL(vertScrollBoard, (dc),__VA_ARGS__)
#define draw_scoreBegin(dc, ...)       DRAW_CALL(scoreBegin,(dc), __VA_ARGS__)
#ifdef XWFEATURE_SCOREONEPASS
# define draw_drawRemText(dc, ...)	DRAW_CALL(drawRemText, dc,__VA_ARGS__)
# define draw_score_drawPlayers(dc, ...)	DRAW_CALL(score_drawPlayers, dc,__VA_ARGS__)
#else
# define draw_measureRemText(dc, ...)           \
    DRAW_CALL(measureRemText, (dc),__VA_ARGS__)
# define draw_drawRemText(dc, ...)              \
    DRAW_CALL(drawRemText, (dc),__VA_ARGS__)
# define draw_measureScoreText(dc, ...)             \
    DRAW_CALL(measureScoreText,(dc),__VA_ARGS__)
# define draw_score_drawPlayer(dc, ...)             \
    DRAW_CALL(score_drawPlayer,(dc),__VA_ARGS__)
#endif
#define draw_score_pendingScore(dc, ...)            \
    DRAW_CALL(score_pendingScore,(dc),__VA_ARGS__)
#define draw_drawTimer(dc, ...)                 \
    DRAW_CALL(drawTimer,(dc),__VA_ARGS__)
#define draw_drawCell(dc, ...)    DRAW_CALL(drawCell,(dc),__VA_ARGS__)
#define draw_invertCell(dc, ...) DRAW_CALL(invertCell,(dc),__VA_ARGS__)
#define draw_drawTile(dc, ...)                  \
    DRAW_CALL(drawTile,(dc),__VA_ARGS__)
#ifdef POINTER_SUPPORT
#define draw_drawTileMidDrag(dc, ...)           \
    DRAW_CALL(drawTileMidDrag,(dc),__VA_ARGS__)
#endif  /* POINTER_SUPPORT */
#define draw_drawTileBack(dc, ...)              \
    DRAW_CALL(drawTileBack, (dc),__VA_ARGS__)
#define draw_drawTrayDivider(dc, ...)           \
    DRAW_CALL(drawTrayDivider,(dc),__VA_ARGS__)
#define draw_clearRect(dc, ...) DRAW_CALL(clearRect,(dc),__VA_ARGS__)
#define draw_drawBoardArrow(dc, ...)            \
    DRAW_CALL(drawBoardArrow,(dc),__VA_ARGS__)

#ifdef XWFEATURE_MINIWIN
# define draw_getMiniWText(dc, e, b) CALL_DRAW_NAME1(getMiniWText, (dc),(e),(b))
# define draw_measureMiniWText(dc, e, t, wp, hp)                \
    CALL_DRAW_NAME3(measureMiniWText, (dc),(e),(t), (wp), (hp))
# define draw_drawMiniWindow(dc, e, t, r, c)                    \
    CALL_DRAW_NAME3(drawMiniWindow, (dc),(e), (t), (r), (c))
#endif

#ifdef DRAW_WITH_PRIMITIVES
# define draw_setClip(dc, e, rn, ro) CALL_DRAW_NAME2(setClip, (dc),(e), (rn), (ro))
# define draw_frameRect(dc, e, r) CALL_DRAW_NAME1(frameRect, (dc),(e), (r))
# define draw_invertRect(dc, e, r) CALL_DRAW_NAME1(invertCell, (dc),(e), (r))
# define draw_drawString(dc, e, s, x, y)                    \
    CALL_DRAW_NAME3(drawString, (dc),(e), (s), (x), (y))
# define draw_drawBitmap(dc, e, bm, x, y)                   \
    CALL_DRAW_NAME3(drawBitmap, (dc),(e), (bm), (x), (y))
# define draw_measureText(dc, e, t, wp, hp)                 \
    CALL_DRAW_NAME3(measureText, (dc),(e), (t), (wp), (hp))

void InitDrawDefaults(DrawCtxVTable* vtable);
#endif /* DRAW_WITH_PRIMITIVES */

#endif
