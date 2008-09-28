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
#include "cedraw.h"
#include "board.h"
#include "draw.h"
#include "mempool.h"

#include "cemain.h"
#include "cedict.h"
#include "cedefines.h"
#include "cedebug.h"
#include "debhacks.h"

#ifndef DRAW_FUNC_NAME
#define DRAW_FUNC_NAME(nam) ce_draw_ ## nam
#endif

#define FCE_TEXT_PADDING 2
#define CE_MINI_V_PADDING 6
#define CE_MINI_H_PADDING 8
#define CE_MINIW_PADDING 0
#define CE_TIMER_PADDING -2
#define IS_TURN_VPAD 2

#define DRAW_FOCUS_FRAME 1
#ifdef DRAW_FOCUS_FRAME
# define CE_FOCUS_BORDER_WIDTH 6
# define TREAT_AS_CURSOR(d,f) ((((f) & CELL_ISCURSOR) != 0) && !(d)->topFocus )
#else
# define TREAT_AS_CURSOR(d,f) (((f) & CELL_ISCURSOR) != 0)
#endif


typedef enum { 
    RFONTS_TRAY
    ,RFONTS_TRAYVAL
    ,RFONTS_CELL
    ,RFONTS_REM
    ,RFONTS_SCORE
    ,RFONTS_SCORE_BOLD

    ,N_RESIZE_FONTS
} RFIndex;

typedef struct _PenColorPair {
    COLORREF ref;
    HGDIOBJ pen;
} PenColorPair;

typedef struct _FontCacheEntry {
    HFONT setFont;
    /* NOTE: indexHt >= glyphHt.  fontHt will usually be > indexHt */
    XP_U16 indexHt;            /* the size we match on, the space we have */
    XP_U16 lfHeight;           /* What CE thinks is the "size" of the font we
                                  found to best fill indexHt */
    XP_U16 glyphHt;            /* range from tops to bottoms of glyphs we use */
    XP_U16 offset;
} FontCacheEntry;

struct CEDrawCtx {
    DrawCtxVTable* vtable;
    
    HWND mainWin;
    CEAppGlobals* globals;
    const DictionaryCtxt* dict;

    COLORREF prevBkColor;

    HBRUSH brushes[CE_NUM_COLORS];
    PenColorPair pens[CE_NUM_COLORS];

    FontCacheEntry fcEntry[N_RESIZE_FONTS];

    HBITMAP rightArrow;
    HBITMAP downArrow;
    HBITMAP origin;

    XP_U16 trayOwner;
    XP_U16 miniLineHt;
    XP_Bool scoreIsVertical;
    XP_Bool topFocus;

    MPSLOT
};

static void ceClearToBkground( CEDrawCtx* dctx, const XP_Rect* rect );
static void ceDrawBitmapInRect( HDC hdc, const RECT* r, HBITMAP bitmap );
static void ceClipToRect( HDC hdc, const RECT* rt );
static void ceClearFontCache( CEDrawCtx* dctx );

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

static void
ceDrawTextClipped( HDC hdc, wchar_t* buf, XP_S16 len, XP_Bool clip,
                   const FontCacheEntry* fce, XP_U16 left, XP_U16 top, 
                   XP_U16 width, XP_U16 hJust )
{
    RECT rect = {
      .left = left,
      .top = top,
      .bottom = top + fce->glyphHt,
      .right = left + width
    };

    if ( clip ) {
      ceClipToRect( hdc, &rect );
    }
    rect.top -= fce->offset;
/*     XP_LOGF( "%s: drawing left: %ld, top: %ld, right: %ld, bottom: %ld", */
/*              __func__, rect.left, rect.top, rect.right, rect.bottom ); */
    DrawText( hdc, buf, len, &rect, DT_SINGLELINE | DT_TOP | hJust );
} /* ceDrawTextClipped */

static void
ceDrawLinesClipped( HDC hdc, const FontCacheEntry* fce, XP_UCHAR* buf, 
                    XP_Bool clip, const RECT* bounds )
{
    XP_U16 top = bounds->top;
    XP_U16 width = bounds->right - bounds->left;

    for ( ; ; ) {
        XP_UCHAR* newline = strstr( buf, XP_CR );
        XP_U16 len = newline==NULL? strlen(buf): newline - buf;
        wchar_t widebuf[len];

        MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, buf, len,
                             widebuf, len );

        ceDrawTextClipped( hdc, widebuf, len, clip, fce, bounds->left, top, 
                           width, DT_CENTER );
        if ( !newline ) {
            break;
        }
        top += fce->glyphHt + FCE_TEXT_PADDING;
        buf = newline + XP_STRLEN(XP_CR);	/* skip '\n' */
    }
} /* ceDrawLinesClipped */

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

static void
makeTestBuf( CEDrawCtx* dctx, XP_UCHAR* buf, XP_U16 bufLen, RFIndex index )
{
    switch( index ) {
    case RFONTS_TRAY:
    case RFONTS_CELL: {
        Tile tile;
        Tile blank = (Tile)-1;
        const DictionaryCtxt* dict = dctx->dict;
        XP_U16 nFaces = dict_numTileFaces( dict );
        Tile tiles[nFaces];
        XP_U16 nOut = 0;
        XP_ASSERT( !!dict );
        XP_ASSERT( nFaces < bufLen );
        if ( dict_hasBlankTile(dict) ) {
            blank = dict_getBlankTile( dict );
        }
        for ( tile = 0; tile < nFaces; ++tile ) {
            if ( tile != blank ) {
                tiles[nOut++] = tile;
            }
        }
        buf[0] = '0';           /* so can use for numbers too */
        (void)dict_tilesToString( dict, tiles, nOut, &buf[1], bufLen-1 );
    }
        break;
    case RFONTS_TRAYVAL:
        strcpy( buf, "Pts0" );     /* all numbers the same :-) */
        break;
    case RFONTS_REM:
        strcpy( buf, "Rem0" );
        break;
    case RFONTS_SCORE:
    case RFONTS_SCORE_BOLD:
        strcpy( buf, "0:" );
        break;
    case N_RESIZE_FONTS:
        XP_ASSERT(0);
    }
    XP_LOGF( "%s=>%s", __func__, buf );
} /* makeTestBuf */

static void
ceMeasureGlyph( HDC hdc, HBRUSH white, wchar_t glyph,
                XP_U16 minTopSeen, XP_U16 maxBottomSeen,
                XP_U16* top, XP_U16* bottom )
{
    SIZE size;
    XP_U16 xx, yy;
    XP_Bool done;

    GetTextExtentPoint32( hdc, &glyph, 1, &size );
    RECT rect = { 0, 0, size.cx, size.cy };
    FillRect( hdc, &rect, white );
    DrawText( hdc, &glyph, 1, &rect, DT_TOP | DT_LEFT );

/*     char tbuf[size.cx+1]; */
/*     for ( yy = 0; yy < size.cy; ++yy ) { */
/*         XP_MEMSET( tbuf, 0, size.cx+1 ); */
/*         for ( xx = 0; xx < size.cx; ++xx ) { */
/*             COLORREF ref = GetPixel( hdc, xx, yy ); */
/*             XP_ASSERT( ref != CLR_INVALID ); */
/*             strcat( tbuf, ref==0? " " : "x" ); */
/*         } */
/*         XP_LOGF( "line[%.2d] = %s", yy, tbuf ); */
/*     } */

    /* Find out if this guy's taller than what we have */
    for ( done = XP_FALSE, yy = 0; yy < minTopSeen && !done; ++yy ) {
        for ( xx = 0; xx < size.cx; ++xx ) {
            COLORREF ref = GetPixel( hdc, xx, yy );
            if ( ref == CLR_INVALID ) {
                break;               /* done this line */
            } else if ( ref == 0 ) { /* a pixel set! */
                *top = yy;
                done = XP_TRUE;
                break;
            }
        }
    }

    /* Extends lower than seen */
    for ( done = XP_FALSE, yy = size.cy - 1; yy > maxBottomSeen && !done;
          --yy ) {
        for ( xx = 0; xx < size.cx; ++xx ) {
            COLORREF ref = GetPixel( hdc, xx, yy );
            if ( ref == CLR_INVALID ) {
                break;
            } else if ( ref == 0 ) { /* a pixel set! */
                *bottom = yy;
                done = XP_TRUE;
                break;
            }
        }
    }
/*     XP_LOGF( "%s: top: %d; bottom: %d", __func__, *top, *bottom ); */
} /* ceMeasureGlyph */

static void
ceMeasureGlyphs( CEDrawCtx* dctx, HDC hdc, wchar_t* str,
                 XP_U16* hasMinTop, XP_U16* hasMaxBottom )
{
    HBRUSH white = dctx->brushes[CE_WHITE_COLOR];
    XP_U16 ii;
    XP_U16 len = wcslen(str);
    XP_U16 minTopSeen, maxBottomSeen;
    XP_U16 maxBottomIndex = 0;
    XP_U16 minTopIndex = 0;

    minTopSeen = 1000;          /* really large... */
    maxBottomSeen = 0;
    for ( ii = 0; ii < len; ++ii ) {
        XP_U16 thisTop, thisBottom;

        /* TODO: Find a way to to keep minTopIndex && maxBottomIndex the same
           IFF there's a character, like Q, that has the lowest point but 
           as high a top as anybody else.  Maybe for > until both are set,
           then >= ? */

        ceMeasureGlyph( hdc, white, str[ii],
                        minTopSeen, maxBottomSeen,
                        &thisTop, &thisBottom );
        if ( thisBottom > maxBottomSeen ) {
            maxBottomSeen = thisBottom;
            maxBottomIndex = ii;
        }
        if ( thisTop < minTopSeen ) {
            minTopSeen = thisTop;
            minTopIndex = ii;
        }
    }

    *hasMinTop = minTopIndex;
    *hasMaxBottom = maxBottomIndex;
} /* ceMeasureGlyphs */

static void
ceClearFontCache( CEDrawCtx* dctx )
{
    XP_U16 ii;
    for ( ii = 0; ii < N_RESIZE_FONTS; ++ii ) {
        if ( !!dctx->fcEntry[ii].setFont ) {
            DeleteObject( dctx->fcEntry[ii].setFont );
        }
    }
    XP_MEMSET( &dctx->fcEntry, 0, sizeof(dctx->fcEntry) );
}

static void
ceFillFontInfo( const CEDrawCtx* dctx, LOGFONT* fontInfo, 
                XP_U16 height/* , XP_Bool bold */ )
{
    XP_MEMSET( fontInfo, 0, sizeof(*fontInfo) );
    fontInfo->lfHeight = height;

/*     if ( bold ) { */
/*         fontInfo->lfWeight = FW_BOLD; */
/*     } else { */
/*         fontInfo->lfWeight = FW_LIGHT; */
/*     } */

    wcscpy( fontInfo->lfFaceName, IS_SMARTPHONE(dctx->globals)?
            L"Segoe Condensed" : L"Tahoma" );
}

static void
ceBestFitFont( CEDrawCtx* dctx, XP_U16 soughtHeight, RFIndex index, 
               FontCacheEntry* fce )
{
    wchar_t widebuf[65];
    XP_U16 len;
    XP_U16 hasMinTop, hasMaxBottom;
    XP_Bool firstPass;
    HBRUSH white = dctx->brushes[CE_WHITE_COLOR];
    HDC memDC = CreateCompatibleDC( NULL );
    HBITMAP memBM;
    XP_U16 testSize;

    char sample[65];
    makeTestBuf( dctx, sample, VSIZE(sample), index );
    len = strlen(sample);
    MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, sample, len,
                         widebuf, len );

    memBM = CreateCompatibleBitmap( memDC, soughtHeight*2, soughtHeight*2 );
    SelectObject( memDC, memBM );

    for ( firstPass = XP_TRUE, testSize = soughtHeight*2; ; --testSize ) {
        LOGFONT fontInfo;
        HFONT testFont;

        ceFillFontInfo( dctx, &fontInfo, testSize );
        testFont = CreateFontIndirect( &fontInfo );

        if ( !!testFont ) {
            XP_U16 thisHeight, top, bottom;

            SelectObject( memDC, testFont );

            /* first time, measure all of them to determine which chars have
               the set's high and low points */
            if ( firstPass ) {
                ceMeasureGlyphs( dctx, memDC, widebuf, &hasMinTop,
                                 &hasMaxBottom );
                firstPass = XP_FALSE;
            } 
            /* Thereafter, just measure the two we know about */
            ceMeasureGlyph( memDC, white, widebuf[hasMinTop], 1000, 0, 
                            &top, &bottom );
            if ( hasMaxBottom != hasMinTop ) {
              ceMeasureGlyph( memDC, white, widebuf[hasMaxBottom],
                              top, bottom, &top, &bottom );
            }
            thisHeight = bottom - top + 1;

            if ( thisHeight <= soughtHeight ) { /* got it!!! */
                fce->setFont = testFont;
                fce->indexHt = soughtHeight;
                fce->lfHeight = testSize;
                fce->offset = top;
                fce->glyphHt = thisHeight;
                XP_LOGF( "Found for %d: indexHt: %d; lfHeight: %d; glyphHt: %d",
                         index, fce->indexHt, fce->lfHeight, fce->glyphHt );

                XP_ASSERT( fce->lfHeight >= fce->indexHt );
                XP_ASSERT( fce->indexHt >= fce->glyphHt );
                break;
            }
            DeleteObject( testFont );
        }
    }

    DeleteObject( memBM );
    DeleteDC( memDC );
} /* ceBestFitFont */

static const FontCacheEntry* 
ceGetSizedFont( CEDrawCtx* dctx, XP_U16 height, RFIndex index )
{
    FontCacheEntry* fce = &dctx->fcEntry[index];
    if ( (0 != height)            /* 0 means use what we have */
         && fce->indexHt != height ) {
        ceBestFitFont( dctx, height, index, fce );
    }

    XP_ASSERT( !!fce->setFont ); /* failing... */
    return fce;
} /* ceGetSizedFont */

static void
ceMeasureText( CEDrawCtx* dctx, HDC hdc, const FontCacheEntry* fce, 
               const XP_UCHAR* str, XP_S16 padding,
               XP_U16* widthP, XP_U16* heightP )
{
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

        maxWidth = (XP_U16)XP_MAX( maxWidth, size.cx );
        if ( !!fce ) {
            size.cy = fce->glyphHt;
            padding = FCE_TEXT_PADDING;        /* minimal */
        }
        height += size.cy;
        dctx->miniLineHt = (XP_U16)size.cy;

        if ( nextStr == NULL ) {
            break;
        }
        height += padding;
        str = nextStr + XP_STRLEN(XP_CR);	/* skip '\n' */
    }

    *widthP = maxWidth;
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

static void
ceGetCharValHts( const XP_Rect* xprect, XP_U16* charHt, XP_U16* valHt )
{
    XP_U16 visHt = xprect->height - TRAY_BORDER;
    /* if tiles are wider than tall we can let them overlap vertically */
    if ( xprect->width > xprect->height ) {
        *valHt = visHt / 2;
        if ( xprect->width > (xprect->height*2) ) {
            *charHt = visHt;
        } else {
          *charHt = (visHt * 4) / 5;
        }
    } else {
        *valHt = visHt / 3;
        *charHt = visHt - *valHt;
    }
}

DLSTATIC XP_Bool
DRAW_FUNC_NAME(boardBegin)( DrawCtx* p_dctx, 
                            const XP_Rect* XP_UNUSED(rect), 
                            DrawFocusState dfs )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    XP_Bool canDraw = !!hdc && !!dctx->dict;
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
    XP_U16 foreColorIndx;
    XP_Bool isPending = (flags & CELL_HIGHLIGHT) != 0;
    XP_Bool isFocussed = TREAT_AS_CURSOR( dctx, flags );
    XP_Bool isDragSrc = (flags & CELL_DRAGSRC) != 0;
    HFONT oldFont;
    const FontCacheEntry* fce;

    XP_ASSERT( !!hdc );

    XPRtoRECT( &rt, xprect );
    ++rt.bottom;
    ++rt.right;
    ceClipToRect( hdc, &rt );

    Rectangle( hdc, rt.left, rt.top, rt.right, rt.bottom );
    textRect = rt;

    InsetRect( &rt, 1, 1 );
    ceClipToRect( hdc, &rt );

    fce = ceGetSizedFont( dctx, XP_MIN(xprect->height-CELL_BORDER,xprect->width), 
                          RFONTS_CELL );
    oldFont = SelectObject( hdc, fce->setFont );

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

        MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, letters, -1,
                             widebuf, VSIZE(widebuf) );
	
        SetTextColor( hdc, dctx->globals->appPrefs.colors[foreColorIndx] );
#ifdef TARGET_OS_WIN32
        MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, letters, -1,
                             widebuf, VSIZE(widebuf) );
#endif
        ceDrawTextClipped( hdc, widebuf, -1, XP_FALSE, fce, xprect->left+1, 
                           xprect->top+2, xprect->width, DT_CENTER );
    } else if ( !isDragSrc && !!bitmap ) {
        makeAndDrawBitmap( dctx, hdc, &rt, XP_TRUE,
                           foreColorIndx, (CEBitmapInfo*)bitmap );
    } else if ( (flags&CELL_ISSTAR) != 0 ) {
        InsetRect( &textRect, 1, 1 );
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
    XP_Bool canDraw = !!hdc && !!dctx->dict;
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
    XP_Bool highlighted = XP_FALSE;
    XP_Bool isFocussed = TREAT_AS_CURSOR(dctx,flags);
    XP_Bool isEmpty = (flags & CELL_ISEMPTY) != 0;

    XPRtoRECT( &rt, xprect );
    ceClipToRect( hdc, &rt );
    ceClearToBkground( dctx, xprect );

    if ( !isEmpty || isFocussed ) {
        XP_U16 backIndex = isFocussed? CE_FOCUS_COLOR : CE_TILEBACK_COLOR;

        SetBkColor( hdc, dctx->globals->appPrefs.colors[backIndex] );

        InsetRect( &rt, 1, 1 );
        Rectangle(hdc, rt.left, rt.top, rt.right, rt.bottom); /* draw frame */
        InsetRect( &rt, 1, 1 );
/*         ceClipToRect( hdc, &rt ); */

        if ( !isEmpty ) {
            index = getPlayerColor(dctx->trayOwner);
            SetTextColor( hdc, dctx->globals->appPrefs.colors[index] );

            /* For some reason Rectangle isn't using the background brush to
               fill, so FillRect has to get called after Rectangle.  Need to
               call InsetRect either way to put chars in the right place. */
            highlighted = (flags & CELL_HIGHLIGHT) != 0;
            if ( highlighted ) {
                /* draw thicker hilight frame */
                Rectangle( hdc, rt.left, rt.top, rt.right, rt.bottom );
                InsetRect( &rt, 1, 1 );
            }
        }

        FillRect( hdc, &rt, dctx->brushes[backIndex] );

        if ( !isEmpty ) {
            const FontCacheEntry* fce;
            /* Dumb to calc these when only needed once.... */
            XP_U16 valHt, charHt;
            ceGetCharValHts( xprect, &charHt, &valHt );

            if ( !highlighted ) {
                InsetRect( &rt, 1, 1 );
            }

            if ( !!letters ) {
                fce = ceGetSizedFont( dctx, charHt, RFONTS_TRAY );
                HFONT oldFont = SelectObject( hdc, fce->setFont );
                MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, letters, -1,
                                     widebuf, VSIZE(widebuf) );

                ceDrawTextClipped( hdc, widebuf, -1, XP_TRUE, fce, 
                                   xprect->left + 4, xprect->top + 4, 
                                   xprect->width, DT_LEFT );
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
                fce = ceGetSizedFont( dctx, valHt, RFONTS_TRAYVAL );
                HFONT oldFont = SelectObject( hdc, fce->setFont );
                swprintf( widebuf, L"%d", val );

                ceDrawTextClipped( hdc, widebuf, -1, XP_TRUE, fce, 
                                   xprect->left + 4,
                                   xprect->top + xprect->height - 4 - fce->glyphHt,
                                   xprect->width - 8, DT_RIGHT );
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
DRAW_FUNC_NAME(scoreBegin)( DrawCtx* p_dctx, const XP_Rect* xprect, 
                            XP_U16 XP_UNUSED(numPlayers), 
                            DrawFocusState XP_UNUSED(dfs) )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    XP_ASSERT( !!globals->hdc );
    SetBkColor( globals->hdc, dctx->globals->appPrefs.colors[CE_BKG_COLOR] );

    dctx->scoreIsVertical = xprect->height > xprect->width;

    /* I don't think the clip rect's set at this point but drawing seems fine
       anyway.... ceClearToBkground() is definitely needed here. */
    ceClearToBkground( (CEDrawCtx*)p_dctx, xprect );
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
DRAW_FUNC_NAME(measureRemText)( DrawCtx* p_dctx, const XP_Rect* xprect, 
                                XP_S16 nTilesLeft, 
                                XP_U16* widthP, XP_U16* heightP )
{
    if ( nTilesLeft > 0 ) {
        CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
        CEAppGlobals* globals = dctx->globals;
        HDC hdc = globals->hdc;
        XP_UCHAR buf[16];
        const FontCacheEntry* fce;
        XP_U16 height;
        HFONT oldFont;

        XP_ASSERT( !!hdc );

        formatRemText( nTilesLeft, dctx->scoreIsVertical, buf );

        height = xprect->height-2;
        if ( height > globals->cellHt ) {
            height = globals->cellHt;
        }
        fce = ceGetSizedFont( dctx, height, RFONTS_REM );
        oldFont = SelectObject( hdc, fce->setFont );
        ceMeasureText( dctx, hdc, fce, buf, 0, widthP, heightP );

        (void)SelectObject( hdc, oldFont );
    } else {
        *widthP = *heightP = 0;
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
    HFONT oldFont;
    const FontCacheEntry* fce;
    RECT rt;

    formatRemText( nTilesLeft, dctx->scoreIsVertical, buf );

    fce = ceGetSizedFont( dctx, 0, RFONTS_REM );
    oldFont = SelectObject( hdc, fce->setFont );
    
    XPRtoRECT( &rt, rInner );
    InsetRect( &rt, 1, 1 );
    ceDrawLinesClipped( hdc, fce, buf, XP_TRUE, &rt );

    (void)SelectObject( hdc, oldFont );
} /* ce_draw_drawRemText */

static void
ceFormatScoreText( CEDrawCtx* dctx, const DrawScoreInfo* dsi, 
                   XP_UCHAR* buf, XP_U16 buflen )
{
    XP_UCHAR bullet[] = {'•', '\0'};
    XP_UCHAR optPart[16];
        
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

    if ( dsi->nTilesLeft >= 0 ) {
        sprintf( optPart, "%s%d", dctx->scoreIsVertical? XP_CR : ":", 
                 dsi->nTilesLeft );
    } else {
        optPart[0] = '\0';
    }

    snprintf( buf, buflen, "%s%d%s%s", 
              bullet, dsi->totalScore, optPart, bullet );
} /* ceFormatScoreText */

DLSTATIC void
DRAW_FUNC_NAME(measureScoreText)( DrawCtx* p_dctx, const XP_Rect* xprect,
                                  const DrawScoreInfo* dsi,
                                  XP_U16* widthP, XP_U16* heightP )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    XP_UCHAR buf[32];
    HFONT oldFont;
    const FontCacheEntry* fce;
    XP_U16 fontHt, cellHt;

    cellHt = globals->cellHt;
    if ( !dctx->scoreIsVertical ) {
        cellHt -= SCORE_TWEAK;
    }
    fontHt = xprect->height;
    if ( fontHt > cellHt ) {
        fontHt = cellHt;
    }
    fontHt -= 2;                /* for whitespace top and bottom  */
    if ( !dsi->selected ) {
        fontHt -= 2;            /* use smaller font for non-selected */
    }

    ceFormatScoreText( dctx, dsi, buf, sizeof(buf) );

    fce = ceGetSizedFont( dctx, fontHt,
                          dsi->selected ? RFONTS_SCORE_BOLD:RFONTS_SCORE );
    oldFont = SelectObject( hdc, fce->setFont );

    ceMeasureText( dctx, hdc, fce, buf, 0, widthP, heightP );

    SelectObject( hdc, oldFont );

    if ( dsi->isTurn && dctx->scoreIsVertical ) {
        *heightP += IS_TURN_VPAD * 2;
    }
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
    XP_UCHAR buf[20];
    HFONT oldFont;
    XP_Bool isFocussed = (dsi->flags & CELL_ISCURSOR) != 0;
    XP_U16 bkIndex = isFocussed ? CE_FOCUS_COLOR : CE_BKG_COLOR;
    const FontCacheEntry* fce;

    fce = ceGetSizedFont( dctx, 0, 
                          dsi->selected ? RFONTS_SCORE_BOLD:RFONTS_SCORE );

    oldFont = SelectObject( hdc, fce->setFont );

    SetTextColor( hdc, dctx->globals->
                  appPrefs.colors[getPlayerColor(dsi->playerNum)] );
    SetBkColor( hdc, dctx->globals->appPrefs.colors[bkIndex] );
        
    XPRtoRECT( &rt, rOuter );
    ceClipToRect( hdc, &rt );
    if ( isFocussed ) {
        FillRect( hdc, &rt, dctx->brushes[CE_FOCUS_COLOR] );
    }

    ceFormatScoreText( dctx, dsi, buf, sizeof(buf) );

    XPRtoRECT( &rt, rInner );

    if ( dsi->isTurn && dctx->scoreIsVertical ) {
        Rectangle( hdc, rt.left, rt.top-IS_TURN_VPAD, rt.right, rt.top );
        Rectangle( hdc, rt.left, rt.bottom, rt.right, 
                   rt.bottom + IS_TURN_VPAD );

        rt.top += IS_TURN_VPAD;
        rt.bottom -= IS_TURN_VPAD;
    }

    ceDrawLinesClipped( hdc, fce, buf, XP_TRUE, &rt );

    SelectObject( hdc, oldFont );
} /* ce_draw_score_drawPlayer */

DLSTATIC void
DRAW_FUNC_NAME(score_pendingScore)( DrawCtx* p_dctx, const XP_Rect* xprect, 
                                    XP_S16 score, XP_U16 playerNum,
                                    CellFlags flags )
{
#   define PTS_OFFSET 2
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;

    wchar_t widebuf[5];
    XP_UCHAR buf[5];
    RECT rt;
    XP_Bool focussed = TREAT_AS_CURSOR(dctx,flags);
    XP_U16 bkIndex = focussed ?  CE_FOCUS_COLOR : CE_BKG_COLOR;
    const FontCacheEntry* fce;
    HFONT oldFont;
    XP_U16 valHt;
    XP_U16 charHt;

    ceGetCharValHts( xprect, &charHt, &valHt );

    /* Little Pts first up top */
    fce = ceGetSizedFont( dctx, valHt, RFONTS_TRAYVAL );
    oldFont = SelectObject( hdc, fce->setFont );

    SetTextColor( hdc, 
                  dctx->globals->appPrefs.colors[getPlayerColor(playerNum)] );
    SetBkColor( hdc, dctx->globals->appPrefs.colors[bkIndex] );

    XPRtoRECT( &rt, xprect );
    ceClipToRect( hdc, &rt );
    FillRect( hdc, &rt, dctx->brushes[bkIndex] );

    ceDrawTextClipped( hdc, L"Pts", -1, XP_FALSE, fce, 
                       xprect->left + PTS_OFFSET, xprect->top + PTS_OFFSET,
                       xprect->width - (PTS_OFFSET*2), DT_CENTER );
    

    fce = ceGetSizedFont( dctx, charHt, RFONTS_TRAY );
    (void)SelectObject( hdc, fce->setFont );

    if ( score < 0 ) {
        buf[0] = '?';
        buf[1] = '?';
        buf[2] = '\0';
    } else {
        sprintf( buf, "%d", score );
    }

    MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, buf, -1,
                         widebuf, VSIZE(widebuf) );
    ceDrawTextClipped( hdc, widebuf, -1, XP_TRUE, fce, 
                       xprect->left + PTS_OFFSET, 
                       xprect->top + xprect->height - charHt,
                       xprect->width - (PTS_OFFSET*2), DT_CENTER );

    SelectObject( hdc, oldFont );
#   undef PTS_OFFSET
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
    HFONT oldFont;
    const FontCacheEntry* fce;

    fce = ceGetSizedFont( dctx, 0, RFONTS_SCORE );

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

    oldFont = SelectObject( hdc, fce->setFont );
    ++rt.top;
    ceDrawLinesClipped( hdc, fce, buf, XP_TRUE, &rt );
    SelectObject( hdc, oldFont );

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
    HDC hdc = GetDC(dctx->mainWin);
    ceMeasureText( dctx, hdc, NULL, str, CE_MINIW_PADDING, widthP, heightP );
    *heightP += CE_MINI_V_PADDING;
    *widthP += CE_MINI_H_PADDING;
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

    ceClearFontCache( dctx );

    DeleteObject( dctx->rightArrow );
    DeleteObject( dctx->downArrow );
    DeleteObject( dctx->origin );

#ifndef DRAW_LINK_DIRECT
    XP_FREE( dctx->mpool, p_dctx->vtable );
#endif

    XP_FREE( dctx->mpool, dctx );
} /* ce_draw_destroyCtxt */

DLSTATIC void
DRAW_FUNC_NAME(dictChanged)( DrawCtx* p_dctx, const DictionaryCtxt* dict )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    /* May want to inval font cache if new dict has different tiles from
       old.  Use dict_tilesAreSame(). */
    dctx->dict = dict;
}

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

void
ce_draw_update( CEDrawCtx* dctx )
{
    XP_U16 i;

    for ( i = 0; i < CE_NUM_COLORS; ++i ) {
        if ( !!dctx->brushes[i] ) {
            DeleteObject( dctx->brushes[i] );
        }
        dctx->brushes[i] = CreateSolidBrush(dctx->globals->appPrefs.colors[i]);
    }
} /* ce_drawctxt_update */

void
ce_draw_erase( CEDrawCtx* dctx, const RECT* invalR )
{
    CEAppGlobals* globals = dctx->globals;
    FillRect( globals->hdc, invalR, dctx->brushes[CE_BKG_COLOR] );
}

CEDrawCtx* 
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
    SET_VTABLE_ENTRY( dctx->vtable, draw_dictChanged, ce );

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

    ce_draw_update( dctx );

    dctx->rightArrow = LoadBitmap( globals->hInst, 
                                   MAKEINTRESOURCE(IDB_RIGHTARROW) );
    dctx->downArrow = LoadBitmap( globals->hInst, 
                                  MAKEINTRESOURCE(IDB_DOWNARROW) );
    dctx->origin = LoadBitmap( globals->hInst, 
                               MAKEINTRESOURCE(IDB_ORIGIN) );

    return dctx;
} /* ce_drawctxt_make */

void
ce_draw_toStream( const CEDrawCtx* dctx, XWStreamCtxt* stream )
{
    XP_U16 ii;

    stream_putU8( stream, N_RESIZE_FONTS );
    for ( ii = 0; ii < N_RESIZE_FONTS; ++ii ) {
        const FontCacheEntry* fce = &dctx->fcEntry[ii];
        stream_putU8( stream, fce->indexHt );
        stream_putU8( stream, fce->lfHeight );
        stream_putU8( stream, fce->offset );
        stream_putU8( stream, fce->glyphHt );
    }
}

//#define DROP_CACHE
void
ce_draw_fromStream( CEDrawCtx* dctx, XWStreamCtxt* stream )
{
    XP_U16 ii;
    XP_U16 nEntries;

    ceClearFontCache( dctx );   /* no leaking! */

    nEntries = (XP_U16)stream_getU8( stream );

    for ( ii = 0; ii < nEntries; ++ii ) {
        FontCacheEntry fce;

        fce.indexHt = (XP_U16)stream_getU8( stream );
        fce.lfHeight = (XP_U16)stream_getU8( stream );
        fce.offset = (XP_U16)stream_getU8( stream );
        fce.glyphHt = (XP_U16)stream_getU8( stream );

        /* We need to read from the file no matter how many entries, but only
           populate what we have room for -- in case N_RESIZE_FONTS was
           different when file written. */
#ifndef DROP_CACHE
        if ( ii < N_RESIZE_FONTS ) {
            LOGFONT fontInfo;
/*             XP_LOGF( "using height %d for index %d", fce.lfHeight, ii ); */
            ceFillFontInfo( dctx, &fontInfo, fce.lfHeight );
            fce.setFont = CreateFontIndirect( &fontInfo );
            XP_ASSERT( !!fce.setFont );

            XP_MEMCPY( &dctx->fcEntry[ii], &fce, sizeof(dctx->fcEntry[ii]) );
        }
#endif
    }
} /* ce_draw_fromStream */
