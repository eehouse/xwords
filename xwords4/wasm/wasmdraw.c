#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "comtypes.h"
#include "wasmdraw.h"

#define COLOR_BACK 255, 255, 255
#define COLOR_BLACK 0, 0, 0

typedef struct _WasmDrawCtx {
    DrawCtxVTable* vtable;
    SDL_Renderer* renderer;
    SDL_Surface* surface;
    SDL_Texture* texture;
    TTF_Font* font12;
    TTF_Font* font20;
    TTF_Font* font36;
    TTF_Font* font48;
} WasmDrawCtx;

static int sBonusColors[4][3] = {
    {0x00, 0xFF, 0x80},
    {0x00, 0x80, 0xFF},
    {0x80, 0x00, 0xFF},
    {0xFF, 0x80, 0x00},
};

static void
rectXPToSDL( SDL_Rect* sdlr, const XP_Rect* rect )
{
    sdlr->x = rect->left;
    sdlr->y = rect->top;
    sdlr->w = rect->width + 1;
    sdlr->h = rect->height + 1;
}

static void
clearRect( WasmDrawCtx* wdctx, const XP_Rect* rect )
{
    SDL_Rect sdlr;
    rectXPToSDL( &sdlr, rect );
    SDL_SetRenderDrawColor( wdctx->renderer, COLOR_BACK, 255 );
    SDL_RenderFillRect( wdctx->renderer, &sdlr );
}

static void
fillRect( WasmDrawCtx* wdctx, const XP_Rect* rect, int colorParts[] )
{
    SDL_Rect sdlr;
    rectXPToSDL( &sdlr, rect );
    SDL_SetRenderDrawColor( wdctx->renderer, colorParts[0], colorParts[1],
                            colorParts[2], 255 );
    SDL_RenderFillRect( wdctx->renderer, &sdlr );
}

static void
frameRect( WasmDrawCtx* wdctx, const XP_Rect* rect )
{
    SDL_Rect sdlr;
    rectXPToSDL( &sdlr, rect );
    SDL_SetRenderDrawColor( wdctx->renderer, COLOR_BLACK, 255 );
    SDL_RenderDrawRect( wdctx->renderer, &sdlr );
}

static void
textInRect( WasmDrawCtx* wdctx, const XP_UCHAR* text, const XP_Rect* rect )
{
    TTF_Font* font;
    if ( rect->height <= 12 ) {
        font = wdctx->font12;
    } else if ( rect->height <= 20 ) {
        font = wdctx->font20;
    } else if ( rect->height <= 36 ) {
        font = wdctx->font36;
    } else {
        XP_LOGFF( "unexpected height: %d", rect->height );
        font = wdctx->font48;
    }

    SDL_Color color = { 0, 0, 0, 255 };
    SDL_Surface* surface = TTF_RenderText_Blended( font, text, color );
    SDL_Texture* texture = SDL_CreateTextureFromSurface( wdctx->renderer, surface );
    SDL_FreeSurface( surface );

    SDL_Rect sdlr;
    rectXPToSDL( &sdlr, rect );
    // SDL_QueryTexture( texture, NULL, NULL, &sdlr.w, &sdlr.h );
    SDL_RenderCopy( wdctx->renderer, texture, NULL, &sdlr );
    SDL_DestroyTexture( texture );
}

static void
drawTile( WasmDrawCtx* wdctx, const XP_UCHAR* face, XP_U16 val, const XP_Rect* rect )
{
    clearRect( wdctx, rect );
    frameRect( wdctx, rect );
    textInRect( wdctx, face, rect );
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
    WasmDrawCtx* wdctx = (WasmDrawCtx*)dctx;
    SDL_RenderPresent( wdctx->renderer );
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
                              XP_S16 score, XP_U16 playerNum,
                              XP_Bool curTurn, CellFlags flags )
{
    WasmDrawCtx* wdctx = (WasmDrawCtx*)dctx;
    clearRect( wdctx, rect );

    XP_Rect tmp = *rect;
    tmp.height /= 2;
    textInRect( wdctx, "Pts:", &tmp );
    tmp.top += tmp.height;
    XP_UCHAR buf[16];
    if ( score >= 0 ) {
        XP_SNPRINTF( buf, VSIZE(buf), "%.3d", score );
    } else {
        XP_STRNCPY( buf, "???", VSIZE(buf)  );
    }
    textInRect( wdctx, buf, &tmp );
}

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
    clearRect( wdctx, rect );

    if ( (flags & (CELL_DRAGSRC|CELL_ISEMPTY)) != 0 ) {
        if ( BONUS_NONE == bonus ) {
            clearRect( wdctx, rect );
        } else {
            fillRect( wdctx, rect, sBonusColors[bonus-1] );
        }
    } else if ( !!text ) {
        textInRect( wdctx, text, rect );
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
    drawTile( wdctx, text, val, rect );
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
    WasmDrawCtx* wdctx = (WasmDrawCtx*)dctx;
    drawTile( wdctx, text, val, rect );
    return XP_TRUE;
}
#endif

static XP_Bool
wasm_draw_drawTileBack( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                        CellFlags flags )
{
    LOG_FUNC();
    WasmDrawCtx* wdctx = (WasmDrawCtx*)dctx;
    drawTile( wdctx, "?", -1, rect );
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
                           HintAtts hintAtts, CellFlags flags )
{
    LOG_FUNC();
    WasmDrawCtx* wdctx = (WasmDrawCtx*)dctx;
    const XP_UCHAR* str = vert ? "|" : "-";
    textInRect( wdctx, str, rect );
}

static void
createSurface( WasmDrawCtx* wdctx, int width, int height )
{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    uint32_t rmask = 0xff000000;
    uint32_t gmask = 0x00ff0000;
    uint32_t bmask = 0x0000ff00;
    uint32_t amask = 0x000000ff;
#else
    uint32_t rmask = 0x000000ff;
    uint32_t gmask = 0x0000ff00;
    uint32_t bmask = 0x00ff0000;
    uint32_t amask = 0xff000000;
#endif
    wdctx->surface = SDL_CreateRGBSurface( 0, width, height, 32,
                                           rmask, gmask, bmask, amask );
    wdctx->renderer = SDL_CreateSoftwareRenderer( wdctx->surface );
    // wdctx->texture = SDL_CreateTextureFromSurface( wdctx->renderer, wdctx->surface );
}

void
wasm_draw_render( DrawCtx* dctx, SDL_Renderer* dest )
{
    LOG_FUNC();
    WasmDrawCtx* wdctx = (WasmDrawCtx*)dctx;

    // SDL_RenderPresent( wdctx->renderer );
    
    SDL_Texture* texture =
        SDL_CreateTextureFromSurface( dest, wdctx->surface );
    SDL_RenderCopyEx( dest, texture, NULL, NULL, 0,
                      NULL, SDL_FLIP_NONE );
    SDL_DestroyTexture( texture );
}

DrawCtx*
wasm_draw_make( MPFORMAL int width, int height )
{
    LOG_FUNC();
    WasmDrawCtx* wdctx = XP_MALLOC( mpool, sizeof(*wdctx) );

    wdctx->font12 = TTF_OpenFont( "assets_dir/FreeSans.ttf", 12 );
    XP_ASSERT( !!wdctx->font12 );
    wdctx->font20 = TTF_OpenFont( "assets_dir/FreeSans.ttf", 20 );
    wdctx->font36 = TTF_OpenFont( "assets_dir/FreeSans.ttf", 36 );
    wdctx->font48 = TTF_OpenFont( "assets_dir/FreeSans.ttf", 48 );

    wdctx->vtable = XP_MALLOC( mpool, sizeof(*wdctx->vtable) );

    SET_VTABLE_ENTRY( wdctx->vtable, draw_clearRect, wasm );
    SET_VTABLE_ENTRY( wdctx->vtable, draw_dictChanged, wasm );
    SET_VTABLE_ENTRY( wdctx->vtable, draw_beginDraw, wasm );

    SET_VTABLE_ENTRY( wdctx->vtable, draw_clearRect, wasm );
    SET_VTABLE_ENTRY( wdctx->vtable, draw_dictChanged, wasm );
    SET_VTABLE_ENTRY( wdctx->vtable, draw_beginDraw, wasm );
    SET_VTABLE_ENTRY( wdctx->vtable, draw_destroyCtxt, wasm );
    SET_VTABLE_ENTRY( wdctx->vtable, draw_endDraw, wasm );
    SET_VTABLE_ENTRY( wdctx->vtable, draw_boardBegin, wasm );
    SET_VTABLE_ENTRY( wdctx->vtable, draw_objFinished, wasm );
    SET_VTABLE_ENTRY( wdctx->vtable, draw_vertScrollBoard, wasm );
    SET_VTABLE_ENTRY( wdctx->vtable, draw_trayBegin, wasm );
    SET_VTABLE_ENTRY( wdctx->vtable, draw_scoreBegin, wasm );
    SET_VTABLE_ENTRY( wdctx->vtable, draw_measureRemText, wasm );
    SET_VTABLE_ENTRY( wdctx->vtable, draw_drawRemText, wasm );
    SET_VTABLE_ENTRY( wdctx->vtable, draw_measureScoreText, wasm );
    SET_VTABLE_ENTRY( wdctx->vtable, draw_score_drawPlayer, wasm );
    SET_VTABLE_ENTRY( wdctx->vtable, draw_score_pendingScore, wasm );
    SET_VTABLE_ENTRY( wdctx->vtable, draw_drawTimer, wasm );
    SET_VTABLE_ENTRY( wdctx->vtable, draw_drawCell, wasm );
    SET_VTABLE_ENTRY( wdctx->vtable, draw_invertCell, wasm );
    SET_VTABLE_ENTRY( wdctx->vtable, draw_drawTile, wasm );
#ifdef POINTER_SUPPORT
    SET_VTABLE_ENTRY( wdctx->vtable, draw_drawTileMidDrag, wasm );
#endif
    SET_VTABLE_ENTRY( wdctx->vtable, draw_drawTileBack, wasm );
    SET_VTABLE_ENTRY( wdctx->vtable, draw_drawTrayDivider, wasm );
    SET_VTABLE_ENTRY( wdctx->vtable, draw_drawBoardArrow, wasm );

    createSurface( wdctx, width, height );

    return (DrawCtx*)wdctx;
}
