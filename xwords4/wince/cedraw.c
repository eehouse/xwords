/* -*-mode: C; fill-column: 78; c-basic-offset: 4;-*- */ 
/* 
 * Copyright 2000-2002 by Eric House (fixin@peak.org).  All rights reserved.
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
/* #include <stdlib.h> */
/* #include <stdio.h> */

#include <Windowsx.h>

#include <stdio.h>              /* for sprintf, etc. */

#include "xptypes.h"
#include "board.h"
#include "draw.h"
#include "mempool.h"

#include "cemain.h"
#include "cedict.h"
#include "cedefines.h"

static void ceClearToBkground( CEDrawCtx* dctx, XP_Rect* rect );
static void ceDrawBitmapInRect( HDC hdc, XP_U32 x, XP_U32 y, 
                                HBITMAP bitmap );


static void
XPRtoRECT( RECT* rt, XP_Rect* xprect )
{
    rt->left = xprect->left;
    rt->top = xprect->top;
    rt->right = rt->left + xprect->width;
    rt->bottom = rt->top + xprect->height;
} /* XPRtoRECT */

static void
makeAndDrawBitmap( CEDrawCtx* dctx, HDC hdc, XP_U32 x, XP_U32 y,
                   COLORREF foreRef, CEBitmapInfo* info )
{
#if 1
    POINT points[2];
    HGDIOBJ forePen;
    XP_U16 nCols, nRows, row, col, rowBytes;
    XP_UCHAR* bits = info->bits;
    HGDIOBJ oldObj;
    forePen = CreatePen( PS_SOLID, 1, foreRef );
    oldObj = SelectObject( hdc, forePen );

    nRows = info->nRows;
    nCols = info->nCols;
    rowBytes = (nCols + 7) / 8;
    while ( (rowBytes % 2) != 0 ) {
        ++rowBytes;
    }

    for ( row = 0; row < nRows; ++row ) {
        for ( col = 0; col < nCols; ++col ) {
            XP_UCHAR byt = bits[col / 8];
            XP_Bool set = (byt & (0x80 >> (col % 8))) != 0;
            if ( set ) {
                points[0].x = x + col;
                points[0].y = y + row;
                points[1].x = x + col + 1;
                points[1].y = y + row + 1;
                Polyline( hdc, points, 2 );
            }
        }
        bits += rowBytes;
    }

    SelectObject( hdc, oldObj );
    DeleteObject( forePen );
#else
    /* I can't get this to work.  Hence the above hack.... */
    HBITMAP bm;
    bm = CreateBitmap( info->nCols, info->nRows, 1, 1,
                       info->bits );
    ceDrawBitmapInRect( hdc, rt->left+2, rt->top+2, bm );

    DeleteObject( bm );
#endif
} /* makeAndDrawBitmap */

static XP_Bool
ce_draw_boardBegin( DrawCtx* p_dctx, XP_Rect* rect, XP_Bool hasfocus )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    XP_Bool canDraw = !!hdc;
    if ( canDraw ) {
        dctx->prevBkColor = GetBkColor( hdc );
    }
    return canDraw;
} /* draw_boardBegin */

static void
ce_draw_boardFinished( DrawCtx* p_dctx )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;

    SetBkColor( hdc, dctx->prevBkColor );
} /* draw_finished */

static XP_U16
getPlayerColor( XP_S16 player )
{
    if ( player < 0 ) {
        return BLACK_COLOR;
    } else {
        return USER_COLOR1 + player;
    }
} /* getPlayerColor */

static XP_Bool
ce_draw_drawCell( DrawCtx* p_dctx, XP_Rect* xprect, 
                  XP_UCHAR* letters, XP_Bitmap* bitmap, 
                  XP_S16 owner, XWBonusType bonus,
                  XP_Bool isBlank, XP_Bool isPending, XP_Bool isStar )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    RECT rt;
    XP_U16 bkIndex;
    XP_UCHAR* cp = NULL;
    COLORREF foreColorRef;

    XP_ASSERT( !!hdc );

    XPRtoRECT( &rt, xprect );
    ++rt.bottom;
    ++rt.right;

    Rectangle( hdc, rt.left, rt.top, rt.right, rt.bottom );
    InsetRect( &rt, 1, 1 );

    if ( (!!letters && letters[0] != '\0' ) || !!bitmap ) {
        if ( isPending ) {
            bkIndex = BLACK_COLOR;
            foreColorRef = dctx->colors[WHITE_COLOR];
        } else {
            foreColorRef = dctx->colors[getPlayerColor(owner)];
            bkIndex = TILEBACK_COLOR;
        }
    } else if ( bonus == BONUS_NONE ) {
        bkIndex = BKG_COLOR;
    } else {
        bkIndex = (bonus - BONUS_DOUBLE_LETTER) + BONUS1_COLOR;
    }

    FillRect( hdc, &rt, dctx->brushes[bkIndex] );

    if ( isBlank ) {
        /* For some reason windoze won't let me paint just the corner pixels
           when certain colors are involved, but it will let me paint the
           whole rect and then erase all but the corners.  File this under
           "don't ask, but it works". */
        RECT tmpRT;
        FillRect( hdc, &rt, dctx->brushes[isPending?WHITE_COLOR:BLACK_COLOR] );

        tmpRT = rt;
        InsetRect( &tmpRT, 0, 2 );
        FillRect( hdc, &tmpRT, dctx->brushes[bkIndex] );

        tmpRT = rt;
        InsetRect( &tmpRT, 1, 0 );
        FillRect( hdc, &tmpRT, dctx->brushes[bkIndex] );
    }

    SetBkColor( hdc, dctx->colors[bkIndex] );

    if ( !!letters && (letters[0] != '\0') ) {
        wchar_t widebuf[4];
        cp = letters;

        XP_MEMSET( widebuf, 0, sizeof(widebuf) );
    
        MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, cp, -1,
                             widebuf, sizeof(widebuf)/sizeof(widebuf[0]) );
	
        SetTextColor( hdc, foreColorRef );
        DrawText( hdc, widebuf, -1, &rt, 
                  DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    } else if ( !!bitmap ) {
        makeAndDrawBitmap( dctx, hdc, rt.left + 2, rt.top + 3, foreColorRef,
                           (CEBitmapInfo*)bitmap );
    } else if ( isStar ) {
        ceDrawBitmapInRect( hdc, rt.left+2, rt.top+1, dctx->origin );
    }

    return XP_TRUE;
} /* ce_draw_drawCell */

static void
ce_draw_invertCell( DrawCtx* p_dctx, XP_Rect* rect )
{
} /* ce_draw_invertCell */

#ifdef DEBUG
static char*
logClipResult( int icrResult )
{
#define caseStr(d)  case d: return #d    
    switch ( icrResult ) {
        caseStr(SIMPLEREGION);
        caseStr(COMPLEXREGION);
        caseStr(NULLREGION);
        caseStr(ERROR);
    }
#undef caseStr
    return "unknown";
} /* logClipResult */
#endif

static void
ce_draw_trayBegin( DrawCtx* p_dctx, XP_Rect* rect, XP_U16 owner,
                   XP_Bool hasfocus )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    dctx->trayOwner = owner;
} /* ce_draw_trayBegin */

static void
ce_draw_trayFinished( DrawCtx* p_dctx )
{
    /*     ce_draw_boardFinished( p_dctx ); */
} /* ce_draw_trayFinished */

static void
drawDrawTileGuts( DrawCtx* p_dctx, XP_Rect* xprect, XP_UCHAR* letters,
                  XP_Bitmap* bitmap, XP_S16 val, XP_Bool highlighted )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    wchar_t widebuf[4];
    RECT rt;

    ceClearToBkground( dctx, xprect );

    SetBkColor( hdc, dctx->colors[TILEBACK_COLOR] );
    SetTextColor( hdc, dctx->colors[getPlayerColor(dctx->trayOwner)] );

    XPRtoRECT( &rt, xprect );

    InsetRect( &rt, 1, 1 );
    Rectangle(hdc, rt.left, rt.top, rt.right, rt.bottom);
    InsetRect( &rt, 1, 1 );

    /* For some reason Rectangle isn't using the background brush to fill, so
       FillRect has to get called after Rectangle.  Need to call InsetRect
       either way to put chars in the right place. */
    if ( highlighted ) {
        Rectangle( hdc, rt.left, rt.top, rt.right, rt.bottom );
        InsetRect( &rt, 1, 1 );
    }
    FillRect( hdc, &rt, dctx->brushes[TILEBACK_COLOR] );
    if ( !highlighted ) {
        InsetRect( &rt, 1, 1 );
    }

    if ( !!letters ) {
        HFONT oldFont = SelectObject( hdc, dctx->trayFont );
        MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, letters, -1,
                             widebuf, sizeof(widebuf)/sizeof(widebuf[0]) );
        DrawText( hdc, widebuf, -1, &rt, DT_SINGLELINE | DT_TOP | DT_LEFT );
        SelectObject( hdc, oldFont );
    } else if ( !!bitmap  ) {
        makeAndDrawBitmap( dctx, hdc, rt.left + 1, rt.top + 4, 
                           dctx->colors[USER_COLOR1+dctx->trayOwner],
                           (CEBitmapInfo*)bitmap );
    }

    if ( val >= 0 ) {
        swprintf( widebuf, L"%d", val );
        DrawText(hdc, widebuf, -1, &rt, DT_SINGLELINE | DT_BOTTOM | DT_RIGHT);	
    }
} /* drawDrawTileGuts */

static void
ce_draw_drawTile( DrawCtx* p_dctx, XP_Rect* xprect, XP_UCHAR* letters,
                  XP_Bitmap* bitmap, XP_S16 val, XP_Bool highlighted )
{
    drawDrawTileGuts( p_dctx, xprect, letters, bitmap, val, highlighted );
} /* ce_draw_drawTile */

static void
ce_draw_drawTileBack( DrawCtx* p_dctx, XP_Rect* xprect )
{
    drawDrawTileGuts( p_dctx, xprect, "?", NULL, -1, XP_FALSE );
} /* ce_draw_drawTileBack */

static void
ce_draw_drawTrayDivider( DrawCtx* p_dctx, XP_Rect* rect, XP_Bool selected )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    RECT rt;

    XPRtoRECT( &rt, rect );
    if ( selected ) {
        Rectangle( hdc, rt.left, rt.top, rt.right, rt.bottom );
    } else {
        FillRect( hdc, &rt, dctx->brushes[BLACK_COLOR] );
    }
} /* ce_draw_drawTrayDivider */

static void
ceClearToBkground( CEDrawCtx* dctx, XP_Rect* rect )
{
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    RECT rt;

    XPRtoRECT( &rt, rect );

    FillRect( hdc, &rt, dctx->brushes[BKG_COLOR] );
} /* ceClearToBkground */

static void 
ce_draw_clearRect( DrawCtx* p_dctx, XP_Rect* rectP )
{
    ceClearToBkground( (CEDrawCtx*)p_dctx, rectP );
} /* ce_draw_clearRect */

static void
ceDrawBitmapInRect( HDC hdc, XP_U32 x, XP_U32 y, HBITMAP bitmap )
{
    BITMAP bmp;
    HDC tmpDC;
    int nBytes;

    tmpDC = CreateCompatibleDC( hdc );
    SelectObject( tmpDC, bitmap );

    nBytes = GetObject( bitmap, sizeof(bmp), &bmp );
    XP_ASSERT( nBytes > 0 );
    if ( nBytes == 0 ) {
        logLastError( "ceDrawBitmapInRect:GetObject" );
    }

    BitBlt( hdc, x, y, bmp.bmWidth, bmp.bmHeight, 
            tmpDC, 0, 0, SRCCOPY );	/* BLACKNESS */

    DeleteDC( tmpDC );
} /* ceDrawBitmapInRect */

static void
ce_draw_drawBoardArrow( DrawCtx* p_dctx, XP_Rect* xprect, 
                        XWBonusType cursorBonus, XP_Bool vertical )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    RECT rt;
    XP_U16 bkIndex;
    HBITMAP cursor;

    XPRtoRECT( &rt, xprect );
    ++rt.bottom;
    ++rt.right;

    Rectangle( hdc, rt.left, rt.top, rt.right, rt.bottom );
    InsetRect( &rt, 1, 1 );

    if ( vertical ) {
        cursor = dctx->downArrow;
    } else {
        cursor = dctx->rightArrow;
    }

    if ( cursorBonus == BONUS_NONE ) {
        bkIndex = BKG_COLOR;
    } else {
        bkIndex = cursorBonus+BONUS1_COLOR;
    }
    FillRect( hdc, &rt, dctx->brushes[bkIndex] );
    SetBkColor( hdc, dctx->colors[bkIndex] );
    SetTextColor( hdc, dctx->colors[BLACK_COLOR] );

    ceDrawBitmapInRect( hdc, rt.left+2, rt.top+1, cursor );

} /* ce_draw_drawBoardArrow */

static void
ce_draw_scoreBegin( DrawCtx* p_dctx, XP_Rect* rect, XP_U16 numPlayers,
                    XP_Bool hasfocus )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    SetBkColor( hdc, dctx->colors[BKG_COLOR] );

    ceClearToBkground( (CEDrawCtx*)p_dctx, rect );
} /* ce_draw_scoreBegin */

static void
formatRemText( HDC hdc, wchar_t* buf, XP_S16 nTilesLeft, SIZE* size )
{
    wchar_t* format = L"Rem:%d";

    if ( nTilesLeft <= 0 ) {
        buf[0] = 0;
        size->cx = size->cy = 0;
    } else {
        swprintf( buf, format, nTilesLeft );
        GetTextExtentPoint32( hdc, buf, wcslen(buf), size );
    }
} /* formatRemText */

static void
ce_draw_measureRemText( DrawCtx* p_dctx, XP_Rect* r, 
                        XP_S16 nTilesLeft, 
                        XP_U16* width, XP_U16* height )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    wchar_t buf[16];
    SIZE size;

    formatRemText( hdc, buf, nTilesLeft, &size );

    *width = (XP_U16)size.cx + 1;   /* 1: don't write up against edge */
    *height = (XP_U16)size.cy;
} /* ce_draw_measureRemText */

static XP_U16
ce_draw_drawRemText( DrawCtx* p_dctx, XP_Rect* rInner, XP_Rect* rOuter, 
                     XP_S16 nTilesLeft )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    wchar_t buf[16];
    RECT rt;
    SIZE size;

    formatRemText( hdc, buf, nTilesLeft, &size );

    XPRtoRECT( &rt, rInner );
    ++rt.left;                  /* 1: don't write up against edge */
    DrawText( hdc, buf, -1, &rt, DT_SINGLELINE | DT_LEFT | DT_VCENTER);

    return (XP_U16)size.cx;
} /* ce_draw_drawRemText */

static void
ceWidthAndText( HDC hdc, wchar_t* buf, DrawScoreInfo* dsi,
                XP_U16* widthP, XP_U16* heightP )
{
    XP_UCHAR borders[] = {'•', '\0'};
    XP_UCHAR tilesLeftTxt[8];
    XP_UCHAR tbuf[10];		/* *9999:7* is 8 chars */
    SIZE size;
    XP_U16 len;

    if ( !dsi->isTurn ) {
        borders[0] = '\0';
    }

    if ( dsi->nTilesLeft >= 0 ) {
        sprintf( tilesLeftTxt, ":%d", dsi->nTilesLeft );
    } else {
        tilesLeftTxt[0] = '\0';
    }
    sprintf( tbuf, "%s%d%s%s", borders, dsi->score, tilesLeftTxt, borders );

    len = MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, tbuf, -1,
                               buf, 10 );

    GetTextExtentPoint32( hdc, buf, len, &size );
    *widthP = (XP_U16)size.cx;
    *heightP = (XP_U16)size.cy;
} /* ceWidthAndText */

static void
ce_draw_measureScoreText( DrawCtx* p_dctx, XP_Rect* r, 
                          DrawScoreInfo* dsi,
                          XP_U16* widthP, XP_U16* heightP )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    wchar_t widebuf[10];
    HFONT newFont;
    HFONT oldFont;

    if ( dsi->selected ) {
        newFont = dctx->selPlayerFont;
    } else {
        newFont = dctx->playerFont;
    }
    oldFont = SelectObject( hdc, newFont );

    ceWidthAndText( hdc, widebuf, dsi, widthP, heightP );

    SelectObject( hdc, oldFont );
} /* ce_draw_measureScoreText */

static void
ce_draw_score_drawPlayer( DrawCtx* p_dctx, 
                          XP_S16 playerNum, /* -1: don't use */
                          XP_Rect* rInner, XP_Rect* rOuter, 
                          DrawScoreInfo* dsi )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    RECT rt;
    XP_U16 width, height;
    wchar_t scoreBuf[20];
    HFONT newFont;
    HFONT oldFont;

    XPRtoRECT( &rt, rInner );
    if ( dsi->selected ) {
        newFont = dctx->selPlayerFont;
    } else {
        newFont = dctx->playerFont;
    }
    oldFont = SelectObject( hdc, newFont );

    SetTextColor( hdc, dctx->colors[getPlayerColor(playerNum)] );

    ceWidthAndText( hdc, scoreBuf, dsi, &width, &height );
    DrawText( hdc, scoreBuf, -1, &rt, 
              DT_SINGLELINE | DT_VCENTER | DT_CENTER );

    SelectObject( hdc, oldFont );
} /* ce_draw_score_drawPlayer */

static void
ce_draw_score_pendingScore( DrawCtx* p_dctx, XP_Rect* rect, XP_S16 score,
                            XP_U16 playerNum )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;

    wchar_t widebuf[5];
    XP_UCHAR buf[5];
    RECT rt;

    SetTextColor( hdc, dctx->colors[BLACK_COLOR] );
    SetBkColor( hdc, dctx->colors[BKG_COLOR] );

    XPRtoRECT( &rt, rect );
    FillRect( hdc, &rt, dctx->brushes[BKG_COLOR] );

    if ( score < 0 ) {
        buf[0] = '?';
        buf[1] = '?';
        buf[2] = '\0';
    } else {
        sprintf( buf, "%d", score );
    }

    MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, buf, -1,
                         widebuf, sizeof(widebuf)/sizeof(widebuf[0]) );
    DrawText(hdc, widebuf, -1, &rt, DT_SINGLELINE | DT_BOTTOM | DT_CENTER);	
    DrawText(hdc, L"Pts", -1, &rt, DT_SINGLELINE | DT_TOP | DT_CENTER);	

} /* ce_draw_score_pendingScore */

static void
ce_draw_scoreFinished( DrawCtx* p_dctx )
{
    /*     ce_draw_boardFinished( p_dctx ); */
} /* ce_draw_scoreFinished */

static void
ce_draw_drawTimer( DrawCtx* p_dctx, XP_Rect* rInner, XP_Rect* rOuter,
                   XP_U16 player, XP_S16 secondsLeft )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    wchar_t widebuf[10];
    XP_U16 mins, secs;
    RECT rt;
    PAINTSTRUCT ps;
    XP_Bool isNegative;

    XPRtoRECT( &rt, rInner );

    isNegative = secondsLeft < 0;
    if ( isNegative ) {
        secondsLeft *= -1;
    }

    mins = secondsLeft / 60;
    secs = secondsLeft % 60;

    swprintf( widebuf, L"%s%.1d:%.2d", isNegative? L"-": L"", mins, secs );
   
    if ( !globals->hdc ) {
        InvalidateRect( dctx->mainWin, &rt, FALSE );
        hdc = BeginPaint( dctx->mainWin, &ps );
    }

    SetTextColor( hdc, dctx->colors[getPlayerColor(player)] );
    ceClearToBkground( dctx, rInner );
    DrawText( hdc, widebuf, -1, &rt, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);	

    if ( !globals->hdc ) {
        EndPaint( dctx->mainWin, &ps );
    }
} /* ce_draw_drawTimer */

static XP_UCHAR*
ce_draw_getMiniWText( DrawCtx* p_dctx, XWMiniTextType whichText )
{
    XP_UCHAR* str;

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
        str = "Trading tiles; tap 'D' when done"; break;
    default:
        XP_ASSERT( XP_FALSE );
    }

    return str;
} /* ce_draw_getMiniWText */

static void
ce_draw_measureMiniWText( DrawCtx* p_dctx, XP_UCHAR* str, 
                          XP_U16* widthP, XP_U16* heightP )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = GetDC(dctx->mainWin);//globals->hdc;
    SIZE size;
    wchar_t widebuf[40];

    MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, str, -1,
                         widebuf, sizeof(widebuf)/sizeof(widebuf[0]) );
    
    GetTextExtentPoint32( hdc, widebuf, wcslen(widebuf), &size );

    *widthP = size.cx + 12;
    *heightP = size.cy + 4;
} /* ce_draw_measureMiniWText */

static void
ce_draw_drawMiniWindow( DrawCtx* p_dctx, XP_UCHAR* text, XP_Rect* rect,
                        void** closureP )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc;
    RECT rt;
    PAINTSTRUCT ps;
    wchar_t widebuf[40];

    XPRtoRECT( &rt, rect );

    if ( !!globals->hdc ) {
        hdc = globals->hdc;
    } else {
        InvalidateRect( dctx->mainWin, &rt, FALSE );
        hdc = BeginPaint( dctx->mainWin, &ps );
    }

    ceClearToBkground( (CEDrawCtx*)p_dctx, rect );

    Rectangle( hdc, rt.left, rt.top, rt.right, rt.bottom );
    InsetRect( &rt, 1, 1 );
    Rectangle( hdc, rt.left, rt.top, rt.right, rt.bottom );

    XP_MEMSET( widebuf, 0, sizeof(widebuf) );
    MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, text, -1,
                         widebuf, sizeof(widebuf)/sizeof(widebuf[0]) );
    DrawText(hdc, widebuf, -1, &rt, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    if ( !globals->hdc ) {
        EndPaint( dctx->mainWin, &ps );
    }
} /* ce_draw_drawMiniWindow */

static void
ce_draw_eraseMiniWindow( DrawCtx* p_dctx, XP_Rect* rect, XP_Bool lastTime,
                         void** closure, XP_Bool* invalUnder )
{
    *invalUnder = XP_TRUE;
} /* ce_draw_eraseMiniWindow */

static void
ce_draw_destroyCtxt( DrawCtx* p_dctx )
{
    XP_U16 i;
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;

    for ( i = 0; i < 5; ++i ) {
        DeleteObject( dctx->brushes[i] );
    }

    DeleteObject( dctx->trayFont );
    DeleteObject( dctx->playerFont );
    DeleteObject( dctx->selPlayerFont );

    DeleteObject( dctx->rightArrow );
    DeleteObject( dctx->downArrow );
    DeleteObject( dctx->origin );

    XP_FREE( dctx->mpool, p_dctx->vtable );
    XP_FREE( dctx->mpool, dctx );
} /* ce_draw_destroyCtxt */

static void
draw_doNothing( DrawCtx* dctx, ... )
{
} /* draw_doNothing */

static void
ceFontsSetup( CEAppGlobals* globals, CEDrawCtx* dctx )
{
    LOGFONT font;

    XP_MEMSET( &font, 0, sizeof(font) );
    font.lfHeight = (CE_TRAY_SCALEV*2)/3;
    font.lfWeight = 600;     /* FW_DEMIBOLD */
    dctx->trayFont = CreateFontIndirect( &font );

    font.lfWeight = FW_LIGHT;
    font.lfHeight = CE_SCORE_HEIGHT - 2;
    dctx->playerFont = CreateFontIndirect( &font );

    font.lfWeight = FW_BOLD;
    font.lfHeight = CE_SCORE_HEIGHT;
    dctx->selPlayerFont = CreateFontIndirect( &font );

} /* ceFontsSetup */

static void
loadColors( XP_U16* ptr, COLORREF* colors )
{
    XP_U16 i;
    for ( i = 0; i < NUM_COLORS; ++i ) {
        XP_U8 r = (XP_U8)*ptr++;
        XP_U8 g = (XP_U8)*ptr++;
        XP_U8 b = (XP_U8)*ptr++;
        *colors++ = RGB( r, g, b );
    }
} /* loadColors */

DrawCtx* 
ce_drawctxt_make( MPFORMAL HWND mainWin, CEAppGlobals* globals )
{
    HRSRC rsrcH;
    HGLOBAL globH;
    CEDrawCtx* dctx = (CEDrawCtx*)XP_MALLOC( mpool,
                                             sizeof(*dctx) );
    XP_U16 i;

    dctx->vtable = (DrawCtxVTable*)XP_MALLOC( mpool, sizeof(*((dctx)->vtable)));

    for ( i = 0; i < sizeof(*dctx->vtable)/4; ++i ) {
        ((void**)(dctx->vtable))[i] = draw_doNothing;
    }

    SET_VTABLE_ENTRY( dctx->vtable, draw_destroyCtxt, ce );

    SET_VTABLE_ENTRY( dctx->vtable, draw_boardBegin, ce );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawCell, ce );
    SET_VTABLE_ENTRY( dctx->vtable, draw_invertCell, ce );

    SET_VTABLE_ENTRY( dctx->vtable, draw_boardFinished, ce );

    SET_VTABLE_ENTRY( dctx->vtable, draw_trayBegin, ce );
    SET_VTABLE_ENTRY( dctx->vtable, draw_trayFinished, ce );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTile, ce );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTileBack, ce );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTrayDivider, ce );

    SET_VTABLE_ENTRY( dctx->vtable, draw_clearRect, ce );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawBoardArrow, ce );

    SET_VTABLE_ENTRY( dctx->vtable, draw_scoreBegin, ce );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureRemText, ce );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawRemText, ce ); 
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureScoreText, ce );
    SET_VTABLE_ENTRY( dctx->vtable, draw_score_drawPlayer, ce );
    SET_VTABLE_ENTRY( dctx->vtable, draw_score_pendingScore, ce );
    SET_VTABLE_ENTRY( dctx->vtable, draw_scoreFinished, ce );

    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTimer, ce );

    SET_VTABLE_ENTRY( dctx->vtable, draw_getMiniWText, ce );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureMiniWText, ce );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawMiniWindow, ce );
    SET_VTABLE_ENTRY( dctx->vtable, draw_eraseMiniWindow, ce );

    dctx->mainWin = mainWin;
    dctx->globals = globals;

    /* load the colors from resource */
    rsrcH = FindResource( globals->hInst, MAKEINTRESOURCE(ID_COLORS_RES),
                          TEXT("CLRS") );
    globH = LoadResource( globals->hInst, rsrcH );
    loadColors( globH, dctx->colors );
    DeleteObject( globH );

    for ( i = 0; i < NUM_COLORS; ++i ) {
        dctx->brushes[i] = CreateSolidBrush( dctx->colors[i] );
    }

    ceFontsSetup( globals, dctx );

    dctx->rightArrow = LoadBitmap( globals->hInst, 
                                   MAKEINTRESOURCE(IDB_RIGHTARROW) );
    XP_DEBUGF( "loaded bitmap: 0x%lx", (unsigned long)dctx->rightArrow );
    dctx->downArrow = LoadBitmap( globals->hInst, 
                                  MAKEINTRESOURCE(IDB_DOWNARROW) );

    dctx->origin = LoadBitmap( globals->hInst, 
                               MAKEINTRESOURCE(IDB_ORIGIN) );

    return (DrawCtx*)dctx;
} /* ce_drawctxt_make */
