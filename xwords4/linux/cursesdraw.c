/* -*- compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 1997-2011 by Eric House (xwords@eehouse.org).  All rights
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
                         XP_S16 XP_UNUSED(playerNum),
                         const DictionaryCtxt* XP_UNUSED(dict) )
{
}

static XP_Bool
curses_draw_beginDraw( DrawCtx* XP_UNUSED(p_dctx) )
{
    return XP_TRUE;
}

static XP_Bool
curses_draw_boardBegin( DrawCtx* XP_UNUSED(p_dctx), 
                        const XP_Rect* XP_UNUSED(rect), 
                        XP_U16 XP_UNUSED(width), XP_U16 XP_UNUSED(height),
                        DrawFocusState XP_UNUSED(dfs) )
{
    return XP_TRUE;
} /* curses_draw_boardBegin */

static XP_Bool
curses_draw_trayBegin( DrawCtx* XP_UNUSED(p_dctx), 
                       const XP_Rect* XP_UNUSED(rect), 
                       XP_U16 XP_UNUSED(owner), 
                       XP_S16 XP_UNUSED(score), 
                       DrawFocusState XP_UNUSED(dfs) )
{
    return XP_TRUE;
} /* curses_draw_trayBegin */

static XP_Bool
curses_draw_scoreBegin( DrawCtx* p_dctx, const XP_Rect* rect, 
                        XP_U16 XP_UNUSED(numPlayers), 
                        const XP_S16* const XP_UNUSED(scores), 
                        XP_S16 XP_UNUSED(remCount), 
                        DrawFocusState XP_UNUSED(dfs) )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    eraseRect( dctx, rect );
    return XP_TRUE;
} /* curses_draw_scoreBegin */

#ifdef XWFEATURE_SCOREONEPASS
static XP_Bool
curses_draw_drawRemText( DrawCtx* p_dctx, XP_S16 nTilesLeft,
                         XP_Bool focussed, XP_Rect* rect )
{
    XP_USE(p_dctx);
    XP_USE(nTilesLeft);
    XP_USE(focussed);
    XP_USE(rect);
    return XP_TRUE;
}
#else
static void
formatRemText( XP_S16 nTilesLeft, const XP_Rect* rect, char* buf, char** lines )
{
    if ( 1 == rect->height ) {
        *lines = buf;
        sprintf( buf, "Rem: %.3d", nTilesLeft );
    } else {
        sprintf( buf, "Rem:" );
        *lines++ = buf;
        buf += 1 + strlen(buf);
        sprintf( buf, "%.3d", nTilesLeft );
        *lines++ = buf;
    }
} /* formatRemText */

static XP_Bool
curses_draw_measureRemText( DrawCtx* XP_UNUSED(dctx), 
                            const XP_Rect* rect, 
                            XP_S16 nTilesLeft, 
                            XP_U16* width, XP_U16* height )
{
    char buf[64];
    char* lines[2] = {0};
    formatRemText( nTilesLeft, rect, buf, lines );
    
    *width = 0;
    int ii;
    for ( ii = 0; ii < VSIZE(lines) && !!lines[ii]; ++ii ) {
        *width = XP_MAX( *width, strlen(lines[ii]) );
    }
    *height = ii;

    return XP_TRUE;
} /* curses_draw_measureRemText */

static void
curses_draw_drawRemText( DrawCtx* p_dctx, const XP_Rect* rInner, 
                         const XP_Rect* rOuter, XP_S16 nTilesLeft,
                         XP_Bool focussed )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    char buf[32];

    char* lines[2] = {0};
    formatRemText( nTilesLeft, rInner, buf, lines );
    int ii;
    for ( ii = 0; ii < VSIZE(lines) && !!lines[ii]; ++ii ) {
        mvwprintw( dctx->boardWin, rInner->top + ii, rInner->left, lines[ii] );
    }    
    if ( focussed ) {
        cursesHiliteRect( dctx->boardWin, rOuter );
    }
} /* curses_draw_drawRemText */
#endif

#ifdef XWFEATURE_SCOREONEPASS
#else
#if 0
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
#endif

static void
formatScoreText( XP_UCHAR* out, const DrawScoreInfo* dsi, const XP_Rect* rect,
                 char** lines )
{
    if ( 2 <= rect->height ) {
        sprintf( out, "%s", dsi->name );
        *lines++ = out;
        out += 1 + strlen(out);
    }

    /* Status/role chars at start/top, if there's room */
    if ( 3 <= rect->height ) {
        out[0] = dsi->isTurn ? 'T': ' ';
        out[1] = dsi->selected ? 'S' : ' ';
        out[2] = dsi->isRobot ? 'r' : ' ';
        out[3] = '\0';
        *lines++ = out;
        out += 4;
    }

    sprintf( out, "%.3d", dsi->totalScore );
    *lines++ = out;
    out += 1 + strlen(out);

#if 0
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
#endif
} /* formatScoreText */

static void
curses_draw_measureScoreText( DrawCtx* XP_UNUSED(p_dctx), 
                              const XP_Rect* rect, 
                              const DrawScoreInfo* dsi,
                              XP_U16* width, XP_U16* height )
{
    XP_UCHAR buf[100];
    char* lines[3] = {0};
    formatScoreText( buf, dsi, rect, lines );

    int ii;
    int max = 0;
    for ( ii = 0; ii < VSIZE(lines) && !!lines[ii]; ++ii ) {
        max = XP_MAX( max, strlen( lines[ii] ) );
    }
    XP_ASSERT( ii <= rect->height );
    *height = ii;
    *width = max;
    XP_ASSERT( *width <= rect->width );
} /* curses_draw_measureScoreText */

static void
curses_draw_score_drawPlayer( DrawCtx* p_dctx, const XP_Rect* rInner, 
                              const XP_Rect* rOuter, 
                              XP_U16 XP_UNUSED(gotPct), const DrawScoreInfo* dsi )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    char buf[100];

    curses_draw_clearRect( p_dctx, rOuter );

    /* print the name and turn/remoteness indicator */
    char* lines[3] = {0};
    formatScoreText( buf, dsi, rInner, lines );
    int ii;
    for ( ii = 0; ii < VSIZE(lines) && !!lines[ii]; ++ii ) {
        char* line = lines[ii];
        int left = rOuter->left + ((rOuter->width - strlen(line)) / 2);
        mvwprintw( dctx->boardWin, rInner->top + ii, left, line );
    }

    if ( (dsi->flags&CELL_ISCURSOR) != 0 ) {
        cursesHiliteRect( dctx->boardWin, rOuter );
    }
} /* curses_draw_score_drawPlayer */
#endif

static void
curses_draw_score_pendingScore( DrawCtx* p_dctx, const XP_Rect* rect, 
                                XP_S16 score, XP_U16 XP_UNUSED(playerNum),
                                XP_Bool XP_UNUSED(curTurn),
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

static XP_Bool
curses_draw_drawCell( DrawCtx* p_dctx, const XP_Rect* rect, 
                      const XP_UCHAR* letter, 
                      const XP_Bitmaps* XP_UNUSED(bitmaps),
                      Tile XP_UNUSED(tile), XP_U16 XP_UNUSED(value), 
                      XP_S16 XP_UNUSED(owner), XWBonusType bonus, 
                      HintAtts XP_UNUSED(hintAtts), CellFlags flags )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    XP_Bool highlight = (flags & (CELL_PENDING|CELL_RECENT|CELL_ISCURSOR)) != 0;
    XP_UCHAR loc[rect->width+1];
    if ( !!letter ) {
        XP_MEMCPY( loc, letter, 1 + strlen(letter) );
    } else {
        XP_MEMSET( loc, '\0', sizeof(loc) );
    }

    if ( highlight ) {
        wstandout( dctx->boardWin );
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

    mvwaddnstr( dctx->boardWin, rect->top, rect->left, loc, -1 );

    if ( highlight ) {
        wstandend( dctx->boardWin );
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
                     const XP_UCHAR* letter, const XP_UCHAR* val )
{
    eraseRect( dctx, rect );

    int toptop, topbot;
    getTops( rect, &toptop, &topbot );

    if ( !!letter ) {
        mvwaddnstr( dctx->boardWin, toptop, rect->left+(rect->width/2),
                    letter, -1 );
    }

    if ( !!val ) {
        int len = strlen( val );
        mvwaddnstr( dctx->boardWin, topbot, rect->left + rect->width - len, 
                    val, len );
    }
} /* curses_stringInTile */

static XP_Bool
curses_draw_drawTile( DrawCtx* p_dctx, const XP_Rect* rect, 
                      const XP_UCHAR* textP, const XP_Bitmaps* XP_UNUSED(bitmaps),
                      XP_U16 val, CellFlags flags )
{
    char numbuf[5];
    XP_UCHAR letterbuf[5];
    char* nump = NULL;
    XP_UCHAR* letterp = NULL;
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    XP_Bool highlight = (flags&(CELL_RECENT|CELL_PENDING|CELL_ISCURSOR)) != 0;

    if ( highlight ) {
        wstandout( dctx->boardWin );
    }

    if ( (flags&CELL_ISEMPTY) == 0 ) {
        if ( !!textP ) {
            snprintf( letterbuf, sizeof(letterbuf), "%s", textP );
        } else {
            letterbuf[0] = '_'; /* BLANK or bitmap */
            letterbuf[1] = '\0';
        }
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

    if ( (flags & (CELL_RECENT|CELL_PENDING)) != 0 ) {
        mvwaddnstr( dctx->boardWin, rect->top+rect->height-1, 
                    rect->left, "*-*", 3 );
    }

    if ( highlight ) {
        wstandend( dctx->boardWin );
    }
    return XP_TRUE;
} /* curses_draw_drawTile */

static XP_Bool
curses_draw_drawTileBack( DrawCtx* p_dctx, const XP_Rect* rect, 
                          CellFlags flags )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    curses_stringInTile( dctx, rect, "?", "?" );
    if ( (flags&CELL_ISCURSOR) != 0 ) {
        cursesHiliteRect( dctx->boardWin, rect );
    }
    return XP_TRUE;
} /* curses_draw_drawTileBack */

static void
curses_draw_drawTrayDivider( DrawCtx* p_dctx, const XP_Rect* rect, 
                             CellFlags flags )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    eraseRect( dctx, rect );

    wmove( dctx->boardWin, rect->top, rect->left + (rect->width/2));
    wvline( dctx->boardWin, '#', rect->height );
    if ( 0 != (flags & CELL_ISCURSOR) ) {
        cursesHiliteRect( dctx->boardWin, rect );
    }
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

#ifdef XWFEATURE_MINIWIN
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
#endif

#if 0
static void
curses_draw_frameTray( DrawCtx* p_dctx, XP_Rect* rect )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)p_dctx;
    box( dctx->boardWin, '*', '+');
} /* curses_draw_frameTray */
#endif

static XP_Bool
draw_doNothing( DrawCtx* XP_UNUSED(dctx), ... )
{
    LOG_FUNC();
    return XP_FALSE;
} /* draw_doNothing */

DrawCtx* 
cursesDrawCtxtMake( WINDOW* boardWin )
{
    CursesDrawCtx* dctx = malloc( sizeof(CursesDrawCtx) );

    dctx->vtable = malloc( sizeof(*(((CursesDrawCtx*)dctx)->vtable)) );

    for ( int ii = 0;
          ii < sizeof(*dctx->vtable)/sizeof(dctx->vtable->m_draw_destroyCtxt);
          ++ii ) {
        ((void**)(dctx->vtable))[ii] = draw_doNothing;
    }

    SET_VTABLE_ENTRY( dctx->vtable, draw_destroyCtxt, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_dictChanged, curses );

    SET_VTABLE_ENTRY( dctx->vtable, draw_beginDraw, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_boardBegin, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_trayBegin, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_scoreBegin, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_objFinished, curses );

#ifdef XWFEATURE_SCOREONEPASS
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawRemText, curses );
#else
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureRemText, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawRemText, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureScoreText, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_score_drawPlayer, curses );
#endif
    SET_VTABLE_ENTRY( dctx->vtable, draw_score_pendingScore, curses );

    SET_VTABLE_ENTRY( dctx->vtable, draw_drawCell, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTile, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTileBack, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTrayDivider, curses );
    
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawBoardArrow, curses );

    SET_VTABLE_ENTRY( dctx->vtable, draw_clearRect, curses );

#ifdef XWFEATURE_MINIWIN
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawMiniWindow, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_getMiniWText, curses );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureMiniWText, curses );
#endif

    dctx->boardWin = boardWin;

    return (DrawCtx*)dctx;
} /* curses_drawctxt_init */

void
cursesDrawCtxtFree( DrawCtx* pdctx )
{
    CursesDrawCtx* dctx = (CursesDrawCtx*)pdctx;
    free( dctx->vtable );
    free( dctx );
}

#endif /* PLATFORM_NCURSES */
