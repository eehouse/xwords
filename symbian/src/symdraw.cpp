/* -*-mode: C; fill-column: 78; c-basic-offset: 4;-*- */ 
/* 
 * Copyright 2005 by Eric House (fixin@peak.org).  All rights reserved.
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


extern "C" {
#include "comtypes.h"
#include "board.h"
#include "draw.h"
#include "mempool.h"

} // extern "C"

#if defined SERIES_60
# include <w32std.h>
# include <eikenv.h>
#elif defined SERIES_80
# include <cknenv.h>
# include <coemain.h>
#else
# error define a series platform!!!!
#endif

#include <stdio.h>

#include "symdraw.h"

#define TRAY_CURSOR_HT 2

enum {
    COLOR_BLACK,
    COLOR_WHITE,

    COLOR_PLAYER1,
    COLOR_PLAYER2,
    COLOR_PLAYER3,
    COLOR_PLAYER4,

    COLOR_DBL_LTTR,
    COLOR_DBL_WORD,
    COLOR_TRPL_LTTR,
    COLOR_TRPL_WORD,

    COLOR_EMPTY,
    COLOR_TILE,

    COLOR_NCOLORS		/* 12 */
};

#ifdef SERIES_60
# define CONST_60 const
#else
# define CONST_60
#endif

typedef struct SymDrawCtxt {
    /* Must be first */
    DrawCtxVTable* vtable;

    CWindowGc* iGC;

    CEikonEnv* iiEikonEnv;      /* iEikonEnv is a macro in Symbian headers!!! */
    CCoeEnv* iCoeEnv;

    CFbsBitmap* rightArrow;
    CFbsBitmap* downArrow;

    CONST_60 CFont* iTileFaceFont;
    CONST_60 CFont* iTileValueFont;
    CONST_60 CFont* iBoardFont;
    CONST_60 CFont* iScoreFont;

    XP_U16 iTrayOwner;
    XP_Bool iTrayHasFocus;
    TRgb colors[COLOR_NCOLORS];

    MPSLOT
} SymDrawCtxt;

static void
textToDesc( TBuf16<64>* buf, XP_UCHAR* txt )
{
    TBuf8<64> tmpDesc( txt );
    buf->Copy( tmpDesc );
} // textToDesc

static void
symLocalRect( TRect* dest, const XP_Rect* src )
{
    dest->Move( src->left, src->top );
    dest->SetWidth( src->width + 1 );
	dest->SetHeight( src->height + 1 );
} // symLocalRect

static void
symClearRect( SymDrawCtxt* sctx, const TRect* rect, TInt clearTo )
{
    sctx->iGC->SetBrushColor( sctx->colors[clearTo] );
    sctx->iGC->SetBrushStyle( CGraphicsContext::ESolidBrush );
    sctx->iGC->SetPenStyle( CGraphicsContext::ENullPen );
    sctx->iGC->DrawRect( *rect );
} // symClearRect

static void
drawFocusRect( SymDrawCtxt* sctx, XP_Rect* rect, XP_Bool hasfocus )
{
    TRect lRect;
    symLocalRect( &lRect, rect );

    lRect.Grow( 2, 2 );   // This is space board.c doesn't know about
    sctx->iGC->SetClippingRect( lRect );

    sctx->iGC->SetBrushStyle( CGraphicsContext::ENullBrush );
    sctx->iGC->SetPenStyle( CGraphicsContext::EDottedPen );
    XP_U16 index = hasfocus? COLOR_BLACK : COLOR_WHITE;
    sctx->iGC->SetPenColor( sctx->colors[index] );
    sctx->iGC->DrawRect( lRect );
} // drawFocusRect

static void
getBonusColor( SymDrawCtxt* sctx, XWBonusType bonus, TRgb* rgb )
{
    XP_U16 index;
    if ( bonus == BONUS_NONE ) {
        index = COLOR_WHITE;
    } else {
        index = COLOR_DBL_LTTR + bonus - 1;
    }
    *rgb = sctx->colors[index];
} // getBonusColor

static void
sym_draw_destroyCtxt( DrawCtx* p_dctx )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    XP_LOGF( "freeing draw ctxt" );
    XP_ASSERT( sctx );
    XP_ASSERT( sctx->vtable );
    XP_FREE( sctx->mpool, sctx->vtable );
    XP_FREE( sctx->mpool, sctx );
}

static XP_Bool
sym_draw_boardBegin( DrawCtx* p_dctx, XP_Rect* rect, 
                     XP_Bool hasfocus )
{
    XP_LOGF( "sym_draw_boardBegin" );
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    drawFocusRect( sctx, rect, hasfocus );
    return XP_TRUE;
}

static void
sym_draw_boardFinished( DrawCtx* p_dctx )
{
}

static XP_Bool
sym_draw_vertScrollBoard( DrawCtx* p_dctx, XP_Rect* rect, 
                          XP_S16 dist )
{
    XP_ASSERT(0);
    return XP_FALSE;
}

static XP_Bool
sym_draw_trayBegin( DrawCtx* p_dctx, XP_Rect* rect, 
                    XP_U16 owner, XP_Bool hasfocus )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    sctx->iTrayOwner = owner;
    sctx->iTrayHasFocus = hasfocus;

    drawFocusRect( sctx, rect, hasfocus );

    return XP_TRUE;
}

static void
sym_draw_trayFinished( DrawCtx* /*dctx*/ )
{
}

static void
makeRemText( XP_UCHAR* buf, XP_U16 bufLen, XP_S16 nLeft )
{
    if ( nLeft < 0 ) {
        nLeft = 0;
    }
    const char* fmt = "Tiles left in pool: %d";

    sprintf( (char*)buf, fmt, nLeft );
    XP_ASSERT( XP_STRLEN(buf) < bufLen );
} // makeRemText

static void 
sym_draw_measureRemText( DrawCtx* p_dctx, XP_Rect* r, 
                         XP_S16 nTilesLeft, 
                         XP_U16* widthP, XP_U16* heightP )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    XP_UCHAR buf[64];
    makeRemText( buf, sizeof(buf), nTilesLeft );
    TBuf16<64> tbuf;
    textToDesc( &tbuf, buf );

    const CFont* font = sctx->iScoreFont;
	*widthP = font->TextWidthInPixels( tbuf );
    *heightP = font->HeightInPixels();
} // sym_draw_measureRemText

static void
sym_draw_drawRemText(DrawCtx* p_dctx, XP_Rect* rInner, 
                     XP_Rect* rOuter, XP_S16 nTilesLeft)
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    XP_UCHAR buf[64];
    makeRemText( buf, sizeof(buf), nTilesLeft );
    TBuf16<64> tbuf;
    textToDesc( &tbuf, buf );

    TRect lRect;
    symLocalRect( &lRect, rInner );
    symClearRect( sctx, &lRect, COLOR_WHITE );

    TPoint point( lRect.iTl.iX, lRect.iBr.iY );
    sctx->iGC->SetPenColor( sctx->colors[COLOR_BLACK] );

    sctx->iGC->UseFont( sctx->iScoreFont );
    sctx->iGC->DrawText( tbuf, point );
    sctx->iGC->DiscardFont();
} // sym_draw_drawRemText

static void
sym_draw_scoreBegin( DrawCtx* p_dctx, XP_Rect* rect, 
                     XP_U16 numPlayers, XP_Bool hasfocus )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    drawFocusRect( sctx, rect, hasfocus );
}

static void
figureScoreText( XP_UCHAR* buf, XP_U16 bufLen, DrawScoreInfo* dsi )
{
    const char* fmt = "%s  %d (%d) %c %c";

    sprintf( (char*)buf, fmt, 
             dsi->name, dsi->score, dsi->nTilesLeft,
             (dsi->isRemote?'R':'L'),
             (dsi->isRobot?'R':'H') );
    XP_ASSERT( XP_STRLEN(buf) < bufLen );
} // figureScoreText

static void
sym_draw_measureScoreText( DrawCtx* p_dctx, XP_Rect* r, 
                           DrawScoreInfo* dsi,
                           XP_U16* widthP, XP_U16* heightP )
{
    XP_UCHAR buf[64];
    figureScoreText( buf, sizeof(buf), dsi );
    TBuf16<64> tbuf;
    textToDesc( &tbuf, buf );

    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    const CFont* font = sctx->iScoreFont;
	TInt width = font->TextWidthInPixels( tbuf );
    TInt height = font->HeightInPixels();

    *widthP = width;
    *heightP = height;
}

static void
sym_draw_score_drawPlayer( DrawCtx* p_dctx, 
                           XP_S16 playerNum, /* -1: don't use */
                           XP_Rect* rInner, XP_Rect* rOuter, 
                           DrawScoreInfo* dsi )
{
    XP_UCHAR buf[64];
    figureScoreText( buf, sizeof(buf), dsi );
    TBuf16<64> tbuf;
    textToDesc( &tbuf, buf );

    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    CONST_60 CFont* font = sctx->iScoreFont;
    sctx->iGC->UseFont( font );
    TInt descent = font->DescentInPixels();

    TRect lRect;
    symLocalRect( &lRect, rInner );

    TRect lRect1;
    symLocalRect( &lRect1, rOuter );
    symClearRect( sctx, &lRect1, COLOR_WHITE );
    if ( dsi->isTurn ) {
        TPoint point( lRect1.iTl.iX, lRect.iBr.iY - descent );
        sctx->iGC->DrawText( _L("T"), point );
    }

    sctx->iGC->SetClippingRect( lRect );
    symClearRect( sctx, &lRect, dsi->selected? COLOR_BLACK:COLOR_WHITE );

    TPoint point( lRect.iTl.iX, lRect.iBr.iY - descent );
    if ( playerNum >= 0 && !dsi->selected ) {
        sctx->iGC->SetPenColor( sctx->colors[playerNum + COLOR_PLAYER1] );
    } else {
        sctx->iGC->SetPenColor( sctx->colors[COLOR_WHITE] );
    }
    sctx->iGC->DrawText( tbuf, point );
    sctx->iGC->CancelClippingRect();

    sctx->iGC->DiscardFont();
}

static void
sym_draw_score_pendingScore( DrawCtx* p_dctx, XP_Rect* rect, 
                             XP_S16 score, XP_U16 playerNum )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    TRect lRect;
    symLocalRect( &lRect, rect );
    lRect.Shrink( 1, 1 );
    lRect.SetHeight( lRect.Height() - TRAY_CURSOR_HT );
    sctx->iGC->SetClippingRect( lRect );

    sctx->iGC->UseFont( sctx->iTileValueFont );

    XP_UCHAR buf[4];
    if ( score >= 0 ) {
        XP_SNPRINTF( buf, sizeof(buf), (XP_UCHAR*)"%0d", score );
    } else {
        XP_SNPRINTF( buf, sizeof(buf), (XP_UCHAR*)"%s", "???" );
    }
    TBuf16<64> bottomBuf;
    textToDesc( &bottomBuf, buf );
    
    TPoint point( lRect.iTl.iX, lRect.iBr.iY );
    sctx->iGC->DrawText( bottomBuf, point );

    TBuf16<64> topBuf;
    textToDesc( &topBuf, (XP_UCHAR*)"Pts:" );
    point.iY = lRect.Center().iY;
    sctx->iGC->DrawText( topBuf, point );

    sctx->iGC->DiscardFont();
}

static void
sym_draw_scoreFinished( DrawCtx* /*dctx*/ )
{
}

static void
sym_draw_drawTimer( DrawCtx* p_dctx, XP_Rect* rInner, XP_Rect* rOuter,
                    XP_U16 player, XP_S16 secondsLeft )
{
}

static void
textInCell( SymDrawCtxt* sctx, XP_UCHAR* text, TRect* lRect, TBool highlight )
{
    if ( highlight ) {
        sctx->iGC->SetPenColor( sctx->colors[COLOR_WHITE] );
    } else {
        sctx->iGC->SetPenColor( sctx->colors[COLOR_BLACK] );
    }
    sctx->iGC->SetPenStyle( CGraphicsContext::ESolidPen );
    sctx->iGC->SetBrushStyle( CGraphicsContext::ENullBrush );
    CONST_60 CFont* font = sctx->iBoardFont;

    TBuf16<64> tbuf;
    textToDesc( &tbuf, text );
    TInt txtWidth = font->TextWidthInPixels( tbuf );

    lRect->Shrink( 2, 2 );
    TInt width = lRect->Width();

    /* Center the text horizontally */
    TPoint point( lRect->iTl.iX + ((width-txtWidth)/2), lRect->iBr.iY );

    sctx->iGC->UseFont( font );
    sctx->iGC->DrawText( tbuf, point ); 
    sctx->iGC->DiscardFont();
}

static XP_Bool
sym_draw_drawCell( DrawCtx* p_dctx, XP_Rect* rect, 
                   /* at least one of these two will be null */
                   XP_UCHAR* text, XP_Bitmap bitmap,
                   XP_S16 owner, /* -1 means don't use */
                   XWBonusType bonus, HintAtts hintAtts,
                   XP_Bool isBlank, XP_Bool highlight, 
                   XP_Bool isStar)
{
    TRect lRect;
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;

    symLocalRect( &lRect, rect );
    sctx->iGC->SetClippingRect( lRect );

    XP_U16 index = COLOR_TILE;
    TRgb rgb;
    if ( highlight ) { 
        rgb = sctx->colors[COLOR_BLACK];
    } else if ( !!bitmap || (!!text && XP_STRLEN((const char*)text) > 0)) {
        rgb = sctx->colors[COLOR_TILE];
    } else {
        getBonusColor( sctx, bonus, &rgb );
    }

    sctx->iGC->SetPenColor( sctx->colors[COLOR_BLACK] );
    sctx->iGC->SetPenStyle( CGraphicsContext::ESolidPen );
    sctx->iGC->SetBrushColor( rgb );
    sctx->iGC->SetBrushStyle( CGraphicsContext::ESolidBrush );
    sctx->iGC->DrawRect( lRect );

    if ( !!bitmap ) {
        XP_ASSERT( 0 );
    } else if ( !!text ) {
        TRect r2(lRect);
        textInCell( sctx, text, &r2, highlight );
    }

    return XP_TRUE;
}

static void
sym_draw_invertCell( DrawCtx* p_dctx, XP_Rect* rect )
{
}

static void
sym_draw_drawTile( DrawCtx* p_dctx, XP_Rect* rect, 
                   /* at least 1 of these two will be null*/
                   XP_UCHAR* text, XP_Bitmap bitmap,
                   XP_S16 val, XP_Bool highlighted )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    XP_U16 index = COLOR_PLAYER1 + sctx->iTrayOwner;

    TRect lRect;
    symLocalRect( &lRect, rect );
    sctx->iGC->SetClippingRect( lRect );
    symClearRect( sctx, &lRect, COLOR_WHITE );

    lRect.Shrink( 1, 1 );
    lRect.SetHeight( lRect.Height() - TRAY_CURSOR_HT );

    sctx->iGC->SetPenColor( sctx->colors[index] );
    sctx->iGC->SetPenStyle( CGraphicsContext::ESolidPen );
    sctx->iGC->SetBrushColor( sctx->colors[COLOR_TILE] );
    sctx->iGC->SetBrushStyle( CGraphicsContext::ESolidBrush );
    sctx->iGC->DrawRect( lRect );

	lRect.Shrink( 1, 1 );
    if ( highlighted ) {
        sctx->iGC->DrawRect( lRect );
    }

	lRect.Shrink( 2, 2 );
    sctx->iGC->SetClippingRect( lRect );

    // now put the text in the thing
    if ( !!text ) {
        sctx->iGC->UseFont( sctx->iTileFaceFont );

        TBuf8<10> tmpDesc((unsigned char*)text);
        TBuf16<10> txtbuf;
        txtbuf.Copy( tmpDesc );
        TInt ht = sctx->iTileFaceFont->HeightInPixels();
        TPoint point( lRect.iTl.iX, lRect.iTl.iY + ht );
        sctx->iGC->DrawText( txtbuf, point );
        sctx->iGC->DiscardFont();
    }

    if ( val > 0 ) {
        XP_UCHAR buf[4];
        sprintf( (char*)buf, (const char*)"%d", (int)val );

        CONST_60 CFont* font = sctx->iTileValueFont;
        sctx->iGC->UseFont( font );

        TBuf8<5> tmpDesc((unsigned char*)buf);
        TBuf16<5> txtbuf;
        txtbuf.Copy( tmpDesc );

        TInt width = font->TextWidthInPixels( txtbuf );
        TInt ht = font->HeightInPixels();
        TPoint point( lRect.iBr.iX - width, lRect.iBr.iY );
        sctx->iGC->DrawText( txtbuf, point );
        sctx->iGC->DiscardFont();
    }
} // sym_draw_drawTile

static void
sym_draw_drawTileBack( DrawCtx* p_dctx, XP_Rect* rect )
{
    sym_draw_drawTile( p_dctx, rect, (XP_UCHAR*)"?", NULL, -1, XP_FALSE );
}

static void
sym_draw_drawTrayDivider( DrawCtx* p_dctx, XP_Rect* rect, 
                          XP_Bool selected )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    TRect lRect;
    symLocalRect( &lRect, rect );
    sctx->iGC->SetClippingRect( lRect );
    symClearRect( sctx, &lRect, COLOR_WHITE );

    lRect.Shrink( 1, 1 );
    lRect.SetHeight( lRect.Height() - TRAY_CURSOR_HT );

    sctx->iGC->SetBrushStyle( CGraphicsContext::ESolidBrush );
    sctx->iGC->SetBrushColor( sctx->colors[COLOR_PLAYER1 + sctx->iTrayOwner] );

    sctx->iGC->DrawRect( lRect );
}

static void
sym_draw_clearRect( DrawCtx* p_dctx, XP_Rect* rect )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    TRect lRect;
    symLocalRect( &lRect, rect );
    sctx->iGC->SetClippingRect( lRect );
    symClearRect( sctx, &lRect, COLOR_WHITE );
}

static void
sym_draw_drawBoardArrow( DrawCtx* p_dctx, XP_Rect* rect, 
                         XWBonusType bonus, XP_Bool vert,
                         HintAtts hintAtts )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    XP_UCHAR* arrow = (XP_UCHAR*)(vert? "|":"-");

    XP_LOGF( "drawBoardArrow: %s", arrow );

#if 0
    gc.BitBlt( point, arrowBmp );
#else
    TRect lRect;
    symLocalRect( &lRect, rect );
    sctx->iGC->SetClippingRect( lRect );

    textInCell( sctx, arrow, &lRect, EFalse );
#endif
}

#ifdef KEY_SUPPORT
static void
sym_draw_drawTrayCursor( DrawCtx* p_dctx, XP_Rect* rect )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    TRect lRect;
    symLocalRect( &lRect, rect );
    lRect.iTl.iY += lRect.Height() - TRAY_CURSOR_HT;
    symClearRect( sctx, &lRect, COLOR_WHITE );
    sctx->iGC->SetClippingRect( lRect );

    sctx->iGC->SetBrushColor( sctx->colors[COLOR_PLAYER1 + sctx->iTrayOwner] ); 
    sctx->iGC->SetBrushStyle( CGraphicsContext::ESolidBrush );
    sctx->iGC->SetPenStyle( CGraphicsContext::ENullPen );
    sctx->iGC->DrawRect( lRect );
}

static void
sym_draw_drawBoardCursor( DrawCtx* p_dctx, XP_Rect* rect )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    TRect lRect;
    symLocalRect( &lRect, rect );

    lRect.Shrink( 1, 1 );
    sctx->iGC->SetClippingRect( lRect );

    sctx->iGC->SetPenColor( sctx->colors[COLOR_BLACK] );
    sctx->iGC->SetPenStyle( CGraphicsContext::ESolidPen );
    sctx->iGC->SetBrushStyle( CGraphicsContext::ENullBrush );
    sctx->iGC->DrawRect( lRect );
}
#endif

static XP_UCHAR*
sym_draw_getMiniWText( DrawCtx* p_dctx, 
                       XWMiniTextType textHint )
{
    return (XP_UCHAR*)"";
}

static void
sym_draw_measureMiniWText( DrawCtx* p_dctx, XP_UCHAR* textP, 
                           XP_U16* width, XP_U16* height )
{
}

static void
sym_draw_drawMiniWindow( DrawCtx* p_dctx, XP_UCHAR* text,
                         XP_Rect* rect, void** closure )
{
}

static void
sym_draw_eraseMiniWindow( DrawCtx* p_dctx, XP_Rect* rect,
                          XP_Bool lastTime, void** closure,
                          XP_Bool* invalUnder )
{
}

static void
figureFonts( SymDrawCtxt* sctx )
{
#if defined SERIES_80
    XP_LOGF( "figureFonts" );
    TBuf<128> fontName;
    CWsScreenDevice* sdev = sctx->iCoeEnv->ScreenDevice();
    TInt nTypes = sdev->NumTypefaces();
    XP_LOGF( "count = %d", nTypes );

    TTypefaceSupport tfSupport;
    TInt smallIndex = -1;
    TInt smallSize = 0x7FFF;

    for ( TInt i = 0; i < nTypes; ++i ) {
        sdev->TypefaceSupport( tfSupport, i );
        fontName = tfSupport.iTypeface.iName.Des();
#if 0        
        TBuf8<128> tmpb;
        tmpb.Copy( fontName );
        XP_UCHAR buf[128];
        XP_MEMCPY( buf, (void*)(tmpb.Ptr()), tmpb.Length() );
        buf[tmpb.Length()] = '\0';
        XP_LOGF( "got font %s: %d - %d, scalable: %s", buf,
                 tfSupport.iMinHeightInTwips, tfSupport.iMaxHeightInTwips,
                 (tfSupport.iIsScalable?"yes":"no") );
#endif
        if ( tfSupport.iMinHeightInTwips < smallSize ) {
            smallIndex = i;
            smallSize = tfSupport.iMinHeightInTwips;
        }
    }

    // Now use the smallest guy
    if ( smallIndex != -1 ) {
        const TInt twipAdjust = 10;
        sdev->TypefaceSupport( tfSupport, smallIndex );
        fontName = tfSupport.iTypeface.iName.Des();

#if 0
        TBuf8<128> tmpb;
        tmpb.Copy( fontName );
        XP_UCHAR buf[128];
        XP_MEMCPY( buf, (void*)(tmpb.Ptr()), tmpb.Length() );
        buf[tmpb.Length()] = '\0';
        XP_LOGF( "using font %s: %d ", buf,
                 tfSupport.iMinHeightInTwips );
#endif
        TFontSpec fontSpecBoard( fontName, (scaleBoardV) * twipAdjust );
        sdev->GetNearestFontInTwips( sctx->iBoardFont, fontSpecBoard );

        TInt tileHt = scaleTrayV - TRAY_CURSOR_HT;
        TFontSpec fontSpecTray( fontName, (tileHt * 2 / 3) * twipAdjust );
        sdev->GetNearestFontInTwips( sctx->iTileFaceFont, fontSpecTray );

        TFontSpec fontSpecVal( fontName, (tileHt / 3) * twipAdjust );
        sdev->GetNearestFontInTwips( sctx->iTileValueFont, fontSpecVal );

        TFontSpec fontSpecScore( fontName, scaleBoardV * twipAdjust );
        sdev->GetNearestFontInTwips( sctx->iScoreFont, fontSpecScore );

    } else {
        sctx->iTileFaceFont = (CFont*)sctx->iCoeEnv->NormalFont();
        sctx->iTileValueFont = sctx->iTileFaceFont;
        sctx->iBoardFont = sctx->iTileFaceFont;
        sctx->iScoreFont = sctx->iTileFaceFont;
    }

#elif defined SERIES_60
    CCoeEnv* ce = sctx->iCoeEnv;
    sctx->iTileFaceFont = ce->NormalFont();
    sctx->iTileValueFont = sctx->iiEikonEnv->DenseFont();
    sctx->iBoardFont = sctx->iiEikonEnv->LegendFont();
    sctx->iScoreFont = sctx->iiEikonEnv->TitleFont();
#endif

    XP_LOGF( "figureFonts done" );
} // figureFonts


DrawCtx* 
sym_drawctxt_make( MPFORMAL CWindowGc* aGC, CCoeEnv* aCoeEnv, 
                   CEikonEnv* aEikonEnv )
{
    XP_LOGF( "in sym_drawctxt_make" );
    SymDrawCtxt* sctx = (SymDrawCtxt*)XP_MALLOC( mpool, sizeof( *sctx ) );
    
    XP_ASSERT( aGC != NULL );

    if ( sctx != NULL ) {
        XP_MEMSET( sctx, 0, sizeof( *sctx ) );
        MPASSIGN( sctx->mpool, mpool );
        sctx->iGC = aGC;
        sctx->iCoeEnv = aCoeEnv;
        sctx->iiEikonEnv = aEikonEnv;

        sctx->vtable = (DrawCtxVTable*)XP_MALLOC( mpool, sizeof(*sctx->vtable) );
        if ( sctx->vtable != NULL ) {

            SET_VTABLE_ENTRY( sctx->vtable, draw_destroyCtxt, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_boardBegin, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_boardFinished, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_vertScrollBoard, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_trayBegin, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_trayFinished, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_measureRemText, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_drawRemText, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_scoreBegin, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_measureScoreText, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_score_drawPlayer, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_score_pendingScore, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_scoreFinished, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_drawTimer, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_drawCell, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_invertCell, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_drawTile, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_drawTileBack, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_drawTrayDivider, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_clearRect, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_drawBoardArrow, sym );
#ifdef KEY_SUPPORT
            SET_VTABLE_ENTRY( sctx->vtable, draw_drawTrayCursor, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_drawBoardCursor, sym );
#endif
            SET_VTABLE_ENTRY( sctx->vtable, draw_getMiniWText, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_measureMiniWText, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_drawMiniWindow, sym );
            SET_VTABLE_ENTRY( sctx->vtable, draw_eraseMiniWindow, sym );

            sctx->colors[COLOR_BLACK] = KRgbBlack;
            sctx->colors[COLOR_WHITE] = KRgbWhite;
            sctx->colors[COLOR_TILE] = TRgb(0x80ffff); // light yellow
            //sctx->colors[COLOR_TILE] = KRgbYellow;

            sctx->colors[COLOR_PLAYER1] = KRgbBlack;
            sctx->colors[COLOR_PLAYER2] = KRgbDarkRed;
            sctx->colors[COLOR_PLAYER4] = KRgbDarkBlue;
            sctx->colors[COLOR_PLAYER3] = KRgbDarkGreen;

            sctx->colors[COLOR_DBL_LTTR] = KRgbYellow;
            sctx->colors[COLOR_DBL_WORD] = KRgbBlue;
            sctx->colors[COLOR_TRPL_LTTR] = KRgbMagenta;
            sctx->colors[COLOR_TRPL_WORD] = KRgbCyan;

            figureFonts( sctx );
        } else {
            XP_FREE( mpool, sctx );
            sctx = NULL;
        }
    }

    XP_LOGF( "leaving sym_drawctxt_make" );

    return (DrawCtx*)sctx;
}
