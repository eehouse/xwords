/* -*-mode: C; fill-column: 77; c-basic-offset: 4; compile-command: "make ARCH=68K_ONLY MEMDEBUG=TRUE";-*- */
/* 
 * Copyright 1999 - 2008 by Eric House (xwords@eehouse.org).  All rights
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

#include <UIResources.h>
#include <SystemMgr.h>
#ifdef XWFEATURE_FIVEWAY
# include <Hs.h>
#endif

#include "draw.h"
#include "dbgutil.h"

#include "palmmain.h"
#include "xwords4defines.h"
#include "LocalizedStrIncludes.h"

#define CHARRECT_WIDTH 12
#define CHARRECT_HEIGHT 14

#define FONT_HEIGHT 8
#define LINE_SPACING 1

#define SCORE_SEP ':'
#define SCORE_SEPSTR ":"

#define TILE_SUBSCRIPT 1             /* draw tile with numbers below letters? */

/* Let's try both for a while */
#define DRAW_FOCUS_FRAME 1
#ifdef DRAW_FOCUS_FRAME
# define TREAT_AS_CURSOR(d,f) ((((f) & CELL_ISCURSOR) != 0) && !(d)->topFocus )
# define FOCUS_BORDER_WIDTH 6
#else
# define TREAT_AS_CURSOR(d,f) (((f) & CELL_ISCURSOR) != 0)
#endif

static XP_Bool palm_common_draw_drawCell( DrawCtx* p_dctx, const XP_Rect* rect,
                                          const XP_UCHAR* letters, 
                                          const XP_Bitmaps* bitmaps, Tile tile,
                                          XP_S16 owner, XWBonusType bonus, 
                                          HintAtts hintAtts, CellFlags flags );
static void palm_bnw_draw_score_drawPlayer( DrawCtx* p_dctx, 
                                            const XP_Rect* rInner, 
                                            const XP_Rect* rOuter, 
                                            const DrawScoreInfo* dsi );
static XP_Bool palm_bnw_draw_trayBegin( DrawCtx* p_dctx, const XP_Rect* rect, 
                                        XP_U16 owner, DrawFocusState dfs );
static void palm_clr_draw_clearRect( DrawCtx* p_dctx, const XP_Rect* rectP );
static void palm_draw_drawMiniWindow( DrawCtx* p_dctx, const XP_UCHAR* text, 
                                      const XP_Rect* rect, void** closureP );
static void doDrawPlayer( PalmDrawCtx* dctx, const DrawScoreInfo* dsi,
                          const XP_Rect* rInner );

#define HIGHRES_PUSH_LOC( dctx ) \
    { \
        XP_U16 oldVal = 0; \
        if ( (dctx)->doHiRes ) { \
            oldVal = WinSetCoordinateSystem( kCoordinatesNative ); \
        } 
#define HIGHRES_POP_LOC(dctx) \
        if ( (dctx)->doHiRes ) { \
            (void)WinSetCoordinateSystem( oldVal ); \
            (dctx)->oldCoord = 0; \
        } \
    }
#define HIGHRES_PUSH_NOPOP( dctx ) \
    if ( (dctx)->doHiRes ) { \
        WinSetCoordinateSystem( kCoordinatesNative ); \
    } 
#define HIGHRES_PUSH( dctx ) \
    if ( (dctx)->doHiRes ) { \
        XP_ASSERT( (dctx)->oldCoord == 0 ); \
        (dctx)->oldCoord = WinSetCoordinateSystem( kCoordinatesNative ); \
    } 
#define HIGHRES_POP(dctx) \
    if ( (dctx)->doHiRes ) { \
         (void)WinSetCoordinateSystem( (dctx)->oldCoord ); \
        (dctx)->oldCoord = 0; \
    } 

static void
pmEraseRect( /* PalmDrawCtx* dctx,  */const XP_Rect* rect )
{
    WinEraseRectangle( (const RectangleType*)rect, 0 );
} /* pmEraseRect */

static void
insetRect2( XP_Rect* rect, XP_S16 byX, XP_S16 byY )
{
    XP_U16 i;
    rect->left += byX;
    rect->top += byY;
    for ( i = 0; i < 2; ++i ) {
        rect->width -= byX;
        rect->height -= byY;
    }
} /* insetRect */

static void
insetRect( XP_Rect* rect, XP_S16 by )
{
    insetRect2( rect, by, by );
} /* insetRect */

static void
drawBitmapAt( DrawCtx* XP_UNUSED(p_dctx), Int16 resID, Int16 x, Int16  y ) 
{
    MemHandle handle;
    handle = DmGetResource( bitmapRsc, resID );
    XP_ASSERT( handle != NULL );

    if ( handle != NULL ) {
        WinDrawBitmap( (BitmapPtr)MemHandleLock(handle), x, y );
        XP_ASSERT( MemHandleLockCount(handle ) == 1 );
        MemHandleUnlock( handle );
        DmReleaseResource( handle );
    }
} /* drawBitmapAt */

static void
bitmapInRect( PalmDrawCtx* dctx, Int16 resID, const XP_Rect* rectP )
{
    XP_U16 left = rectP->left;
    XP_U16 top = rectP->top;
    if ( dctx->globals->gState.showGrid ) {
        ++left;
        ++top;
    }
    drawBitmapAt( (DrawCtx*)dctx, resID, left, top );
} /* bitmapInRect */

# define BMP_WIDTH 16
# define BMP_HT 16

static void
measureFace( PalmDrawCtx* dctx, XP_UCHAR face, PalmFontHtInfo* fhi )
{
    WinHandle win;
    BitmapType* bitmap;
    Coord x, y;
    Err err;
    XP_Bool gotIt;
    XP_U16 top = 0;
    XP_U16 bottom = 0;
    XP_UCHAR ch = (XP_UCHAR)face;

    bitmap = BmpCreate( BMP_WIDTH, BMP_HT, 1, NULL, &err );
    if ( err == errNone ) {
        win = WinCreateBitmapWindow( bitmap, &err );
        if ( err == errNone ) {
            WinHandle oldWin = WinSetDrawWindow( win );

            WinSetBackColor( 0 ); /* white */
            (void)WinSetTextColor( 1 ); /* black */
            (void)WinSetDrawMode( winOverlay );

            WinDrawChars( &ch, 1, 0, 0 );

            /* Scan from top for top, then from bottom for botton */
            gotIt = XP_FALSE;
            for ( y = 0; !gotIt && y < BMP_HT; ++y ) {
                for ( x = 0; !gotIt && x < BMP_WIDTH; ++x ) {
                    IndexedColorType pxl = WinGetPixel( x, y );
                    if ( pxl != 0 ) {
                        top = y;
                        gotIt = XP_TRUE;
                    }
                }
            }

            gotIt = XP_FALSE;
            for ( y = BMP_HT - 1; !gotIt && y >= 0; --y ) {
                for ( x = 0; !gotIt && x < BMP_WIDTH; ++x ) {
                    IndexedColorType pxl = WinGetPixel( x, y );
                    if ( pxl != 0 ) {
                        bottom = y;
                        gotIt = XP_TRUE;
                    }
                }
            }

            (void)WinSetDrawWindow( oldWin );
            WinDeleteWindow( win, false );

            /* There should be a way to avoid this, but HIGHRES_PUSH after
               WinSetDrawWindow isn't working...  Fix this... */
            if ( dctx->doHiRes ) {
                top *= 2;
                bottom *= 2;
            }

            fhi->topOffset = top;
            fhi->height = bottom - top + 1;

        }
        BmpDelete( bitmap );
    }
} /* measureFace */

static void
checkFontOffsets( PalmDrawCtx* dctx, const DictionaryCtxt* dict )
{
    XP_LangCode code;
    XP_U16 nFaces;
    Tile tile;

    code = dict_getLangCode( dict );
    if ( code != dctx->fontLangCode ) {

        if ( !!dctx->fontHtInfo ) {
            XP_FREE( dctx->mpool, dctx->fontHtInfo );
        }

        nFaces = dict_numTileFaces( dict );
        dctx->fontHtInfo = XP_MALLOC( dctx->mpool,
                                      nFaces * sizeof(*dctx->fontHtInfo) );

        for ( tile = 0; tile < nFaces; ++tile ) {
            XP_UCHAR face[2];
            if ( 1 == dict_tilesToString( dict, &tile, 1, 
                                          face, sizeof(face) ) ) {
                measureFace( dctx, face[0], &dctx->fontHtInfo[tile] );
            }
        }

        dctx->fontLangCode = code;
    }
}

static XP_Bool
palm_common_draw_boardBegin( DrawCtx* p_dctx, const XP_Rect* rect, 
                             DrawFocusState dfs )
{
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;
    PalmAppGlobals* globals = dctx->globals;
    if ( !globals->gState.showGrid ) {
        WinDrawRectangleFrame(rectangleFrame, (RectangleType*)rect);
    }

#ifdef DRAW_FOCUS_FRAME
    dctx->topFocus = dfs == DFS_TOP;
#endif
    return XP_TRUE;
} /* palm_common_draw_boardBegin */

#ifdef COLOR_SUPPORT
static XP_Bool
palm_clr_draw_boardBegin( DrawCtx* p_dctx, const XP_Rect* rect, 
                          DrawFocusState dfs )
{
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;

    WinPushDrawState();

    WinSetForeColor( dctx->drawingPrefs->drawColors[COLOR_BLACK] );
    WinSetTextColor( dctx->drawingPrefs->drawColors[COLOR_BLACK] );
    WinSetBackColor( dctx->drawingPrefs->drawColors[COLOR_WHITE] );

    HIGHRES_PUSH_NOPOP(dctx);

    palm_common_draw_boardBegin( p_dctx, rect, dfs );

    return XP_TRUE;
} /* palm_clr_draw_boardBegin */

static void
palm_draw_objFinished( DrawCtx* p_dctx, BoardObjectType typ, 
                       const XP_Rect* rect, DrawFocusState dfs )
{
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;
#if defined XWFEATURE_FIVEWAY && defined DRAW_FOCUS_FRAME
    if ( (dfs == DFS_TOP) && (typ == OBJ_BOARD) ) {
        XP_Rect r;
        XP_U16 i;
        IndexedColorType oldColor;

        oldColor
            = WinSetForeColor( dctx->drawingPrefs->drawColors[COLOR_CURSOR] );

        r.left = rect->left + 1;
        r.top = rect->top + 1;
        r.width = rect->width - 1;
        r.height = rect->height - 1;

        for ( i = dctx->doHiRes?FOCUS_BORDER_WIDTH:FOCUS_BORDER_WIDTH/2;
              i > 0; --i ) { 
            insetRect( &r, 1 );
            WinDrawRectangleFrame(rectangleFrame, (RectangleType*)&r );
        }
        (void)WinSetForeColor( oldColor );
    }
#endif

    if ( typ == OBJ_BOARD ) {
        WinPopDrawState();
    } else if ( typ == OBJ_TRAY ) {
        WinSetClip( &dctx->oldTrayClip );
        WinPopDrawState();
    } else if ( typ == OBJ_SCORE ) {
        WinSetClip( &dctx->oldScoreClip );
        HIGHRES_POP(dctx);
    }
} /* palm_draw_objFinished */

static XP_Bool
palm_clr_draw_drawCell( DrawCtx* p_dctx, const XP_Rect* rect, 
                        const XP_UCHAR* letters, const XP_Bitmaps* bitmaps,
                        Tile tile, XP_S16 owner, XWBonusType bonus, 
                        HintAtts hintAtts, CellFlags flags )
{
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;    
    IndexedColorType color;
    XP_U16 index;
    XP_Bool isCursor = TREAT_AS_CURSOR( dctx, flags );
    XP_Bool isPending = ((flags & CELL_HIGHLIGHT) != 0);
    XP_Bool dragSrc = ((flags & CELL_DRAGSRC) != 0);

    if ( isCursor ) {
        index = COLOR_CURSOR;
    } else if ( isPending && !dragSrc ) { 
        /* don't color background if will invert */
        index = COLOR_WHITE;
    } else if ( (!!bitmaps || !!letters) && !dragSrc ) {
        index = COLOR_TILE;
    } else if ( bonus == BONUS_NONE ) { 
        index = COLOR_EMPTY;
    } else {
        index = COLOR_DBL_LTTR + bonus - 1;
    }
    color = dctx->drawingPrefs->drawColors[index];
    WinSetBackColor( color );

    if ( !!letters ) {
        if ( (owner >= 0) && !isPending ) {
            index = COLOR_PLAYER1 + owner;
        } else {
            index = COLOR_BLACK;
        }

        color = dctx->drawingPrefs->drawColors[index];
        WinSetTextColor( color );
    }

    return palm_common_draw_drawCell( p_dctx, rect, letters, bitmaps, 
                                      tile, owner, bonus, hintAtts, flags );
} /* palm_clr_draw_drawCell */

static void
palm_clr_draw_score_drawPlayer( DrawCtx* p_dctx, const XP_Rect* rInner, 
                                const XP_Rect* rOuter, 
                                const DrawScoreInfo* dsi )
{
    XP_U16 playerNum = dsi->playerNum;
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;
    XP_U16 txtIndex;
    IndexedColorType newColor;

    WinPushDrawState();

    if ( dsi->flags && CELL_ISCURSOR ) {
        WinSetBackColor( dctx->drawingPrefs->drawColors[COLOR_CURSOR] );
        pmEraseRect( rOuter );
    }

    if ( dsi->selected ) {
        XP_Rect r = *rInner;
        insetRect2( &r, (r.width - rOuter->width) >> 2, 
                    (r.height - rOuter->height) >> 2 );
        WinSetBackColor( dctx->drawingPrefs->drawColors[COLOR_BLACK] );
        pmEraseRect( &r );
        txtIndex = COLOR_WHITE;
    } else {
        txtIndex = COLOR_PLAYER1+playerNum;
    }

    newColor = dctx->drawingPrefs->drawColors[txtIndex];
    (void)WinSetTextColor( newColor );
    (void)WinSetForeColor( newColor );

    doDrawPlayer( dctx, dsi, rInner );

    WinPopDrawState();
} /* palm_clr_draw_score_drawPlayer */
#endif  /* #ifdef COLOR_SUPPORT */

static void
palmDrawHintBorders( PalmDrawCtx* dctx, const XP_Rect* rect, 
                     HintAtts hintAtts )
{
    if ( 0 != (HINT_BORDER_EDGE & hintAtts) ) {
        XP_Rect frame = *rect;
        XP_U16 width = dctx->doHiRes ? 4 : 2;
        XP_Bool showGrid = dctx->globals->gState.showGrid;
        if ( showGrid ) {
            ++width;
        }

        if ( (hintAtts & HINT_BORDER_LEFT) != 0 ) {
            XP_Rect r = frame;
            r.width = width;
            WinDrawRectangle( (RectangleType*)&r, 0 );
        }
        if ( (hintAtts & HINT_BORDER_TOP) != 0 ) {
            XP_Rect r = frame;
            r.height = width;
            WinDrawRectangle( (RectangleType*)&r, 0 );
        }
        if ( (hintAtts & HINT_BORDER_RIGHT) != 0 ) {
            XP_Rect r = frame;
            r.left += r.width - width;
            if ( showGrid ) {
                ++r.left;
            }
            r.width = width;
            WinDrawRectangle( (RectangleType*)&r, 0 );
        }
        if ( (hintAtts & HINT_BORDER_BOTTOM) != 0 ) {
            XP_Rect r = frame;
            r.top += r.height - width;
            if ( showGrid ) {
                ++r.top;
            }
            r.height = width;
            WinDrawRectangle( (RectangleType*)&r, 0 );
        }
    }
} /* palmDrawHintBorders */

static XP_Bool
palm_common_draw_drawCell( DrawCtx* p_dctx, const XP_Rect* rect, 
                           const XP_UCHAR* letters, const XP_Bitmaps* bitmaps,
                           Tile tile, XP_S16 owner, XWBonusType bonus, 
                           HintAtts hintAtts, CellFlags flags )
{
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;
    GraphicsAbility able = dctx->able;
    XP_Rect localR = *rect;
    XP_U16 len;
    RectangleType saveClip, intersectR;
    PalmAppGlobals* globals = dctx->globals;
    Boolean showGrid = globals->gState.showGrid;
    Boolean showBonus = bonus != BONUS_NONE;
    Boolean complete;
    XP_Bool showEmpty = 0 != (flags & (CELL_ISEMPTY|CELL_DRAGSRC));

    if ( showGrid ) {
        ++localR.width;
        ++localR.height;
    }

    WinGetClip( &saveClip );

    RctGetIntersection( &saveClip, (RectangleType*)&localR, &intersectR );

    /* If there's no rect left inside the clip rgn, exit.  But if the rect's
       only partial go ahead and draw, but still return false indicating that
       we'd like to be allowed to draw again.  This is necessary when a cell
       needs to be redrawn for two reasons, e.g. because its bottom half
       overlaps a form that's gone away (and that supplied the clip region)
       and because its top is covered by the trading miniwindow that's also
       going away.  Under no circumstances draw outside the clip region or
       risk overdrawing menus, other forms, etc. */

    if ( intersectR.extent.x == 0 || intersectR.extent.y == 0 ) {
        return XP_FALSE;
    } else if ( intersectR.extent.x < localR.width || 
                intersectR.extent.y < localR.height ) {
        complete = XP_FALSE;
    } else {
        complete = XP_TRUE;
    }
    WinSetClip( (RectangleType*)&intersectR );

    if ( showGrid ) {
        insetRect( &localR, 1 );
    }

    pmEraseRect( &localR );

    if ( showEmpty ) {
        /* do nothing */
    } else if ( !!bitmaps ) {
        XP_Bool doColor = (able == COLOR) && (owner >= 0);
        XP_U16 x = localR.left+1;
        XP_U16 y = localR.top+1;
        /* cheating again; this belongs in a palm_clr method.  But the
           special bitmaps are rare enough that we shouldn't change the palm
           draw state every time. */
        if ( doColor ) {
            WinSetForeColor( 
                dctx->drawingPrefs->drawColors[COLOR_PLAYER1+owner] );
        }

        if ( dctx->doHiRes ) {
            ++x;
            ++y;
        }
        XP_ASSERT( bitmaps->nBitmaps > 0 );
        WinDrawBitmap( bitmaps->bmps[0], x, y );
        if ( doColor ) {
            WinSetForeColor( dctx->drawingPrefs->drawColors[COLOR_BLACK] );
        }
        showBonus = doColor;	/* skip bonus in B&W case; can't draw both! */
    } else if ( !!letters ) {
        len = XP_STRLEN( (const char*)letters );
        XP_ASSERT( len > 0 );
        if ( len > 0 ) {
            XP_S16 strWidth = FntCharsWidth( (const char*)letters, len );
            XP_U16 x, y;
            x = localR.left + ((localR.width-strWidth) / 2);
            y = localR.top - dctx->fontHtInfo[tile].topOffset;
            /* '+ 1' below causes us to round up.  Without this "Q" is drawn
               with its top higher than other ASCII letters because it's
               taller; looks bad. */
            y += (localR.height + 1 - dctx->fontHtInfo[tile].height) / 2;
            if ( len == 1 ) {
                ++x;
            }

            WinDrawChars( (const char*)letters, len, x, y );

            showBonus = XP_FALSE;
        }
    }

    if ( ((flags & CELL_ISSTAR) != 0) && showEmpty ) {
        bitmapInRect( dctx, STAR_BMP_RES_ID, rect );
    } else if ( showBonus && (able == ONEBIT) ) {
        /* this is my one refusal to totally factor bandw and color
           code */
        WinSetPattern( (const CustomPatternType*)
                       &dctx->u.bnw.valuePatterns[bonus-1] );
        WinFillRectangle( (RectangleType*)&localR, 0 );
    } else if ( !showBonus && showEmpty && !showGrid ) {
        /* should this be in the v-table so I don't have to test each
           time? */
        RectangleType r;
        r.topLeft.x = localR.left + ((PALM_BOARD_SCALE-1)/2);
        r.topLeft.y = localR.top + ((PALM_BOARD_SCALE-1)/2);

        if ( dctx->doHiRes ) {
            r.topLeft.x += PALM_BOARD_SCALE/2;
            r.topLeft.y += PALM_BOARD_SCALE/2;
        }

        if ( globals->romVersion >= 35 ) {
            WinDrawPixel( r.topLeft.x, r.topLeft.y );
        } else {
            r.extent.x = r.extent.y = 1;
            WinDrawRectangle( &r, 0 );
        }
    }

    if ( !showEmpty && (flags & CELL_HIGHLIGHT) != 0 ) {
        if ( !TREAT_AS_CURSOR( dctx, flags ) ) {
            XP_ASSERT( !!bitmaps ||
                       (!!letters && XP_STRLEN((const char*)letters)>0));
            WinInvertRectangle( (RectangleType*)&localR, 0 );
        }
    }

    if ( showGrid ) {
        WinDrawRectangleFrame(rectangleFrame, (RectangleType*)&localR);
    }

    if ( ((flags & CELL_ISBLANK) != 0) && !showEmpty ) {
        WinEraseRectangleFrame( roundFrame, (RectangleType*)&localR );
    }

    palmDrawHintBorders( dctx, rect, hintAtts );

    WinSetClip( &saveClip );
    return complete;
} /* palm_common_draw_drawCell */

void
palm_draw_destroyCtxt( DrawCtx* p_dctx )
{
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;

    XP_FREE( dctx->mpool, p_dctx->vtable );
    if ( !!dctx->fontHtInfo ) {
        XP_FREE( dctx->mpool, dctx->fontHtInfo );
    }
    XP_FREE( dctx->mpool, dctx );
} /* palm_draw_destroyCtxt */


static void
palm_draw_dictChanged( DrawCtx* p_dctx, const DictionaryCtxt* dict )
{
    checkFontOffsets( (PalmDrawCtx*)p_dctx, dict );
}

static void
palm_draw_invertCell( DrawCtx* p_dctx, const XP_Rect* rect )
{
    XP_Rect localR = *rect;
    /*     insetRect( &localR, 3 ); */
    localR.top += 3;
    localR.left += 3;
    localR.width -= 5;
    localR.height -= 5;

    HIGHRES_PUSH_LOC( (PalmDrawCtx*)p_dctx );
    WinInvertRectangle( (RectangleType*)&localR, 0 );
    HIGHRES_POP_LOC( (PalmDrawCtx*)p_dctx );
} /* palm_draw_invertCell */

static XP_Bool
palm_clr_draw_trayBegin( DrawCtx* p_dctx, const XP_Rect* rect, 
                         XP_U16 owner, DrawFocusState dfs )
{
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;

    dctx->trayOwner = owner;

    WinPushDrawState();
    WinSetBackColor( dctx->drawingPrefs->drawColors[COLOR_TILE] );
    WinSetTextColor( dctx->drawingPrefs->drawColors[COLOR_PLAYER1+owner] );
    WinSetForeColor( dctx->drawingPrefs->drawColors[COLOR_PLAYER1+owner] );

    HIGHRES_PUSH_NOPOP(dctx);

    palm_bnw_draw_trayBegin( p_dctx, rect, owner, dfs );
    return XP_TRUE;
} /* palm_clr_draw_trayBegin */

static XP_Bool
palm_bnw_draw_trayBegin( DrawCtx* p_dctx, const XP_Rect* rect, 
                         XP_U16 XP_UNUSED(owner),
                         DrawFocusState dfs )
{
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;

    WinGetClip( &dctx->oldTrayClip );
    WinSetClip( (RectangleType*)rect );
    dctx->topFocus = dfs == DFS_TOP;
    return XP_TRUE;
} /* palm_bnw_draw_trayBegin */

static void
smallBoldStringAt( const char* str, XP_U16 len, XP_S16 x, XP_U16 y )
{
    UInt32 oldMode = WinSetScalingMode( kTextScalingOff );
    FontID curFont = FntGetFont();
    FntSetFont( boldFont );

    /* negative x means position backwards from it */
    if ( x < 0 ) {
        x = (-x) - FntCharsWidth( str, len );
    }

    WinDrawChars( str, len, x, y );

    FntSetFont( curFont );
    WinSetScalingMode( oldMode );
} /* smallBoldStringAt */

static void
palm_draw_drawTile( DrawCtx* p_dctx, const XP_Rect* rect, 
                    const XP_UCHAR* letters, const XP_Bitmaps* bitmaps,
                    XP_U16 val, CellFlags flags )
{
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;
    char valBuf[3];
    XP_Rect localR = *rect;
    XP_U16 len, width;
    XP_U16 doubler = 1;
    const XP_Bool cursor = (flags & CELL_ISCURSOR) != 0;
    XP_Bool empty = (flags & CELL_ISEMPTY) != 0;

    WinPushDrawState();

    if ( dctx->doHiRes ) {
        doubler = 2;
    }

    draw_clearRect( p_dctx, &localR );
    
    if ( cursor ) {
        (void)WinSetBackColor( dctx->drawingPrefs->drawColors[COLOR_CURSOR] );
        if ( dctx->topFocus ) {
            WinEraseRectangle( (const RectangleType*)&localR, 0 );
        }
    }

    localR.height -= 3 * doubler;
    localR.top += 2 * doubler;
    localR.width -= 3 * doubler;
    localR.left += 2 * doubler;

    if ( (cursor && !dctx->topFocus) || (!empty && !cursor) ) {
        /* this will fill it with the tile background color */
        WinEraseRectangle( (const RectangleType*)&localR, 0 );
    }

    if ( !empty ) {
        /* Draw the number before the letter.  Some PalmOS version don't
           honor the winOverlay flag and erase.  Better to erase the value
           than the letter. */
        if ( (flags & CELL_VALHIDDEN) == 0 ) {
            (void)StrPrintF( valBuf, "%d", val );
            len = XP_STRLEN((const char*)valBuf);

            if ( dctx->doHiRes && dctx->oneDotFiveAvail ) {
                smallBoldStringAt( valBuf, len, 
                                   -(localR.left + localR.width), 
                                   localR.top + localR.height - dctx->fntHeight
                                   - 1 );
            } else {
                width = FntCharsWidth( valBuf, len );
                WinDrawChars( valBuf, len, localR.left + localR.width - width,
                              localR.top + localR.height - (10*doubler) );
            }
        }

        if ( !!bitmaps ) {
            XP_ASSERT( bitmaps->nBitmaps > 1 );
            WinDrawBitmap( (BitmapPtr)(bitmaps->bmps[1]), 
                           localR.left+(2*doubler), 
                           localR.top+(2*doubler) );
        } else if ( !!letters ) {
            if ( *letters != LETTER_NONE ) { /* blank */
                FontID curFont = FntSetFont( largeFont );

                HIGHRES_PUSH_LOC( dctx );

                WinDrawChars( (char*)letters, 1, 
                              localR.left + (1*doubler), 
                              localR.top + (0*doubler) );

                HIGHRES_POP_LOC(dctx);

                FntSetFont( curFont );
            }
        }

        WinDrawRectangleFrame( rectangleFrame, (RectangleType*)&localR );
        if ( (flags & CELL_HIGHLIGHT) != 0 ) {
            insetRect( &localR, 1 );
            WinDrawRectangleFrame(rectangleFrame, (RectangleType*)&localR );
            if ( dctx->doHiRes ) {
                insetRect( &localR, 1 );
                WinDrawRectangleFrame(rectangleFrame, 
                                      (RectangleType*)&localR );
            }
        }
    }
    WinPopDrawState();
} /* palm_draw_drawTile */

#ifdef POINTER_SUPPORT
static void
palm_draw_drawTileMidDrag( DrawCtx* p_dctx, const XP_Rect* rect, 
                           const XP_UCHAR* letters, const XP_Bitmaps* bitmaps,
                           XP_U16 val, XP_U16 owner, CellFlags flags )
{
    /* let trayBegin code take care of pushing color env changes. */
    draw_trayBegin( p_dctx, rect, owner, DFS_NONE );
    palm_draw_drawTile( p_dctx, rect, letters, bitmaps, val, flags );
    draw_objFinished( p_dctx, OBJ_TRAY, rect, DFS_NONE );
}
#endif

static void
palm_draw_drawTileBack( DrawCtx* p_dctx, const XP_Rect* rect, CellFlags flags )
{
    palm_draw_drawTile( p_dctx, rect, "?", (XP_Bitmap)NULL, 
                        0, (flags & CELL_ISCURSOR) | CELL_VALHIDDEN );
} /* palm_draw_drawTileBack */

static void
palm_draw_drawTrayDivider( DrawCtx* p_dctx, const XP_Rect* rect, 
                           CellFlags flags )
{
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;
    XP_Rect lRect = *rect;
    XP_Bool selected = (flags & CELL_HIGHLIGHT) != 0;
    XP_Bool cursor = TREAT_AS_CURSOR(dctx, flags);

    if ( cursor ) {
        (void)WinSetBackColor( dctx->drawingPrefs->drawColors[COLOR_CURSOR] );
    }
    WinEraseRectangle( (const RectangleType*)&lRect, 0 );

    ++lRect.left;
    --lRect.width;
    if ( cursor ) {
        insetRect( &lRect, 2 );
    }

    if ( selected ) {
        short pattern[] = { 0xFF00, 0xFF00, 0xFF00, 0xFF00 };

        WinSetPattern( (const CustomPatternType*)&pattern );
        WinFillRectangle( (RectangleType*)&lRect, 0 );

    } else {
        WinDrawRectangle( (RectangleType*)&lRect, 0 );
    }
} /* palm_draw_drawTrayDivider */

static void 
palm_bnw_draw_clearRect( DrawCtx* XP_UNUSED(p_dctx), const XP_Rect* rectP )
{
    pmEraseRect( rectP );
} /* palm_draw_clearRect */

static void 
palm_clr_draw_clearRect( DrawCtx* p_dctx, const XP_Rect* rectP )
{
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;
    IndexedColorType oldColor;

    oldColor = WinSetBackColor( dctx->drawingPrefs->drawColors[COLOR_WHITE] );
    pmEraseRect( rectP );
    WinSetBackColor( oldColor );
} /* palm_clr_draw_clearRect */

static void
palm_clr_draw_drawMiniWindow( DrawCtx* p_dctx, const XP_UCHAR* text, 
                              const XP_Rect* rect, void** closureP )
{
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;
    WinSetBackColor( dctx->drawingPrefs->drawColors[COLOR_WHITE] );
    WinSetTextColor( dctx->drawingPrefs->drawColors[COLOR_BLACK] );

    palm_draw_drawMiniWindow( p_dctx, text, rect, closureP );
} /* palm_clr_draw_drawMiniWindow */

static void
palm_draw_drawBoardArrow( DrawCtx* p_dctx, const XP_Rect* rectP, 
                          XWBonusType XP_UNUSED(cursorBonus), XP_Bool vertical,
                          HintAtts hintAtts, CellFlags flags )
{
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;
    RectangleType oldClip;

    Int16 resID = vertical? DOWN_ARROW_RESID:RIGHT_ARROW_RESID;

    WinGetClip( &oldClip );
    WinSetClip( (RectangleType*)rectP );

    bitmapInRect( dctx, resID, rectP );
    palmDrawHintBorders( dctx, rectP, hintAtts );

    WinSetClip( &oldClip );
} /* palm_draw_drawBoardArrow */

#ifdef COLOR_SUPPORT
static void
palm_clr_draw_drawBoardArrow( DrawCtx* p_dctx, const XP_Rect* rectP, 
                              XWBonusType cursorBonus, XP_Bool vertical,
                              HintAtts hintAtts, CellFlags flags )
{
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;
    XP_U16 index;

    if ( TREAT_AS_CURSOR( dctx, flags ) ) {
        index = COLOR_CURSOR;
    } else if ( cursorBonus == BONUS_NONE ) { 
        index = COLOR_EMPTY;
    } else {
        index = COLOR_DBL_LTTR + cursorBonus - 1;
    }

    WinSetBackColor( dctx->drawingPrefs->drawColors[index] );
    palm_draw_drawBoardArrow( p_dctx, rectP, cursorBonus, vertical, 
                              hintAtts, flags );
} /* palm_clr_draw_drawBoardArrow */
#endif

static void
palm_draw_scoreBegin( DrawCtx* p_dctx, const XP_Rect* rect, 
                      XP_U16 XP_UNUSED(numPlayers), 
                      const XP_S16* XP_UNUSED(scores), 
                      XP_S16 XP_UNUSED(remCount), 
                      DrawFocusState XP_UNUSED(dfs) )
{
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;

    HIGHRES_PUSH( dctx );

    WinGetClip( &dctx->oldScoreClip );
    WinSetClip( (RectangleType*)rect );

    pmEraseRect( rect );
} /* palm_draw_scoreBegin */

/* rectContainsRect: Dup of something in board.c.  They could share if I were
 * willing to link from here out.
 */
static XP_Bool
rectContainsRect( const XP_Rect* rect1, const XP_Rect* rect2 )
{
    return ( rect1->top <= rect2->top
             && rect1->left <= rect2->left
             && rect1->top + rect1->height >= rect2->top + rect2->height
             && rect1->left + rect1->width >= rect2->left + rect2->width );
} /* rectContainsRect */

static XP_Bool
palm_draw_vertScrollBoard( DrawCtx* XP_UNUSED(p_dctx), XP_Rect* rect, 
                           XP_S16 dist, DrawFocusState dfs )
{
    RectangleType clip;
    XP_Bool canDoIt;

    /* if the clip rect doesn't contain the scroll rect we can't do anything
       right now: WinScrollRectangle won't do its job. */
    WinGetClip( &clip );
    canDoIt = rectContainsRect( (XP_Rect*)&clip, rect );

    if ( canDoIt ) {
        RectangleType vacated;
        WinDirectionType dir;

        if ( dist >= 0 ) {
            dir = winUp;
        } else {
            dir = winDown;
            dist = -dist;
        }

#ifdef PERIMETER_FOCUS
        if ( dfs == DFS_TOP ) {
            rect->height -= FOCUS_BORDER_WIDTH;
            if ( dir == winDown ) {
                rect->top += FOCUS_BORDER_WIDTH;
            }
        }
#endif

        WinScrollRectangle( (RectangleType*)rect, dir, dist, &vacated );
        *rect = *(XP_Rect*)&vacated;
    }
    return canDoIt;
} /* palm_draw_vertScrollBoard */

/* Given some text, determine its bounds and draw it if requested, else
 * return the bounds.  If the width of the string exceeds that of the rect in
 * which it can be fit, split it at ':'.
 */
static void
palmMeasureDrawText( PalmDrawCtx* dctx, XP_Rect* bounds, XP_UCHAR* txt, 
                     XP_Bool vertical, XP_Bool isTurn, XP_UCHAR skipChar, 
                     XP_Bool draw )
{
    XP_U16 len = XP_STRLEN( (const char*)txt );
    XP_U16 widths[2];
    XP_U16 maxWidth, height;
    XP_U16 nLines = 1;
    XP_U16 secondLen = 0;
    XP_UCHAR* second = NULL;
    XP_U16 doubler = 1;

    if ( dctx->doHiRes ) {
        doubler = 2;
    }

    widths[0] = FntCharsWidth( (const char*)txt, len ) + 1;

    if ( widths[0] > bounds->width ) {
        XP_UCHAR ch[2];
        ch[0] = skipChar; 
        ch[1] = '\0';

        XP_ASSERT( skipChar );
        second = (XP_UCHAR*)StrStr( (const char*)txt, (const char*)ch );
        XP_ASSERT( !!second );
        ++second;		/* colon's on the first line */
        secondLen = XP_STRLEN( (const char*)second );

        len -= secondLen;

        if ( skipChar ) {
            --len;
        }

        widths[0] = FntCharsWidth( (const char*)txt, len );
        widths[1] = FntCharsWidth( (const char*)second, secondLen );
        maxWidth = XP_MAX( widths[0], widths[1] );
        ++nLines;
    } else {
        maxWidth = widths[0];
    }

    height = nLines * FONT_HEIGHT + ( LINE_SPACING * (nLines-1) );
    if ( vertical && isTurn ) {
        height += 5;		/* for the horizontal bars */
    }
    height *= doubler;
        
    XP_ASSERT( height <= bounds->height );
    XP_ASSERT( maxWidth <= bounds->width );

    if ( draw ) {
        XP_U16 x, y;

        /* Center what we'll be drawing by advancing the appropriate
           coordinate to eat up half the extra space */
        x = bounds->left + 1;// + (bounds->width - widths[0]) / 2;
        y = bounds->top;
        if ( vertical && isTurn ) {
            y += 1;
        } else {
            y -= 2;
            if ( dctx->doHiRes ) {
                --y;                     /* tweak it up one high-res pixel */
            }
        }

        WinDrawChars( (const char*)txt, len, x, y );
        if ( nLines == 2 ) {
            XP_ASSERT( vertical );
            y += (FONT_HEIGHT + LINE_SPACING) * doubler;
            x = bounds->left + ((bounds->width - widths[1]) / 2);
            WinDrawChars( (const char*)second, secondLen, x, y );
        }

    } else {
        /* return the measurements */
        bounds->width = maxWidth;
        bounds->height = height;
    }

} /* palmMeasureDrawText */

static void
palmFormatRemText( PalmDrawCtx* dctx, XP_UCHAR* buf, XP_S16 nTilesLeft )
{
    const XP_UCHAR* remStr = (*dctx->getResStrFunc)(dctx->globals, 
                                                    STR_REMTILES);
    if ( nTilesLeft < 0 ) {
        nTilesLeft = 0;
    }
    (void)StrPrintF( (char*)buf, (const char*)remStr, nTilesLeft );
} /* palmFormatRemText */

static void
palm_draw_measureRemText( DrawCtx* p_dctx, const XP_Rect* rect, 
                          XP_S16 nTilesLeft, XP_U16* widthP, XP_U16* heightP )
{
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;
    PalmAppGlobals* globals = dctx->globals;
    XP_UCHAR buf[10];
    XP_Rect localRect;
    XP_Bool isVertical = !globals->gState.showGrid;

    palmFormatRemText( dctx, buf, nTilesLeft );

    localRect = *rect;
    palmMeasureDrawText( dctx, &localRect, buf, isVertical, XP_FALSE,
                         ':', XP_FALSE );
    
    *widthP = localRect.width;
    *heightP = localRect.height;
} /* palm_draw_measureRemText */

static void
palm_draw_drawRemText( DrawCtx* p_dctx, const XP_Rect* rInner, 
                       const XP_Rect* rOuter, XP_S16 nTilesLeft,
                       XP_Bool focussed )
{
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;
    PalmAppGlobals* globals = dctx->globals;
    XP_UCHAR buf[10];

    XP_Bool isVertical = !globals->gState.showGrid;

    if ( focussed ) {
        WinPushDrawState();
        WinSetBackColor( dctx->drawingPrefs->drawColors[COLOR_CURSOR] );
        pmEraseRect( rOuter );
    }

    palmFormatRemText( dctx, buf, nTilesLeft );

    palmMeasureDrawText( dctx, (XP_Rect*)rInner, buf, isVertical, XP_FALSE,
                         ':', XP_TRUE );

    if ( focussed ) {
        WinPopDrawState();
    }
} /* palm_draw_drawRemText */

/* Measure text that'll be drawn for player.  If vertical, it'll often get
 * split into two lines, esp after the number of remaining tiles appears.
 */
static void
palmFormatScore( char* buf, const DrawScoreInfo* dsi, XP_Bool vertical )
{
    char borders[] = {'•', '\0'};
    char remBuf[10];
    char* remPart = remBuf;

    if ( vertical || !dsi->isTurn ) {
        borders[0] = '\0';
    }

    if ( dsi->nTilesLeft >= 0 ) {
        StrPrintF( remPart, SCORE_SEPSTR "%d", dsi->nTilesLeft );
    } else {
        *remPart = '\0';
    }

    (void)StrPrintF( buf, "%s%d%s%s", borders, dsi->totalScore, 
                     remPart, borders );
} /* palmFormatScore */

static void
palm_draw_measureScoreText( DrawCtx* p_dctx, const XP_Rect* rect, 
                            const DrawScoreInfo* dsi,
                            XP_U16* widthP, XP_U16* heightP )
{
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;
    PalmAppGlobals* globals = dctx->globals;
    char buf[20];
    /*     FontID oldFont = 0; */
    XP_Bool vertical = !globals->gState.showGrid;
    XP_Rect localRect = *rect;

    /*     if ( !vertical && dsi->selected ) { */
    /* 	oldFont = FntGetFont(); */
    /* 	FntSetFont( boldFont ); */
    /*     } */

    palmFormatScore( buf, dsi, vertical );
    palmMeasureDrawText( dctx, &localRect, (XP_UCHAR*)buf, dsi->isTurn, 
                         vertical, SCORE_SEP, XP_FALSE );

    *widthP = localRect.width;
    *heightP = localRect.height;

    /*     result = widthAndText( buf, score, nTilesInTray, isTurn,  */
    /* 			   !globals->gState.showGrid, &ignore, ignoreLines ); */

    /*     if ( !vertical && dsi->selected ) { */
    /* 	FntSetFont( oldFont ); */
    /*     } */
} /* palm_draw_measureScoreText */

static void
doDrawPlayer( PalmDrawCtx* dctx, const DrawScoreInfo* dsi,
              const XP_Rect* rInner )
{
    PalmAppGlobals* globals = dctx->globals;
    XP_Bool vertical = !globals->gState.showGrid;
    XP_UCHAR scoreBuf[20];

    palmFormatScore( (char*)scoreBuf, dsi, vertical );
    palmMeasureDrawText( dctx, (XP_Rect*)rInner, (XP_UCHAR*)scoreBuf, vertical,
                         dsi->isTurn, SCORE_SEP, XP_TRUE );

    if ( vertical && dsi->isTurn ) {
        RectangleType r = *(RectangleType*)rInner;
        XP_U16 x, y;

        x = r.topLeft.x;
        y = r.topLeft.y + 1;

        WinDrawLine( x, y, x + r.extent.x - 1, y);
        y += r.extent.y - 3;
        WinDrawLine( x, y, x + r.extent.x - 1, y );
    }
}

static void
palm_bnw_draw_score_drawPlayer( DrawCtx* p_dctx, const XP_Rect* rInner, 
                                const XP_Rect* rOuter, 
                                const DrawScoreInfo* dsi )
{
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;

    doDrawPlayer( dctx, dsi, rInner );

    if ( dsi->selected ) {
        WinInvertRectangle( (RectangleType*)rInner, 0 );
    }
} /* palm_bnw_draw_score_drawPlayer */

#define PENDING_DIGITS 3
static void
palm_draw_score_pendingScore( DrawCtx* p_dctx, const XP_Rect* rect, 
                              XP_S16 score, XP_U16 XP_UNUSED(playerNum),
                              CellFlags flags )
{
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;
    char buf[PENDING_DIGITS+1] = "000";
    RectangleType oldClip, newClip;
    XP_U16 x = rect->left + 1;
    IndexedColorType oclr = 0;

    XP_ASSERT( flags == CELL_NONE || flags == CELL_ISCURSOR );

    HIGHRES_PUSH_LOC(dctx);

    if ( flags != CELL_NONE ) {
        oclr = WinSetBackColor( dctx->drawingPrefs->drawColors[COLOR_CURSOR] );
    }

    WinGetClip( &oldClip );
    RctGetIntersection( &oldClip, (RectangleType*)rect, &newClip );
    if ( newClip.extent.y > 0 ) {
        WinSetClip( &newClip );
        pmEraseRect( rect );

        if ( score >= 0 ) {
            XP_UCHAR tbuf[4];
            UInt16 len;
            if ( score <= 999 ) {
                StrPrintF( (char*)tbuf, "%d", score );
            } else {
                StrCopy( (char*)tbuf, "wow" ); /* thanks, Marcus :-) */
            }
            len = XP_STRLEN( (const char*)tbuf );
            XP_MEMCPY( &buf[PENDING_DIGITS-len], tbuf, len );
        } else {
            StrCopy( buf, "???" );
        }

        /* There's no room for the pts string if we're in highres mode and
           WinSetScalingMode isn't available. */
        if ( !dctx->doHiRes || dctx->oneDotFiveAvail ) {
            const XP_UCHAR* str = (*dctx->getResStrFunc)( dctx->globals, 
                                                          STR_PTS );

            if ( dctx->oneDotFiveAvail ) {
                smallBoldStringAt( (const char*)str, XP_STRLEN((const char*)str), 
                                   x, rect->top );
            } else {
                WinDrawChars( (const char*)str, 
                              XP_STRLEN((const char*)str), x, rect->top );
            }
        }

        WinDrawChars( buf, PENDING_DIGITS, x, 
                      rect->top + (rect->height/2) - 1 );
        WinSetClip( &oldClip );
    }

    if ( flags != 0 ) {
        (void)WinSetBackColor( oclr );
    }

    HIGHRES_POP_LOC(dctx);
} /* palm_draw_score_pendingScore */

static void
palmFormatTimerText( XP_UCHAR* buf, XP_S16 secondsLeft )
{
    XP_U16 minutes, seconds;
    XP_UCHAR secBuf[6];

    if ( secondsLeft < 0 ) {
        *buf++ = '-';
        secondsLeft *= -1;
    }
    minutes = secondsLeft / 60;
    seconds = secondsLeft % 60;

    /* StrPrintF can't do 0-padding; do it manually.  Otherwise 5:03 will
       come out 5:3 */
    StrPrintF( (char*)secBuf, "0%d", seconds );
    StrPrintF( (char*)buf, "%d:%s", minutes,
               secBuf[2] == '\0'? secBuf:&secBuf[1] );
} /* palmFormatTimerText */

static void
palm_draw_drawTimer( DrawCtx* p_dctx, const XP_Rect* rect, 
                     XP_U16 XP_UNUSED(player), XP_S16 secondsLeft )
{
    /* This is called both from within drawScoreboard and not, meaning that
     * sometimes draw_boardBegin() and draw_boardFinished() bracket it and
     * sometimes they don't.  So it has to do its own HIGHRES_PUSH_LOC stuff.
     */
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;
    XP_UCHAR buf[10];
    XP_Rect localR = *rect;
    RectangleType saveClip;
    XP_U16 len, width, y;

    HIGHRES_PUSH_LOC(dctx);

    palmFormatTimerText( buf, secondsLeft );
    len = XP_STRLEN( (const char*)buf );

    width = FntCharsWidth( (const char*)buf, len );

    pmEraseRect( &localR );

    if ( width < localR.width ) {
        localR.left += localR.width - width;
        localR.width = width;
    }

    y = localR.top - 2;
    if ( dctx->doHiRes ) {
        y -= 1;                 /* tweak it up one high-res pixel */
    }

    WinGetClip( &saveClip );
    WinSetClip( (RectangleType*)&localR );

    WinDrawChars( (const char*)buf, len, localR.left, y );
    WinSetClip( &saveClip );

    HIGHRES_POP_LOC(dctx);
} /* palm_draw_drawTimer */

#define MINI_LINE_HT 12
#define MINI_V_PADDING 6
#define MINI_H_PADDING 8

static const XP_UCHAR*
palm_draw_getMiniWText( DrawCtx* p_dctx, XWMiniTextType textHint )
{
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx; 
    const XP_UCHAR* str;
    XP_U16 strID = 0;		/* make compiler happy */

    switch( textHint ) {
    case BONUS_DOUBLE_LETTER:
        strID = STR_DOUBLE_LETTER; break;
    case BONUS_DOUBLE_WORD:
        strID = STR_DOUBLE_WORD; break;
    case BONUS_TRIPLE_LETTER:
        strID = STR_TRIPLE_LETTER; break;
    case BONUS_TRIPLE_WORD:
        strID = STR_TRIPLE_WORD; break;
    case INTRADE_MW_TEXT:
        strID = STR_TRADING_REMINDER; break;
    default:
        XP_ASSERT( XP_FALSE );
    }

    str = (*dctx->getResStrFunc)( dctx->globals, strID );

    return str;
} /* palm_draw_getMiniWText */

static void
splitString( const XP_UCHAR* str, XP_U16* nBufsP, XP_UCHAR** bufs )
{
    XP_U16 nBufs = 0;

    for ( ; ; ) {
        XP_UCHAR* nextStr = StrStr( str, XP_CR );
        XP_U16 len = nextStr==NULL? XP_STRLEN(str): nextStr - str;

        XP_ASSERT( nextStr != str );

        XP_MEMCPY( bufs[nBufs], str, len );
        bufs[nBufs][len] = '\0';
        ++nBufs;

        if ( nextStr == NULL ) {
            break;
        }
        str = nextStr + XP_STRLEN(XP_CR);	/* skip '\n' */
    }
    *nBufsP = nBufs;
} /* splitString */

#define VALUE_HINT_RECT_HEIGHT 12
#define VALUE_HINT_RECT_HEIGHT_HR 24
static XP_U16
getMiniLineHt( PalmDrawCtx* dctx )
{
    if ( dctx->doHiRes ) {
        return VALUE_HINT_RECT_HEIGHT_HR;
    } else {
        return VALUE_HINT_RECT_HEIGHT;
    }
} /* getMiniLineHt */

static void
palm_draw_measureMiniWText( DrawCtx* p_dctx, const XP_UCHAR* str, 
                            XP_U16* widthP, XP_U16* heightP )
{
    XP_U16 maxWidth, height;
    XP_UCHAR buf1[48];
    XP_UCHAR buf2[48];
    XP_UCHAR* bufs[2] = { buf1, buf2 };
    XP_U16 nBufs, i;

    HIGHRES_PUSH_LOC( (PalmDrawCtx*)p_dctx );
    FntSetFont( stdFont );

    splitString( str, &nBufs, bufs );

    for ( height = 0, maxWidth = 0, i = 0; i < nBufs; ++i ) {
        XP_U16 oneWidth = 8 + FntCharsWidth( (const char*)bufs[i], 
                                             XP_STRLEN(bufs[i]) );

        maxWidth = XP_MAX( maxWidth, oneWidth );
        height += getMiniLineHt((PalmDrawCtx*)p_dctx);
    }

    *widthP = maxWidth;
    *heightP = height + 4;

    HIGHRES_POP_LOC( (PalmDrawCtx*)p_dctx );
} /* palm_draw_measureMiniWText */

static void
palm_draw_drawMiniWindow( DrawCtx* p_dctx, const XP_UCHAR* text, 
                          const XP_Rect* rect, void** XP_UNUSED(closureP) )
{
    XP_Rect localR;
    XP_UCHAR buf1[48];
    XP_UCHAR buf2[48];
    XP_UCHAR* bufs[2] = { buf1, buf2 };
    XP_U16 nBufs, i, offset;
    PalmDrawCtx* dctx = (PalmDrawCtx*)p_dctx;

    HIGHRES_PUSH_LOC(dctx);

    localR = *rect;
    insetRect( &localR, 1 );
    WinEraseRectangle( (RectangleType*)&localR, 0 );
    localR.left++;
    localR.top++;
    localR.width -= 3;
    localR.height -= 3;
    WinDrawRectangleFrame( popupFrame, (RectangleType*)&localR );

    splitString( text, &nBufs, bufs );

    offset = 0;
    for ( i = 0; i < nBufs; ++i ) {
        XP_UCHAR* txt = bufs[i];
        XP_U16 len = XP_STRLEN( txt );
        XP_U16 width = FntCharsWidth( txt, len );

        WinDrawChars( (const char*)txt, len,
                      localR.left + ((localR.width-width)/2),
                      localR.top + 1 + offset );
        offset += getMiniLineHt( dctx );
    }

    HIGHRES_POP_LOC( dctx );
} /* palm_draw_drawMiniWindow */

static void
draw_doNothing( DrawCtx* XP_UNUSED(dctx), ... )
{
} /* draw_doNothing */

DrawCtx* 
palm_drawctxt_make( MPFORMAL GraphicsAbility able, 
                    PalmAppGlobals* globals,
                    GetResStringFunc getRSF, DrawingPrefs* drawprefs )
{
    PalmDrawCtx* dctx;
    XP_U16 i;
    XP_U16 cWinWidth, cWinHeight;

    dctx = XP_MALLOC( mpool, sizeof(PalmDrawCtx) );
    XP_MEMSET( dctx, 0, sizeof(PalmDrawCtx) );

    MPASSIGN(dctx->mpool, mpool);

    dctx->able = able;
    dctx->doHiRes = globals->hasHiRes && globals->width == 320;
    dctx->globals = globals;
    dctx->getResStrFunc = getRSF;
    dctx->drawingPrefs = drawprefs;

    dctx->vtable = XP_MALLOC( mpool,
                              sizeof(*(((PalmDrawCtx*)dctx)->vtable)) );
    for ( i = 0; i < sizeof(*dctx->vtable)/4; ++i ) {
        ((void**)(dctx->vtable))[i] = draw_doNothing;
    }

    /* To keep the number of entry points this file has to 1, all
       functions but the initter called from here must go through a
       vtable call.  so....*/
    dctx->drawBitmapFunc = drawBitmapAt;

    SET_VTABLE_ENTRY( dctx->vtable, draw_destroyCtxt, palm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_dictChanged, palm );

    SET_VTABLE_ENTRY( dctx->vtable, draw_invertCell, palm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTile, palm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTileBack, palm );
#ifdef POINTER_SUPPORT
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTileMidDrag, palm );
#endif
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTrayDivider, palm );

    SET_VTABLE_ENTRY( dctx->vtable, draw_drawBoardArrow, palm );

    SET_VTABLE_ENTRY( dctx->vtable, draw_scoreBegin, palm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_vertScrollBoard, palm );

    SET_VTABLE_ENTRY( dctx->vtable, draw_measureRemText, palm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawRemText, palm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureScoreText, palm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_score_pendingScore, palm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_objFinished, palm );

    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTimer, palm );

    SET_VTABLE_ENTRY( dctx->vtable, draw_getMiniWText, palm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureMiniWText, palm );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawMiniWindow, palm );

    if ( able == COLOR ) {
#ifdef COLOR_SUPPORT
        SET_VTABLE_ENTRY( dctx->vtable, draw_boardBegin, palm_clr );
        SET_VTABLE_ENTRY( dctx->vtable, draw_drawCell, palm_clr );
        SET_VTABLE_ENTRY( dctx->vtable, draw_score_drawPlayer, palm_clr );
        SET_VTABLE_ENTRY( dctx->vtable, draw_trayBegin, palm_clr );
        SET_VTABLE_ENTRY( dctx->vtable, draw_clearRect, palm_clr );
        SET_VTABLE_ENTRY( dctx->vtable, draw_drawMiniWindow, palm_clr );

        SET_VTABLE_ENTRY( dctx->vtable, draw_drawBoardArrow, palm_clr );
#else
        XP_ASSERT(0);
#endif
    } else {
        SET_VTABLE_ENTRY( dctx->vtable, draw_boardBegin, palm_common );
        SET_VTABLE_ENTRY( dctx->vtable, draw_drawCell, palm_common );
        SET_VTABLE_ENTRY( dctx->vtable, draw_score_drawPlayer, palm_bnw );
        SET_VTABLE_ENTRY( dctx->vtable, draw_trayBegin, palm_bnw );
        SET_VTABLE_ENTRY( dctx->vtable, draw_clearRect, palm_bnw );
    }

    cWinWidth = CHARRECT_WIDTH;
    cWinHeight = CHARRECT_HEIGHT;
    if ( dctx->doHiRes ) {
        cWinWidth *= 2;
        cWinHeight *= 2;
    }

    if ( able == COLOR ) {
    } else {
        short patBits[] = { 0x8844, 0x2211, 0x8844, 0x2211,
                            0xaa55, 0xaa55, 0xaa55, 0xaa55,  
                            0xCC66, 0x3399, 0xCC66, 0x3399,
                            0xCCCC, 0x3333, 0xCCCC, 0x3333 };
        XP_MEMCPY( &dctx->u.bnw.valuePatterns[0], patBits, sizeof(patBits) );
    }

    dctx->fntHeight = FntBaseLine();
    dctx->oneDotFiveAvail = globals->oneDotFiveAvail;

    return (DrawCtx*)dctx;
} /* palm_drawctxt_make */

