#include <SDL2/SDL.h>

#include "comtypes.h"
#include "wasmdraw.h"

#define COLOR_BACK 255, 255, 255
#define COLOR_BLACK 0, 0, 0

typedef struct _WasmDrawCtx {
    DrawCtxVTable* vtable;
    SDL_Renderer* renderer;
} WasmDrawCtx;

static int sBonusColors[4][3] = {
    {0x00, 0xFF, 0x80},
    {0x00, 0x80, 0xFF},
    {0x80, 0x00, 0xFF},
    {0xFF, 0x80, 0x00},
};

static void
clearRect( WasmDrawCtx* wdctx, const XP_Rect* rect )
{
    SDL_Rect sdl_rect = { .x = rect->left,
                          .y = rect->top,
                          .w = rect->width,
                          .h = rect->height,
    };
    SDL_SetRenderDrawColor( wdctx->renderer, COLOR_BACK, 255 );
    SDL_RenderFillRect( wdctx->renderer, &sdl_rect );
}

static void
fillRect( WasmDrawCtx* wdctx, const XP_Rect* rect, int colorParts[] )
{
    SDL_Rect sdl_rect = { .x = rect->left,
                          .y = rect->top,
                          .w = rect->width,
                          .h = rect->height,
    };
    SDL_SetRenderDrawColor( wdctx->renderer, colorParts[0], colorParts[1],
                            colorParts[2], 255 );
    SDL_RenderFillRect( wdctx->renderer, &sdl_rect );
}

static void
frameRect( WasmDrawCtx* wdctx, const XP_Rect* rect )
{
    SDL_Rect sdl_rect = { .x = rect->left,
                          .y = rect->top,
                          .w = rect->width,
                          .h = rect->height,
    };
    SDL_SetRenderDrawColor( wdctx->renderer, COLOR_BLACK, 255 );
    SDL_RenderDrawRect( wdctx->renderer, &sdl_rect );
}

static void
wasm_draw_dictChanged( DrawCtx* dctx, XWEnv xwe, XP_S16 playerNum,
                       const DictionaryCtxt* dict )
{
    LOG_FUNC();
}

static XP_Bool
wasm_draw_beginDraw( DrawCtx* dctx, XWEnv xwe )
{
    LOG_FUNC();
    return XP_TRUE;
}

static void
wasm_draw_destroyCtxt( DrawCtx* dctx, XWEnv xwe )
{
    LOG_FUNC();
}

static void
wasm_draw_endDraw( DrawCtx* dctx, XWEnv xwe )
{
    LOG_FUNC();
}

static XP_Bool
wasm_draw_boardBegin( DrawCtx* dctx, XWEnv xwe,
                      const XP_Rect* rect, 
                      XP_U16 hScale, XP_U16 vScale,
                      DrawFocusState dfs )
{
    LOG_FUNC();
    /* WasmDrawCtx* wdctx = (WasmDrawCtx*)dctx; */
    /* SDL_SetRenderDrawColor( wdctx->renderer, 255, 0, 0, 0 ); */
    return XP_TRUE;
}

static void
wasm_draw_objFinished( DrawCtx* dctx, XWEnv xwe, BoardObjectType typ,
                       const XP_Rect* rect, 
                       DrawFocusState dfs ){ LOG_FUNC(); }

static XP_Bool
wasm_draw_vertScrollBoard(DrawCtx* dctx, XWEnv xwe, XP_Rect* rect,
                          XP_S16 dist, DrawFocusState dfs )
{
    LOG_FUNC();
    return XP_FALSE;
}

static XP_Bool
wasm_draw_trayBegin( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                     XP_U16 owner, XP_S16 score,
                     DrawFocusState dfs )
{
    LOG_FUNC();
    return XP_TRUE;
}

static XP_Bool
wasm_draw_scoreBegin( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                      XP_U16 numPlayers, 
                      const XP_S16* const scores,
                      XP_S16 remCount, DrawFocusState dfs )
{
    LOG_FUNC();
    return XP_TRUE;
}

static XP_Bool
wasm_draw_measureRemText( DrawCtx* dctx, XWEnv xwe, const XP_Rect* r,
                          XP_S16 nTilesLeft,
                          XP_U16* width, XP_U16* height )
{
    LOG_FUNC();
    return XP_FALSE;
}

static void
wasm_draw_drawRemText( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rInner,
                       const XP_Rect* rOuter,
                       XP_S16 nTilesLeft, XP_Bool focussed )
{
    LOG_FUNC();
}

static void
wasm_draw_measureScoreText( DrawCtx* dctx, XWEnv xwe,
                            const XP_Rect* r, 
                            const DrawScoreInfo* dsi,
                            XP_U16* width, XP_U16* height ){ LOG_FUNC(); }
static void
wasm_draw_score_drawPlayer( DrawCtx* dctx, XWEnv xwe,
                            const XP_Rect* rInner, 
                            const XP_Rect* rOuter, 
                            XP_U16 gotPct, 
                            const DrawScoreInfo* dsi ){ LOG_FUNC(); }
static void
wasm_draw_score_pendingScore( DrawCtx* dctx, XWEnv xwe,
                              const XP_Rect* rect,
                              XP_S16 score,
                              XP_U16 playerNum,
                              XP_Bool curTurn,
                              CellFlags flags ){ LOG_FUNC(); }

static void
wasm_draw_drawTimer( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                     XP_U16 player, XP_S16 secondsLeft,
                     XP_Bool turnDone ){ LOG_FUNC(); }

static XP_Bool
wasm_draw_drawCell( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                    /* at least one of these two will be
                       null */
                    const XP_UCHAR* text,
                    const XP_Bitmaps* bitmaps,
                    Tile tile, XP_U16 value,
                    XP_S16 owner, /* -1 means don't use */
                    XWBonusType bonus, HintAtts hintAtts,
                    CellFlags flags )
{
    WasmDrawCtx* wdctx = (WasmDrawCtx*)dctx;
    if ( BONUS_NONE == bonus ) {
        clearRect( wdctx, rect );
    } else {
        fillRect( wdctx, rect, sBonusColors[bonus-1] );
    }
    frameRect( wdctx, rect );

    return XP_TRUE;
}

static void
wasm_draw_invertCell( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect )
{
    LOG_FUNC();
}

static XP_Bool
wasm_draw_drawTile( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                    /* at least 1 of these 2 will be
                       null*/
                    const XP_UCHAR* text,
                    const XP_Bitmaps* bitmaps,
                    XP_U16 val, CellFlags flags )
{
    XP_LOGFF( "(text=%s)", text );
    WasmDrawCtx* wdctx = (WasmDrawCtx*)dctx;
    clearRect( wdctx, rect );
    frameRect( wdctx, rect );
    return XP_TRUE;
}

#ifdef POINTER_SUPPORT
static XP_Bool
wasm_draw_drawTileMidDrag( DrawCtx* dctx, XWEnv xwe,
                           const XP_Rect* rect, 
                           /* at least 1 of these 2 will
                              be null*/
                           const XP_UCHAR* text, 
                           const XP_Bitmaps* bitmaps,
                           XP_U16 val, XP_U16 owner, 
                           CellFlags flags )
{
    LOG_FUNC();
    return XP_TRUE;
}
#endif

static XP_Bool
wasm_draw_drawTileBack( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                        CellFlags flags )
{
    LOG_FUNC();
    return XP_TRUE;
}

static void
wasm_draw_drawTrayDivider( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                           CellFlags flags )
{
    LOG_FUNC();
}

static void
wasm_draw_clearRect( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect )
{
    WasmDrawCtx* wdctx = (WasmDrawCtx*)dctx;
    clearRect( wdctx, rect );
}

static void
wasm_draw_drawBoardArrow ( DrawCtx* dctx, XWEnv xwe,
                           const XP_Rect* rect, 
                           XWBonusType bonus, XP_Bool vert,
                           HintAtts hintAtts, CellFlags flags)
{
    LOG_FUNC();
}

DrawCtx*
wasm_draw_make( MPFORMAL SDL_Renderer* renderer )
{
    WasmDrawCtx* dctx = XP_MALLOC( mpool, sizeof(*dctx) );
    dctx->renderer = renderer;

    dctx->vtable = XP_MALLOC( mpool, sizeof(*dctx->vtable) );

    SET_VTABLE_ENTRY( dctx->vtable, draw_clearRect, wasm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_dictChanged, wasm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_beginDraw, wasm );

        SET_VTABLE_ENTRY( dctx->vtable, draw_clearRect, wasm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_dictChanged, wasm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_beginDraw, wasm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_destroyCtxt, wasm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_endDraw, wasm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_boardBegin, wasm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_objFinished, wasm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_vertScrollBoard, wasm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_trayBegin, wasm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_scoreBegin, wasm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureRemText, wasm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawRemText, wasm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureScoreText, wasm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_score_drawPlayer, wasm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_score_pendingScore, wasm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTimer, wasm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawCell, wasm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_invertCell, wasm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTile, wasm );
#ifdef POINTER_SUPPORT
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTileMidDrag, wasm );
#endif
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTileBack, wasm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTrayDivider, wasm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawBoardArrow, wasm );

    return (DrawCtx*)dctx;
}
