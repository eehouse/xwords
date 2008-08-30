/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2003 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifdef DRAW_WITH_PRIMITIVES

#include "draw.h"
#include "xptypes.h"

static void
insetRect( XP_Rect* r, XP_S16 amt )
{
    r->top += amt;
    r->left += amt;
    amt *= 2;
    r->width -= amt;
    r->height -= amt;
} /* insetRect */

static void
getRemText( XP_UCHAR* buf, XP_U16 bufSize, XP_S16 nTilesLeft )
{
    if ( nTilesLeft > 0 ) {
        XP_SNPRINTF( buf, bufSize, "rem: %d", nTilesLeft );
    } else {
        buf[0] = '\0';
    }
} /* getRemText */

static void
default_draw_measureRemText( DrawCtx* dctx, const XP_Rect* XP_UNUSED(r),
                             XP_S16 nTilesLeft, 
                             XP_U16* widthP, XP_U16* heightP )
{
    XP_U16 width, height;

    if ( nTilesLeft > 0 ) {
        XP_UCHAR buf[20];
        getRemText( buf, sizeof(buf), nTilesLeft );
        draw_measureText( dctx, buf, &width, &height );
    } else {
        width = height = 0;
    }

    *widthP = width;
    *heightP = height;
} /* default_draw_measureRemText */

static void 
default_draw_drawRemText( DrawCtx* dctx, const XP_Rect* XP_UNUSED(rInner), 
                          const XP_Rect* rOuter, XP_S16 nTilesLeft )
{
    XP_Rect oldClip;
    XP_UCHAR buf[10];

    getRemText( buf, sizeof(buf), nTilesLeft );

    draw_setClip( dctx, rOuter, &oldClip );
    draw_drawString( dctx, buf, rOuter->left, rOuter->top );
    draw_setClip( dctx, &oldClip, NULL );
} /* default_draw_drawRemText */

static void
formatScore( XP_UCHAR* buf, XP_U16 bufSize, const DrawScoreInfo* dsi )
{
    XP_UCHAR remBuf[10];
    XP_UCHAR* selStr;

    if ( dsi->selected ) {
        selStr = "*";
    } else {
        selStr = "";
    }

    if ( dsi->nTilesLeft >= 0 ) {
        XP_SNPRINTF( remBuf, sizeof(remBuf), ":%d", dsi->nTilesLeft );
    } else {
        remBuf[0] = '\0';
    }
    
    XP_SNPRINTF( buf, bufSize, "%s%d%s%s", selStr, dsi->totalScore, 
                 remBuf, selStr );
} /* formatScore */

static void
default_draw_measureScoreText( DrawCtx* dctx, const XP_Rect* XP_UNUSED(r), 
                               const DrawScoreInfo* dsi,
                               XP_U16* widthP, XP_U16* heightP )
{
    XP_UCHAR buf[20];
    formatScore( buf, sizeof(buf), dsi );
    draw_measureText( dctx, buf, widthP, heightP );
} /* default_draw_measureScoreText */

static void
default_draw_score_drawPlayer( DrawCtx* dctx, 
                               const XP_Rect* rInner, const XP_Rect* rOuter, 
                               const DrawScoreInfo* dsi )
{
    XP_Rect oldClip;
    XP_UCHAR buf[20];

    draw_setClip( dctx, rInner, &oldClip );
    draw_clearRect( dctx, rOuter );

    formatScore( buf, sizeof(buf), dsi );
    draw_drawString( dctx, buf, rInner->left, rInner->top );

    draw_setClip( dctx, &oldClip, NULL );
} /* default_draw_score_drawPlayer */

static XP_Bool
default_draw_drawCell( DrawCtx* dctx, const XP_Rect* rect, 
                       const XP_UCHAR* text, 
                       const XP_Bitmap bitmap,
                       Tile XP_UNUSED(tile), XP_S16 XP_UNUSED(owner),
                       XWBonusType bonus, HintAtts XP_UNUSED(hintAtts),
                       CellFlags flags )
{
    XP_Rect oldClip;
    XP_Rect inset = *rect;
    insetRect( &inset, 1 );

    draw_setClip( dctx, rect, &oldClip );

    draw_clearRect( dctx, rect );

    if ( !!text && text[0] != 0 ) {
        draw_drawString( dctx, text, inset.left, inset.top );
    } else if ( !!bitmap ) {
        draw_drawBitmap( dctx, bitmap, inset.left, inset.top );
    } else if ( bonus != BONUS_NONE ) {
        XP_UCHAR* bstr;
        switch( bonus ) {
        case BONUS_DOUBLE_LETTER:
            bstr = "*";
            break;
        case BONUS_DOUBLE_WORD:
            bstr = "%";
            break;
        case BONUS_TRIPLE_LETTER:
            bstr = "#";
            break;
        case BONUS_TRIPLE_WORD:
            bstr = "@";
            break;
        default:
            XP_ASSERT(0);
            break;
        }
        draw_drawString( dctx, bstr, inset.left, inset.top );
    }
    
    if ( 0 != (flags & CELL_HIGHLIGHT) ) {
        draw_invertRect( dctx, &inset );
    }        
    
    draw_frameRect( dctx, rect );
    draw_setClip( dctx, &oldClip, NULL );

    return XP_TRUE;
} /* default_draw_drawCell */

static void
default_draw_drawBoardArrow( DrawCtx* dctx, const XP_Rect* rect, 
                             XWBonusType XP_UNUSED(bonus), XP_Bool vert,
                             HintAtts XP_UNUSED(hintAtts),
                             CellFlags XP_UNUSED(flags) )
{
    XP_Rect oldClip;
    XP_UCHAR* arrow;

    if ( vert ) {
        arrow = "|";
    } else {
        arrow = "-";
    }

    draw_setClip( dctx, rect, &oldClip );
    draw_clearRect( dctx, rect );
    draw_frameRect( dctx, rect );
    draw_drawString( dctx, arrow, rect->left+1, rect->top+1 );
    draw_setClip( dctx, &oldClip, NULL );
} /* default_draw_drawBoardArrow */

static void
default_draw_drawTile( DrawCtx* dctx, const XP_Rect* rect, 
                       const XP_UCHAR* text, 
                       const XP_Bitmap bitmap,
                       XP_S16 val, CellFlags flags )
{
    XP_Rect oldClip;
    XP_Rect inset = *rect;

    draw_setClip( dctx, rect, &oldClip );
    draw_clearRect( dctx, rect );

    draw_frameRect( dctx, rect );

    if ( 0 != (flags & CELL_HIGHLIGHT) ) {
        insetRect( &inset, 1 );
        draw_frameRect( dctx, &inset );
        insetRect( &inset, 1 );
    } else {
        insetRect( &inset, 2 );
    }

    if ( !!text && text[0] != '\0' ) {
        draw_drawString( dctx, text, inset.left, inset.top );
    } else if ( !!bitmap ) {
        draw_drawBitmap( dctx, bitmap, inset.left, inset.top );
    }

    if ( val >= 0 ) {
        XP_UCHAR sbuf[4];
        XP_U16 width, height;
        XP_U16 x, y;

        XP_SNPRINTF( sbuf, sizeof(sbuf), "%d", val );
        draw_measureText( dctx, sbuf, &width, &height );

        x = inset.left + inset.width - width;
        y = inset.top + inset.height - height;
        draw_drawString( dctx, sbuf, x, y );
    }

    draw_setClip( dctx, &oldClip, NULL );
} /* default_draw_drawTile */

static void
default_draw_drawTileBack( DrawCtx* dctx, const XP_Rect* rect,
                           CellFlags XP_UNUSED(flags) )
{
    default_draw_drawTile( dctx, rect, "?", NULL, -1, XP_FALSE );
} /* default_draw_drawTileBack */

static void
default_draw_drawTrayDivider( DrawCtx* dctx, const XP_Rect* rect, 
                              CellFlags XP_UNUSED(flags))
{
    XP_Rect r = *rect;
    draw_clearRect( dctx, rect );
    if ( r.width > 2 ) {
        r.width -= 2;
        r.left += 1;
    }
    draw_frameRect( dctx, &r );
} /* default_draw_drawTrayDivider */

static void
default_draw_score_pendingScore( DrawCtx* dctx, 
                                 const XP_Rect* rect, 
                                 XP_S16 score, 
                                 XP_U16 XP_UNUSED(playerNum),
                                 CellFlags XP_UNUSED(flags) )
{
    XP_UCHAR buf[5];
    XP_Rect oldClip;
    XP_Rect r;
    XP_U16 width, height;
    XP_UCHAR* stxt;

    draw_setClip( dctx, rect, &oldClip );

    XP_MEMCPY( &r, rect, sizeof(r) );
    ++r.left;                   /* don't erase neighbor's border */
    --r.width;
    draw_clearRect( dctx, &r );

    draw_drawString( dctx, "pts", r.left, r.top );

    if ( score >= 0 ) {
        XP_SNPRINTF( buf, sizeof(buf), "%d", score );
        stxt = buf;
    } else {
        stxt = "???";
    }
    draw_measureText( dctx, stxt, &width, &height );
    draw_drawString( dctx, stxt, r.left, r.top + r.height - height );

    draw_setClip( dctx, &oldClip, NULL );
} /* default_draw_score_pendingScore */

static const XP_UCHAR*
default_draw_getMiniWText( DrawCtx* XP_UNUSED(dctx), XWMiniTextType textHint )
{
    char* str;

    switch( textHint ) {
    case BONUS_DOUBLE_LETTER:
        str = "Double letter"; break;
    case BONUS_DOUBLE_WORD:
        str = "Double word"; break;
    case BONUS_TRIPLE_LETTER:
        str = "Triple letter"; break;
    case BONUS_TRIPLE_WORD:
        str = "Triple word"; break;
    case INTRADE_MW_TEXT:
        str = "Trading tiles;\nclick D when done"; break;
    default:
        XP_ASSERT( XP_FALSE );
    }
    return str;
} /* default_draw_getMiniWText */

static void
default_draw_measureMiniWText( DrawCtx* dctx, const XP_UCHAR* textP, 
                               XP_U16* widthP, XP_U16* heightP )
{
    draw_measureText( dctx, textP, widthP, heightP );

    /* increase for frame */
    *widthP += 2;
    *heightP += 2;
} /* default_draw_measureMiniWText */

static void
default_draw_drawMiniWindow( DrawCtx* dctx, const XP_UCHAR* text,
                             const XP_Rect* rect, void** XP_UNUSED(closure) )
{
    XP_Rect oldClip;

    draw_setClip( dctx, rect, &oldClip );

    draw_clearRect( dctx, rect );
    draw_frameRect( dctx, rect );
    draw_drawString( dctx, text, rect->left+1, rect->top+1 );

    draw_setClip( dctx, &oldClip, NULL );
} /* default_draw_drawMiniWindow */

void
InitDrawDefaults( DrawCtxVTable* vtable )
{
    SET_VTABLE_ENTRY( vtable, draw_measureRemText, default );
    SET_VTABLE_ENTRY( vtable, draw_drawRemText, default );
    SET_VTABLE_ENTRY( vtable, draw_measureScoreText, default );
    SET_VTABLE_ENTRY( vtable, draw_score_drawPlayer, default );
    SET_VTABLE_ENTRY( vtable, draw_drawCell, default );
    SET_VTABLE_ENTRY( vtable, draw_drawBoardArrow, default );
    SET_VTABLE_ENTRY( vtable, draw_drawTile, default );
    SET_VTABLE_ENTRY( vtable, draw_drawTileBack, default );
    SET_VTABLE_ENTRY( vtable, draw_drawTrayDivider, default );
    SET_VTABLE_ENTRY( vtable, draw_score_pendingScore, default );

    SET_VTABLE_ENTRY( vtable, draw_getMiniWText, default );
    SET_VTABLE_ENTRY( vtable, draw_measureMiniWText, default );
    SET_VTABLE_ENTRY( vtable, draw_drawMiniWindow, default );
} /* InitDrawDefaults */

#endif
