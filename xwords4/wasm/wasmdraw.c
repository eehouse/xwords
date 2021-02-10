#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#include "comtypes.h"
#include "strutils.h"
#include "wasmdraw.h"
#include "dbgutil.h"

typedef struct _WasmDrawCtx {
    DrawCtxVTable* vtable;
    SDL_Renderer* renderer;
    SDL_Surface* surface;
    SDL_Texture* texture;
    TTF_Font* font12;
    TTF_Font* font20;
    TTF_Font* font36;
    TTF_Font* font48;

    SDL_Surface* arrowDown;
    SDL_Surface* arrowRight;
    SDL_Surface* origin;

    int trayOwner;
    XP_Bool inTrade;
} WasmDrawCtx;

static SDL_Color sBonusColors[4] = {
    {0xAF, 0xAF, 0x00, 0xFF},
    {0x00, 0xAF, 0xAF,0xFF},
    {0xAF, 0x00, 0xAF,0xFF},
    {0xAF, 0xAF, 0xAF,0xFF},
};

static XP_UCHAR* sBonusSummaries[] = {
    "2L",
    "2W",
    "3L",
    "3W",
};

static SDL_Color sPlayerColors[4] = {
    {0x00, 0x00, 0x00, 0xFF},
    {0xFF, 0x00, 0x00, 0xFF},
    {0x80, 0x00, 0xFF, 0xFF},
    {0xFF, 0x80, 0x00, 0xFF},
};

enum { BLACK,
       WHITE,
       COLOR_FOCUS,
       COLOR_NOTILE,
       COLOR_TILE_BACK,
       COLOR_BONUSHINT,
       COLOR_BACK,

       N_COLORS,
};

static SDL_Color sOtherColors[N_COLORS] = {
    {0x00, 0x00, 0x00, 0xFF},   /* BLACK */
    {0xFF, 0xFF, 0xFF, 0xFF},   /* WHITE */
    {0x70, 0x70, 0xFF, 0xFF}, /* COLOR_FOCUS, */
    {0xFF, 0xFF, 0xFF, 0xFF},   /* COLOR_NOTILE, */
    {0xFF, 0xFF, 0x99, 0xFF}, /* COLOR_TILE_BACK, */
    {0x7F, 0x7F, 0x7F, 0xFF},/* COLOR_BONUSHINT, */
    {0xFF, 0xFF, 0xFF, 0xFF}, /* COLOR_BACK, */
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
    Uint8 red, green, blue, alpha;
    SDL_GetRenderDrawColor( wdctx->renderer, &red, &green, &blue, &alpha );
    const SDL_Color* back = &sOtherColors[COLOR_BACK];
    SDL_SetRenderDrawColor( wdctx->renderer, back->r, back->g, back->b, back->a );

    SDL_Rect sdlr;
    rectXPToSDL( &sdlr, rect );
    SDL_RenderFillRect( wdctx->renderer, &sdlr );

    SDL_SetRenderDrawColor( wdctx->renderer, red, green, blue, alpha );
}

static void
setPlayerColor( WasmDrawCtx* wdctx, int owner )
{
    SDL_Color colorParts = sPlayerColors[owner];
    SDL_SetRenderDrawColor( wdctx->renderer, colorParts.r, colorParts.g,
                            colorParts.b, colorParts.a );
}

static void
fillRect( WasmDrawCtx* wdctx, const XP_Rect* rect, const SDL_Color* colorParts )
{
    SDL_Rect sdlr;
    rectXPToSDL( &sdlr, rect );
    Uint8 red, green, blue, alpha;

    SDL_GetRenderDrawColor( wdctx->renderer, &red, &green, &blue, &alpha );
    SDL_SetRenderDrawColor( wdctx->renderer, colorParts->r, colorParts->g,
                            colorParts->b, colorParts->a );
    SDL_RenderFillRect( wdctx->renderer, &sdlr );
    SDL_SetRenderDrawColor( wdctx->renderer, red, green, blue, alpha );
}

static void
frameRect( WasmDrawCtx* wdctx, const XP_Rect* rect )
{
    SDL_Rect sdlr;
    rectXPToSDL( &sdlr, rect );
    const SDL_Color* black = &sOtherColors[BLACK];
    SDL_SetRenderDrawColor( wdctx->renderer, black->r, black->g,
                            black->b, black->a );
    SDL_RenderDrawRect( wdctx->renderer, &sdlr );
}

static void
textInRect( WasmDrawCtx* wdctx, const XP_UCHAR* text, const XP_Rect* rect,
            const SDL_Color* color )
{
    TTF_Font* font;
    if ( rect->height <= 12 ) {
        font = wdctx->font12;
    } else if ( rect->height <= 20 ) {
        font = wdctx->font20;
    } else if ( rect->height <= 36 ) {
        font = wdctx->font36;
    } else {
        font = wdctx->font48;
    }

    XP_Rect tmpR = *rect;

    SDL_Color black = { 0, 0, 0, 255 };
    if ( NULL == color ) {
        color = &black;
    }
    SDL_Surface* surface = TTF_RenderText_Blended( font, text, *color );
    SDL_Texture* texture = SDL_CreateTextureFromSurface( wdctx->renderer, surface );
    SDL_FreeSurface( surface );

    int width, height;
    SDL_QueryTexture( texture, NULL, NULL, &width, &height );
    /* XP_LOGFF( "have w: %d; h: %d; got w: %d; h: %d", */
    /*           tmpR.width, tmpR.height, width, height ); */
    tmpR.width = XP_MIN( width, tmpR.width );
    tmpR.height = XP_MIN( height, tmpR.height );

    SDL_Rect sdlr;
    rectXPToSDL( &sdlr, &tmpR );
    SDL_RenderCopy( wdctx->renderer, texture, NULL, &sdlr );
    SDL_DestroyTexture( texture );
}

static void
imgInRect( WasmDrawCtx* wdctx, SDL_Surface* img, const XP_Rect* rect )
{
    SDL_Texture* texture = SDL_CreateTextureFromSurface( wdctx->renderer, img );
    SDL_Rect sdlr;
    rectXPToSDL( &sdlr, rect );
    SDL_RenderCopy( wdctx->renderer, texture, NULL, &sdlr );
    SDL_DestroyTexture( texture );
}

static void
drawTile( WasmDrawCtx* wdctx, const XP_UCHAR* face, int val,
          int owner, const XP_Rect* rect )
{
    clearRect( wdctx, rect );
    frameRect( wdctx, rect );

    XP_Rect tmp = *rect;
    tmp.width = 3 * tmp.width / 4;
    tmp.height = 3 * tmp.height / 4;
    textInRect( wdctx, face, &tmp, &sPlayerColors[owner] );

    if ( 0 <= val ) {
        XP_Rect tmp = *rect;
        tmp.width = tmp.width / 4;
        tmp.left += tmp.width * 3;
        tmp.height = tmp.height / 4;
        tmp.top += tmp.height * 3;
        XP_UCHAR buf[4];
        XP_SNPRINTF( buf, VSIZE(buf), "%d", val );
        textInRect( wdctx, buf, &tmp, &sPlayerColors[owner] );
    }
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
    // LOG_FUNC();
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
    XP_LOGFF( "(owner=%d)", owner );
    WasmDrawCtx* wdctx = (WasmDrawCtx*)dctx;
    wdctx->trayOwner = owner;
    return XP_TRUE;
}

static XP_Bool
wasm_draw_scoreBegin( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                      XP_U16 numPlayers, 
                      const XP_S16* const scores,
                      XP_S16 remCount, DrawFocusState dfs )
{
    LOG_FUNC();
    WasmDrawCtx* wdctx = (WasmDrawCtx*)dctx;
    clearRect( wdctx, rect );
    return XP_TRUE;
}

static XP_Bool
wasm_draw_measureRemText( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                          XP_S16 nTilesLeft,
                          XP_U16* width, XP_U16* height )
{
    LOG_FUNC();
    XP_Bool drawIt = 0 <= nTilesLeft;
    if ( drawIt ) {
        XP_UCHAR buf[4];
        XP_SNPRINTF( buf, VSIZE(buf), "%d", nTilesLeft );

        WasmDrawCtx* wdctx = (WasmDrawCtx*)dctx;
        textInRect( wdctx, buf, rect, NULL );
        *width = *height = rect->height;
    }
    return drawIt;
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
                            const XP_Rect* rect, 
                            const DrawScoreInfo* dsi,
                            XP_U16* width, XP_U16* height )
{
    LOG_FUNC();
    *width = rect->width / 2;
    *height = rect->height;
}

static void
wasm_draw_score_drawPlayer( DrawCtx* dctx, XWEnv xwe,
                            const XP_Rect* rInner, 
                            const XP_Rect* rOuter, 
                            XP_U16 gotPct, 
                            const DrawScoreInfo* dsi )
{
    XP_LOGFF( "(playerNum: %d)", dsi->playerNum );
    WasmDrawCtx* wdctx = (WasmDrawCtx*)dctx;
    XP_UCHAR buf[32];
    XP_SNPRINTF( buf, VSIZE(buf), "%s: %d", dsi->name, dsi->totalScore );
    textInRect( wdctx, buf, rInner, &sPlayerColors[dsi->playerNum] );
}

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
    textInRect( wdctx, "Pts:", &tmp, NULL );
    tmp.top += tmp.height;
    XP_UCHAR buf[16];
    if ( score >= 0 ) {
        XP_SNPRINTF( buf, VSIZE(buf), "%.3d", score );
    } else {
        XP_STRNCPY( buf, "???", VSIZE(buf)  );
    }
    textInRect( wdctx, buf, &tmp, NULL );
}

static void
wasm_draw_drawTimer( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                     XP_U16 player, XP_S16 secondsLeft,
                     XP_Bool turnDone ){ LOG_FUNC(); }

static void
markBlank( WasmDrawCtx* wdctx, const XP_Rect* rect, const SDL_Color* backColor )
{
}

static void
drawCrosshairs( WasmDrawCtx* wdctx, const XP_Rect* rect, CellFlags flags )
{
    const SDL_Color* color = &sOtherColors[COLOR_FOCUS];
    if ( 0 != (flags & CELL_CROSSHOR) ) {
        XP_Rect hairRect = *rect;
        insetRect( &hairRect, 0, hairRect.height / 3 );
        fillRect( wdctx, &hairRect, color );
    }
    if ( 0 != (flags & CELL_CROSSVERT) ) {
        XP_Rect hairRect = *rect;
        insetRect( &hairRect, hairRect.width / 3, 0 );
        fillRect( wdctx, &hairRect, color );
    }
}

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
    const SDL_Color* backColor = NULL;
    XP_Bool empty = 0 != (flags & (CELL_DRAGSRC|CELL_ISEMPTY));
    XP_Bool pending = 0 != (flags & CELL_PENDING);
    XP_Bool recent = 0 != (flags & CELL_RECENT);
    XP_UCHAR* bonusStr = NULL;

    /* if ( wdctx->inTrade ) { */
    /*     fillRectOther( rect, CommonPrefs.COLOR_BACKGRND ); */
    /* } */

    if ( owner < 0 ) {
        owner = 0;
    }
    const SDL_Color* foreColor = &sPlayerColors[owner];

    if ( 0 != (flags & CELL_ISCURSOR) ) {
        backColor = &sOtherColors[COLOR_FOCUS];
    } else if ( empty ) {
        if ( 0 == bonus ) {
            backColor = &sOtherColors[COLOR_NOTILE];
        } else {
            backColor = &sBonusColors[bonus-1];
            bonusStr = sBonusSummaries[bonus-1];
        }
    } else if ( pending || recent ) {
        foreColor = &sOtherColors[WHITE];
        backColor = &sOtherColors[BLACK];
    } else {
        backColor = &sOtherColors[COLOR_TILE_BACK];
    }

    fillRect( wdctx, rect, backColor );

    if ( empty ) {
        if ( (CELL_ISSTAR & flags) != 0 ) {
            imgInRect( wdctx, wdctx->origin, rect );
        } else if ( NULL != bonusStr ) {
            const SDL_Color* color = &sOtherColors[COLOR_BONUSHINT];
            /* m_fillPaint.setColor( adjustColor(color) ); */
            /* Rect brect = new Rect( rect ); */
            /* brect.inset( 0, brect.height()/10 ); */
            /* drawCentered( bonusStr, brect, m_fontDims ); */
            textInRect( wdctx, bonusStr, rect, color );
        }
    } else {
        textInRect( wdctx, text, rect, foreColor );
    }

    if ( (CELL_ISBLANK & flags) != 0 ) {
        markBlank( wdctx, rect, backColor );
    }

    // frame the cell
    frameRect( wdctx, rect );

    drawCrosshairs( wdctx, rect, flags );
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
    WasmDrawCtx* wdctx = (WasmDrawCtx*)dctx;
    drawTile( wdctx, text, val, wdctx->trayOwner, rect );
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
    drawTile( wdctx, text, val, wdctx->trayOwner, rect );
    return XP_TRUE;
}
#endif

static XP_Bool
wasm_draw_drawTileBack( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                        CellFlags flags )
{
    LOG_FUNC();
    WasmDrawCtx* wdctx = (WasmDrawCtx*)dctx;
    drawTile( wdctx, "?", -1, wdctx->trayOwner, rect );
    return XP_TRUE;
}

static void
wasm_draw_drawTrayDivider( DrawCtx* dctx, XWEnv xwe, const XP_Rect* rect,
                           CellFlags flags )
{
    WasmDrawCtx* wdctx = (WasmDrawCtx*)dctx;
    fillRect( wdctx, rect, &sPlayerColors[wdctx->trayOwner] );
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
    WasmDrawCtx* wdctx = (WasmDrawCtx*)dctx;
    const XP_UCHAR* str = vert ? "|" : "-";
    SDL_Surface* img = vert ? wdctx->arrowDown : wdctx->arrowRight;

    imgInRect( wdctx, img, rect );
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

    wdctx->arrowDown = IMG_Load( "assets_dir/ic_downarrow.png" );
    wdctx->arrowRight = IMG_Load( "assets_dir/ic_rightarrow.png" );
    wdctx->origin = IMG_Load( "assets_dir/ic_origin.png" );

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
