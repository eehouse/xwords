/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */ 
/* 
 * Copyright 1997-2000 by Eric House (xwords@eehouse.org).  All rights reserved.
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

typedef struct CursesDrawCtx {
    DrawCtxVTable* vtable;

    WINDOW* boardWin;

} CursesDrawCtx;

static void curses_draw_clearRect( DrawCtx* p_dctx, const XP_Rect* rectP );

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
    LOG_FUNC();
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
drawFocusRect( CursesDrawCtx* dctx, DrawFocusState dfs, const XP_Rect* rect )
{
    WINDOW* boardWin = dctx->boardWin;
    if ( dfs == DFS_NONE ) {
        drawRect( boardWin, rect, '|', '-' ); 
    } else if ( dfs == DFS_TOP ) {
        drawRect( boardWin, rect, '*', '*' ); 
    } else if ( dfs == DFS_DIVED ) {
        drawRect( boardWin, rect, '+', '+' ); 
    } else {
        XP_ASSERT(0);
    }
}

static XP_Bool
curses_draw_boardBegin( DrawCtx* p_dctx, const DictionaryCtxt* XP_UNUSED(dict),
                        const XP_Rect* rect, DrawFocusState dfs )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    drawFocusRect( dctx, dfs, rect );
    return XP_TRUE;
} /* draw_finish */

static XP_Bool
curses_draw_trayBegin( DrawCtx* p_dctx, const XP_Rect* rect, 
                       XP_U16 XP_UNUSED(owner), DrawFocusState dfs )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    drawFocusRect( dctx, dfs, rect );
    return XP_TRUE;
} /* draw_finish */

static void
curses_draw_scoreBegin( DrawCtx* p_dctx, const XP_Rect* rect, 
                        XP_U16 XP_UNUSED(numPlayers), DrawFocusState dfs )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    drawFocusRect( dctx, dfs, rect );
} /* curses_draw_scoreBegin */

static void
formatRemText( char* buf, XP_S16 nTilesLeft )
{
    strcpy( buf, "Tiles left in pool: " );
    buf += strlen( buf );
    if ( nTilesLeft < 0 ) {
        strcpy( buf, "***" );
    } else {
        sprintf( buf, "%.3d", nTilesLeft );
    }
} /* formatRemText */

static void
curses_draw_measureRemText( DrawCtx* XP_UNUSED(dctx), 
                            const XP_Rect* XP_UNUSED(r), 
                            XP_S16 nTilesLeft, 
                            XP_U16* width, XP_U16* height )
{
    char buf[32];

    formatRemText( buf, nTilesLeft );
    
    *width = strlen(buf);
    *height = 1;
} /* curses_draw_measureRemText */

static void
curses_draw_drawRemText( DrawCtx* p_dctx, const XP_Rect* rInner, 
                         const XP_Rect* XP_UNUSED(rOuter), XP_S16 nTilesLeft )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    char buf[32];

    formatRemText( buf, nTilesLeft );
    mvwprintw( dctx->boardWin, rInner->top, rInner->left, buf );
} /* curses_draw_drawRemText */

#define SCORE_COL 25
#define RECENT_COL 31

static void
formatScoreText( XP_UCHAR* buf, const DrawScoreInfo* dsi )
{
    XP_S16 nTilesLeft = dsi->nTilesLeft;
    XP_Bool isRobot = dsi->isRobot;
    char label = isRobot? 'r':'n';
    int len;
    char recbuf[32];
    XP_U16 recBufLen = sizeof(recbuf);

    if ( nTilesLeft < 0 ) {
        nTilesLeft = MAX_TRAY_TILES;
    }
    if ( dsi->isRemote ) {
        label = toupper(label);
    }

    len = sprintf( buf, "%c:%c [%c] %s (%d)", 
                   (dsi->isTurn? 'T' : ' '), 
                   (dsi->selected? 'S' : ' '), 
                   label, dsi->name, nTilesLeft );
    while ( len < SCORE_COL ) {
        ++len;
        strcat( buf, " " );
    }
    len += sprintf( buf + len, "%.3d", dsi->score );

    if ( (*dsi->lsc)( dsi->lscClosure, dsi->playerNum, recbuf, &recBufLen ) ) {
        while ( len < RECENT_COL ) {
            ++len;
            strcat( buf, " " );
        }
        strcat( buf, recbuf );
    }

} /* formatScoreText */

static void
curses_draw_measureScoreText( DrawCtx* XP_UNUSED(p_dctx), 
                              const XP_Rect* XP_UNUSED(r), 
                              const DrawScoreInfo* dsi,
                              XP_U16* width, XP_U16* height )
{
    XP_UCHAR buf[100];
    formatScoreText( buf, dsi );

    *width = strlen( buf );
    *height = 1;		/* one line per player */
} /* curses_draw_measureScoreText */

static void
curses_draw_score_pendingScore( DrawCtx* p_dctx, const XP_Rect* rect, 
                                XP_S16 score, XP_U16 XP_UNUSED(playerNum) )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    char buf[4];

    if ( score >= 0 ) {
        sprintf( buf, "%.3d", score );
    } else {
        strcpy( buf, "???" );
    }

    mvwprintw( dctx->boardWin, rect->top+1, rect->left, "pt:" );
    mvwprintw( dctx->boardWin, rect->top+2, rect->left, "%s", buf );
    wrefresh( dctx->boardWin );
} /* curses_draw_score_pendingScore */

static void
curses_draw_boardFinished( DrawCtx* p_dctx )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    wrefresh( dctx->boardWin );
} /* curses_draw_boardFinished */

static void
curses_draw_trayFinished( DrawCtx* p_dctx )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    wrefresh( dctx->boardWin );
} /* draw_finished */

static void
curses_draw_scoreFinished( DrawCtx* p_dctx )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    wrefresh( dctx->boardWin );
} /* draw_finished */

#define MY_PAIR 1

static void
curses_draw_score_drawPlayer( DrawCtx* p_dctx, const XP_Rect* rInner, 
                              const XP_Rect* rOuter, const DrawScoreInfo* dsi )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    char buf[100];
    int y = rInner->top;

    curses_draw_clearRect( p_dctx, rOuter );

    /* first blank out the whole thing! */
    mvwhline( dctx->boardWin, y, rOuter->left, ' ', rOuter->width );

    /* print the name and turn/remoteness indicator */
    formatScoreText( buf, dsi );
    mvwprintw( dctx->boardWin, y, rOuter->left, buf );
} /* curses_draw_score_drawPlayer */

static XP_Bool
curses_draw_drawCell( DrawCtx* p_dctx, const XP_Rect* rect, 
                      const XP_UCHAR* letter, XP_Bitmap XP_UNUSED(bitmap),
                      Tile XP_UNUSED(tile), XP_S16 XP_UNUSED(owner), 
                      XWBonusType bonus, HintAtts XP_UNUSED(hintAtts), 
                      XP_Bool XP_UNUSED(isBlank), XP_Bool highlight, 
                      XP_Bool XP_UNUSED(isStar) )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    XP_UCHAR loc[4];
    XP_ASSERT( XP_STRLEN(letter) < sizeof(loc) );
    XP_STRNCPY( loc, letter, sizeof(loc) );

    if ( loc[0] == LETTER_NONE ) {
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

    if ( highlight ) {
        wstandout( dctx->boardWin );
    }

    mvwaddnstr( dctx->boardWin, rect->top, rect->left, loc, 
                strlen(loc) );

    if ( highlight ) {
        wstandend( dctx->boardWin );
    }

    return XP_TRUE;
} /* curses_draw_drawCell */

static void
curses_stringInTile( CursesDrawCtx* dctx, const XP_Rect* rect, 
                     XP_UCHAR* letter, XP_UCHAR* val )
{
    eraseRect( dctx, rect );

    mvwaddnstr( dctx->boardWin, rect->top+1, rect->left+(rect->width/2),
                letter, strlen(letter) );

    if ( !!val ) {
        int len = strlen( val );
        mvwaddnstr( dctx->boardWin, rect->top+rect->height-2, 
                    rect->left + rect->width - len, val, len );
    }
} /* curses_stringInTile */

static void
curses_draw_drawTile( DrawCtx* p_dctx, const XP_Rect* rect, 
                      const XP_UCHAR* textP, XP_Bitmap XP_UNUSED(bitmap),
                      XP_S16 val, XP_Bool highlighted )
{
    char numbuf[5];
    char letterbuf[5];
    char* nump = NULL;
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;

    letterbuf[0] = !!textP? *textP: '_'; /* BLANK or bitmap */
    letterbuf[1] = '\0';
    if ( val >= 0 ) {
        sprintf( numbuf, "%.2d", val );
        if ( numbuf[0] == '0' ) {
            numbuf[0] = ' ';
        }
        nump = numbuf;
    }

    curses_stringInTile( dctx, rect, letterbuf, nump );

    if ( highlighted ) {
        mvwaddnstr( dctx->boardWin, rect->top+rect->height-1, 
                    rect->left, "*-*", 3 );
    }
} /* curses_draw_drawTile */

static  void
curses_draw_drawTileBack( DrawCtx* p_dctx, const XP_Rect* rect )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    curses_stringInTile( dctx, rect, "?", "?" );
} /* curses_draw_drawTileBack */

static void
curses_draw_drawTrayDivider( DrawCtx* p_dctx, const XP_Rect* rect, 
                             XP_Bool XP_UNUSED(selected) )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    wmove( dctx->boardWin, rect->top, rect->left );
    wvline( dctx->boardWin, '#', rect->height );

} /* curses_draw_drawTrayDivider */

static void
curses_draw_drawBoardArrow( DrawCtx* p_dctx, const XP_Rect* rect, 
                            XWBonusType XP_UNUSED(cursorBonus), 
                            XP_Bool vertical, HintAtts XP_UNUSED(hintAtts) )
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
curses_draw_drawCursor( DrawCtx* p_dctx, BoardObjectType XP_UNUSED(typ), 
                        const XP_Rect* rect )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    cursesHiliteRect( dctx->boardWin, rect );
} /* curses_draw_drawBoardCursor */

static void 
curses_draw_clearRect( DrawCtx* p_dctx, const XP_Rect* rectP )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    XP_Rect rect = *rectP;

    eraseRect( dctx, &rect );
} /* curses_draw_clearRect */

static XP_UCHAR*
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

    smallerR.top = rect->top + 1;
    smallerR.left = rect->left + 1;
    smallerR.width = rect->width - 2;
    smallerR.height = rect->height - 2;

    eraseRect( dctx, rect );
    drawRect( dctx->boardWin, &smallerR, '|', '-' );

    mvwprintw( dctx->boardWin, smallerR.top, smallerR.left, text, 
               strlen(text) );
} /* curses_draw_drawMiniWindow */

static void
curses_draw_eraseMiniWindow( DrawCtx* XP_UNUSED(p_dctx), 
                             const XP_Rect* XP_UNUSED(rect),
                             XP_Bool XP_UNUSED(lastTime), 
                             void** XP_UNUSED(closure),
                             XP_Bool* invalUnder )
{
    *invalUnder = XP_TRUE;
} /*  curses_draw_eraseMiniWindow*/

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
    SET_VTABLE_ENTRY( dctx->vtable, draw_boardBegin, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_boardFinished, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_trayBegin, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_trayFinished, curses );

    SET_VTABLE_ENTRY( dctx->vtable, draw_scoreBegin, curses );

    SET_VTABLE_ENTRY( dctx->vtable, draw_measureRemText, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawRemText, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureScoreText, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_score_pendingScore, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_score_drawPlayer, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_scoreFinished, curses );

    SET_VTABLE_ENTRY( dctx->vtable, draw_drawCell, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTile, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTileBack, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTrayDivider, curses );
    
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawBoardArrow, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawCursor, curses );

    SET_VTABLE_ENTRY( dctx->vtable, draw_clearRect, curses );

    SET_VTABLE_ENTRY( dctx->vtable, draw_drawMiniWindow, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_eraseMiniWindow, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_getMiniWText, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureMiniWText, curses );


    /*     SET_VTABLE_ENTRY( dctx, draw_getBonusText, gtk ); */
    /*     SET_VTABLE_ENTRY( dctx, draw_eraseMiniWindow, gtk ); */


    /*     SET_VTABLE_ENTRY( dctx, draw_frameTray, curses ); */

    dctx->boardWin = boardWin;

    return (DrawCtx*)dctx;
} /* curses_drawctxt_init */

#endif /* PLATFORM_NCURSES */
