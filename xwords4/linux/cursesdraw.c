/* -*-mode: C; fill-column: 78; c-basic-offset: 4; compile-command: "make MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 1997-2008 by Eric House (xwords@eehouse.org).  All rights
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

#ifdef PLATFORM_NCURSES

#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

#include "cursesmain.h"
#include "draw.h"
#include "board.h"
#include "dbgutil.h"

typedef struct CursesDrawCtx {
    DrawCtxVTable* vtable;

    WINDOW* boardWin;

} CursesDrawCtx;

static void curses_draw_clearRect( DrawCtx* p_dctx, const XP_Rect* rectP );
static void getTops( const XP_Rect* rect, int* toptop, int* topbot );

static void
drawRect( WINDOW* win, const XP_Rect* rect, char vert, char hor )
{
    wmove( win, rect->top-1, rect->left );
    whline( win, hor, rect->width );
    wmove( win, rect->top+rect->height, rect->left );
    whline( win, hor, rect->width );

    wmove( win, rect->top, rect->left-1 );
    wvline( win, vert, rect->height );
    wmove( win, rect->top, rect->left+rect->width );
    wvline( win, vert, rect->height );
} /* drawRect */

static void
eraseRect( CursesDrawCtx* dctx, const XP_Rect* rect )
{
    int y, bottom = rect->top + rect->height;
    for ( y = rect->top; y < bottom; ++y ) {
        mvwhline( dctx->boardWin, y, rect->left, ' ', rect->width );
    }
} /* eraseRect */

static void
cursesHiliteRect( WINDOW* window, const XP_Rect* rect )
{
    int right, width, x, y;
    width = rect->width;
    right = width + rect->left;
    wstandout( window );
    for ( y = rect->top; y < rect->top + rect->height; ++y ) {
        for ( x = rect->left; x < right; ++x ) {
            chtype cht = mvwinch( window, y, x );
            char ch = cht & A_CHARTEXT;
            mvwaddch( window, y, x, ch );
        }
    }
    wstandend( window );
}

static void
curses_draw_destroyCtxt( DrawCtx* XP_UNUSED(p_dctx) )
{
    // CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
} /* draw_setup */

static void
curses_draw_dictChanged( DrawCtx* XP_UNUSED(p_dctx), 
                         const DictionaryCtxt* XP_UNUSED(dict) )
{
}

static XP_Bool
curses_draw_boardBegin( DrawCtx* XP_UNUSED(p_dctx), 
                        const XP_Rect* XP_UNUSED(rect), 
                        DrawFocusState XP_UNUSED(dfs) )
{
    return XP_TRUE;
} /* draw_finish */

static XP_Bool
curses_draw_trayBegin( DrawCtx* XP_UNUSED(p_dctx), 
                       const XP_Rect* XP_UNUSED(rect), 
                       XP_U16 XP_UNUSED(owner), 
                       DrawFocusState XP_UNUSED(dfs) )
{
    return XP_TRUE;
} /* draw_finish */

static void
curses_draw_scoreBegin( DrawCtx* p_dctx, const XP_Rect* rect, 
                        XP_U16 XP_UNUSED(numPlayers), 
                        const XP_S16* const XP_UNUSED(scores), 
                        XP_S16 XP_UNUSED(remCount), 
                        DrawFocusState XP_UNUSED(dfs) )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    eraseRect( dctx, rect );
} /* curses_draw_scoreBegin */

static void
formatRemText( char* buf, int bufLen, XP_S16 nTilesLeft, int width )
{
    snprintf( buf, bufLen, "Tiles left in pool: %.3d", nTilesLeft );
    if ( strlen(buf)+1 >= width ) {
        snprintf( buf, bufLen, "Rem: %.3d", nTilesLeft );
    }
} /* formatRemText */

static void
curses_draw_measureRemText( DrawCtx* XP_UNUSED(dctx), 
                            const XP_Rect* r, 
                            XP_S16 nTilesLeft, 
                            XP_U16* width, XP_U16* height )
{
    char buf[32];
    formatRemText( buf, sizeof(buf), nTilesLeft, r->width );
    
    *width = strlen(buf);
    *height = 1;
} /* curses_draw_measureRemText */

static void
curses_draw_drawRemText( DrawCtx* p_dctx, const XP_Rect* rInner, 
                         const XP_Rect* XP_UNUSED(rOuter), XP_S16 nTilesLeft,
                         XP_Bool focussed )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    char buf[32];

    formatRemText( buf, sizeof(buf), nTilesLeft, rInner->width );
    mvwprintw( dctx->boardWin, rInner->top, rInner->left, buf );
    if ( focussed ) {
        cursesHiliteRect( dctx->boardWin, rInner );
    }
} /* curses_draw_drawRemText */

static int
fitIn( char* buf, int len, int* rem, const char* str )
{
    int slen = strlen(str);
    if ( !!rem && (*rem != 0) ) {
        ++len;
        --*rem;
    }
    if ( slen > len ) {
        slen = len;
    }

    memcpy( buf, str, slen );
    return len;
} /* fitIn */

static void
formatScoreText( XP_UCHAR* out, int outLen, const DrawScoreInfo* dsi,
                 int width )
{
    /* Long and short formats.  We'll try long first.  If it fits, cool.
       Otherwise we use short.  Either way, we fill the whole rect so it can
       overwrite anything that was there before.*/
    char tmp[width+1];
    char buf[width+1];
    int scoreWidth = 4;

    XP_ASSERT( width < outLen );

    XP_MEMSET( buf, ' ', width );
    buf[width] = '\0';

    /* Status/role chars at start */
    if ( dsi->isTurn ) {
        buf[0] = 'T';
    }
    if ( dsi->selected ) {
        buf[1] = 'S';
    }
    if ( dsi->isRobot) {
        buf[2] = 'r';
    }

    /* Score always goes at end.  Will overwrite status if width is really small */
    snprintf( tmp, scoreWidth, "%.3d", dsi->totalScore );
    memcpy( &buf[width-scoreWidth+1], tmp, scoreWidth-1 );

    /* Now we want to fit name, rem tiles, last score, and last move, if
       there's space.  Allocate to each so they're in columns. */

    width -= 8;                 /* status chars plus space; score plus space */
    if ( width > 0 ) {
        int pos = 4;
        int nCols = 2;
        int perCol = (width - ( nCols - 1)) / nCols; /* not counting spaces */
        if ( perCol > 0 ) {
            int rem = (width - ( nCols - 1)) % nCols;

            pos += 1 + fitIn( &buf[pos], perCol, &rem, dsi->name );

            XP_U16 len = sizeof(tmp);
            if ( (*dsi->lsc)( dsi->lscClosure, dsi->playerNum, tmp, &len ) ) {
                char* s = tmp;
                if ( len > perCol ) {
                    /* We want to preserve the score first, then the first part of
                       word.  That is, WORD:20 prints as W0:20, not WORD: or
                       RD:20 */
                    char* colon = strstr( tmp, ":" );
                    if ( colon ) {
                        s += len - perCol;
                        memmove( s, tmp, colon - tmp - (len - perCol) );
                    }
                }
                pos += 1 + fitIn( &buf[pos], perCol, NULL, s );
            }
        } else {
            fitIn( &buf[pos], width, NULL, dsi->name );
        }
    }        

    snprintf( out, outLen, "%s", buf );
} /* formatScoreText */

static void
curses_draw_measureScoreText( DrawCtx* XP_UNUSED(p_dctx), 
                              const XP_Rect* r, 
                              const DrawScoreInfo* dsi,
                              XP_U16* width, XP_U16* height )
{
    XP_UCHAR buf[100];
    formatScoreText( buf, sizeof(buf), dsi, r->width );

    *width = strlen( buf );
    XP_ASSERT( *width <= r->width );
    *height = 1;		/* one line per player */
} /* curses_draw_measureScoreText */

static void
curses_draw_score_pendingScore( DrawCtx* p_dctx, const XP_Rect* rect, 
                                XP_S16 score, XP_U16 XP_UNUSED(playerNum),
                                CellFlags XP_UNUSED(flags) )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    char buf[4];

    if ( score >= 0 ) {
        sprintf( buf, "%.3d", score );
    } else {
        strcpy( buf, "???" );
    }

    int toptop, topbot;
    getTops( rect, &toptop, &topbot );

    mvwprintw( dctx->boardWin, toptop, rect->left, "pt:" );
    mvwprintw( dctx->boardWin, topbot, rect->left, "%s", buf );
} /* curses_draw_score_pendingScore */

static void
curses_draw_objFinished( DrawCtx* p_dctx, BoardObjectType XP_UNUSED(typ), 
                         const XP_Rect* XP_UNUSED(rect), 
                         DrawFocusState XP_UNUSED(dfs) )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    wrefresh( dctx->boardWin );
} /* curses_draw_objFinished */

#define MY_PAIR 1

static void
curses_draw_score_drawPlayer( DrawCtx* p_dctx, const XP_Rect* rInner, 
                              const XP_Rect* rOuter, const DrawScoreInfo* dsi )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    char buf[100];
    int y = rInner->top;

    curses_draw_clearRect( p_dctx, rOuter );

    /* print the name and turn/remoteness indicator */
    formatScoreText( buf, sizeof(buf), dsi, rInner->width );
    mvwprintw( dctx->boardWin, y, rOuter->left, buf );

    if ( (dsi->flags&CELL_ISCURSOR) != 0 ) {
        cursesHiliteRect( dctx->boardWin, rOuter );
    }
} /* curses_draw_score_drawPlayer */

static XP_Bool
curses_draw_drawCell( DrawCtx* p_dctx, const XP_Rect* rect, 
                      const XP_UCHAR* letter, 
                      const XP_Bitmaps* XP_UNUSED(bitmaps),
                      Tile XP_UNUSED(tile), XP_S16 XP_UNUSED(owner), 
                      XWBonusType bonus, HintAtts XP_UNUSED(hintAtts), 
                      CellFlags flags )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    XP_UCHAR loc[4] = { ' ', ' ', ' ', '\0' };
    XP_ASSERT( rect->width < sizeof(loc) );
    if ( !!letter ) {
        XP_MEMCPY( loc, letter, strlen(letter) );
    }

    /* in case it's not 1x1 */
    eraseRect( dctx, rect );

    if ( (flags & (CELL_DRAGSRC|CELL_ISEMPTY)) != 0 ) {
        switch ( bonus ) {
        case BONUS_DOUBLE_LETTER:
            loc[0] = '+'; break;
        case BONUS_DOUBLE_WORD:
            loc[0] = '*'; break;
        case BONUS_TRIPLE_LETTER:
            loc[0] = '^'; break;
        case BONUS_TRIPLE_WORD:
            loc[0] = '#'; break;
        default:
            loc[0] = ' ';
        } /* switch */
    }

    if ( (flags&CELL_HIGHLIGHT) != 0 ) {
        wstandout( dctx->boardWin );
    }

    mvwaddnstr( dctx->boardWin, rect->top, rect->left, 
                loc, rect->width );

    if ( (flags&CELL_HIGHLIGHT) != 0 ) {
        wstandend( dctx->boardWin );
    }

    if ( (flags&CELL_ISCURSOR) != 0 ) {
        cursesHiliteRect( dctx->boardWin, rect );
    }

    return XP_TRUE;
} /* curses_draw_drawCell */

static void
getTops( const XP_Rect* rect, int* toptop, int* topbot )
{
    int top = rect->top;
    if ( rect->height >= 4 ) {
        ++top;
    }
    *toptop = top;

    top = rect->top + rect->height - 1;
    if ( rect->height >= 3 ) {
        --top;
    }
    *topbot = top;
} /* getTops */

static void
curses_stringInTile( CursesDrawCtx* dctx, const XP_Rect* rect, 
                     XP_UCHAR* letter, XP_UCHAR* val )
{
    eraseRect( dctx, rect );

    int toptop, topbot;
    getTops( rect, &toptop, &topbot );

    if ( !!letter ) {
        mvwaddnstr( dctx->boardWin, toptop, rect->left+(rect->width/2),
                    letter, strlen(letter) );
    }

    if ( !!val ) {
        int len = strlen( val );
        mvwaddnstr( dctx->boardWin, topbot, rect->left + rect->width - len, 
                    val, len );
    }
} /* curses_stringInTile */

static void
curses_draw_drawTile( DrawCtx* p_dctx, const XP_Rect* rect, 
                      const XP_UCHAR* textP, const XP_Bitmaps* XP_UNUSED(bitmaps),
                      XP_U16 val, CellFlags flags )
{
    char numbuf[5];
    char letterbuf[5];
    char* nump = NULL;
    char* letterp = NULL;
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;

    if ( (flags&CELL_ISEMPTY) == 0 ) {
        letterbuf[0] = !!textP? *textP: '_'; /* BLANK or bitmap */
        letterbuf[1] = '\0';
        if ( (flags&CELL_VALHIDDEN) == 0  ) {
            sprintf( numbuf, "%.2d", val );
            if ( numbuf[0] == '0' ) {
                numbuf[0] = ' ';
            }
            nump = numbuf;
        }
        letterp = letterbuf;
    }
    curses_stringInTile( dctx, rect, letterp, nump );

    if ( (flags&CELL_HIGHLIGHT) != 0 ) {
        mvwaddnstr( dctx->boardWin, rect->top+rect->height-1, 
                    rect->left, "*-*", 3 );
    }

    if ( (flags&CELL_ISCURSOR) != 0 ) {
        cursesHiliteRect( dctx->boardWin, rect );
    }
} /* curses_draw_drawTile */

static  void
curses_draw_drawTileBack( DrawCtx* p_dctx, const XP_Rect* rect, 
                          CellFlags flags )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    curses_stringInTile( dctx, rect, "?", "?" );
    if ( (flags&CELL_ISCURSOR) != 0 ) {
        cursesHiliteRect( dctx->boardWin, rect );
    }
} /* curses_draw_drawTileBack */

static void
curses_draw_drawTrayDivider( DrawCtx* p_dctx, const XP_Rect* rect, 
                             CellFlags XP_UNUSED(flags) )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    wmove( dctx->boardWin, rect->top, rect->left );
    wvline( dctx->boardWin, '#', rect->height );

} /* curses_draw_drawTrayDivider */

static void
curses_draw_drawBoardArrow( DrawCtx* p_dctx, const XP_Rect* rect, 
                            XWBonusType XP_UNUSED(cursorBonus), 
                            XP_Bool vertical, HintAtts XP_UNUSED(hintAtts),
                            CellFlags XP_UNUSED(flags) )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
#if 1
    char ch = vertical?'|':'-';
    mvwaddch( dctx->boardWin, rect->top, rect->left, ch );
#else
    chtype curChar = mvwinch(dctx->boardWin, rect->top, rect->left );
    wstandout( dctx->boardWin );
    mvwaddch( dctx->boardWin, rect->top, rect->left, curChar);
    wstandend( dctx->boardWin );
#endif
} /* curses_draw_drawBoardArrow */

static void 
curses_draw_clearRect( DrawCtx* p_dctx, const XP_Rect* rectP )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    XP_Rect rect = *rectP;

    eraseRect( dctx, &rect );
} /* curses_draw_clearRect */

static const XP_UCHAR*
curses_draw_getMiniWText( DrawCtx* XP_UNUSED(p_dctx), 
                          XWMiniTextType XP_UNUSED(textHint) )
{
    return "Trading...";
} /* curses_draw_getMiniWText */

static void
curses_draw_measureMiniWText( DrawCtx* XP_UNUSED(p_dctx), const XP_UCHAR* str, 
                              XP_U16* widthP, XP_U16* heightP )
{
    *widthP = strlen(str) + 4;
    *heightP = 3;
} /* curses_draw_measureMiniWText */

static void
curses_draw_drawMiniWindow( DrawCtx* p_dctx, const XP_UCHAR* text,
                            const XP_Rect* rect, void** XP_UNUSED(closure) )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    XP_Rect smallerR;

    XP_ASSERT(0);               /* does this really get called? */

    smallerR.top = rect->top + 1;
    smallerR.left = rect->left + 1;
    smallerR.width = rect->width - 2;
    smallerR.height = rect->height - 2;

    eraseRect( dctx, rect );
    drawRect( dctx->boardWin, &smallerR, '|', '-' );

    mvwprintw( dctx->boardWin, smallerR.top, smallerR.left, text, 
               strlen(text) );
} /* curses_draw_drawMiniWindow */

#if 0
static void
curses_draw_frameTray( DrawCtx* p_dctx, XP_Rect* rect )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    box( dctx->boardWin, '*', '+');
} /* curses_draw_frameTray */
#endif

static void
draw_doNothing( DrawCtx* XP_UNUSED(dctx), ... )
{
} /* draw_doNothing */

DrawCtx* 
cursesDrawCtxtMake( WINDOW* boardWin )
{
    CursesDrawCtx* dctx = malloc( sizeof(CursesDrawCtx) );
    short i;

    dctx->vtable = malloc( sizeof(*(((CursesDrawCtx*)dctx)->vtable)) );

    for ( i = 0; i < sizeof(*dctx->vtable)/4; ++i ) {
        ((void**)(dctx->vtable))[i] = draw_doNothing;
    }

    SET_VTABLE_ENTRY( dctx->vtable, draw_destroyCtxt, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_dictChanged, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_boardBegin, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_objFinished, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_trayBegin, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_scoreBegin, curses );

    SET_VTABLE_ENTRY( dctx->vtable, draw_measureRemText, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawRemText, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureScoreText, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_score_pendingScore, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_score_drawPlayer, curses );

    SET_VTABLE_ENTRY( dctx->vtable, draw_drawCell, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTile, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTileBack, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTrayDivider, curses );
    
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawBoardArrow, curses );

    SET_VTABLE_ENTRY( dctx->vtable, draw_clearRect, curses );

    SET_VTABLE_ENTRY( dctx->vtable, draw_drawMiniWindow, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_getMiniWText, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureMiniWText, curses );

    dctx->boardWin = boardWin;

    return (DrawCtx*)dctx;
} /* curses_drawctxt_init */

#endif /* PLATFORM_NCURSES */
