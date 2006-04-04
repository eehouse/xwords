/* -*-mode: C; fill-column: 78; c-basic-offset: 4;-*- */ 
/* 
 * Copyright 2005 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <eikapp.h>

#if defined SERIES_60

# include <w32std.h>
# include <eikenv.h>
# include "xwords_60.mbg"

# define BMNAME( file, bm ) file ## _60 ## bm

#elif defined SERIES_80
# include <eikappui.h>
# include <cknenv.h>
# include <coemain.h>
# include "xwords_80.mbg"

# define BMNAME( file, bm ) file ## _80 ## bm

#else
# error define a series platform!!!!
#endif

#include <stdio.h>

#include "symdraw.h"

#define TRAY_CURSOR_HT 2

enum {
    COLOR_BLACK,
    COLOR_WHITE,
    COLOR_CURSOR,

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

    CFbsBitmap* iRightArrow;
    CFbsBitmap* iDownArrow;
    CFbsBitmap* iStar;
    CFbsBitmap* iTurnIcon;
    CFbsBitmap* iTurnIconMask;
    CFbsBitmap* iRobotIcon;
    CFbsBitmap* iRobotIconMask;

    CONST_60 CFont* iTileFaceFont;
    CONST_60 CFont* iTileValueFont;
    CONST_60 CFont* iBoardFont;
    CONST_60 CFont* iScoreFont;

    XP_U16 iTrayOwner;
    XP_Bool iAllFontsSame;
    TRgb colors[COLOR_NCOLORS];

    MPSLOT
} SymDrawCtxt;

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
drawFocusRect( SymDrawCtxt* sctx, const XP_Rect* rect, XP_Bool hasfocus )
{
    TRect lRect;
    symLocalRect( &lRect, rect );

    lRect.Grow( 2, 2 );   // This is space board.c doesn't know about

    sctx->iGC->SetBrushStyle( CGraphicsContext::ENullBrush );
    sctx->iGC->SetPenStyle( CGraphicsContext::ESolidPen );
    XP_U16 index = SC(XP_U16,(hasfocus? COLOR_CURSOR : COLOR_WHITE));
    sctx->iGC->SetPenColor( sctx->colors[index] );
    TInt i;
    for ( i = 0; i < 2; ++i ) {
        sctx->iGC->DrawRect( lRect );
        lRect.Shrink( 1, 1 );
    }
} // drawFocusRect

static void
getBonusColor( SymDrawCtxt* sctx, XWBonusType bonus, TRgb* rgb )
{
    TInt index;
    if ( bonus == BONUS_NONE ) {
        index = COLOR_WHITE;
    } else {
        index = COLOR_DBL_LTTR + bonus - 1;
    }
    *rgb = sctx->colors[index];
} // getBonusColor

static void
drawBitmap( SymDrawCtxt* sctx, CFbsBitmap* bmp, CFbsBitmap* mask, 
            const TRect* aRect )
{
    TRect rect( *aRect );
    TSize bmpSize = bmp->SizeInPixels();
    if ( bmpSize.iWidth <= rect.Width()
         && bmpSize.iHeight <= rect.Height() ) {

        rect.Move( (rect.Width() - bmpSize.iWidth)  / 2,
                    (rect.Height() - bmpSize.iHeight) / 2 );
        rect.SetSize( bmpSize );

        TRect imgRect( TPoint(0,0), bmpSize );
        sctx->iGC->BitBltMasked( rect.iTl, bmp, imgRect, mask, ETrue );
    } else {
        XP_LOGF( "bitmap too big" );
    }
} /* drawBitmap */

static void
sym_draw_destroyCtxt( DrawCtx* p_dctx )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    XP_LOGF( "freeing draw ctxt" );

    CWsScreenDevice* sdev = sctx->iCoeEnv->ScreenDevice();
    sdev->ReleaseFont( sctx->iTileFaceFont );
    if ( ! sctx->iAllFontsSame ) {
        sdev->ReleaseFont( sctx->iTileValueFont );
        sdev->ReleaseFont( sctx->iBoardFont );
        sdev->ReleaseFont( sctx->iScoreFont );
    }

    delete sctx->iRightArrow;
    delete sctx->iDownArrow;
    delete sctx->iStar;
    delete sctx->iTurnIcon;
    delete sctx->iTurnIconMask;
    delete sctx->iRobotIcon;
    delete sctx->iRobotIconMask;

    XP_ASSERT( sctx );
    XP_ASSERT( sctx->vtable );
    XP_FREE( sctx->mpool, sctx->vtable );
    XP_FREE( sctx->mpool, sctx );
}

static XP_Bool
sym_draw_boardBegin( DrawCtx* p_dctx, const DictionaryCtxt* dict, 
                     const XP_Rect* rect, XP_Bool hasfocus )
{
    XP_LOGF( "sym_draw_boardBegin" );
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    drawFocusRect( sctx, rect, hasfocus );
    return XP_TRUE;
}

static void
sym_draw_boardFinished( DrawCtx* /*p_dctx*/ )
{
}

#ifdef DEBUG
static XP_Bool
sym_draw_vertScrollBoard( DrawCtx* /*p_dctx*/, XP_Rect* /*rect*/, 
                          XP_S16 /*dist*/ )
{
    XP_ASSERT(0);
    return XP_FALSE;
}
#endif

static XP_Bool
sym_draw_trayBegin( DrawCtx* p_dctx, const XP_Rect* rect, 
                    XP_U16 owner, XP_Bool hasfocus )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    sctx->iTrayOwner = owner;

    drawFocusRect( sctx, rect, hasfocus );

    return XP_TRUE;
}

static void
sym_draw_trayFinished( DrawCtx* /*dctx*/ )
{
}

static void
makeRemText( TBuf16<64>* buf, XP_S16 nLeft )
{
    if ( nLeft < 0 ) {
        nLeft = 0;
    }
    buf->Num( nLeft );
    buf->Insert( 0, _L("Tiles left in pool: ") );
} // makeRemText

static void 
sym_draw_measureRemText( DrawCtx* p_dctx, const XP_Rect* /*r*/, 
                         XP_S16 nTilesLeft, 
                         XP_U16* widthP, XP_U16* heightP )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    TBuf16<64> tbuf;
    makeRemText( &tbuf, nTilesLeft );

    CONST_60 CFont* font = sctx->iScoreFont;
	*widthP = (XP_S16)font->TextWidthInPixels( tbuf );
    *heightP = (XP_S16)font->HeightInPixels();
} // sym_draw_measureRemText

static void
sym_draw_drawRemText(DrawCtx* p_dctx, const XP_Rect* rInner, 
                     const XP_Rect* /*rOuter*/, XP_S16 nTilesLeft)
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    TBuf16<64> tbuf;
    makeRemText( &tbuf, nTilesLeft );

    TRect lRect;
    symLocalRect( &lRect, rInner );

    sctx->iGC->SetPenStyle( CGraphicsContext::ESolidPen );
    sctx->iGC->SetPenColor( KRgbBlack );
    sctx->iGC->SetBrushColor( KRgbGray );
    sctx->iGC->SetBrushStyle( CGraphicsContext::ESolidBrush );

    CONST_60 CFont* font = sctx->iScoreFont;
    sctx->iGC->UseFont( font );
    sctx->iGC->DrawText( tbuf, lRect, lRect.Height() - 2 );
    sctx->iGC->DiscardFont();
} // sym_draw_drawRemText

static void
sym_draw_scoreBegin( DrawCtx* p_dctx, const XP_Rect* rect, 
                     XP_U16 /*numPlayers*/, XP_Bool hasfocus )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    drawFocusRect( sctx, rect, hasfocus );
}

static void
sym_draw_measureScoreText( DrawCtx* p_dctx, const XP_Rect* /*r*/, 
                           const DrawScoreInfo* /*dsi*/,
                           XP_U16* widthP, XP_U16* heightP )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    CONST_60 CFont* font = sctx->iScoreFont;
    TInt height = font->HeightInPixels();

    *widthP = 10;               /* whatever; we're only using rOuter */
    *heightP = SC( XP_U16, height );
} /* sym_draw_measureScoreText */

/* We want the elements of a scoreboard to line up in columns.  So draw them
 * one at a time. NAME SCORE TILE_LEFT ?LAST_SCORE?  Might be better to show
 * robot-ness and local/remoteness with colors?  Turn is with an icon.
 */
static void
sym_draw_score_drawPlayer( DrawCtx* p_dctx, const XP_Rect* /*rInner*/, 
                           const XP_Rect* rOuter, const DrawScoreInfo* dsi )
{
    const TInt KTurnIconWidth = 16;
    const TInt KNameColumnWidth = 90;
    const TInt KScoreColumnWidth = 25;
/*     const TInt KTilesLeftColumnWidth = 15; */
    const TInt KLastMoveColumnWidth = 100; /* will be clipped down */

    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    CONST_60 CFont* font = sctx->iScoreFont;

    TRect lRect;
    symLocalRect( &lRect, rOuter );
    TInt rightEdge = lRect.iBr.iX;
    symClearRect( sctx, &lRect, dsi->selected? COLOR_BLACK:COLOR_WHITE );

    TInt fontHeight = font->AscentInPixels();
    TInt baseline = fontHeight + ((lRect.Height() - fontHeight) / 2);
    
    /* The y coords of the rect stay the same, but the x coords change as we
       do each column. The first time, turn-icon column, the left edge is
       already where we want it. */
    lRect.iBr.iX = lRect.iTl.iX + KTurnIconWidth;
    symClearRect( sctx, &lRect, COLOR_WHITE );
    if ( dsi->isTurn ) {
        drawBitmap( sctx, sctx->iTurnIcon, sctx->iTurnIconMask, &lRect );
    } else if ( dsi->isRobot ) {
        drawBitmap( sctx, sctx->iRobotIcon, sctx->iRobotIconMask, &lRect );
    }

    XP_U16 playerNum = dsi->playerNum;
    if ( dsi->selected ) {
        sctx->iGC->SetPenColor( sctx->colors[COLOR_WHITE] );
    } else {
        sctx->iGC->SetPenColor( sctx->colors[playerNum + COLOR_PLAYER1] );
    }
    sctx->iGC->SetBrushStyle( CGraphicsContext::ENullBrush );

    TBuf16<64> tbuf;
    sctx->iGC->UseFont( font );

    /* Draw name */
    lRect.iTl.iX = lRect.iBr.iX + 1; /* add one to get name away from edge */
    lRect.iBr.iX += KNameColumnWidth;
    tbuf.Copy( TBuf8<32>(dsi->name) );
    if ( dsi->isRemote ) {
        tbuf.Insert( 0, _L("[") );
        tbuf.Append( _L("]") );
    }
    sctx->iGC->DrawText( tbuf, lRect, baseline );

    /* Draw score, right-justified */
    lRect.iTl.iX = lRect.iBr.iX;
    lRect.iBr.iX += KScoreColumnWidth;
    tbuf.Num( dsi->score );
    sctx->iGC->DrawText( tbuf, lRect, baseline, CGraphicsContext::ERight );

    /* Draw last move */
    lRect.iTl.iX = lRect.iBr.iX + 6; /* 6 gives it some spacing from
                                        r-justified score */
    lRect.iBr.iX += KLastMoveColumnWidth;
    XP_UCHAR buf[32];
    XP_U16 len = sizeof(buf);
    if ( (*dsi->lsc)( dsi->lscClosure, playerNum, buf, &len ) ) {
        tbuf.Copy( TBuf8<32>(buf) );
        if ( lRect.iBr.iX > rightEdge ) {
            lRect.iBr.iX = rightEdge;
        } 
        sctx->iGC->DrawText( tbuf, lRect, baseline );
    }

    sctx->iGC->DiscardFont();
} /* sym_draw_score_drawPlayer */

static void
sym_draw_score_pendingScore( DrawCtx* p_dctx, const XP_Rect* rect, 
                             XP_S16 score, XP_U16 /*playerNum*/ )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    TRect lRect;
    symLocalRect( &lRect, rect );
    lRect.Shrink( 1, 1 );
    lRect.SetHeight( lRect.Height() - TRAY_CURSOR_HT );
    sctx->iGC->SetPenColor( sctx->colors[COLOR_BLACK] );
    sctx->iGC->SetBrushStyle( CGraphicsContext::ESolidBrush );
    sctx->iGC->SetBrushColor( sctx->colors[COLOR_WHITE]  );

    sctx->iGC->UseFont( sctx->iTileValueFont );

    TInt halfHeight = lRect.Height() / 2;
    lRect.iBr.iY -= halfHeight;
    sctx->iGC->DrawText( _L("Pts:"), lRect, halfHeight-2,
                         CGraphicsContext::ERight );

    TBuf16<8> buf;
    if ( score >= 0 ) {
        buf.Num( score );
    } else {
        buf.Copy( _L("???") );
    }

    lRect.iTl.iY += halfHeight;
    lRect.iBr.iY += halfHeight;
    sctx->iGC->DrawText( buf, lRect, halfHeight-2, CGraphicsContext::ERight );

    sctx->iGC->DiscardFont();
}

static void
sym_draw_scoreFinished( DrawCtx* /*dctx*/ )
{
}

static void
sym_draw_drawTimer( DrawCtx* /*p_dctx*/, const XP_Rect* /*rInner*/, 
                    const XP_Rect* /*rOuter*/,
                    XP_U16 /*player*/, XP_S16 /*secondsLeft*/ )
{
}

static void
textInCell( SymDrawCtxt* sctx, const XP_UCHAR* text, const TRect* lRect,
            TBool highlight )
{
    if ( highlight ) {
        sctx->iGC->SetPenColor( sctx->colors[COLOR_WHITE] );
    } else {
        sctx->iGC->SetPenColor( sctx->colors[COLOR_BLACK] );
    }
    sctx->iGC->SetBrushStyle( CGraphicsContext::ENullBrush );
    CONST_60 CFont* font = sctx->iBoardFont;

    TBuf16<64> tbuf;
    tbuf.Copy( TPtrC8(text) );

    TInt ht = font->AscentInPixels();
    TInt baseOffset = ht + ((lRect->Height() - ht) / 2);

    sctx->iGC->UseFont( font );
    sctx->iGC->DrawText( tbuf, *lRect, baseOffset, CGraphicsContext::ECenter );
    sctx->iGC->DiscardFont();
} /* textInCell */

static XP_Bool
sym_draw_drawCell( DrawCtx* p_dctx, const XP_Rect* rect, 
                   /* at least one of these two will be null */
                   const XP_UCHAR* text, XP_Bitmap bitmap,
                   Tile tile, XP_S16 /*owner*/, /* -1 means don't use */
                   XWBonusType bonus, HintAtts /*hintAtts*/,
                   XP_Bool isBlank, XP_Bool highlight, 
                   XP_Bool isStar )
{
    TRect lRect;
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;

    symLocalRect( &lRect, rect );

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
        drawBitmap( sctx, (CFbsBitmap*)bitmap, (CFbsBitmap*)bitmap, &lRect );
    } else if ( !!text && (*text != '\0') ) {
        TRect r2(lRect);
        textInCell( sctx, text, &r2, highlight );
    } else if ( isStar ) {
        drawBitmap( sctx, sctx->iStar, sctx->iStar, &lRect );
    }

    if ( isBlank ) {
        lRect.Shrink( 1, 0 );
        sctx->iGC->DrawLine( lRect.iTl, TPoint(lRect.iTl.iX, lRect.iBr.iY ) );
        lRect.Shrink( 1, 0 );
        /* draws to right of points; second Shrink is easier than subbing 1
           from x coords */
        sctx->iGC->DrawLine( TPoint(lRect.iBr.iX, lRect.iTl.iY), lRect.iBr );
    }

    return XP_TRUE;
} /* sym_draw_drawCell */

static void
sym_draw_invertCell( DrawCtx* /*p_dctx*/, const XP_Rect* /*rect*/ )
{
}

static void
sym_draw_drawTile( DrawCtx* p_dctx, const XP_Rect* rect, 
                   /* at least 1 of these two will be null*/
                   const XP_UCHAR* text, XP_Bitmap bitmap,
                   XP_S16 val, XP_Bool highlighted )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    XP_U16 index = SC(XP_U16,COLOR_PLAYER1 + sctx->iTrayOwner);

    TRect lRect;
    symLocalRect( &lRect, rect );
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

    // now put the text in the thing
    if ( !!bitmap ) {
        drawBitmap( sctx, (CFbsBitmap*)bitmap, (CFbsBitmap*)bitmap, &lRect );
    } else if ( !!text ) {
        CONST_60 CFont* font = sctx->iTileFaceFont;

        TBuf16<4> txtbuf;
        txtbuf.Copy( TBuf8<4>(text) );
        TInt ht = font->AscentInPixels();
        TPoint point( lRect.iTl.iX, lRect.iTl.iY + ht );

        sctx->iGC->UseFont( font );
        sctx->iGC->DrawText( txtbuf, point );
        sctx->iGC->DiscardFont();
    }

    if ( val >= 0 ) {
        CONST_60 CFont* font = sctx->iTileValueFont;

        TBuf16<5> txtbuf;
        txtbuf.Num( val );
        TInt width = font->TextWidthInPixels( txtbuf );
        TPoint point( lRect.iBr.iX - width, lRect.iBr.iY );

        sctx->iGC->UseFont( font );
        sctx->iGC->DrawText( txtbuf, point );
        sctx->iGC->DiscardFont();
    }
} // sym_draw_drawTile

static void
sym_draw_drawTileBack( DrawCtx* p_dctx, const XP_Rect* rect )
{
    sym_draw_drawTile( p_dctx, rect, (XP_UCHAR*)"?", NULL, -1, XP_FALSE );
}

static void
sym_draw_drawTrayDivider( DrawCtx* p_dctx, const XP_Rect* rect, 
                          XP_Bool /*selected*/ )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    TRect lRect;
    symLocalRect( &lRect, rect );
    symClearRect( sctx, &lRect, COLOR_WHITE );

    lRect.Shrink( 1, 1 );
    lRect.SetHeight( lRect.Height() - TRAY_CURSOR_HT );

    sctx->iGC->SetBrushStyle( CGraphicsContext::ESolidBrush );
    sctx->iGC->SetBrushColor( sctx->colors[COLOR_PLAYER1 + sctx->iTrayOwner] );

    sctx->iGC->DrawRect( lRect );
}

static void
sym_draw_clearRect( DrawCtx* p_dctx, const XP_Rect* rect )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    TRect lRect;
    symLocalRect( &lRect, rect );
    symClearRect( sctx, &lRect, COLOR_WHITE );
}

static void
sym_draw_drawBoardArrow( DrawCtx* p_dctx, const XP_Rect* rect, 
                         XWBonusType bonus, XP_Bool vert,
                         HintAtts /*hintAtts*/ )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;

    TRect lRect;
    symLocalRect( &lRect, rect );

    TRgb rgb;
    getBonusColor( sctx, bonus, &rgb );
    sctx->iGC->SetBrushColor( rgb );
    sctx->iGC->SetBrushStyle( CGraphicsContext::ESolidBrush );

    CFbsBitmap* arrow = vert? sctx->iDownArrow : sctx->iRightArrow;
    drawBitmap( sctx, arrow, arrow, &lRect );
} /* sym_draw_drawBoardArrow */

#ifdef KEY_SUPPORT
static void
sym_draw_drawTrayCursor( DrawCtx* p_dctx, const XP_Rect* rect )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    TRect lRect;
    symLocalRect( &lRect, rect );
    lRect.iTl.iY += lRect.Height() - TRAY_CURSOR_HT;
    symClearRect( sctx, &lRect, COLOR_WHITE );

    sctx->iGC->SetBrushColor( sctx->colors[COLOR_CURSOR] );
    sctx->iGC->SetBrushStyle( CGraphicsContext::ESolidBrush );
    sctx->iGC->SetPenStyle( CGraphicsContext::ENullPen );
    sctx->iGC->DrawRect( lRect );
}

static void
sym_draw_drawBoardCursor( DrawCtx* p_dctx, const XP_Rect* rect )
{
    SymDrawCtxt* sctx = (SymDrawCtxt*)p_dctx;
    TRect lRect;
    symLocalRect( &lRect, rect );


    sctx->iGC->SetPenColor( sctx->colors[COLOR_CURSOR] );
    sctx->iGC->SetPenStyle( CGraphicsContext::ESolidPen );
    sctx->iGC->SetBrushStyle( CGraphicsContext::ENullBrush );
    TInt i;
    for ( i = 0; i <= 1; ++i ) {
        sctx->iGC->DrawRect( lRect );
        lRect.Shrink( 1, 1 );
    }
}
#endif

static XP_UCHAR*
sym_draw_getMiniWText( DrawCtx* /*p_dctx*/, 
                       XWMiniTextType /*textHint*/ )
{
    return (XP_UCHAR*)"";
}

static void
sym_draw_measureMiniWText( DrawCtx* /*p_dctx*/, const XP_UCHAR* /*textP*/, 
                           XP_U16* /*width*/, XP_U16* /*height*/ )
{
}

static void
sym_draw_drawMiniWindow( DrawCtx* /*p_dctx*/, const XP_UCHAR* /*text*/,
                         const XP_Rect* /*rect*/, void** /*closure*/ )
{
}

static void
sym_draw_eraseMiniWindow( DrawCtx* /*p_dctx*/, const XP_Rect* /*rect*/,
                          XP_Bool /*lastTime*/, void** /*closure*/,
                          XP_Bool* /*invalUnder*/ )
{
}

static void
figureFonts( SymDrawCtxt* sctx )
{
    /* Look at FontUtils class for info on fonts. */
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
        if (
#ifdef SYM_ARMI
            tfSupport.iIsScalable &&
#endif
            tfSupport.iMinHeightInTwips < smallSize ) {
            smallIndex = i;
            smallSize = tfSupport.iMinHeightInTwips;
        }
    }

    // Now use the smallest guy
    if ( smallIndex != -1 ) {
        sdev->TypefaceSupport( tfSupport, smallIndex );
        fontName = tfSupport.iTypeface.iName.Des();

        TFontSpec fontSpecBoard( fontName, scaleBoardV );
        sdev->GetNearestFontInPixels( sctx->iBoardFont, fontSpecBoard );

        TInt tileHt = scaleTrayV - TRAY_CURSOR_HT;
        TFontSpec fontSpecTray( fontName, tileHt * 3 / 4 );
        sdev->GetNearestFontInPixels( sctx->iTileFaceFont, fontSpecTray );

        TFontSpec fontSpecVal( fontName, tileHt / 3 );
        sdev->GetNearestFontInPixels( sctx->iTileValueFont, fontSpecVal );

        TFontSpec fontSpecScore( fontName, scaleBoardV );
        sdev->GetNearestFontInPixels( sctx->iScoreFont, fontSpecScore );

    } else {
        sctx->iTileFaceFont = (CFont*)sctx->iCoeEnv->NormalFont();
        sctx->iTileValueFont = sctx->iTileFaceFont;
        sctx->iBoardFont = sctx->iTileFaceFont;
        sctx->iScoreFont = sctx->iTileFaceFont;
        sctx->iAllFontsSame = XP_TRUE;
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
                   CEikonEnv* aEikonEnv, CEikApplication* aApp )
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
#ifdef DEBUG
            /* Shouldn't get called as thing stand now */
            SET_VTABLE_ENTRY( sctx->vtable, draw_vertScrollBoard, sym );
#endif
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
            sctx->colors[COLOR_CURSOR] = TRgb(0x0000FF);
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
            
            TFileName bitmapFile = aApp->BitmapStoreName();

            XP_LOGF( "loading bitmaps0" );
            sctx->iDownArrow = new (ELeave) CFbsBitmap();
            User::LeaveIfError( sctx->iDownArrow->
                                Load(bitmapFile, 
                                     BMNAME(EMbmXwords,Downarrow_80) ) );
            sctx->iRightArrow = new (ELeave) CFbsBitmap();
            User::LeaveIfError( sctx->iRightArrow->
                                Load(bitmapFile, 
                                     BMNAME(EMbmXwords,Rightarrow_80) ) );
            sctx->iStar = new (ELeave) CFbsBitmap();
            User::LeaveIfError( sctx->iStar->
                                Load(bitmapFile, 
                                     BMNAME(EMbmXwords,Star_80) ) );

            sctx->iTurnIcon = new (ELeave) CFbsBitmap();
            User::LeaveIfError( sctx->iTurnIcon->
                                Load(bitmapFile, 
                                     BMNAME(EMbmXwords,Turnicon_80) ) );
            sctx->iTurnIconMask = new (ELeave) CFbsBitmap();
            User::LeaveIfError( sctx->iTurnIconMask->
                                Load(bitmapFile, 
                                     BMNAME(EMbmXwords,Turniconmask_80) ) );

            sctx->iRobotIcon = new (ELeave) CFbsBitmap();
            User::LeaveIfError( sctx->iRobotIcon->
                                Load(bitmapFile, 
                                     BMNAME(EMbmXwords,Robot_80) ) );
            sctx->iRobotIconMask = new (ELeave) CFbsBitmap();
            User::LeaveIfError( sctx->iRobotIconMask->
                                Load(bitmapFile, 
                                     BMNAME(EMbmXwords,Robotmask_80) ) );

            XP_LOGF( "done loading bitmaps" );
        } else {
            XP_FREE( mpool, sctx );
            sctx = NULL;
        }
    }

    XP_LOGF( "leaving sym_drawctxt_make" );

    return (DrawCtx*)sctx;
}
