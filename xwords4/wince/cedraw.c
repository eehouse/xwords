/* -*- fill-column: 77; compile-command: "make TARGET_OS=wince DEBUG=TRUE"; -*- */
/* 
 * Copyright 2000-2008 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <windowsx.h>

#include <stdio.h>              /* for sprintf, etc. */

#include "xptypes.h"
#include "board.h"
#include "draw.h"
#include "mempool.h"

#include "cemain.h"
#include "cedict.h"
#include "cedefines.h"
#include "debhacks.h"

#ifndef DRAW_FUNC_NAME
#define DRAW_FUNC_NAME(nam) ce_draw_ ## nam
#endif

#define CE_MINI_V_PADDING 6
#define CE_MINIW_PADDING 0
#define CE_SCORE_PADDING -3
#define CE_REM_PADDING -1
#define CE_TIMER_PADDING -2
#define IS_TURN_VPAD 2

#define DRAW_FOCUS_FRAME 1
#ifdef DRAW_FOCUS_FRAME
# define CE_FOCUS_BORDER_WIDTH 6
# define TREAT_AS_CURSOR(d,f) ((((f) & CELL_ISCURSOR) != 0) && !(d)->topFocus )
#else
# define TREAT_AS_CURSOR(d,f) (((f) & CELL_ISCURSOR) != 0)
#endif



static void ceClearToBkground( CEDrawCtx* dctx, const XP_Rect* rect );
static void ceDrawBitmapInRect( HDC hdc, const RECT* r, HBITMAP bitmap );


static void
XPRtoRECT( RECT* rt, const XP_Rect* xprect )
{
    rt->left = xprect->left;
    rt->top = xprect->top;
    rt->right = rt->left + xprect->width;
    rt->bottom = rt->top + xprect->height;
} /* XPRtoRECT */

static HGDIOBJ
ceGetPen( CEDrawCtx* dctx, XP_U16 colorIndx, XP_U16 width )
{
    PenColorPair* pair = &dctx->pens[colorIndx];
    HGDIOBJ pen = pair->pen;
    COLORREF ref = dctx->globals->appPrefs.colors[colorIndx];

    /* Make sure cached value is still good */
    if ( !!pen && (ref != pair->ref) ) {
        DeleteObject( pen );
        pen = NULL;
    }

    if ( !pen ) {
        pen = CreatePen( PS_SOLID, width, ref );
        pair->pen = pen;
        pair->ref = ref;
    }
    return pen;
}

static void
makeAndDrawBitmap( CEDrawCtx* dctx, HDC hdc, const RECT* bnds, XP_Bool center,
                   XP_U16 colorIndx, CEBitmapInfo* info )
{
#if 1
    HGDIOBJ forePen = ceGetPen( dctx, colorIndx, 1 );
    POINT points[2];
    XP_U16 nCols, nRows, row, col, rowBytes;
    XP_S32 x, y;
    XP_U8* bits = info->bits;
    HGDIOBJ oldObj;
    oldObj = SelectObject( hdc, forePen );

    nRows = info->nRows;
    nCols = info->nCols;
    rowBytes = (nCols + 7) / 8;
    while ( (rowBytes % 2) != 0 ) {
        ++rowBytes;
    }

    x = bnds->left;
    y = bnds->top;
    if ( center ) {
        /* the + 1 is to round up */
        x += ((bnds->right - bnds->left) - nCols + 1) / 2;
        y += ((bnds->bottom - bnds->top) - nRows + 1) / 2;
    }

    for ( row = 0; row < nRows; ++row ) {
        for ( col = 0; col < nCols; ++col ) {
            XP_U8 byt = bits[col / 8];
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
#else
    /* I can't get this to work.  Hence the above hack.... */
    HBITMAP bm;
    bm = CreateBitmap( info->nCols, info->nRows, 1, 1,
                       info->bits );
    ceDrawBitmapInRect( hdc, rt->left+2, rt->top+2, bm );

    DeleteObject( bm );
#endif
} /* makeAndDrawBitmap */

/* CE doesn't have FrameRect, so we'll roll our own */
static int
ceDrawFocusRect( HDC hdc, CEDrawCtx* dctx, LPCRECT rect )
{
    RECT lr = *rect;
    HGDIOBJ oldObj;
    HGDIOBJ pen = ceGetPen( dctx, CE_FOCUS_COLOR, CE_FOCUS_BORDER_WIDTH );

    InsetRect( &lr, CE_FOCUS_BORDER_WIDTH/2, CE_FOCUS_BORDER_WIDTH/2 );

    oldObj = SelectObject( hdc, pen );

    MoveToEx( hdc, lr.left, lr.top, NULL );
    LineTo( hdc, lr.right, lr.top );
    LineTo( hdc, lr.right, lr.bottom );
    LineTo( hdc, lr.left, lr.bottom );
    LineTo( hdc, lr.left, lr.top );

    SelectObject( hdc, oldObj );
    return 0;
}

static void
ceClipToRect( /* CEDrawCtx* dctx,  */HDC hdc, const RECT* rt )
{
    /*
NULLREGION 	Region is empty.
SIMPLEREGION 	Region is a single rectangle.
COMPLEXREGION 	Region is more than one rectangle.
ERROR
    */
#if 0
    /* Docs suggest I should be able to reuse the region but it doesn't
       work.  At least under WINE... */
    HRGN clipRgn = dctx->clipRgn;
    int ret;
    if ( !clipRgn ) {
        clipRgn = CreateRectRgn( rt->left, rt->top, rt->right, rt->bottom );
        dctx->clipRgn = clipRgn;
    } else {
        (void)SetRectRgn( clipRgn, rt->left, rt->top, rt->right, rt->bottom );
    }
    (void)SelectClipRgn( hdc, clipRgn ); /* returns SIMPLEREGION */
#else
    HRGN clipRgn = CreateRectRgn( rt->left, rt->top, rt->right, rt->bottom );
    SelectClipRgn( hdc, clipRgn );
    DeleteObject( clipRgn );
#endif
} /* ceClipToRect */

static HFONT
ceGetSizedFont( CEDrawCtx* dctx, XP_U16 height, RFIndex index )
{
    if ( dctx->setFontHt[index] != height ) {
        HFONT font;
        LOGFONT fontInfo;

        XP_MEMSET( &fontInfo, 0, sizeof(fontInfo) );
        fontInfo.lfHeight = height;
        font = CreateFontIndirect( &fontInfo );

        if ( !!dctx->setFont[index] ) {
            DeleteObject( dctx->setFont[index] );
        }

        dctx->setFont[index] = font;
        dctx->setFontHt[index] = height;
    }
    return dctx->setFont[index];
}

#if 0
/* I'm trying to measure individual chars, but GetGlyphOutline and
   GetTextExtentExPointW both return the full font height for any char, even
   '.'.  GetGlyphOutline fails altogether on XP.  Work in progress...
 */
static void
logAllChars( HDC hdc, wchar_t* widebuf, XP_U16 len )
{
    wchar_t tmp[2] = { 0, 0 };
    int i;

    for ( i = 0; i < len; ++i ) {
        GLYPHMETRICS gm;
        DWORD dw;
        XP_MEMSET( &gm, 0, sizeof(gm) );
        dw = GetGlyphOutline( hdc,
                              widebuf[i], GGO_METRICS,
                              &gm,
                              0,      // size of data buffer
                              NULL,    // data buffer
                              NULL );
/*         GetTextExtentPoint32( hdc, &widebuf[i], 1, &size ); */
        tmp[0] = widebuf[i];
        XP_LOGW( "letter: ", tmp );
        XP_LOGF( "width: %d; height: %d (pt: %ld,%ld)", gm.gmBlackBoxX, 
                 gm.gmBlackBoxY, gm.gmptGlyphOrigin.x, gm.gmptGlyphOrigin.y );
    }
}
#endif

static void
ceMeasureText( CEDrawCtx* dctx, const XP_UCHAR* str, XP_S16 padding,
               XP_U16* widthP, XP_U16* heightP )
{
    HDC hdc = GetDC(dctx->mainWin);//globals->hdc;
    XP_U16 height = 0;
    XP_U16 maxWidth = 0;

    for ( ; ; ) {
        wchar_t widebuf[32];
        XP_UCHAR* nextStr = strstr( str, XP_CR );
        XP_U16 len = nextStr==NULL? strlen(str): nextStr - str;
        SIZE size;

        XP_ASSERT( nextStr != str );

        MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, str, len,
                             widebuf, VSIZE(widebuf) );
        GetTextExtentPoint32( hdc, widebuf, len, &size );

/*         logAllChars( hdc, widebuf, len ); */

        maxWidth = (XP_U16)XP_MAX( maxWidth, size.cx );
        height += size.cy;
        dctx->miniLineHt = (XP_U16)size.cy;

        if ( nextStr == NULL ) {
            break;
        }
        height += padding;
        str = nextStr + XP_STRLEN(XP_CR);	/* skip '\n' */
    }

    *widthP = maxWidth + 8;
    *heightP = height;
} /* ceMeasureText */

static void
drawTextLines( CEDrawCtx* dctx, HDC hdc, const XP_UCHAR* text, XP_S16 padding,
               const RECT* rp, int flags )
{
    wchar_t widebuf[128];
    RECT textRt = *rp;

    for ( ; ; ) { /* draw up to the '\n' each time */
        XP_UCHAR* nextStr = strstr( text, XP_CR );
        XP_U16 len;

        if ( nextStr == NULL ) {
            len = XP_STRLEN(text);
        } else {
            len = nextStr - text;
        }

        MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, text, len,
                             widebuf, VSIZE(widebuf) );

        textRt.bottom = textRt.top + dctx->miniLineHt;

        DrawText( hdc, widebuf, len, &textRt, flags );

        if ( nextStr == NULL ) {
            break;
        }
        textRt.top = textRt.bottom + padding;
        text = nextStr + XP_STRLEN(XP_CR);
    }
} /* drawTextLines */

DLSTATIC XP_Bool
DRAW_FUNC_NAME(boardBegin)( DrawCtx* p_dctx, 
                            const DictionaryCtxt* XP_UNUSED(dict), 
                            const XP_Rect* XP_UNUSED(rect), 
                            DrawFocusState dfs )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    XP_Bool canDraw = !!hdc;
    if ( canDraw ) {
        dctx->prevBkColor = GetBkColor( hdc );
        dctx->topFocus = dfs == DFS_TOP;
    }
    return canDraw;
} /* draw_boardBegin */

DLSTATIC void
DRAW_FUNC_NAME(objFinished)( DrawCtx* p_dctx, BoardObjectType typ, 
                             const XP_Rect* rect, DrawFocusState dfs )
{
#ifdef DRAW_FOCUS_FRAME
    if ( (dfs == DFS_TOP) && (typ == OBJ_BOARD || typ == OBJ_TRAY) ) {
        CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
        CEAppGlobals* globals = dctx->globals;
        HDC hdc = globals->hdc;
        RECT rt;

        XPRtoRECT( &rt, rect );
        ceClipToRect( hdc, &rt );
        
        ceDrawFocusRect( hdc, dctx, &rt );
    }
#endif
}

static XP_U16
getPlayerColor( XP_S16 player )
{
    if ( player < 0 ) {
        return CE_BLACK_COLOR;
    } else {
        return CE_USER_COLOR1 + player;
    }
} /* getPlayerColor */

static void
ceDrawLine( HDC hdc, XP_S32 x1, XP_S32 y1, XP_S32 x2, XP_S32 y2 )
{
    MoveToEx( hdc, x1, y1, NULL );
    LineTo( hdc, x2, y2);
} /* ceDrawLine */

static void
ceDrawHintBorders( HDC hdc, const XP_Rect* xprect, HintAtts hintAtts )
{
    if ( hintAtts != HINT_BORDER_NONE && hintAtts != HINT_BORDER_CENTER ) {
        RECT rt;
        XPRtoRECT( &rt, xprect );
        InsetRect( &rt, 1, 1 );

        if ( (hintAtts & HINT_BORDER_LEFT) != 0 ) {
            ceDrawLine( hdc, rt.left, rt.top, rt.left, rt.bottom+1 );
        }
        if ( (hintAtts & HINT_BORDER_RIGHT) != 0 ) {
            ceDrawLine( hdc, rt.right, rt.top, rt.right, rt.bottom+1 );
        }
        if ( (hintAtts & HINT_BORDER_TOP) != 0 ) {
            ceDrawLine( hdc, rt.left, rt.top, rt.right+1, rt.top );
        }
        if ( (hintAtts & HINT_BORDER_BOTTOM) != 0 ) {
            ceDrawLine( hdc, rt.left, rt.bottom, rt.right+1, rt.bottom );
        }
    }
} /* ceDrawHintBorders */

DLSTATIC XP_Bool
DRAW_FUNC_NAME(drawCell)( DrawCtx* p_dctx, const XP_Rect* xprect, 
                          const XP_UCHAR* letters, const XP_Bitmap bitmap, 
                          Tile XP_UNUSED(tile), XP_S16 owner, 
                          XWBonusType bonus, HintAtts hintAtts,
                          CellFlags flags )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    RECT rt;
    RECT textRect;
    XP_U16 bkIndex;
    const XP_UCHAR* cp = NULL;
    XP_U16 foreColorIndx;
    XP_Bool isPending = (flags & CELL_HIGHLIGHT) != 0;
    XP_Bool isFocussed = TREAT_AS_CURSOR( dctx, flags );
    XP_Bool isDragSrc = (flags & CELL_DRAGSRC) != 0;
    HFONT oldFont;

    XP_ASSERT( !!hdc );

    oldFont = SelectObject( hdc, ceGetSizedFont( dctx, xprect->height,
                                                 RFONTS_CELL) );

    XPRtoRECT( &rt, xprect );
    ++rt.bottom;
    ++rt.right;
    ceClipToRect( hdc, &rt );

    Rectangle( hdc, rt.left, rt.top, rt.right, rt.bottom );
    textRect = rt;
    InsetRect( &textRect, 1, 1 );

    InsetRect( &rt, 1, 1 );
    ceClipToRect( hdc, &rt );

    /* always init to silence compiler warning */
    foreColorIndx = getPlayerColor(owner);

    if ( !isDragSrc && ((!!letters && letters[0] != '\0' ) || !!bitmap )) {
        if ( isPending ) {
            bkIndex = CE_BLACK_COLOR;
            foreColorIndx = CE_WHITE_COLOR;
        } else {
            bkIndex = CE_TILEBACK_COLOR;
            // foreColorIndx already has right val
        }
    } else if ( bonus == BONUS_NONE ) {
        bkIndex = CE_BKG_COLOR;
    } else {
        bkIndex = (bonus - BONUS_DOUBLE_LETTER) + CE_BONUS1_COLOR;
    }

    if ( isFocussed ) {
        bkIndex = CE_FOCUS_COLOR;
    }

    FillRect( hdc, &rt, dctx->brushes[bkIndex] );

    if ( (flags&CELL_ISBLANK) != 0 ) {
        /* For some reason windoze won't let me paint just the corner pixels
           when certain colors are involved, but it will let me paint the
           whole rect and then erase all but the corners.  File this under
           "don't ask, but it works". */
        RECT tmpRT;
        FillRect( hdc, &rt, 
                  dctx->brushes[isPending?CE_WHITE_COLOR:CE_BLACK_COLOR] );
        tmpRT = rt;
        InsetRect( &tmpRT, 0, 2 );
        FillRect( hdc, &tmpRT, dctx->brushes[bkIndex] );

        tmpRT = rt;
        InsetRect( &tmpRT, 1, 0 );
        FillRect( hdc, &tmpRT, dctx->brushes[bkIndex] );
    }

    SetBkColor( hdc, dctx->globals->appPrefs.colors[bkIndex] );

    if ( !isDragSrc && !!letters && (letters[0] != '\0') ) {
        wchar_t widebuf[4];
        cp = letters;

        XP_MEMSET( widebuf, 0, sizeof(widebuf) );
    
        MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, cp, -1,
                             widebuf, VSIZE(widebuf) );
	
        SetTextColor( hdc, dctx->globals->appPrefs.colors[foreColorIndx] );
#ifdef TARGET_OS_WIN32
        MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, letters, -1,
                             widebuf, VSIZE(widebuf) );
#endif
        DrawText( hdc, widebuf, -1, &textRect, 
                  DT_SINGLELINE | DT_VCENTER | DT_CENTER);

    } else if ( !isDragSrc && !!bitmap ) {
        makeAndDrawBitmap( dctx, hdc, &rt, XP_TRUE,
                           foreColorIndx, (CEBitmapInfo*)bitmap );
    } else if ( (flags&CELL_ISSTAR) != 0 ) {
        ceDrawBitmapInRect( hdc, &textRect, dctx->origin );
    }

    ceDrawHintBorders( hdc, xprect, hintAtts );

    SelectObject( hdc, oldFont );

    return XP_TRUE;
} /* ce_draw_drawCell */

DLSTATIC void
DRAW_FUNC_NAME(invertCell)( DrawCtx* XP_UNUSED(p_dctx), 
                            const XP_Rect* XP_UNUSED(rect) )
{
} /* ce_draw_invertCell */

#ifdef DEBUG
#if 0
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
#endif

DLSTATIC XP_Bool
DRAW_FUNC_NAME(trayBegin)( DrawCtx* p_dctx, const XP_Rect* XP_UNUSED(rect),
                           XP_U16 owner, DrawFocusState dfs )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    XP_Bool canDraw = !!hdc;
    if ( canDraw ) {
        dctx->trayOwner = owner;
        dctx->topFocus = dfs == DFS_TOP;
    }
    return canDraw;
} /* ce_draw_trayBegin */

static void
drawDrawTileGuts( DrawCtx* p_dctx, const XP_Rect* xprect, 
                  const XP_UCHAR* letters,
                  XP_Bitmap bitmap, XP_S16 val, CellFlags flags )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    wchar_t widebuf[4];
    RECT rt;
    XP_U16 index;
    XP_Bool highlighted;
    XP_Bool isFocussed = TREAT_AS_CURSOR(dctx,flags);
    XP_Bool isEmpty = (flags & CELL_ISEMPTY) != 0;

    XPRtoRECT( &rt, xprect );
    ceClipToRect( hdc, &rt );
    ceClearToBkground( dctx, xprect );

    if ( !isEmpty || isFocussed ) {
        XP_U16 backIndex = isFocussed? CE_FOCUS_COLOR : CE_TILEBACK_COLOR;

        SetBkColor( hdc, dctx->globals->appPrefs.colors[backIndex] );

        InsetRect( &rt, 1, 1 );
        Rectangle(hdc, rt.left, rt.top, rt.right, rt.bottom);
        InsetRect( &rt, 1, 1 );
        ceClipToRect( hdc, &rt );

        if ( !isEmpty ) {
            index = getPlayerColor(dctx->trayOwner);
            SetTextColor( hdc, dctx->globals->appPrefs.colors[index] );

            /* For some reason Rectangle isn't using the background brush to
               fill, so FillRect has to get called after Rectangle.  Need to
               call InsetRect either way to put chars in the right place. */
            highlighted = (flags & CELL_HIGHLIGHT) != 0;
            if ( highlighted ) {
                Rectangle( hdc, rt.left, rt.top, rt.right, rt.bottom );
                InsetRect( &rt, 1, 1 );
            }
        }

        FillRect( hdc, &rt, dctx->brushes[backIndex] );

        if ( !isEmpty ) {
            XP_U16 thirdHt = xprect->height / 3;
            if ( !highlighted ) {
                InsetRect( &rt, 1, 1 );
            }

            if ( !!letters ) {
                HFONT font = ceGetSizedFont( dctx, thirdHt*2, 
                                             RFONTS_TRAY );
                HFONT oldFont = SelectObject( hdc, font );
                MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, letters, -1,
                                     widebuf, 
                                     VSIZE(widebuf) );
                DrawText( hdc, widebuf, -1, &rt, 
                          DT_SINGLELINE | DT_TOP | DT_LEFT );
                SelectObject( hdc, oldFont );
            } else if ( !!bitmap  ) {
                RECT lrt = rt;
                XP_U16 tmp = CE_USER_COLOR1+dctx->trayOwner;
                ++lrt.left;
                lrt.top += 4;
                makeAndDrawBitmap( dctx, hdc, &lrt, XP_FALSE,
                                   dctx->globals->appPrefs.colors[tmp],
                                   (CEBitmapInfo*)bitmap );
            }

            if ( val >= 0 ) {
                HFONT font = ceGetSizedFont( dctx, thirdHt, 
                                             RFONTS_TRAYVAL );
                HFONT oldFont = SelectObject( hdc, font );
                swprintf( widebuf, L"%d", val );
                DrawText(hdc, widebuf, -1, &rt, 
                         DT_SINGLELINE | DT_BOTTOM | DT_RIGHT);	
                SelectObject( hdc, oldFont );
            }
        }
    }
} /* drawDrawTileGuts */

DLSTATIC void
DRAW_FUNC_NAME(drawTile)( DrawCtx* p_dctx, const XP_Rect* xprect, 
                          const XP_UCHAR* letters, XP_Bitmap bitmap, 
                          XP_S16 val, CellFlags flags )
{
    drawDrawTileGuts( p_dctx, xprect, letters, bitmap, val, flags );
} /* ce_draw_drawTile */

#ifdef POINTER_SUPPORT
DLSTATIC void
DRAW_FUNC_NAME(drawTileMidDrag)( DrawCtx* p_dctx, const XP_Rect* xprect, 
                                 const XP_UCHAR* letters, XP_Bitmap bitmap, 
                                 XP_S16 val, XP_U16 owner, CellFlags flags )
{
    draw_trayBegin( p_dctx, xprect, owner, DFS_NONE );
    drawDrawTileGuts( p_dctx, xprect, letters, bitmap, val, flags );
} /* ce_draw_drawTile */
#endif

DLSTATIC void
DRAW_FUNC_NAME(drawTileBack)( DrawCtx* p_dctx, const XP_Rect* xprect,
                              CellFlags flags )
{
    drawDrawTileGuts( p_dctx, xprect, "?", NULL, -1, flags );
} /* ce_draw_drawTileBack */

DLSTATIC void
DRAW_FUNC_NAME(drawTrayDivider)( DrawCtx* p_dctx, const XP_Rect* rect, 
                                 CellFlags flags )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    RECT rt;
    XP_Bool selected = (flags & CELL_HIGHLIGHT) != 0;
    XP_Bool isFocussed = TREAT_AS_CURSOR(dctx,flags);

    XPRtoRECT( &rt, rect );
    ceClipToRect( hdc, &rt );

    if ( isFocussed ) {
        FillRect( hdc, &rt, dctx->brushes[CE_FOCUS_COLOR] );
        InsetRect( &rt, 0, (rt.bottom - rt.top) >> 2 );
    }

    if ( selected ) {
        Rectangle( hdc, rt.left, rt.top, rt.right, rt.bottom );
    } else {
        FillRect( hdc, &rt, dctx->brushes[CE_BLACK_COLOR] );
    }
} /* ce_draw_drawTrayDivider */

static void
ceClearToBkground( CEDrawCtx* dctx, const XP_Rect* rect )
{
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    RECT rt;

    XPRtoRECT( &rt, rect );

    FillRect( hdc, &rt, dctx->brushes[CE_BKG_COLOR] );
} /* ceClearToBkground */

static void
ceDrawBitmapInRect( HDC hdc, const RECT* rect, HBITMAP bitmap )
{
    BITMAP bmp;
    HDC tmpDC;
    int nBytes;
    int x = rect->left;
    int y = rect->top;

    tmpDC = CreateCompatibleDC( hdc );
    SelectObject( tmpDC, bitmap );

    (void)IntersectClipRect( tmpDC, x, y, rect->right, rect->bottom );

    nBytes = GetObject( bitmap, sizeof(bmp), &bmp );
    XP_ASSERT( nBytes > 0 );
    if ( nBytes == 0 ) {
        logLastError( "ceDrawBitmapInRect:GetObject" );
    }

    x += ((rect->right - x) - bmp.bmWidth) / 2;
    y += ((rect->bottom - y) - bmp.bmHeight) / 2;

    BitBlt( hdc, x, y, bmp.bmWidth, bmp.bmHeight, 
            tmpDC, 0, 0, SRCCOPY );	/* BLACKNESS */

    DeleteDC( tmpDC );
} /* ceDrawBitmapInRect */

DLSTATIC void
DRAW_FUNC_NAME(drawBoardArrow)( DrawCtx* p_dctx, const XP_Rect* xprect, 
                                XWBonusType cursorBonus, XP_Bool vertical,
                                HintAtts hintAtts, CellFlags flags )
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
    ceClipToRect( hdc, &rt );

    Rectangle( hdc, rt.left, rt.top, rt.right, rt.bottom );
    InsetRect( &rt, 1, 1 );
    ceClipToRect( hdc, &rt );

    if ( vertical ) {
        cursor = dctx->downArrow;
    } else {
        cursor = dctx->rightArrow;
    }

    if ( TREAT_AS_CURSOR( dctx, flags ) ) {
        bkIndex = CE_FOCUS_COLOR;
    } else if ( cursorBonus == BONUS_NONE ) {
        bkIndex = CE_BKG_COLOR;
    } else {
        bkIndex = cursorBonus - BONUS_DOUBLE_LETTER + CE_BONUS1_COLOR;
    }
    FillRect( hdc, &rt, dctx->brushes[bkIndex] );
    SetBkColor( hdc, dctx->globals->appPrefs.colors[bkIndex] );
    SetTextColor( hdc, dctx->globals->appPrefs.colors[CE_BLACK_COLOR] );

    ceDrawBitmapInRect( hdc, &rt, cursor );

    ceDrawHintBorders( hdc, xprect, hintAtts );
} /* ce_draw_drawBoardArrow */

DLSTATIC void
DRAW_FUNC_NAME(scoreBegin)( DrawCtx* p_dctx, const XP_Rect* rect, 
                            XP_U16 XP_UNUSED(numPlayers), 
                            DrawFocusState XP_UNUSED(dfs) )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    SetBkColor( hdc, dctx->globals->appPrefs.colors[CE_BKG_COLOR] );

    dctx->scoreIsVertical = rect->height > rect->width;

    /* I don't think the clip rect's set at this point but drawing seems fine
       anyway.... ceClearToBkground() is definitely needed here. */
    ceClearToBkground( (CEDrawCtx*)p_dctx, rect );
} /* ce_draw_scoreBegin */

static void
formatRemText( XP_S16 nTilesLeft, XP_Bool isVertical, XP_UCHAR* buf )
{
    char* fmt;
    XP_ASSERT( nTilesLeft > 0 );

    if ( isVertical ) {
        fmt = "Rem" XP_CR "%d";
    } else {
        fmt = "Rem:%d";
    }
    sprintf( buf, fmt, nTilesLeft );
} /* formatRemText */

DLSTATIC void
DRAW_FUNC_NAME(measureRemText)( DrawCtx* p_dctx, const XP_Rect* XP_UNUSED(r), 
                                XP_S16 nTilesLeft, 
                                XP_U16* width, XP_U16* height )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;

    if ( nTilesLeft > 0 ) {
        XP_UCHAR buf[16];
        formatRemText( nTilesLeft, dctx->scoreIsVertical, buf );
        ceMeasureText( dctx, buf, CE_REM_PADDING, width, height );
    } else {
        *width = *height = 0;
    }
} /* ce_draw_measureRemText */

DLSTATIC void
DRAW_FUNC_NAME(drawRemText)( DrawCtx* p_dctx, const XP_Rect* rInner, 
                             const XP_Rect* XP_UNUSED(rOuter), 
                             XP_S16 nTilesLeft )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    XP_UCHAR buf[16];
    RECT rt;

    formatRemText( nTilesLeft, dctx->scoreIsVertical, buf );

    XPRtoRECT( &rt, rInner );
    ceClipToRect( hdc, &rt );
    ++rt.left;                  /* 1: don't write up against edge */
    drawTextLines( dctx, hdc, buf, CE_REM_PADDING, &rt, 
                   DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_CENTER );
} /* ce_draw_drawRemText */

static void
ceWidthAndText( CEDrawCtx* dctx, const DrawScoreInfo* dsi, 
                XP_UCHAR* buf, XP_U16* widthP, XP_U16* heightP )
{
    XP_UCHAR bullet[] = {'•', '\0'};
    XP_UCHAR tmp[16];
        
    /* For a horizontal scoreboard, we want *300:6*
     * For a vertical, it's
     *       
     *      300
     *       6 
     *
     * with IS_TURN_VPAD-height rects above and below
     */       

    if ( !dsi->isTurn || dctx->scoreIsVertical ) {
        bullet[0] = '\0';
    }

    strcpy( buf, bullet );

    sprintf( tmp, "%d", dsi->totalScore );
    strcat( buf, tmp );

    if ( dsi->nTilesLeft >= 0 ) {
        strcat( buf, dctx->scoreIsVertical? XP_CR : ":" );

        sprintf( tmp, "%d", dsi->nTilesLeft );
        strcat( buf, tmp );
    }

    strcat( buf, bullet );

    ceMeasureText( dctx, buf, CE_SCORE_PADDING, widthP, heightP );
    if ( dsi->isTurn && dctx->scoreIsVertical ) {
        *heightP += IS_TURN_VPAD * 2;
    }
} /* ceWidthAndText */

DLSTATIC void
DRAW_FUNC_NAME(measureScoreText)( DrawCtx* p_dctx, const XP_Rect* XP_UNUSED(r),
                                  const DrawScoreInfo* dsi,
                                  XP_U16* widthP, XP_U16* heightP )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    XP_UCHAR buf[32];
    HFONT newFont;
    HFONT oldFont;

    if ( dsi->selected ) {
        newFont = dctx->selPlayerFont;
    } else {
        newFont = dctx->playerFont;
    }
    oldFont = SelectObject( hdc, newFont );

    ceWidthAndText( dctx, dsi, buf, widthP, heightP );

    SelectObject( hdc, oldFont );
} /* ce_draw_measureScoreText */

DLSTATIC void
DRAW_FUNC_NAME(score_drawPlayer)( DrawCtx* p_dctx, 
                                  const XP_Rect* rInner, 
                                  const XP_Rect* rOuter, 
                                  const DrawScoreInfo* dsi )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    RECT rt;
    XP_U16 width, height;
    XP_UCHAR scoreBuf[20];
    HFONT newFont;
    HFONT oldFont;
    XP_Bool frameScore;
    XP_Bool isFocussed = (dsi->flags & CELL_ISCURSOR) != 0;
    XP_U16 bkIndex = isFocussed ? CE_FOCUS_COLOR : CE_BKG_COLOR;

    if ( dsi->selected ) {
        newFont = dctx->selPlayerFont;
    } else {
        newFont = dctx->playerFont;
    }
    oldFont = SelectObject( hdc, newFont );

    SetTextColor( hdc, dctx->globals->
                  appPrefs.colors[getPlayerColor(dsi->playerNum)] );
    SetBkColor( hdc, dctx->globals->appPrefs.colors[bkIndex] );
        
    XPRtoRECT( &rt, rOuter );
    ceClipToRect( hdc, &rt );
    if ( isFocussed ) {
        FillRect( hdc, &rt, dctx->brushes[CE_FOCUS_COLOR] );
    }

    ceWidthAndText( dctx, dsi, scoreBuf, &width, &height );

    XPRtoRECT( &rt, rInner );
    ++rt.top;                   /* tweak for ce */

    frameScore = dsi->isTurn && dctx->scoreIsVertical;
    if ( frameScore ) {
        rt.top += IS_TURN_VPAD;
        rt.bottom -= IS_TURN_VPAD;
    }

    drawTextLines( dctx, hdc, scoreBuf, CE_SCORE_PADDING, &rt, 
                   DT_SINGLELINE | DT_VCENTER | DT_CENTER );

    if ( frameScore ) {
        Rectangle( hdc, rt.left, rt.top-IS_TURN_VPAD, rt.right, rt.top );
        Rectangle( hdc, rt.left, rt.bottom, rt.right, 
                   rt.bottom + IS_TURN_VPAD );
    }

    SelectObject( hdc, oldFont );
} /* ce_draw_score_drawPlayer */

DLSTATIC void
DRAW_FUNC_NAME(score_pendingScore)( DrawCtx* p_dctx, const XP_Rect* rect, 
                                    XP_S16 score, XP_U16 XP_UNUSED(playerNum),
                                    CellFlags flags )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;

    wchar_t widebuf[5];
    XP_UCHAR buf[5];
    RECT rt, clip;
    XP_Bool focussed = TREAT_AS_CURSOR(dctx,flags);
    XP_U16 bkIndex = focussed ?  CE_FOCUS_COLOR : CE_BKG_COLOR;

    HFONT font = ceGetSizedFont( dctx, rect->height/2, RFONTS_PTS );    
    HFONT oldFont = SelectObject( hdc, font );

    SetTextColor( hdc, dctx->globals->appPrefs.colors[CE_BLACK_COLOR] );
    SetBkColor( hdc, dctx->globals->appPrefs.colors[bkIndex] );

    XPRtoRECT( &rt, rect );
    FillRect( hdc, &rt, dctx->brushes[bkIndex] );
    /* Use a separate rect for clipping since shrinking puts text to close
       together on smartphone's small screen. */
    clip = rt;
    InsetRect( &clip, 2, 2 );
    ceClipToRect( hdc, &clip );

    DrawText(hdc, L"Pts", -1, &rt, DT_SINGLELINE | DT_TOP | DT_CENTER);

    if ( score < 0 ) {
        buf[0] = '?';
        buf[1] = '?';
        buf[2] = '\0';
    } else {
        sprintf( buf, "%d", score );
    }

    MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, buf, -1,
                         widebuf, VSIZE(widebuf) );
    DrawText(hdc, widebuf, -1, &rt, DT_SINGLELINE | DT_BOTTOM | DT_CENTER);	
    SelectObject( hdc, oldFont );
} /* ce_draw_score_pendingScore */

DLSTATIC void
DRAW_FUNC_NAME(drawTimer)( DrawCtx* p_dctx, const XP_Rect* rInner, 
                           const XP_Rect* XP_UNUSED(rOuter),
                           XP_U16 player, XP_S16 secondsLeft )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    XP_UCHAR buf[16];
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

    snprintf( buf, sizeof(buf),
             dctx->scoreIsVertical? "%s%.1dm" XP_CR "%.2ds" : "%s%.1d:%.2d", 
             isNegative? "-": "", mins, secs );
   
    if ( !hdc ) {
        InvalidateRect( dctx->mainWin, &rt, FALSE );
        hdc = BeginPaint( dctx->mainWin, &ps );
    }

    ceClipToRect( hdc, &rt );

    SetTextColor( hdc, dctx->globals->appPrefs.colors[getPlayerColor(player)] );
    SetBkColor( hdc, dctx->globals->appPrefs.colors[CE_BKG_COLOR] );
    ceClearToBkground( dctx, rInner );
    drawTextLines( dctx, hdc, buf, CE_TIMER_PADDING, &rt, 
                   DT_SINGLELINE | DT_VCENTER | DT_CENTER);

    if ( !globals->hdc ) {
        EndPaint( dctx->mainWin, &ps );
    }
} /* ce_draw_drawTimer */

DLSTATIC const XP_UCHAR*
DRAW_FUNC_NAME(getMiniWText)( DrawCtx* XP_UNUSED(p_dctx), 
                              XWMiniTextType whichText )
{
    XP_UCHAR* str;

    switch( whichText ) {
    case BONUS_DOUBLE_LETTER:
        str = "Double letter"; 
        break;
    case BONUS_DOUBLE_WORD:
        str = "Double word"; 
        break;
    case BONUS_TRIPLE_LETTER:
        str = "Triple letter"; 
        break;
    case BONUS_TRIPLE_WORD:
        str = "Triple word"; 
        break;
    case INTRADE_MW_TEXT:
        str = "Trading tiles." XP_CR "Select 'Turn done' when ready"; 
        break;
    default:
        XP_ASSERT( XP_FALSE );
        str = NULL;
        break;
    }

    return str;
} /* ce_draw_getMiniWText */

DLSTATIC void
DRAW_FUNC_NAME(measureMiniWText)( DrawCtx* p_dctx, const XP_UCHAR* str, 
                                  XP_U16* widthP, XP_U16* heightP )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    ceMeasureText( dctx, str, CE_MINIW_PADDING, widthP, heightP );
    *heightP += CE_MINI_V_PADDING;
} /* ce_draw_measureMiniWText */

DLSTATIC void
DRAW_FUNC_NAME(drawMiniWindow)( DrawCtx* p_dctx, const XP_UCHAR* text, 
                                const XP_Rect* rect, 
                                void** XP_UNUSED(closureP) )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc;
    RECT rt, textRt;
    PAINTSTRUCT ps;

    XPRtoRECT( &rt, rect );

    if ( !!globals->hdc ) {
        hdc = globals->hdc;
    } else {
        InvalidateRect( dctx->mainWin, &rt, FALSE );
        hdc = BeginPaint( dctx->mainWin, &ps );
    }
    ceClipToRect( hdc, &rt );

    ceClearToBkground( dctx, rect );

    SetBkColor( hdc, dctx->globals->appPrefs.colors[CE_BKG_COLOR] );
    SetTextColor( hdc, dctx->globals->appPrefs.colors[CE_BLACK_COLOR] );

    Rectangle( hdc, rt.left, rt.top, rt.right, rt.bottom );
    InsetRect( &rt, 1, 1 );
    Rectangle( hdc, rt.left, rt.top, rt.right, rt.bottom );

    textRt = rt;
    textRt.top += 2;
    InsetRect( &textRt, 3, 0 );

    drawTextLines( dctx, hdc, text, CE_MINIW_PADDING, &textRt, 
                   DT_CENTER | DT_VCENTER );

    if ( !globals->hdc ) {
        EndPaint( dctx->mainWin, &ps );
    }
} /* ce_draw_drawMiniWindow */

DLSTATIC void
DRAW_FUNC_NAME(destroyCtxt)( DrawCtx* p_dctx )
{
    XP_U16 i;
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;

    for ( i = 0; i < CE_NUM_COLORS; ++i ) {
        DeleteObject( dctx->brushes[i] );
        if ( !!dctx->pens[i].pen ) {
            DeleteObject( dctx->pens[i].pen );
        }
    }

    DeleteObject( dctx->playerFont );
    DeleteObject( dctx->selPlayerFont );
    for ( i = 0; i < N_RESIZE_FONTS; ++i ) {
        if ( !!dctx->setFont[i] ) {
            DeleteObject( dctx->setFont[i] );
        }
    }

    DeleteObject( dctx->rightArrow );
    DeleteObject( dctx->downArrow );
    DeleteObject( dctx->origin );

#ifndef DRAW_LINK_DIRECT
    XP_FREE( dctx->mpool, p_dctx->vtable );
#endif

    XP_FREE( dctx->mpool, dctx );
} /* ce_draw_destroyCtxt */

#ifdef DRAW_LINK_DIRECT
DLSTATIC XP_Bool
DRAW_FUNC_NAME(vertScrollBoard)( DrawCtx* p_dctx, XP_Rect* rect, 
                                 XP_S16 dist, DrawFocusState XP_UNUSED(dfs) )
{
    XP_Bool success = XP_FALSE;
    /* board passes in the whole board rect, so we need to subtract from it
       the height of the area to be overwritten.  If dist is negative, the
       dest is above the src.  Otherwise it's below. */

    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    int destY, srcY;
    RECT rt;
    XP_Bool down = dist <= 0;

    XPRtoRECT( &rt, rect );
    ceClipToRect( globals->hdc, &rt );

    if ( down ) {
        srcY = rect->top;
        dist = -dist;           /* make it positive */
        destY = srcY + dist;
    } else {
        destY = rect->top;
        srcY = destY + dist;
    }

    success = FALSE != BitBlt( globals->hdc,  /* HDC hdcDest, */
                               rect->left,    /* int nXDest */
                               destY, 
                               rect->width,   /* width */
                               rect->height - dist,  /* int nHeight */
                               globals->hdc,  /* HDC hdcSrc */
                               rect->left,    /* int nXSrc */
                               srcY, 
                               SRCCOPY );     /* DWORD dwRop */
    /* need to return the rect that must still be redrawn */
    if ( success ) {
        if ( !down ) {
            rect->top += rect->height - dist;
        }
        rect->height = dist;
    }
    return success;
}
#else  /* #ifdef DRAW_LINK_DIRECT */
static void
ce_draw_doNothing( DrawCtx* dctx, ... )
{
} /* ce_draw_doNothing */
#endif

static void
ceFontsSetup( CEDrawCtx* dctx )
{
    LOGFONT font;

    XP_MEMSET( &font, 0, sizeof(font) );
    wcscpy( font.lfFaceName, L"Arial" );

    font.lfWeight = FW_LIGHT;
    font.lfHeight = CE_SCORE_HEIGHT - 2;
    dctx->playerFont = CreateFontIndirect( &font );

    font.lfWeight = FW_BOLD;
    font.lfHeight = CE_SCORE_HEIGHT;
    dctx->selPlayerFont = CreateFontIndirect( &font );
} /* ceFontsSetup */

void
ce_drawctxt_update( DrawCtx* p_dctx )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    XP_U16 i;

    for ( i = 0; i < CE_NUM_COLORS; ++i ) {
        if ( !!dctx->brushes[i] ) {
            DeleteObject( dctx->brushes[i] );
        }
        dctx->brushes[i] = CreateSolidBrush(dctx->globals->appPrefs.colors[i]);
    }
} /* ce_drawctxt_update */

DrawCtx* 
ce_drawctxt_make( MPFORMAL HWND mainWin, CEAppGlobals* globals )
{
    CEDrawCtx* dctx = (CEDrawCtx*)XP_MALLOC( mpool,
                                             sizeof(*dctx) );
    XP_MEMSET( dctx, 0, sizeof(*dctx) );
    MPASSIGN(dctx->mpool, mpool);

#ifndef DRAW_LINK_DIRECT
    dctx->vtable = (DrawCtxVTable*)XP_MALLOC( mpool, sizeof(*((dctx)->vtable)));

    for ( i = 0; i < sizeof(*dctx->vtable)/4; ++i ) {
        ((void**)(dctx->vtable))[i] = ce_draw_doNothing;
    }

    SET_VTABLE_ENTRY( dctx->vtable, draw_destroyCtxt, ce );

    SET_VTABLE_ENTRY( dctx->vtable, draw_boardBegin, ce );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawCell, ce );
    SET_VTABLE_ENTRY( dctx->vtable, draw_invertCell, ce );

    SET_VTABLE_ENTRY( dctx->vtable, draw_trayBegin, ce );
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

    SET_VTABLE_ENTRY( dctx->vtable, draw_objFinished, ce );

    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTimer, ce );

    SET_VTABLE_ENTRY( dctx->vtable, draw_getMiniWText, ce );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureMiniWText, ce );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawMiniWindow, ce );
#endif

    dctx->mainWin = mainWin;
    dctx->globals = globals;

    ce_drawctxt_update( (DrawCtx*)dctx );

    ceFontsSetup( dctx );

    dctx->rightArrow = LoadBitmap( globals->hInst, 
                                   MAKEINTRESOURCE(IDB_RIGHTARROW) );
    dctx->downArrow = LoadBitmap( globals->hInst, 
                                  MAKEINTRESOURCE(IDB_DOWNARROW) );
    dctx->origin = LoadBitmap( globals->hInst, 
                               MAKEINTRESOURCE(IDB_ORIGIN) );

    return (DrawCtx*)dctx;
} /* ce_drawctxt_make */
