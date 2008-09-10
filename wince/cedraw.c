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
#include "debhacks.h"

#ifndef DRAW_FUNC_NAME
#define DRAW_FUNC_NAME(nam) ce_draw_ ## nam
#endif

//#define MEASURE_OLD_WAY

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


typedef enum { 
    RFONTS_TRAY
    ,RFONTS_TRAYVAL
    ,RFONTS_CELL
    ,RFONTS_PTS

    ,N_RESIZE_FONTS
} RFIndex;

typedef struct _PenColorPair {
    COLORREF ref;
    HGDIOBJ pen;
} PenColorPair;

typedef struct _FontCacheEntry {
    HFONT setFont;
    XP_U16 setFontHt;
    XP_U16 offset;
    XP_U16 actualHt;
} FontCacheEntry;

struct CEDrawCtx {
    DrawCtxVTable* vtable;
    
    HWND mainWin;
    CEAppGlobals* globals;
    const DictionaryCtxt* dict;

    COLORREF prevBkColor;

    HBRUSH brushes[CE_NUM_COLORS];
    PenColorPair pens[CE_NUM_COLORS];

    HFONT selPlayerFont;
    HFONT playerFont;

    FontCacheEntry fcEntry[N_RESIZE_FONTS];

    HBITMAP rightArrow;
    HBITMAP downArrow;
    HBITMAP origin;

    XP_U16 trayOwner;
    XP_U16 miniLineHt;
    XP_Bool scoreIsVertical;
    XP_Bool topFocus;
    XP_Bool beenCleared;

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
    RECT rect; 

    rect.left = left;
    rect.top = top;
    rect.bottom = top + fce->actualHt;
    rect.right = left + width;

    if ( clip ) {
      ceClipToRect( hdc, &rect );
    }
    rect.top -= fce->offset;
/*     XP_LOGF( "%s: drawing left: %ld, top: %ld, right: %ld, bottom: %ld", */
/*              __func__, rect.left, rect.top, rect.right, rect.bottom ); */
    DrawText( hdc, buf, len, &rect, DT_SINGLELINE | DT_TOP | hJust );
} /* ceDrawTextClipped */

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
makeTestBuf( CEDrawCtx* dctx, XP_UCHAR* buf, XP_UCHAR bufLen, RFIndex index )
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
        XP_ASSERT( !!dict && nFaces < bufLen );
        if ( dict_hasBlankTile(dict) ) {
            blank = dict_getBlankTile( dict );
        }
        for ( tile = 0; tile < nFaces; ++tile ) {
            if ( tile != blank ) {
                tiles[nOut++] = tile;
            }
        }
        (void)dict_tilesToString( dict, tiles, nOut, buf, bufLen );
    }
        break;
    case RFONTS_TRAYVAL:
        strcpy( buf, "0" );     /* all numbers the same :-) */
        break;
    case RFONTS_PTS:
        strcpy( buf, "Pts:0?" );
        break;
    case N_RESIZE_FONTS:
        XP_ASSERT(0);
    }
    XP_LOGF( "%s=>%s", __func__, buf );
} /* makeTestBuf */

#ifndef MEASURE_OLD_WAY
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
    for ( done = XP_FALSE, yy = size.cy - 1; yy > maxBottomSeen && !done; --yy ) {
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
ceMeasureGlyphs( CEDrawCtx* dctx, HDC hdc, /* HFONT font,  */wchar_t* str,
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

/*     XP_LOGF( "offset: %d; height: %d", minTopSeen, maxBottomSeen - minTopSeen + 1 ); */
/*     XP_LOGF( "offset came from %d; height from %d", minTopIndex, maxBottomIndex ); */
    *hasMinTop = minTopIndex;
    *hasMaxBottom = maxBottomIndex;
} /* ceMeasureGlyphs */
#endif

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

#ifdef MEASURE_OLD_WAY
static void
ceMeasureTextHt( HFONT font, const char* str, XP_U16* ht, XP_U16* offset )
{
    XP_U16 len = strlen(str);
    wchar_t widebuf[len+1];
    HDC memDC; 
    HBITMAP memBM;
    int xx, yy;
    SIZE size;
    int firstLine = 0, lastLine = -1;

    memDC = CreateCompatibleDC( NULL );
    MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, str, len,
                         widebuf, VSIZE(widebuf) );

    SelectObject( memDC, font );
    GetTextExtentPoint32( memDC, widebuf, len, &size );

    memBM = CreateCompatibleBitmap( memDC, size.cx, size.cy );
    SelectObject( memDC, memBM );

    RECT r = { 0, 0, size.cx, size.cy };
    DrawText( memDC, widebuf, len, &r, DT_TOP | DT_LEFT );


/* int GetDIBits( */
/*   HDC hdc,           // handle to DC */
/*   HBITMAP hbmp,      // handle to bitmap */
/*   UINT uStartScan,   // first scan line to set */
/*   UINT cScanLines,   // number of scan lines to copy */
/*   LPVOID lpvBits,    // array for bitmap bits */
/*   LPBITMAPINFO lpbi, // bitmap data buffer */
/*   UINT uUsage        // RGB or palette index */
/* ); */

/*     for ( yy = 0; yy < size.cy; ++yy ) { */
/*         char tbuf[121] = { '\0' }; */
/*         for ( xx = 0; xx < size.cx; ++xx ) { */
/*             COLORREF ref = GetPixel( memDC, xx, yy ); */
/*             if ( ref != CLR_INVALID ) { */
/*                 strcat( tbuf, ref==0? " " : "x" ); */
/*             } */
/*         } */
/*         XP_LOGF( "line[%.2d] = %s", yy, tbuf ); */
/*     } */

    /* Try replacing GetPixel with something that loads in scan lines.
       Should be faster... */

    /* Get offset first: iterate down from top */
    for ( yy = 0; yy < size.cy && firstLine == 0; ++yy ) {
        for ( xx = 0; xx < size.cx; ++xx ) {
            COLORREF ref = GetPixel( memDC, xx, yy );
            if ( ref == CLR_INVALID ) {
                break;
            } else if ( ref == 0 ) { /* a pixel set! */
                firstLine = yy;
                break;
            }
        }
    }

    /* Get height: iterate up from bottom */
    for ( yy = size.cy - 1; yy >= 0 && lastLine < 0; --yy ) {
        for ( xx = 0; xx < size.cx; ++xx ) {
            COLORREF ref = GetPixel( memDC, xx, yy );
            if ( ref == CLR_INVALID ) {
                break;
            } else if ( ref == 0 ) { /* a pixel set! */
                lastLine = yy;
                break;
            }
        }
    }

    DeleteObject( memBM );
    DeleteDC( memDC );

    *offset = firstLine;
    *ht = lastLine - firstLine + 1;
/*     XP_LOGF( "%s(%s)=>ht: %d; offset: %d", __func__, str, *ht, *offset ); */
} /* ceMeasureTextHt */
#endif

static void
ceBestFitFont( CEDrawCtx* dctx, XP_U16 soughtHeight, RFIndex index, 
               FontCacheEntry* fce )
{
#ifdef MEASURE_OLD_WAY
    HFONT font;
    LOGFONT fontInfo;
    char buf[65];
    XP_U16 offset;
    XP_U16 actualHt;
    XP_U16 trialHt;

/*     XP_LOGF( "%s: calculating for index: %d; height: %d", */
/*              __func__, index, height ); */

    makeTestBuf( dctx, buf, VSIZE(buf), index );

    for ( trialHt = soughtHeight; ; /*++trialHt*/ ) {
        XP_MEMSET( &fontInfo, 0, sizeof(fontInfo) );
        fontInfo.lfHeight = trialHt;
        HFONT testFont = CreateFontIndirect( &fontInfo );
        XP_U16 testOffset, testHt;

        /*             XP_LOGF( "%s: looking for ht %d with testht %d", __func__, height, trialHt ); */
        ceMeasureTextHt( testFont, buf, &testHt, &testOffset );
        if ( testHt > soughtHeight ) {
            /* we've gone too far, so choose last that fit!!! */
            XP_ASSERT( !!font );
            DeleteObject( testFont );
            break;
            /*             } else if ( trialHt == height /\* first time through *\/ */
            /*                         && testOffset > 0 ) { /\* for safety *\/ */
            /*                 trialHt += testOffset; */
        } else {
            ++trialHt;
        }
        DeleteObject( font );
        font = testFont;
        offset = testOffset;
        actualHt = testHt;
    }

    if ( !!fce->setFont ) {
        DeleteObject( fce->setFont );
    }

    fce->setFont = font;
    fce->setFontHt = soughtHeight;
    fce->offset = offset;
    fce->actualHt = actualHt;
#else
    wchar_t widebuf[65];
    XP_U16 len;
    XP_U16 hasMinTop, hasMaxBottom;
    XP_Bool firstPass;
    HBRUSH white = dctx->brushes[CE_WHITE_COLOR];
    HDC memDC = CreateCompatibleDC( NULL );
    HBITMAP memBM;
    XP_U16 testSize;

    XP_LOGF( "%s(index=%d)", __func__, index );

    char sample[65];
    makeTestBuf( dctx, sample, VSIZE(sample), index );
    len = strlen(sample);
    MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, sample, len,
                         widebuf, len );

    memBM = CreateCompatibleBitmap( memDC, 64, 64 );
    SelectObject( memDC, memBM );

    for ( firstPass = XP_TRUE, testSize = soughtHeight*2; ; --testSize ) {
        LOGFONT fontInfo;
        XP_MEMSET( &fontInfo, 0, sizeof(fontInfo) );
        fontInfo.lfHeight = testSize;
        HFONT testFont = CreateFontIndirect( &fontInfo );
        if ( !!testFont ) {
            XP_U16 thisHeight, top, bottom;

            SelectObject( memDC, testFont );

            /* first time, measure all of them to determine which chars have
               high and low points */
            if ( firstPass ) {
                ceMeasureGlyphs( dctx, memDC, widebuf, &hasMinTop,
                                 &hasMaxBottom );
                firstPass = XP_FALSE;
            } 
            /* Thereafter, just measure the two we know about */
            ceMeasureGlyph( memDC, white, sample[hasMinTop], 1000, 0, 
                            &top, &bottom );
            ceMeasureGlyph( memDC, white, sample[hasMaxBottom],
                            top, bottom, &top, &bottom );
            thisHeight = bottom - top + 1;

            if ( thisHeight <= soughtHeight ) { /* got it!!! */
                fce->setFont = testFont;
                fce->setFontHt = soughtHeight;
                fce->offset = top;
                fce->actualHt = thisHeight;
                XP_LOGF( "Looking for %d; PICKED %d", 
                         soughtHeight, thisHeight );
                break;
            }
            DeleteObject( testFont );
            XP_LOGF( "Looking for %d; rejected %d", soughtHeight, thisHeight );
        }
    }

    DeleteObject( memBM );
    DeleteDC( memDC );
#endif
} /* ceBestFitFont */

static const FontCacheEntry* 
ceGetSizedFont( CEDrawCtx* dctx, XP_U16 height, RFIndex index )
{
    FontCacheEntry* fce = &dctx->fcEntry[index];
    if ( (0 != height)            /* 0 means use what we have */
         && fce->setFontHt != height ) {
        ceBestFitFont( dctx, height, index, fce );
    }

    XP_ASSERT( !!fce->setFont ); /* failing... */
    return fce;
} /* ceGetSizedFont */

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

    fce = ceGetSizedFont( dctx, xprect->height - 3, RFONTS_CELL );
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
            /* Dumb to calc these when only needed once.... */
            XP_U16 visHt = xprect->height - 5;
            XP_U16 valHt = visHt / 3;
            XP_U16 charHt = visHt - valHt;

            if ( !highlighted ) {
                InsetRect( &rt, 1, 1 );
            }

            if ( !!letters ) {
                const FontCacheEntry* fce;
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
                const FontCacheEntry* fce;
                fce = ceGetSizedFont( dctx, valHt, RFONTS_TRAYVAL );
                HFONT oldFont = SelectObject( hdc, fce->setFont );
                swprintf( widebuf, L"%d", val );

                ceDrawTextClipped( hdc, widebuf, -1, XP_TRUE, fce, 
                                   xprect->left + 4,
                                   xprect->top + xprect->height - 4 - fce->actualHt,
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
DRAW_FUNC_NAME(scoreBegin)( DrawCtx* p_dctx, const XP_Rect* rect, 
                            XP_U16 XP_UNUSED(numPlayers), 
                            DrawFocusState XP_UNUSED(dfs) )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    XP_ASSERT( !!globals->hdc );
    SetBkColor( globals->hdc, dctx->globals->appPrefs.colors[CE_BKG_COLOR] );

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
DRAW_FUNC_NAME(measureRemText)( DrawCtx* p_dctx, const XP_Rect* xprect, 
                                XP_S16 nTilesLeft, 
                                XP_U16* width, XP_U16* height )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;

    if ( nTilesLeft > 0 ) {
        XP_UCHAR buf[16];
        formatRemText( nTilesLeft, dctx->scoreIsVertical, buf );
        ceMeasureText( dctx, buf, CE_REM_PADDING, width, height );
        if ( *width > xprect->width ) {
            *width = xprect->width;
        }
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
    rt.top -= 2;
    drawTextLines( dctx, hdc, buf, CE_REM_PADDING, &rt, 
                   DT_SINGLELINE | DT_LEFT | DT_VCENTER/* | DT_CENTER*/ );
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
    const FontCacheEntry* fce
        = ceGetSizedFont( dctx, (xprect->height-(2*PTS_OFFSET))/2, RFONTS_PTS );
    HFONT oldFont = SelectObject( hdc, fce->setFont );

    SetTextColor( hdc, 
                  dctx->globals->appPrefs.colors[getPlayerColor(playerNum)] );
    SetBkColor( hdc, dctx->globals->appPrefs.colors[bkIndex] );

    XPRtoRECT( &rt, xprect );
    ceClipToRect( hdc, &rt );
    FillRect( hdc, &rt, dctx->brushes[bkIndex] );

    ceDrawTextClipped( hdc, L"Pts", -1, XP_FALSE, fce, 
                       xprect->left + PTS_OFFSET, xprect->top + PTS_OFFSET,
                       xprect->width - (PTS_OFFSET*2), DT_CENTER );

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
                       xprect->top + xprect->height - PTS_OFFSET - fce->actualHt,
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

static void
ceFontsSetup( CEDrawCtx* dctx )
{
    LOGFONT font;

    XP_MEMSET( &font, 0, sizeof(font) );
    wcscpy( font.lfFaceName, L"Arial" );

    font.lfWeight = FW_LIGHT;
    font.lfHeight = CE_SCORE_HEIGHT;
    dctx->playerFont = CreateFontIndirect( &font );

    font.lfWeight = FW_BOLD;
    font.lfHeight = CE_SCORE_HEIGHT+2;
    dctx->selPlayerFont = CreateFontIndirect( &font );
} /* ceFontsSetup */

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

    ceFontsSetup( dctx );

    dctx->rightArrow = LoadBitmap( globals->hInst, 
                                   MAKEINTRESOURCE(IDB_RIGHTARROW) );
    dctx->downArrow = LoadBitmap( globals->hInst, 
                                  MAKEINTRESOURCE(IDB_DOWNARROW) );
    dctx->origin = LoadBitmap( globals->hInst, 
                               MAKEINTRESOURCE(IDB_ORIGIN) );

    return dctx;
} /* ce_drawctxt_make */
