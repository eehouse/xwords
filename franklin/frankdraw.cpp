/* -*-mode: C; fill-column: 78; c-basic-offset: 4;-*- */ 
/* 
 * Copyright 1997-2001 by Eric House (xwords@eehouse.org).  All rights reserved.
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
#include <stdlib.h>

extern "C" {
#include "xptypes.h"
#include "board.h"
#include "draw.h"
#include "mempool.h"
} /* extern "C" */

#include "frankmain.h"

static void
insetRect( RECT* r, short i )
{
    r->x += i;
    r->y += i;

    i *= 2;
    r->width -= i;
    r->height -= i;
} /* insetRect */

static void
eraseRect(FrankDrawCtx* dctx, RECT* rect )
{
    dctx->window->DrawRectFilled( rect, COLOR_WHITE );
} /* eraseRect */

static XP_Bool
frank_draw_boardBegin( DrawCtx* p_dctx, const DictionaryCtxt* dict, 
                       const XP_Rect* rect, XP_Bool hasfocus )
{
    return XP_TRUE;
} /* draw_finish */

static void
frank_draw_boardFinished( DrawCtx* p_dctx )
{
    //    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
} /* draw_finished */

static void
XP_RectToRECT( RECT* d, const XP_Rect* s )
{
    d->x = s->left;
    d->y = s->top;
    d->width = s->width;
    d->height = s->height;
} /* XP_RectToRECT */

/* Called by methods that draw cells, i.e. drawCell and drawCursor, to do
 * common prep of the area.
 */
static void
cellDrawPrep( FrankDrawCtx* dctx, const XP_Rect* xprect, RECT* insetRect )
{
    XP_RectToRECT( insetRect, xprect );

    ++insetRect->height;
    ++insetRect->width;

    eraseRect( dctx, insetRect );
} /* cellDrawPrep */

static XP_Bool
frank_draw_drawCell( DrawCtx* p_dctx, const XP_Rect* xprect, 
                     const XP_UCHAR* letters, const XP_Bitmap* bitmap, 
                     Tile tile, XP_S16 owner, XWBonusType bonus,
                     HintAtts hintAtts,
                     XP_Bool isBlank, XP_Bool isPending, XP_Bool isStar )
{
    FrankDrawCtx* dctx = (FrankDrawCtx*)p_dctx;
    RECT rectInset;
    XP_Bool showGrid = XP_TRUE;

    cellDrawPrep( dctx, xprect, &rectInset );

    if ( !!letters ) {
        if ( *letters == LETTER_NONE ) {
            if ( bonus != BONUS_NONE ) {
                XP_ASSERT( bonus <= 4 );
#ifdef USE_PATTERNS
                dctx->window->DrawTiledImage( xprect->left, xprect->top, 
                                              xprect->width, 
                                              xprect->height,
                                              &dctx->bonusImages[bonus-1] );
#else
                RECT filledR = rectInset;
                COLOR color;
                insetRect( &filledR, 1 );

                switch( bonus ) {
                case BONUS_DOUBLE_LETTER:
                    color = COLOR_GRAY53;
                    break;
                case BONUS_TRIPLE_LETTER:
                    color = COLOR_GRAY40;
                    break;
                case BONUS_DOUBLE_WORD:
                    color = COLOR_GRAY27;
                    break;
                case BONUS_TRIPLE_WORD:
                    color = COLOR_GRAY13;
                    break;
                default:
                    color = COLOR_WHITE; /* make compiler happy */
                    XP_ASSERT(0);
                }

                dctx->window->DrawRectFilled( &filledR, color ); 
#endif
            }

            if ( isStar ) {
                dctx->window->DrawImage( xprect->left+2, xprect->top+2, 
                                         &dctx->startMark );
            }

        } else {
            dctx->window->DrawText( (const char*)letters, rectInset.x+2, 
                                    rectInset.y+1 );
        }
    } else if ( !!bitmap ) {
        dctx->window->DrawImage( xprect->left+2, xprect->top+2,
                                 (IMAGE*)bitmap );
    }

    if ( showGrid ) {
        COLOR color = isBlank? COLOR_GRAY53:COLOR_BLACK;
        dctx->window->DrawRect( &rectInset, color );
    }
    if ( isPending ) {
        insetRect( &rectInset, 1 );
        dctx->window->InvertRect( &rectInset );	
    }

    return XP_TRUE;
} /* frank_draw_drawCell */

static void
frank_draw_invertCell( DrawCtx* p_dctx, const XP_Rect* rect )
{
    FrankDrawCtx* dctx = (FrankDrawCtx*)p_dctx;
    RECT r;

    XP_RectToRECT( &r, rect );
    r.x += 3;
    r.y += 3;
    r.width -= 5;
    r.height -= 5;

/*     insetRect( &r, 2 ); */
    dctx->window->InvertRect( &r );
    GUI_UpdateNow();
} /* frank_draw_invertCell */

static XP_Bool
frank_draw_trayBegin( DrawCtx* p_dctx, const XP_Rect* rect, XP_U16 owner,
                      XP_Bool hasfocus )
{
/*     FrankDrawCtx* dctx = (FrankDrawCtx*)p_dctx; */
/*     clip? */
/*     eraseRect( dctx, rect ); */
    return XP_TRUE;
} /* frank_draw_trayBegin */

static void
frank_draw_drawTile( DrawCtx* p_dctx, const XP_Rect* xprect, 
                     const XP_UCHAR* letter,
                     XP_Bitmap* bitmap, XP_S16 val, XP_Bool highlighted )
{
    FrankDrawCtx* dctx = (FrankDrawCtx*)p_dctx;
    char numbuf[3];
    U32 width;
    XP_Rect insetR = *xprect;
    RECT rect;

    XP_RectToRECT( &rect, xprect );

    eraseRect( dctx, &rect );

    /* frame the tile */

    dctx->window->DrawRect( &rect, COLOR_BLACK );
	
    if ( !!letter ) {
        if ( *letter != LETTER_NONE ) { /* blank */
            dctx->window->DrawText( (char*)letter, rect.x+1, rect.y+1, 
                                    dctx->trayFont );
        }
    } else if ( !!bitmap ) {
        dctx->window->DrawImage( rect.x+2, rect.y+3, (IMAGE*)bitmap );
    }

    if ( val >= 0 ) {
        sprintf( (char*)numbuf, (const char*)"%d", val );
        width = GUI_TextWidth( dctx->valFont, numbuf, strlen(numbuf)); 
        U16 height = GUI_FontHeight( dctx->valFont ); 
        dctx->window->DrawText( (char*)numbuf, rect.x+rect.width - width - 1, 
                                rect.y + rect.height - height - 1, 
                                dctx->valFont );
    }
    
    if ( highlighted ) {
        insetRect( &rect, 1 );
        dctx->window->DrawRect( &rect, COLOR_BLACK );
    }
} /* frank_draw_drawTile */

static void
frank_draw_drawTileBack( DrawCtx* p_dctx, const XP_Rect* xprect )
{
/*     FrankDrawCtx* dctx = (FrankDrawCtx*)p_dctx; */

    frank_draw_drawTile( p_dctx, xprect, (XP_UCHAR*)"?", 
                         (XP_Bitmap*)NULL, -1, XP_FALSE );
} /* frank_draw_drawTileBack */

static void
frank_draw_drawTrayDivider( DrawCtx* p_dctx, const XP_Rect* rect, 
                            XP_Bool selected )
{
    FrankDrawCtx* dctx = (FrankDrawCtx*)p_dctx;
    RECT winRect;
    XP_RectToRECT( &winRect, rect );

    eraseRect( dctx, &winRect );

    ++winRect.x;
    winRect.width -= 2;

    COLOR color;
    if ( selected ) {
        color = COLOR_GRAY27;
    } else {
        color = COLOR_BLACK;
    }

    dctx->window->DrawRectFilled( &winRect, color );
} /* frank_draw_drawTrayDivider */

static void 
frank_draw_clearRect( DrawCtx* p_dctx, const XP_Rect* rectP )
{
    FrankDrawCtx* dctx = (FrankDrawCtx*)p_dctx;
    RECT rect;

    XP_RectToRECT( &rect, rectP );

    eraseRect( dctx, &rect );

} /* frank_draw_clearRect */

static void
frank_draw_drawBoardArrow( DrawCtx* p_dctx, const XP_Rect* xprect, 
                           XWBonusType cursorBonus, XP_Bool vertical,
                           HintAtts hintAtts )
{
    FrankDrawCtx* dctx = (FrankDrawCtx*)p_dctx;
    RECT rect;

    cellDrawPrep( dctx, xprect, &rect );

    dctx->window->DrawImage( rect.x+3, rect.y+2, 
                             vertical?&dctx->downcursor:&dctx->rightcursor );
    /* frame the cell */
    dctx->window->DrawRect( &rect, COLOR_BLACK );
} /* frank_draw_drawBoardArrow */

static void
frank_draw_scoreBegin( DrawCtx* p_dctx, const XP_Rect* rect, 
                       XP_U16 numPlayers, XP_Bool hasfocus )
{
    FrankDrawCtx* dctx = (FrankDrawCtx*)p_dctx;
    RECT r;

    XP_RectToRECT( &r, rect );
    eraseRect( dctx, &r );
} /* frank_draw_scoreBegin */

static void
frank_draw_measureRemText( DrawCtx* p_dctx, const XP_Rect* r, 
                           XP_S16 nTilesLeft, 
                           XP_U16* width, XP_U16* height )
{
    FrankDrawCtx* dctx = (FrankDrawCtx*)p_dctx;

    *height = SCORE_HEIGHT;

    char buf[15];
    sprintf( (char*)buf, "rem:%d", nTilesLeft );
    *width  = GUI_TextWidth( dctx->scoreFnt, (char*)buf, 
                             strlen(buf) ); 
} /* frank_draw_measureRemText */

static void
frank_draw_drawRemText( DrawCtx* p_dctx, const XP_Rect* rInner, 
                        const XP_Rect* rOuter, XP_S16 nTilesLeft )
{
    FrankDrawCtx* dctx = (FrankDrawCtx*)p_dctx;
    char buf[15];
    sprintf( (char*)buf, "rem:%d", nTilesLeft );
    dctx->window->DrawText( (char*)buf, rInner->left, rInner->top, 
                            dctx->scoreFnt );
} /* frank_draw_drawRemText */

static XP_U16
scoreWidthAndText( char* buf, const FONT* font, const DrawScoreInfo* dsi )
{
    char* borders = "";
    if ( dsi->isTurn ) {
        borders = "*";
    }

    sprintf( buf, "%s%.3d", borders, dsi->score );
    if ( dsi->nTilesLeft >= 0 ) {
        char nbuf[10];
        sprintf( nbuf, ":%d", dsi->nTilesLeft );
        strcat( buf, nbuf );
    }
    strcat( buf, borders );
    
    S32 width = GUI_TextWidth( font, buf, strlen(buf) );
    return width;
} /* scoreWidthAndText */

static void
frank_draw_measureScoreText( DrawCtx* p_dctx, const XP_Rect* r, 
                             const DrawScoreInfo* dsi,
                             XP_U16* width, XP_U16* height )
{
    FrankDrawCtx* dctx = (FrankDrawCtx*)p_dctx;
    char buf[20];
    const FONT* font = dsi->selected? dctx->scoreFntBold:dctx->scoreFnt;

    *height = SCORE_HEIGHT;
    *width = scoreWidthAndText( buf, font, dsi );
} /* frank_draw_measureScoreText */

static void
frank_draw_score_drawPlayer( DrawCtx* p_dctx, 
                             const XP_Rect* rInner, const XP_Rect* rOuter, 
                             const DrawScoreInfo* dsi )
{
    FrankDrawCtx* dctx = (FrankDrawCtx*)p_dctx;
    char buf[20];
    XP_U16 x;
    const FONT* font = dsi->selected? dctx->scoreFntBold:dctx->scoreFnt;

    S32 width = scoreWidthAndText( buf, font, dsi );

    x = rInner->left + ((rInner->width - width) /2);

    dctx->window->DrawText( buf, x, rInner->top, font );
} /* frank_draw_score_drawPlayer */

static void
frank_draw_score_pendingScore( DrawCtx* p_dctx, const XP_Rect* rect, 
                               XP_S16 score, XP_U16 playerNum )
{
    FrankDrawCtx* dctx = (FrankDrawCtx*)p_dctx;
    char buf[5];
    XP_U16 left;

    if ( score >= 0 ) {
        sprintf( buf, "%.3d", score );
    } else {
        strcpy( buf, "???" );
    }

    RECT r;
    XP_RectToRECT( &r, rect );
    eraseRect( dctx, &r );

    left = r.x+1;
    dctx->window->DrawText( "Pts:", left, r.y, dctx->valFont );
    dctx->window->DrawText( buf, left, r.y+(r.height/2), 
                            dctx->scoreFnt );
} /* frank_draw_score_pendingScore */

static void
frank_draw_scoreFinished( DrawCtx* p_dctx )
{

} /* frank_draw_scoreFinished */

static U16
frankFormatTimerText( char* buf, XP_S16 secondsLeft )
{
    XP_U16 minutes, seconds;
    XP_U16 nChars = 0;

    if ( secondsLeft < 0 ) {
        *buf++ = '-';
        secondsLeft *= -1;
        ++nChars;
    }

    minutes = secondsLeft / 60;
    seconds = secondsLeft % 60;

    char secBuf[6];
    sprintf( secBuf, "0%d", seconds );

    nChars += sprintf( buf, "%d:%s", minutes, 
                       secBuf[2] == '\0'? secBuf:&secBuf[1] );
    return nChars;
} /* frankFormatTimerText */

static void
frank_draw_drawTimer( DrawCtx* p_dctx, const XP_Rect* rInner, 
                      const XP_Rect* rOuter, XP_U16 player, 
                      XP_S16 secondsLeft )
{
    FrankDrawCtx* dctx = (FrankDrawCtx*)p_dctx;
    char buf[12];

    (void)frankFormatTimerText( buf, secondsLeft );

    XP_DEBUGF( "drawing timer text: %s at %d,%d", buf, rInner->left, 
               rInner->top );
    RECT r;
    XP_RectToRECT( &r, rInner );
    eraseRect( dctx, &r );
    dctx->window->DrawText( buf, rInner->left, rInner->top, dctx->scoreFnt );
} /* frank_draw_drawTimer */

static XP_UCHAR*
frank_draw_getMiniWText( DrawCtx* p_dctx, XWMiniTextType whichText )
{
    char* str = (char*)NULL;

    switch( whichText ) {
    case BONUS_DOUBLE_LETTER:
        str = "Double letter"; break;
    case BONUS_DOUBLE_WORD:
        str = "Double word"; break;
    case BONUS_TRIPLE_LETTER:
        str = "Triple letter"; break;
    case BONUS_TRIPLE_WORD:
        str = "Triple word"; break;
    case INTRADE_MW_TEXT:	
        str = "Click D when done"; break;
    default:
        XP_ASSERT( XP_FALSE );
    }
    return (XP_UCHAR*)str;
} /* frank_draw_getMiniWText */

static void
frank_draw_measureMiniWText( DrawCtx* p_dctx, const XP_UCHAR* str, 
                             XP_U16* widthP, XP_U16* heightP )
{
    FrankDrawCtx* dctx = (FrankDrawCtx*)p_dctx;
    *widthP = 6 + GUI_TextWidth( dctx->scoreFnt, (const char*)str,
                                 strlen((const char*)str) ); 
    *heightP = 6 + GUI_FontHeight( dctx->scoreFnt ); 
} /* frank_draw_measureMiniWText */

static void
frank_draw_drawMiniWindow( DrawCtx* p_dctx, const XP_UCHAR* text, 
                           const XP_Rect* rect, void** closureP )
{
    FrankDrawCtx* dctx = (FrankDrawCtx*)p_dctx;
    RECT r;

    XP_RectToRECT( &r, rect );

    eraseRect( dctx, &r );

    --r.width;
    --r.height;
    ++r.x;
    ++r.y;
    dctx->window->DrawRect( &r, COLOR_BLACK );
    --r.x;
    --r.y;
    eraseRect( dctx, &r );
    dctx->window->DrawRect( &r, COLOR_BLACK );

    dctx->window->DrawText( (const char*)text, r.x+2, r.y+2, dctx->scoreFnt );
} /* frank_draw_drawMiniWindow */

static void
frank_draw_eraseMiniWindow( DrawCtx* p_dctx, const XP_Rect* rect, 
                            XP_Bool lastTime,
                            void** closure, XP_Bool* invalUnder )
{
/*     FrankDrawCtx* dctx = (FrankDrawCtx*)p_dctx; */
    *invalUnder = XP_TRUE;
} /* frank_draw_eraseMiniWindow */

#define SET_GDK_COLOR( c, r, g, b ) { \
     c.red = (r); \
     c.green = (g); \
     c.blue = (b); \
}
static void
draw_doNothing( DrawCtx* dctx, ... )
{
} /* draw_doNothing */


const unsigned char downcursor_bits[] = {
    0x00, 0x00, 
    0x10, 0x00, 
    0x10, 0x00, 
    0x10, 0x00, 
    0xfe, 0x00, 
    0x7c, 0x00,
    0x38, 0x00, 
    0x10, 0x00, 
    0x00, 0x00,
};

const unsigned char rightcursor_bits[] = {
    0x00, 0x00, 0x10, 0x00, 0x18, 0x00, 0x1c, 0x00, 0xfe, 0x00, 0x1c, 0x00,
    0x18, 0x00, 0x10, 0x00, 0x00, 0x00
};
const unsigned char startMark_bits[] = {
    0xc1, 0x80,    /* ##-- ---# # */
    0xe3, 0x80,	   /* ###- --## # */
    0x77, 0x00,	   /* -### -### - */
    0x3e, 0x00,	   /* --## ###- - */
    0x1c, 0x00,	   /* ---# ##-- - */
    0x3e, 0x00,	   /* --## ###- - */
    0x77, 0x00,	   /* -### -### - */
    0xe3, 0x80,	   /* ###- --## # */
    0xc1, 0x80,	   /* ##-- ---# # */
};

#ifdef USE_PATTERNS
const unsigned char bonus_bits[][4] = {
    { 0x88, 0x44, 0x22, 0x11 },
    { 0xaa, 0x55, 0xaa, 0x55 },
    { 0xCC, 0x66, 0x33, 0x99 },
    { 0xCC, 0xCC, 0x33, 0x33 }
};
#endif

DrawCtx* 
frank_drawctxt_make( MPFORMAL CWindow* window )
{
    FrankDrawCtx* dctx = (FrankDrawCtx*)XP_MALLOC( mpool,
                                                   sizeof(FrankDrawCtx) );
    U16 i;

    dctx->vtable = (DrawCtxVTable*)
        XP_MALLOC( mpool, sizeof(*(((FrankDrawCtx*)dctx)->vtable)) );

    for ( i = 0; i < sizeof(*dctx->vtable)/4; ++i ) {
        ((void**)(dctx->vtable))[i] = draw_doNothing;
    }

/*     SET_VTABLE_ENTRY( dctx, draw_destroyCtxt, frank_ ); */
    SET_VTABLE_ENTRY( dctx->vtable, draw_boardBegin, frank );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawCell, frank );
    SET_VTABLE_ENTRY( dctx->vtable, draw_invertCell, frank );

    SET_VTABLE_ENTRY( dctx->vtable, draw_boardFinished, frank );

    SET_VTABLE_ENTRY( dctx->vtable, draw_trayBegin, frank );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTile, frank );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTileBack, frank );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTrayDivider, frank );

    SET_VTABLE_ENTRY( dctx->vtable, draw_clearRect, frank );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawBoardArrow, frank );

    SET_VTABLE_ENTRY( dctx->vtable, draw_scoreBegin, frank );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureRemText, frank );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawRemText, frank );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureScoreText, frank );
    SET_VTABLE_ENTRY( dctx->vtable, draw_score_drawPlayer, frank );
    SET_VTABLE_ENTRY( dctx->vtable, draw_score_pendingScore, frank );
    SET_VTABLE_ENTRY( dctx->vtable, draw_scoreFinished, frank );

    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTimer, frank );

    SET_VTABLE_ENTRY( dctx->vtable, draw_getMiniWText, frank );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureMiniWText, frank );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawMiniWindow, frank );
    SET_VTABLE_ENTRY( dctx->vtable, draw_eraseMiniWindow, frank );

    dctx->window = window;

    dctx->valFont = GUI_GetFont( 9, CTRL_NORMAL ); 
    dctx->scoreFnt = GUI_GetFont( 12, CTRL_NORMAL );
    dctx->scoreFntBold = GUI_GetFont( 12, CTRL_BOLD );
    dctx->trayFont = GUI_GetFont( 16, CTRL_NORMAL ); 

    IMAGE downcursor = { 9, 9, 2,
                         COLOR_MODE_MONO, 0, (const COLOR *) 0,
                         (U8*)downcursor_bits };
    XP_MEMCPY( (IMAGE*)&dctx->downcursor, &downcursor, 
               sizeof(dctx->downcursor) );

    IMAGE rightcursor = { 9, 9, 2,
                          COLOR_MODE_MONO, 0, (const COLOR *) 0,
                          (U8*)rightcursor_bits };
    XP_MEMCPY( (IMAGE*)&dctx->rightcursor, &rightcursor, 
               sizeof(dctx->rightcursor) );

    IMAGE startMark = { 9, 9, 2,
                        COLOR_MODE_MONO, 0, (const COLOR *) 0,
                        (U8*)startMark_bits };
    XP_MEMCPY( (IMAGE*)&dctx->startMark, &startMark, 
               sizeof(dctx->startMark) );

#ifdef USE_PATTERNS
    for ( i = 0; i < BONUS_LAST; ++i ) {
        IMAGE bonus = { 8, 4, 1,
                        COLOR_MODE_MONO, 0, (const COLOR *) 0,
                        (U8*)bonus_bits[i] };
        XP_MEMCPY( (IMAGE*)&dctx->bonusImages[i], &bonus, 
                   sizeof(dctx->bonusImages[i]) );
    }
#endif
    return (DrawCtx*)dctx;
} /* frank_drawctxt_make */
