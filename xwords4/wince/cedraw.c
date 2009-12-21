/* -*- fill-column: 77; compile-command: "make TARGET_OS=wince DEBUG=TRUE"; -*- */
/* 
 * Copyright 2000-2009 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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
#include "ceresstr.h"
#include "debhacks.h"
#include "strutils.h"

#ifndef DRAW_FUNC_NAME
#define DRAW_FUNC_NAME(nam) ce_draw_ ## nam
#endif

#define FCE_TEXT_PADDING 2
#define CE_MINI_V_PADDING 6
#define CE_MINI_H_PADDING 8
#define CE_MINIW_PADDING 0
#define CE_TIMER_PADDING -2
#define SCORE_HPAD 2

//#define DRAW_FOCUS_FRAME 1
#ifdef DRAW_FOCUS_FRAME
# define CE_FOCUS_BORDER_WIDTH 6
# define TREAT_AS_CURSOR(d,f) ((((f) & CELL_ISCURSOR) != 0) && !(d)->topFocus )
#else
# define TREAT_AS_CURSOR(d,f) (((f) & CELL_ISCURSOR) != 0)
#endif


typedef enum { NO_FOCUS, SINGLE_FOCUS, TOP_FOCUS } CeFocusLevel;

typedef enum { 
    RFONTS_TRAY
    ,RFONTS_TRAYNOVAL
    ,RFONTS_TRAYVAL
    ,RFONTS_CELL
    ,RFONTS_PTS
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
    XP_U16 indexWidth;             /* width we match on.  0 if we don't care */
    XP_U16 lfHeight;           /* What CE thinks is the "size" of the font we
                                  found to best fill indexHt */
    XP_U16 glyphHt;            /* range from tops to bottoms of glyphs we use */
    XP_U16 offset;
    XP_U16 minLen;
} FontCacheEntry;

typedef struct _CeBMCacheEntry {
    const XP_UCHAR* letters;          /* pointer into dict; don't free!!! */
    HBITMAP bms[2];
} CeBMCacheEntry;

struct CEDrawCtx {
    DrawCtxVTable* vtable;
    
    HWND mainWin;
    CEAppGlobals* globals;
    const DictionaryCtxt* dict;

    XP_UCHAR scoreCache[MAX_NUM_PLAYERS][32];

    COLORREF prevBkColor;

    HBRUSH brushes[CE_NUM_COLORS];
#ifdef DRAW_FOCUS_FRAME
    PenColorPair pens[CE_NUM_COLORS];
#endif
    HGDIOBJ hintPens[MAX_NUM_PLAYERS];

    FontCacheEntry fcEntry[N_RESIZE_FONTS];
    CeBMCacheEntry bmCache[3];  /* 3: max specials in current use */

    HBITMAP rightArrow;
    HBITMAP downArrow;
    HBITMAP origin;
#ifdef XWFEATURE_RELAY
    HBITMAP netArrow;
#endif
    XP_U16 trayOwner;
    XP_U16 miniLineHt;
    XP_U16 maxScoreLen;
    XP_Bool scoreIsVertical;
    XP_Bool topFocus;

    MPSLOT
};

static void ceClearToBkground( CEDrawCtx* dctx, const XP_Rect* rect );
static void ceDrawBitmapInRect( HDC hdc, const RECT* r, HBITMAP bitmap,
                                XP_Bool stretch );
static void ceClipToRect( HDC hdc, const RECT* rt );
static void ceClearFontCache( CEDrawCtx* dctx );

#ifdef DEBUG
const char*
RFI2Str( RFIndex rfi )
{
    const char* str;
# define CASE_STR(c)  case c: str = #c; break
    switch( rfi ) {
        CASE_STR( RFONTS_TRAY );
        CASE_STR( RFONTS_TRAYNOVAL );
        CASE_STR( RFONTS_TRAYVAL );
        CASE_STR( RFONTS_CELL );
        CASE_STR( RFONTS_PTS );
        CASE_STR( RFONTS_SCORE );
        CASE_STR( RFONTS_SCORE_BOLD );
    case N_RESIZE_FONTS:
        XP_ASSERT(0);
        return "<unknown>";
    }
# undef CASE_STR
    return str;
}
#else
# define RFI2Str( rfi ) ""
#endif

static void
XPRtoRECT( RECT* rt, const XP_Rect* xprect )
{
    rt->left = xprect->left;
    rt->top = xprect->top;
    rt->right = rt->left + xprect->width;
    rt->bottom = rt->top + xprect->height;
} /* XPRtoRECT */

static void
ceDeleteObjectNotNull( HGDIOBJ obj )
{
    if ( !!obj ) {
        DeleteObject( obj );
    }
}

#ifdef DRAW_FOCUS_FRAME
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
#endif

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
ceDrawLinesClipped( HDC hdc, const FontCacheEntry* fce, const XP_UCHAR* buf, 
                    UINT codePage, XP_Bool clip, const RECT* bounds )
{
    XP_U16 top = bounds->top;
    XP_U16 width = bounds->right - bounds->left;

    for ( ; ; ) {
        XP_UCHAR* newline = strstr( buf, XP_CR );
        XP_U16 len = newline==NULL? strlen(buf): newline - buf;
        XP_U16 wlen;
        wchar_t widebuf[len];

        wlen = MultiByteToWideChar( codePage, 0, buf, len, widebuf, len );

        ceDrawTextClipped( hdc, widebuf, wlen, clip, fce, bounds->left, top, 
                           width, DT_CENTER );
        if ( !newline ) {
            break;
        }
        top += fce->glyphHt + FCE_TEXT_PADDING;
        buf = newline + XP_STRLEN(XP_CR);	/* skip '\n' */
    }
} /* ceDrawLinesClipped */

/* CE doesn't have FrameRect, so we'll roll our own */
#ifdef DRAW_FOCUS_FRAME
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
#endif

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
makeTestBuf( CEDrawCtx* dctx, RFIndex index, XP_UCHAR* buf, XP_U16 bufLen )
{
    switch( index ) {
    case RFONTS_TRAY:
    case RFONTS_TRAYNOVAL:
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
        strcpy( buf, "10" );     /* all numbers the same :-) */
        break;
    case RFONTS_PTS:
        strcpy( buf, "123p" );
        break;
    case RFONTS_SCORE:
    case RFONTS_SCORE_BOLD:
        strcpy( buf, "0:" );
        break;
    case N_RESIZE_FONTS:
        XP_ASSERT(0);
    }
} /* makeTestBuf */

// #define LOG_BITMAP
#ifdef LOG_BITMAP
static void
logBitmap( const BITMAP* bm, XP_U16 width, XP_U16 height )
{
    int ii, jj, kk;
    XP_U8* ptr = bm->bmBits;

    XP_ASSERT( height <= bm->bmHeight );
    XP_ASSERT( width <= bm->bmWidth );

    for ( ii = 0; ii < height; ++ii ) {
        XP_UCHAR str[width+1];
        int count = 0;
        for ( jj = 0; jj < bm->bmWidthBytes && count < width; ++jj ) {
            XP_U8 byt = ptr[(ii*bm->bmWidthBytes)+jj];
            for ( kk = 0; kk < 8; ++kk ) {
                str[count++] = (byt & 0x80) == 0 ? '.':'+';
                if ( count == width ) {
                    break;
                }
                byt <<= 1;
            }
        }
        XP_ASSERT( count == width );
        str[count] = 0;
        XP_LOGF( "%.2d %s", ii, str );
    }
}
#else
# define logBitmap( a, b, c )
#endif

static XP_Bool
anyBitSet( const XP_U8* rowPtr, XP_S16 rowBits )
{
    XP_Bool set = XP_FALSE;

    XP_ASSERT( ((int)rowPtr & 0x0001) == 0 ); /* ptr to word? */
    for ( ; rowBits > 0; rowBits -= 8, ++rowPtr ) {
        XP_U8 byt = *rowPtr;
        if ( rowBits < 8 ) {
            byt &= (0xFF << (8-rowBits));
        }
        if ( 0 != byt ) {
            set = XP_TRUE;
            break;
        }
    }
    return set;
} /* anyBitSet */

static void
ceMeasureGlyph( HDC hdc, HBITMAP bmp, wchar_t glyph,
                XP_U16 minTopSeen, XP_U16 maxBottomSeen,
                XP_U16* top, XP_U16* bottom )
{
    SIZE size;
    XP_U16 yy;

    GetTextExtentPoint32( hdc, &glyph, 1, &size );
    RECT rect = { 0, 0, size.cx, size.cy };
    DrawText( hdc, &glyph, 1, &rect, DT_TOP | DT_LEFT );

    BITMAP bminfo;
#ifdef DEBUG
    int result =
#endif
        GetObject( bmp, sizeof(bminfo), &bminfo );
    XP_ASSERT( result != 0 );

    /* Find out if this guy's taller than what we have */
    const XP_U8* rowPtr = bminfo.bmBits;
    if ( *rowPtr != 0x00 ) {
#ifdef DEBUG
        wchar_t wbuf[2] = { glyph, 0 };
        XP_LOGW( __func__, wbuf );
#endif
        logBitmap( &bminfo, size.cx, size.cy );
    }
    for ( yy = 0; yy < minTopSeen; ++yy ) {
        if ( anyBitSet( rowPtr, size.cx ) ) {
            *top = yy;
            break;
        }
        rowPtr += bminfo.bmWidthBytes;
    }

    /* Extends lower than seen */
    for ( yy = size.cy - 1, rowPtr = bminfo.bmBits + (bminfo.bmWidthBytes * yy);
          yy > maxBottomSeen; --yy, rowPtr -= bminfo.bmWidthBytes ) {
        if ( anyBitSet( rowPtr, size.cx ) ) {
            *bottom = yy;
            break;
        }
    }
/*     XP_LOGF( "%s: top: %d; bottom: %d", __func__, *top, *bottom ); */
} /* ceMeasureGlyph */

static void
ceMeasureGlyphs( HDC hdc, HBITMAP bmp, wchar_t* str,
                 XP_U16* hasMinTop, XP_U16* hasMaxBottom )
{
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

        ceMeasureGlyph( hdc, bmp, str[ii], minTopSeen, maxBottomSeen,
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
        ceDeleteObjectNotNull( dctx->fcEntry[ii].setFont );
    }
    XP_MEMSET( &dctx->fcEntry, 0, sizeof(dctx->fcEntry) );
}

static void
ceClearBmCache( CEDrawCtx* dctx )
{
    XP_U16 ii;
    CeBMCacheEntry* entry;

    for ( entry = dctx->bmCache, ii = 0; 
          ii < VSIZE(dctx->bmCache); ++ii, ++entry ) {
        ceDeleteObjectNotNull( entry->bms[0] );
        ceDeleteObjectNotNull( entry->bms[1] );
    }

    /* clear letters in case we're changing not deleting */
    XP_MEMSET( &dctx->bmCache[0], 0, sizeof(dctx->bmCache) );
} /* ceClearBmCache */

static void
ceFillFontInfo( const CEDrawCtx* dctx, LOGFONT* fontInfo, 
                XP_U16 height/* , XP_Bool bold */ )
{
    XP_MEMSET( fontInfo, 0, sizeof(*fontInfo) );
    fontInfo->lfHeight = height;
    fontInfo->lfQuality = PROOF_QUALITY;

/*     if ( bold ) { */
/*         fontInfo->lfWeight = FW_BOLD; */
/*     } else { */
/*         fontInfo->lfWeight = FW_LIGHT; */
/*     } */
    wcscpy( fontInfo->lfFaceName, 
#ifdef FORCE_FONT
            FORCE_FONT
#else
            IS_SMARTPHONE(dctx->globals)? L"Segoe Condensed" : L"Tahoma"
#endif
            );
}

static void
ceBestFitFont( CEDrawCtx* dctx, RFIndex index, const XP_U16 soughtHeight, 
               const XP_U16 soughtWidth, /* pass 0 to ignore width */
               const XP_U16 minLen, FontCacheEntry* fce )
{
    wchar_t widebuf[65];
    wchar_t widthBuf[minLen];
    XP_U16 len, wlen;
    XP_U16 hasMinTop = 0, hasMaxBottom = 0; /* make compiler happy */
    XP_Bool firstPass;
    HDC memDC;
    HBITMAP memBM;
    XP_U16 testHeight = soughtHeight * 2;
    HFONT testFont = NULL;
    /* For nextFromHeight and nextFromWidth, 0 means "found" */
    XP_U16 nextFromHeight = soughtHeight;
    XP_U16 nextFromWidth = soughtWidth; /* starts at 0 if width ignored */

    char sample[65];

    makeTestBuf( dctx, index, sample, VSIZE(sample) );
    len = 1 + strlen(sample);
    wlen = MultiByteToWideChar( CP_UTF8, 0, sample, len, 
                                widebuf, VSIZE(widebuf) );
    if ( soughtWidth != 0 ) {
        XP_U16 ii;
        XP_ASSERT( minLen > 0 );
        for ( ii = 0; ii < minLen; ++ii ) {
            widthBuf[ii] = widebuf[0]; /* one char for the sample */
        }
    }

    struct {
        BITMAPINFOHEADER hdr;
        RGBQUAD bmiColors[2];   /* these matter.  Dunno why */
    } bmi_mono;
    XP_MEMSET( &bmi_mono, 0, sizeof(bmi_mono) );

	bmi_mono.hdr.biSize = sizeof(bmi_mono.hdr);
    bmi_mono.hdr.biWidth = testHeight;
    bmi_mono.hdr.biHeight = -testHeight; /* negative means 0,0 at top left */
	bmi_mono.hdr.biBitCount = 1;
	bmi_mono.hdr.biPlanes = 1;
    bmi_mono.hdr.biCompression = BI_RGB;
    bmi_mono.bmiColors[0].rgbRed = 0xFF;
    bmi_mono.bmiColors[0].rgbGreen = 0xFF;
    bmi_mono.bmiColors[0].rgbBlue = 0xFF;

    memDC = CreateCompatibleDC( NULL );
    memBM = CreateDIBSection( memDC, (BITMAPINFO*)&bmi_mono, DIB_RGB_COLORS, 
                              NULL, NULL, 0 );
    SelectObject( memDC, memBM );

    for ( firstPass = XP_TRUE; ;  ) {
        XP_U16 prevHeight = testHeight;
        LOGFONT fontInfo;

        ceDeleteObjectNotNull( testFont );

        ceFillFontInfo( dctx, &fontInfo, testHeight );
        testFont = CreateFontIndirect( &fontInfo );

        if ( !!testFont ) {
            XP_U16 thisHeight = 0, top, bottom;

            SelectObject( memDC, testFont );

            /* first time, measure all of them to determine which chars have
               the set's high and low points.  Note that we need to measure
               even if height already fits because that's how glyphHt and top
               are calculated. */
            if ( nextFromHeight > 0 || nextFromWidth > 0 ) {
                if ( firstPass ) {
                    ceMeasureGlyphs( memDC, memBM, widebuf, 
                                     &hasMinTop, &hasMaxBottom );
                    firstPass = XP_FALSE;
                } 
                /* Thereafter, just measure the two we know about */
                ceMeasureGlyph( memDC, memBM, widebuf[hasMinTop], 1000, 0, 
                                &top, &bottom );
                if ( hasMaxBottom != hasMinTop ) {
                    ceMeasureGlyph( memDC, memBM, widebuf[hasMaxBottom],
                                    top, bottom, &top, &bottom );
                }

                thisHeight = bottom - top + 1;

                if ( nextFromHeight > 0 ) { /* skip if height already fits */
                    /* If we don't meet the height test, continue based on
                       best guess at height.  Only after height looks ok do
                       we try based on width */

                    if ( thisHeight > soughtHeight ) {   /* height too big... */ 
                        nextFromHeight = 1 + ((testHeight * soughtHeight)
                                              / thisHeight);
                    } else {
                        nextFromHeight = 0;               /* we're good */
                    }
                }
            }

            if ( (soughtWidth > 0) && (nextFromWidth > 0) ) {
                SIZE size;
                GetTextExtentPoint32( memDC, widthBuf, minLen, &size );

                if ( size.cx > soughtWidth ) { /* width too big... */
                    nextFromWidth = 1 + ((testHeight * soughtWidth) / size.cx);
                } else {
                    nextFromWidth = 0;
                }
            }
                
            if ( (0 == nextFromWidth) && (0 == nextFromHeight) ) {
                /* we get here, we have our font */
                fce->setFont = testFont;
                fce->indexHt = soughtHeight;
                fce->indexWidth = soughtWidth;
                fce->lfHeight = testHeight;
                fce->offset = top;
                fce->glyphHt = thisHeight;
                fce->minLen = minLen;
                break;
            } 

            if ( nextFromHeight > 0 ) {
                testHeight = nextFromHeight;
            }
            if ( nextFromWidth > 0 && nextFromWidth < testHeight ) {
                testHeight = nextFromWidth;
            }
            if ( testHeight >= prevHeight ) {
                /* guarantee progress regardless of rounding errors */
                testHeight = prevHeight - 1;
            }
        }
    }

    DeleteObject( memBM );
    DeleteDC( memDC );
} /* ceBestFitFont */

static const FontCacheEntry* 
ceGetSizedFont( CEDrawCtx* dctx, RFIndex index, XP_U16 height, XP_U16 width, 
                XP_U16 minLen )
{
    FontCacheEntry* fce = &dctx->fcEntry[index];
    if ( (0 != height)            /* 0 means use what we have */
         && ( (fce->indexHt != height)
              || (fce->indexWidth != width) 
              || (fce->minLen != minLen) ) ) {
        /* XP_LOGF( "%s: no match for %s (have %d, want %d (width %d, minLen %d) " */
        /*          "so recalculating",  */
        /*          __func__, RFI2Str(index), fce->indexHt, height, width, minLen ); */
        ceBestFitFont( dctx, index, height, width, minLen, fce );
    }

    XP_ASSERT( !!fce->setFont );
    return fce;
} /* ceGetSizedFont */

static HBITMAP
checkBMCache( CEDrawCtx* dctx, HDC hdc, const XP_UCHAR* letters, 
              const XP_U16 index, const XP_Bitmaps* bitmaps, XP_Bool* cached )
{
    HBITMAP bm = NULL;
    CeBMCacheEntry* entry = NULL;
    XP_Bool canCache = XP_FALSE;

    XP_ASSERT( !!letters );
    XP_ASSERT( index < 2 );

    XP_U16 ii;
    for ( ii = 0, entry = dctx->bmCache; ii < VSIZE(dctx->bmCache); 
          ++ii, ++entry ) {
        if ( !entry->letters ) { /* available */
            entry->letters = letters;
            canCache = XP_TRUE;
            break;
        } else if ( entry->letters == letters ) {
            canCache = XP_TRUE;
            bm = entry->bms[index]; /* may be null */
            break;
        }
    }

#ifdef DEBUG
    if ( !canCache ) {
        XP_WARNF( "%s: unable to cache bitmap for %s", __func__, letters );
    }
#endif

    if ( !bm ) {
        const CEBitmapInfo* info = (const CEBitmapInfo*)(bitmaps->bmps[index]);
        XP_U16 row, col;
        COLORREF black = dctx->globals->appPrefs.colors[CE_BLACK_COLOR];
        COLORREF white = dctx->globals->appPrefs.colors[CE_WHITE_COLOR];
        HDC tmpDC;
        XP_U8* bits = info->bits;
        XP_U16 nCols = info->nCols;
        XP_U16 nRows = info->nRows;
        XP_U16 rowBytes = (info->nCols + 7) / 8; /* rows are 8-bit padded */

        bm = CreateBitmap( info->nCols, info->nRows, 1, 1, NULL );
        tmpDC = CreateCompatibleDC( hdc );
        SelectObject( tmpDC, bm );
        for ( row = 0; row < nRows; ++row ) {
            for ( col = 0; col < nCols; ++col ) {
                /* Optimize me... */
                XP_U8 byt = bits[col / 8];
                XP_Bool set = (byt & (0x80 >> (col % 8))) != 0;
                (void)SetPixel( tmpDC, col, row, set ? black:white );
            }
            bits += rowBytes;
        }
        DeleteDC( tmpDC );

#ifdef DEBUG
        {
            BITMAP bmp;
            int nBytes = GetObject( bm, sizeof(bmp), &bmp );
            XP_ASSERT( nBytes > 0 );
            XP_ASSERT( bmp.bmWidth == nCols );
            XP_ASSERT( bmp.bmHeight == nRows );
        }
#endif
        if ( canCache && entry->bms[index] != bm ) {
            XP_ASSERT( !entry->bms[index] || (entry->bms[index] == bm) );
            entry->bms[index] = bm;
        }
    }

    *cached = canCache;
    return bm;
} /* checkBMCache */

static void
makeAndDrawBitmap( CEDrawCtx* dctx, HDC hdc, const RECT* bnds, 
                   const XP_UCHAR* letters, const XP_Bitmaps* bitmaps, 
                   XP_Bool center )
{
    XP_Bool cached;
    XP_U16 index;
    RECT lrt = *bnds;
    XP_U16 useSize;

    SIZE size;
    GetTextExtentPoint32( hdc, L"M", 1, &size );
    useSize = size.cx;

    for ( index = bitmaps->nBitmaps - 1; ; --index ) {
        const CEBitmapInfo* info = (const CEBitmapInfo*)(bitmaps->bmps[index]);
        if ( info->nCols <= useSize && info->nRows <= useSize ) {
            break;
        } else if ( index == 0 ) {
            /* none fits */
            useSize = XP_MAX( info->nCols, info->nRows );
            break;
        }
    }

    if ( center ) {
        lrt.left += ((lrt.right - lrt.left) - useSize) / 2;
        lrt.top += ((lrt.bottom - lrt.top) - useSize) / 2;
    }
    lrt.right = lrt.left + useSize;
    lrt.bottom = lrt.top + useSize;

    HBITMAP bm = checkBMCache( dctx, hdc, letters, index, bitmaps, &cached );

    ceDrawBitmapInRect( hdc, &lrt, bm, XP_TRUE );

    if ( !cached ) {
        DeleteObject( bm );
    }
} /* makeAndDrawBitmap */

static void
ceMeasureText( CEDrawCtx* dctx, HDC hdc, const FontCacheEntry* fce, 
               const XP_UCHAR* str, XP_S16 padding,
               XP_U16* widthP, XP_U16* heightP )
{
    XP_U16 height = 0;
    XP_U16 maxWidth = 0;

    for ( ; ; ) {
        wchar_t widebuf[64];
        XP_UCHAR* nextStr = strstr( str, XP_CR );
        XP_U16 len = nextStr==NULL? strlen(str): nextStr - str;
        XP_U16 wlen;
        SIZE size;

        XP_ASSERT( nextStr != str );

        wlen = MultiByteToWideChar( CP_UTF8, 0, str, len,
                                    widebuf, VSIZE(widebuf) );
        (void)GetTextExtentPoint32( hdc, widebuf, wlen, &size );

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
        XP_U16 len, wlen;

        if ( nextStr == NULL ) {
            len = XP_STRLEN(text);
        } else {
            len = nextStr - text;
        }

        wlen = MultiByteToWideChar( CP_UTF8, 0, text, len,
                                    widebuf, VSIZE(widebuf) );

        textRt.bottom = textRt.top + dctx->miniLineHt;

        DrawText( hdc, widebuf, wlen, &textRt, flags );

        if ( nextStr == NULL ) {
            break;
        }
        textRt.top = textRt.bottom + padding;
        text = nextStr + XP_STRLEN(XP_CR);
    }
} /* drawTextLines */

static void
ceGetCharValHts( const CEDrawCtx* dctx, const XP_Rect* xprect, 
                 XP_Bool valHidden, XP_U16* charHtP, XP_U16* valHtP )
{
    XP_U16 visHt = xprect->height - TRAY_BORDER;
    XP_U16 visWidth = xprect->width - 5; /* ??? */
    XP_U16 minHt;
    XP_U16 charHt, valHt;

    /* if tiles are wider than tall we can let them overlap vertically */
    if ( valHidden ) {
        valHt = 0;
        charHt = visHt;
    } else if ( visWidth > visHt ) {
        if ( visWidth > (visHt*2) ) {
            charHt = visHt;
            valHt = (3*visHt) / 4;
        } else {
            charHt = (visHt * 4) / 5;
            valHt = visHt / 2;
        }
    } else {
        valHt = visHt / 3;
        charHt = visHt - valHt;
    }

    minHt = dctx->globals->cellHt - CELL_BORDER;
    if ( charHt < minHt ) {
        charHt = minHt;
    }

    if ( !valHidden && (valHt < MIN_CELL_HEIGHT - CELL_BORDER - 2) ) {
        valHt = MIN_CELL_HEIGHT - CELL_BORDER - 2;
    }
    *valHtP = valHt;
    *charHtP = charHt;
/*     XP_LOGF( "%s(width:%d, height:%d)=>char: %d, val:%d", __func__, */
/*              xprect->width, xprect->height, *charHt, *valHt ); */
} /* ceGetCharValHts */

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
DRAW_FUNC_NAME(objFinished)( DrawCtx* XP_UNUSED(p_dctx), 
                             BoardObjectType XP_UNUSED(typ), 
                             const XP_Rect* XP_UNUSED(rect), 
                             DrawFocusState XP_UNUSED(dfs) )
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
        return CE_PLAYER0_COLOR + player;
    }
} /* getPlayerColor */

static void
ceDrawLine( HDC hdc, XP_S32 x1, XP_S32 y1, XP_S32 x2, XP_S32 y2 )
{
    MoveToEx( hdc, x1, y1, NULL );
    LineTo( hdc, x2, y2);
} /* ceDrawLine */

static void
ceDrawHintBorders( CEDrawCtx* dctx, const XP_Rect* xprect, HintAtts hintAtts )
{
    if ( hintAtts != HINT_BORDER_NONE && hintAtts != HINT_BORDER_CENTER ) {
        RECT rt;
        HDC hdc = dctx->globals->hdc;
        HGDIOBJ pen, oldPen;

        pen = dctx->hintPens[dctx->trayOwner];
        if ( !pen ) {
            COLORREF ref = dctx->globals->appPrefs.colors[CE_PLAYER0_COLOR
                                                          + dctx->trayOwner];
            pen = CreatePen( PS_SOLID, 4, ref ); /* 4 must be an even number
                                                    thx to clipping */
            dctx->hintPens[dctx->trayOwner] = pen;
        }

        XPRtoRECT( &rt, xprect );
        InsetRect( &rt, 1, 1 );

        oldPen = SelectObject( hdc, pen );

        if ( (hintAtts & HINT_BORDER_LEFT) != 0 ) {
            ceDrawLine( hdc, rt.left+1, rt.top, rt.left+1, rt.bottom+1 );
        }
        if ( (hintAtts & HINT_BORDER_RIGHT) != 0 ) {
            ceDrawLine( hdc, rt.right, rt.top, rt.right, rt.bottom+1 );
        }
        if ( (hintAtts & HINT_BORDER_TOP) != 0 ) {
            ceDrawLine( hdc, rt.left, rt.top+1, rt.right+1, rt.top+1 );
        }
        if ( (hintAtts & HINT_BORDER_BOTTOM) != 0 ) {
            ceDrawLine( hdc, rt.left, rt.bottom, rt.right+1, rt.bottom );
        }
        (void)SelectObject( hdc, oldPen );
    }
} /* ceDrawHintBorders */

static void
ceSetTextColor( HDC hdc, const CEDrawCtx* dctx, XP_U16 index )
{
    SetTextColor( hdc, dctx->globals->appPrefs.colors[index] );
}

static void
ceSetBkColor( HDC hdc, const CEDrawCtx* dctx, XP_U16 index )
{
    SetBkColor( hdc, dctx->globals->appPrefs.colors[index] );
}

DLSTATIC XP_Bool
DRAW_FUNC_NAME(drawCell)( DrawCtx* p_dctx, const XP_Rect* xprect, 
                          const XP_UCHAR* letters, 
                          const XP_Bitmaps* bitmaps, 
                          Tile XP_UNUSED(tile), XP_S16 owner, 
                          XWBonusType bonus, HintAtts hintAtts,
                          CellFlags flags )
{
#ifndef NO_DRAW
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

    XP_ASSERT( xprect->height == globals->cellHt );
    fce = ceGetSizedFont( dctx, RFONTS_CELL, xprect->height - CELL_BORDER, 
                          0, 0 );
    oldFont = SelectObject( hdc, fce->setFont );

    /* always init to silence compiler warning */
    foreColorIndx = getPlayerColor(owner);

    if ( !isDragSrc && (!!letters || !!bitmaps) ) {
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
        bkIndex = (bonus - BONUS_DOUBLE_LETTER) + CE_BONUS0_COLOR;
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

    ceSetBkColor( hdc, dctx, bkIndex );
    ceSetTextColor( hdc, dctx, foreColorIndx );

    if ( !isDragSrc && !!bitmaps ) {
        makeAndDrawBitmap( dctx, hdc, &rt, letters, bitmaps, XP_TRUE );
    } else if ( !isDragSrc && !!letters && (letters[0] != '\0') ) {
        wchar_t widebuf[4];

        MultiByteToWideChar( CP_UTF8, 0, letters, -1,
                             widebuf, VSIZE(widebuf) );
        ceDrawTextClipped( hdc, widebuf, -1, XP_FALSE, fce, xprect->left+1, 
                           xprect->top+2, xprect->width, DT_CENTER );
    } else if ( (flags&CELL_ISSTAR) != 0 ) {
        ceSetTextColor( hdc, dctx, CE_BLACK_COLOR );
        ceDrawBitmapInRect( hdc, &textRect, dctx->origin, XP_FALSE );
    }

    ceDrawHintBorders( dctx, xprect, hintAtts );

    SelectObject( hdc, oldFont );
#endif
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
                  const XP_UCHAR* letters, const XP_Bitmaps* bitmaps,
                  XP_U16 val, CellFlags flags )
{
#ifndef NO_DRAW
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    RECT rt;
    XP_U16 index;
    XP_Bool highlighted = XP_FALSE;
    CeFocusLevel focLevel;
    XP_Bool isEmpty = (flags & CELL_ISEMPTY) != 0;

    if ( 0 != (flags & CELL_ISCURSOR) ) {
        if ( dctx->topFocus ) {
            focLevel = TOP_FOCUS;
        } else {
            focLevel = SINGLE_FOCUS;
        }
    } else {
        focLevel = NO_FOCUS;
    }

    XPRtoRECT( &rt, xprect );
    ceClipToRect( hdc, &rt );
    FillRect( hdc, &rt, dctx->brushes[focLevel == TOP_FOCUS?CE_FOCUS_COLOR:CE_BKG_COLOR] );

    if ( !isEmpty || focLevel == SINGLE_FOCUS ) { /* don't draw anything unless SINGLE_FOCUS */
        XP_U16 backIndex = focLevel == NO_FOCUS? CE_TILEBACK_COLOR : CE_FOCUS_COLOR;

        ceSetBkColor( hdc, dctx, backIndex );

        InsetRect( &rt, 1, 0 );
        ++rt.top;                                              /* inset top but not bottom */
        Rectangle( hdc, rt.left, rt.top, rt.right, rt.bottom); /* draw frame */
        InsetRect( &rt, 1, 1 );

        if ( !isEmpty ) {
            index = getPlayerColor(dctx->trayOwner);
            ceSetTextColor( hdc, dctx, index );

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
            wchar_t widebuf[4];
            /* Dumb to calc these when only needed once.... */
            XP_U16 valHt, charHt;
            XP_Bool valHidden = 0 != (flags & CELL_VALHIDDEN);

            if ( !highlighted ) {
                InsetRect( &rt, 1, 1 );
            }

            ceGetCharValHts( dctx, xprect, valHidden, &charHt, &valHt );
            fce = ceGetSizedFont( dctx, valHidden ? RFONTS_TRAYNOVAL:RFONTS_TRAY,
                                  charHt, 0, 0 );

            if ( !!bitmaps || !!letters ) {
                HFONT oldFont = SelectObject( hdc, fce->setFont );
                XP_U16 wlen = MultiByteToWideChar( CP_UTF8, 0, letters,
                                                   -1, widebuf, VSIZE(widebuf) );

                /* see if there's room to use text instead of bitmap */
                if ( !!bitmaps && valHidden ) {
                    SIZE size;
                    GetTextExtentPoint32( hdc, widebuf, wlen - 1, /* drop null */
                                          &size );
                    if ( size.cx < (rt.right - rt.left) ) {
                        bitmaps = NULL; /* use the letters instead */
                    }
                }

                if ( !!bitmaps ) {
                    makeAndDrawBitmap( dctx, hdc, &rt, letters, bitmaps,
                                       valHidden );
                } else if ( !!letters ) {
                    ceDrawTextClipped( hdc, widebuf, -1, XP_TRUE, fce, 
                                       xprect->left + 4, xprect->top + 4, 
                                       xprect->width - 8, 
                                       valHidden?DT_CENTER:DT_LEFT );
                }
                SelectObject( hdc, oldFont );
            }

            if ( !valHidden ) {
                fce = ceGetSizedFont( dctx, RFONTS_TRAYVAL, valHt, 0, 0 );
                HFONT oldFont = SelectObject( hdc, fce->setFont );
                swprintf( widebuf, L"%d", val );

                ceDrawTextClipped( hdc, widebuf, -1, XP_TRUE, fce, 
                                   xprect->left + 4,
                                   xprect->top + xprect->height - 4 
                                   - fce->glyphHt,
                                   xprect->width - 8, DT_RIGHT );
                SelectObject( hdc, oldFont );
            }
        }
    }
#endif
} /* drawDrawTileGuts */

DLSTATIC void
DRAW_FUNC_NAME(drawTile)( DrawCtx* p_dctx, const XP_Rect* xprect, 
                          const XP_UCHAR* letters, const XP_Bitmaps* bitmaps,
                          XP_U16 val, CellFlags flags )
{
    drawDrawTileGuts( p_dctx, xprect, letters, bitmaps, val, flags );
} /* ce_draw_drawTile */

#ifdef POINTER_SUPPORT
DLSTATIC void
DRAW_FUNC_NAME(drawTileMidDrag)( DrawCtx* p_dctx, const XP_Rect* xprect, 
                                 const XP_UCHAR* letters, 
                                 const XP_Bitmaps* bitmaps, XP_U16 val, 
                                 XP_U16 owner, CellFlags flags )
{
    if ( draw_trayBegin( p_dctx, xprect, owner, DFS_NONE ) ) {
        drawDrawTileGuts( p_dctx, xprect, letters, bitmaps, val, flags );
    }
} /* ce_draw_drawTile */
#endif

DLSTATIC void
DRAW_FUNC_NAME(drawTileBack)( DrawCtx* p_dctx, const XP_Rect* xprect,
                              CellFlags flags )
{
    drawDrawTileGuts( p_dctx, xprect, "?", NULL, 0, flags | CELL_VALHIDDEN );
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
        FillRect( hdc, &rt, dctx->brushes[dctx->trayOwner+CE_PLAYER0_COLOR] );
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

/* Draw bitmap in rect.  Use StretchBlt to fit it to the rect, but don't
 * change the proportions.
 */
static void
ceDrawBitmapInRect( HDC hdc, const RECT* rect, HBITMAP bitmap, 
                    XP_Bool stretch )
{
    BITMAP bmp;
    int nBytes;

    XP_ASSERT( !!bitmap );
    nBytes = GetObject( bitmap, sizeof(bmp), &bmp );
    XP_ASSERT( nBytes > 0 );
    if ( nBytes == 0 ) {
        logLastError( "ceDrawBitmapInRect:GetObject" );
    } else {
        int left = rect->left;
        int top = rect->top;
        XP_U16 width = rect->right - left;
        XP_U16 height = rect->bottom - top;
        HDC tmpDC;

        if ( !stretch ) {
            /* Find dimensions that'll fit multiplying an integral number of
               times */
            XP_U16 factor = XP_MIN( width/bmp.bmWidth, height/bmp.bmHeight );
            if ( factor == 0 ) {
                XP_LOGF( "%s: cell at %dx%d too small for bitmap at %ldx%ld",
                         __func__, width, height, bmp.bmWidth, bmp.bmHeight );
                factor = 1;
            }

            width = bmp.bmWidth * factor;
            height = bmp.bmHeight * factor;
        }

        tmpDC = CreateCompatibleDC( hdc );
        SelectObject( tmpDC, bitmap );

        (void)IntersectClipRect( tmpDC, left, top, rect->right, rect->bottom );

        left += ((rect->right - left) - width) / 2;
        top += ((rect->bottom - top) - height) / 2;

        StretchBlt( hdc, left, top, width, height, 
                    tmpDC, 0, 0, bmp.bmWidth, bmp.bmHeight, SRCCOPY );
        DeleteDC( tmpDC );
    }
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
        bkIndex = cursorBonus - BONUS_DOUBLE_LETTER + CE_BONUS0_COLOR;
    }
    FillRect( hdc, &rt, dctx->brushes[bkIndex] );
    ceSetBkColor( hdc, dctx, bkIndex );
    ceSetTextColor( hdc, dctx, CE_BLACK_COLOR );

    ceDrawBitmapInRect( hdc, &rt, cursor, XP_FALSE );

    ceDrawHintBorders( dctx, xprect, hintAtts );
} /* ce_draw_drawBoardArrow */

DLSTATIC void
DRAW_FUNC_NAME(scoreBegin)( DrawCtx* p_dctx, const XP_Rect* xprect, 
                            XP_U16 numPlayers, const XP_S16* const scores,
                            XP_S16 remCount, DrawFocusState XP_UNUSED(dfs) )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    RECT rt;
    HDC hdc = dctx->globals->hdc;
    XP_U16 ii;
    XP_U16 scoreLen = 0;

    XP_ASSERT( !!hdc );
    ceSetBkColor( hdc, dctx, CE_BKG_COLOR );

    dctx->scoreIsVertical = xprect->height > xprect->width;

    for ( ii = 0; ii <= numPlayers; ++ii ) {
        XP_UCHAR buf[8];
        XP_S16 num = ii < numPlayers? scores[ii] : remCount;
        XP_U16 len = XP_SNPRINTF( buf, VSIZE(buf), "%d", num );
        if ( len > scoreLen ) {
            scoreLen = len;
        }
    }
    dctx->maxScoreLen = scoreLen;

    /* I don't think the clip rect's set at this point but drawing seems fine
       anyway.... ceClearToBkground() is definitely needed here. */
    XPRtoRECT( &rt, xprect );
    ceClipToRect( hdc, &rt );
    ceClearToBkground( (CEDrawCtx*)p_dctx, xprect );
} /* ce_draw_scoreBegin */

static void
formatRemText( XP_S16 nTilesLeft, XP_UCHAR* buf, XP_U16 bufLen )
{
    XP_ASSERT( nTilesLeft > 0 );
    XP_SNPRINTF( buf, bufLen, "%d", nTilesLeft );
} /* formatRemText */

static void
scoreFontDims( CEDrawCtx* dctx, const XP_Rect* rect,
               XP_U16* width, XP_U16* height )
{
    XP_U16 fontHt, cellHt;
    fontHt = rect->height;

    cellHt = dctx->globals->cellHt;
    if ( !dctx->scoreIsVertical ) {
        cellHt -= SCORE_TWEAK;
    }

    if ( fontHt > cellHt ) {
        fontHt = cellHt;
    }
    fontHt -= 2;                /* for whitespace top and bottom  */
    *height = fontHt;
    *width = rect->width - 2;
}

DLSTATIC void
DRAW_FUNC_NAME(measureRemText)( DrawCtx* p_dctx, const XP_Rect* xprect, 
                                XP_S16 nTilesLeft, 
                                XP_U16* widthP, XP_U16* heightP )
{
    if ( nTilesLeft > 0 ) {
        CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
        CEAppGlobals* globals = dctx->globals;
        HDC hdc = globals->hdc;
        XP_UCHAR buf[4];
        const FontCacheEntry* fce;
        XP_U16 width, height;
        HFONT oldFont;

        XP_ASSERT( !!hdc );

        formatRemText( nTilesLeft, buf, VSIZE(buf) );

        scoreFontDims( dctx, xprect, &width, &height );

        fce = ceGetSizedFont( dctx, RFONTS_SCORE, height, width, 
                              dctx->maxScoreLen );
        oldFont = SelectObject( hdc, fce->setFont );
        ceMeasureText( dctx, hdc, fce, buf, 0, widthP, heightP );

        (void)SelectObject( hdc, oldFont );
    } else {
        *widthP = *heightP = 0;
    }
} /* ce_draw_measureRemText */

DLSTATIC void
DRAW_FUNC_NAME(drawRemText)( DrawCtx* p_dctx, const XP_Rect* rInner, 
                             const XP_Rect* rOuter, 
                             XP_S16 nTilesLeft, XP_Bool focussed )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    XP_UCHAR buf[4];
    HFONT oldFont;
    const FontCacheEntry* fce;
    RECT rt;
    XP_U16 bkColor;

    formatRemText( nTilesLeft, buf, VSIZE(buf) );

    XPRtoRECT( &rt, rOuter );

    ceSetTextColor( hdc, dctx, CE_BLACK_COLOR );
    bkColor = focussed ? CE_FOCUS_COLOR : CE_TILEBACK_COLOR;
    ceSetBkColor( hdc, dctx, bkColor );
    FillRect( hdc, &rt, dctx->brushes[bkColor] );

    XPRtoRECT( &rt, rInner );
    fce = ceGetSizedFont( dctx, RFONTS_SCORE, 0, 0, 0 );
    oldFont = SelectObject( hdc, fce->setFont );
    
    ceDrawLinesClipped( hdc, fce, buf, CP_UTF8, XP_TRUE, &rt );

    (void)SelectObject( hdc, oldFont );
} /* ce_draw_drawRemText */

static void
ceFormatScoreText( CEDrawCtx* dctx, const DrawScoreInfo* dsi, XP_U16 nameLen )
{
    XP_UCHAR optPart[16];
    XP_UCHAR name[nameLen+1+1];
    const XP_UCHAR* div = dctx->scoreIsVertical? XP_CR : ":";
        
    /* For a horizontal scoreboard, we want [name:]300[:6]
     * For a vertical, it's
     *
     *     [name]
     *      300
     *      [6]
     */

    while ( nameLen >= 1 ) {
        snprintf( name, nameLen+1, "%s", dsi->name );
        if ( name[nameLen-1] == ' ' ) { /* don't end with space */
            --nameLen;
        } else {
            XP_STRCAT( name, div );
            break;
        }
    }
    if ( nameLen < 1 ) {
        name[0] = '\0';
    }

    if ( dsi->nTilesLeft >= 0 ) {
        sprintf( optPart, "%s%d", div, dsi->nTilesLeft );
    } else {
        optPart[0] = '\0';
    }

    snprintf( dctx->scoreCache[dsi->playerNum], 
              VSIZE(dctx->scoreCache[dsi->playerNum]), 
              "%s%d%s", name, dsi->totalScore, optPart );
} /* ceFormatScoreText */

DLSTATIC void
DRAW_FUNC_NAME(measureScoreText)( DrawCtx* p_dctx, const XP_Rect* xprect,
                                  const DrawScoreInfo* dsi,
                                  XP_U16* widthP, XP_U16* heightP )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;
    HFONT oldFont;
    const FontCacheEntry* fce;
    XP_U16 fontWidth, fontHt;
    XP_U16 nameLen;
    XP_U16 targetWidth = xprect->width;

    scoreFontDims( dctx, xprect, &fontWidth, &fontHt );

    if ( !dctx->scoreIsVertical ) {
        targetWidth -= (SCORE_HPAD * 2);
    }

    fce = ceGetSizedFont( dctx, dsi->selected ? RFONTS_SCORE_BOLD:RFONTS_SCORE,
                          fontHt, fontWidth, dctx->maxScoreLen );
    nameLen = dsi->isTurn? XP_STRLEN(dsi->name) : 0;
    oldFont = SelectObject( hdc, fce->setFont );

    /* Iterate until score line fits.  This currently fails when the score
       itself won't fit (veritical scoreboard on large screen) because it's
       the name that's being shrunk but the width is eventually determined by
       the score that isn't shrinking.  Need to bound font size by width of
       "000" in vertical-scoreboard case.  PENDING */

    for ( ; ; ) {
        XP_U16 width, height;
        ceFormatScoreText( dctx, dsi, nameLen );

        ceMeasureText( dctx, hdc, fce, dctx->scoreCache[dsi->playerNum], 0, 
                       &width, &height );

        if ( width <= targetWidth && height <= xprect->height ) {
            if ( !dctx->scoreIsVertical ) {
                width += (SCORE_HPAD * 2);
            }
            *widthP = width;
            *heightP = height;
            break;
        }

        // XP_ASSERT( dsi->isTurn );  firing.  See comment above.
        if ( --nameLen == 0 ) {
            break;
        }
    }

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
    HFONT oldFont;
    XP_Bool isFocussed = (dsi->flags & CELL_ISCURSOR) != 0;
    XP_U16 bkIndex = isFocussed ? CE_FOCUS_COLOR : CE_BKG_COLOR;
    const FontCacheEntry* fce;

    fce = ceGetSizedFont( dctx, dsi->selected ? RFONTS_SCORE_BOLD:RFONTS_SCORE,
                          0, 0, 0 );

    oldFont = SelectObject( hdc, fce->setFont );

    ceSetTextColor( hdc, dctx, getPlayerColor(dsi->playerNum) );
    ceSetBkColor( hdc, dctx, bkIndex );
        
    XPRtoRECT( &rt, rOuter );
    ceClipToRect( hdc, &rt );
    if ( isFocussed ) {
        FillRect( hdc, &rt, dctx->brushes[CE_FOCUS_COLOR] );
    }

    XPRtoRECT( &rt, rInner );

    if ( !dctx->scoreIsVertical ) {
        rt.left += SCORE_HPAD;
        rt.right -= SCORE_HPAD;
    }
    ceDrawLinesClipped( hdc, fce, dctx->scoreCache[dsi->playerNum], 
                        CP_ACP, XP_TRUE, &rt );

    SelectObject( hdc, oldFont );
} /* ce_draw_score_drawPlayer */

DLSTATIC void
DRAW_FUNC_NAME(score_pendingScore)( DrawCtx* p_dctx, const XP_Rect* xprect, 
                                    XP_S16 score, XP_U16 playerNum,
                                    CellFlags flags )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    CEAppGlobals* globals = dctx->globals;
    HDC hdc = globals->hdc;

    wchar_t widebuf[5];
    RECT rt;
    XP_Bool focussed = TREAT_AS_CURSOR(dctx,flags);
    XP_U16 bkIndex = focussed ? CE_FOCUS_COLOR : CE_BKG_COLOR;
    const FontCacheEntry* fce;
    HFONT oldFont;
    XP_U16 spareHt;

    if ( score < 0 ) {
        widebuf[0] = '?';
        widebuf[1] = '?';
        widebuf[2] = '\0';
    } else {
        swprintf( widebuf, L"%dp", score );
    }

    fce = ceGetSizedFont( dctx, RFONTS_PTS, xprect->height, xprect->width, 
                          wcslen(widebuf) );
    spareHt = xprect->height - fce->glyphHt;

    oldFont = SelectObject( hdc, fce->setFont );

    ceSetTextColor( hdc, dctx, getPlayerColor(playerNum) );
    ceSetBkColor( hdc, dctx, bkIndex );

    XPRtoRECT( &rt, xprect );
    ceClipToRect( hdc, &rt );
    FillRect( hdc, &rt, dctx->brushes[bkIndex] );

    ceDrawTextClipped( hdc, widebuf, -1, XP_FALSE, fce, 
                       xprect->left, xprect->top + (spareHt/2),
                       xprect->width, DT_CENTER );

    (void)SelectObject( hdc, oldFont );
} /* ce_draw_score_pendingScore */

DLSTATIC void
DRAW_FUNC_NAME(drawTimer)( DrawCtx* p_dctx, const XP_Rect* xprect, 
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

    fce = ceGetSizedFont( dctx, RFONTS_SCORE, 0, 0, 0 );

    XPRtoRECT( &rt, xprect );

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

    ceSetTextColor( hdc, dctx, getPlayerColor(player) );
    ceSetBkColor( hdc, dctx, CE_BKG_COLOR );
    ceClearToBkground( dctx, xprect );

    /* distribute any extra space */
    XP_U16 width, height;
    ceMeasureText( dctx, hdc, fce, buf, 0, &width, &height );
    XP_S16 diff = (xprect->height - height) / 2;
    rt.top += diff;
    rt.bottom -= diff;

    oldFont = SelectObject( hdc, fce->setFont );
    ceDrawLinesClipped( hdc, fce, buf, CP_ACP, XP_TRUE, &rt );
    SelectObject( hdc, oldFont );

    if ( !globals->hdc ) {
        EndPaint( dctx->mainWin, &ps );
    }
} /* ce_draw_drawTimer */

DLSTATIC const XP_UCHAR*
DRAW_FUNC_NAME(getMiniWText)( DrawCtx* p_dctx, 
                              XWMiniTextType whichText )
{
    const XP_UCHAR* str = NULL;
    XP_U16 resID = 0;

    switch( whichText ) {
    case BONUS_DOUBLE_LETTER:
        resID = IDS_DOUBLE_LETTER;
        break;
    case BONUS_DOUBLE_WORD:
        resID = IDS_DOUBLE_WORD;
        break;
    case BONUS_TRIPLE_LETTER:
        resID = IDS_TRIPLE_LETTER;
        break;
    case BONUS_TRIPLE_WORD:
        resID = IDS_TRIPLE_WORD;
        break;
    case INTRADE_MW_TEXT:
        resID = IDS_INTRADE_MW;
        break;
    default:
        XP_ASSERT( XP_FALSE );
        break;
    }


    if ( resID != 0 ) {
        CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
        str = ceGetResString( dctx->globals, resID );
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

    ceSetBkColor( hdc, dctx, CE_BKG_COLOR );
    ceSetTextColor( hdc, dctx, CE_BLACK_COLOR );

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
    XP_U16 ii;
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;

    for ( ii = 0; ii < CE_NUM_COLORS; ++ii ) {
        DeleteObject( dctx->brushes[ii] );
#ifdef DRAW_FOCUS_FRAME
        ceDeleteObjectNotNull( dctx->pens[ii].pen );
#endif
    }

    for ( ii = 0; ii < VSIZE(dctx->hintPens); ++ii ) {
        ceDeleteObjectNotNull( dctx->hintPens[ii] );
    }

    ceClearBmCache( dctx );
    ceClearFontCache( dctx );

    DeleteObject( dctx->rightArrow );
    DeleteObject( dctx->downArrow );
    DeleteObject( dctx->origin );

#ifdef XWFEATURE_RELAY
    DeleteObject( dctx->netArrow );
#endif

#ifndef DRAW_LINK_DIRECT
    XP_FREE( dctx->mpool, p_dctx->vtable );
#endif

    XP_FREE( dctx->mpool, dctx );
} /* ce_draw_destroyCtxt */

DLSTATIC void
DRAW_FUNC_NAME(dictChanged)( DrawCtx* p_dctx, const DictionaryCtxt* dict )
{
    CEDrawCtx* dctx = (CEDrawCtx*)p_dctx;
    XP_ASSERT( !!dict );

    ceClearBmCache( dctx );

    /* If we don't yet have a dict, stick with the cache we have, which is
       either empty or came from the saved game and likely belong with the
       dict we're getting now. */
    if ( !!dctx->dict && !dict_tilesAreSame( dctx->dict, dict ) ) {
        ceClearFontCache( dctx );
    }
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
    XP_U16 ii;

    for ( ii = 0; ii < CE_NUM_COLORS; ++ii ) {
        ceDeleteObjectNotNull( dctx->brushes[ii] );
        dctx->brushes[ii] = CreateSolidBrush(dctx->globals->appPrefs.colors[ii]);
    }
} /* ce_drawctxt_update */

static void
drawColoredRect( CEDrawCtx* dctx, const RECT* invalR, XP_U16 index )
{
    CEAppGlobals* globals = dctx->globals;
    FillRect( globals->hdc, invalR, dctx->brushes[index] );
}

void
ce_draw_erase( CEDrawCtx* dctx, const RECT* invalR )
{
    drawColoredRect( dctx, invalR, CE_BKG_COLOR );
}

void
ce_draw_focus( CEDrawCtx* dctx, const RECT* invalR )
{
    drawColoredRect( dctx, invalR, CE_FOCUS_COLOR );
}

#ifdef XWFEATURE_RELAY
void
ce_draw_status( CEDrawCtx* dctx, const RECT* rect, CeNetState state )
{
    RECT localR = *rect;
    XP_U16 share;
    HDC hdc = dctx->globals->hdc;
    XP_Bool hasRed;

    FillRect( hdc, &localR, dctx->brushes[CE_BKG_COLOR] );
    InsetRect( &localR, 1, 1 );

/*     static int count = 0; */
/*     state = count++ % CENSTATE_NSTATES; */

    hasRed = state < (CENSTATE_NSTATES - 1);

    /* First state is all-red.  Last is all-green.  In between we have red on
       the right, green on the left. */

    share = localR.right - localR.left;
    if ( hasRed ) {
        ceSetTextColor( hdc, dctx, CE_BKG_COLOR );

        share /= CENSTATE_NSTATES-1;
        share *= state;
    } else {
        ceSetTextColor( hdc, dctx, CE_BLACK_COLOR );
    }

    if ( share > 0 ) {
        XP_U16 oldRight = localR.right;
        localR.right = localR.left + share;
        ceClipToRect( hdc, &localR );
        ceSetBkColor( hdc, dctx, CE_PLAYER3_COLOR );
        ceDrawBitmapInRect( hdc, rect, dctx->netArrow, XP_TRUE );
        localR.right = oldRight;
    }
    if ( hasRed ) {
        localR.left += share;
        ceClipToRect( hdc, &localR );
        ceSetBkColor( hdc, dctx, CE_PLAYER1_COLOR );
        ceDrawBitmapInRect( hdc, rect, dctx->netArrow, XP_TRUE );
    }
}
#endif

#ifndef _WIN32_WCE
HBRUSH
ce_draw_getFocusBrush( const CEDrawCtx* dctx )
{
    return dctx->brushes[CE_FOCUS_COLOR];
}
#endif

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

#ifdef XWFEATURE_RELAY
    dctx->netArrow = LoadBitmap( globals->hInst, 
                                 MAKEINTRESOURCE(IDB_NETARROW) );
#endif

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
        if ( fce->indexHt > 0 ) {
            stream_putU8( stream, fce->indexWidth );
            stream_putU8( stream, fce->lfHeight );
            stream_putU8( stream, fce->offset );
            stream_putU8( stream, fce->glyphHt );
        }
    }
}

//#define DROP_CACHE
void
ce_draw_fromStream( CEDrawCtx* dctx, XWStreamCtxt* stream, XP_U8 flags )
{
    XP_U16 ii;
    XP_U16 nEntries;

    ceClearFontCache( dctx );   /* no leaking! */

    nEntries = (XP_U16)stream_getU8( stream );

    for ( ii = 0; ii < nEntries; ++ii ) {
        FontCacheEntry fce;

        fce.indexHt = (XP_U16)stream_getU8( stream );
        if ( fce.indexHt > 0 ) {
            if ( flags >= CE_GAMEFILE_VERSION2 ) {
                fce.indexWidth = (XP_U16)stream_getU8( stream );
            }
            fce.lfHeight = (XP_U16)stream_getU8( stream );
            fce.offset = (XP_U16)stream_getU8( stream );
            fce.glyphHt = (XP_U16)stream_getU8( stream );

            /* We need to read from the file no matter how many entries, but
               only populate what we have room for -- in case N_RESIZE_FONTS
               was different when file written. */
#ifndef DROP_CACHE
            if ( ii < N_RESIZE_FONTS ) {
                LOGFONT fontInfo;

                ceFillFontInfo( dctx, &fontInfo, fce.lfHeight );
                fce.setFont = CreateFontIndirect( &fontInfo );
                XP_ASSERT( !!fce.setFont );
                if ( !!fce.setFont ) {
                    XP_MEMCPY( &dctx->fcEntry[ii], &fce, 
                               sizeof(dctx->fcEntry[ii]) );
                }
            }
#endif
        }
    }
} /* ce_draw_fromStream */
