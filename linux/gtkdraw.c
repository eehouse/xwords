/* -*-mode: C; fill-column: 78; c-basic-offset: 4;-*- */ 
/* 
 * Copyright 1997-2002 by Eric House (fixin@peak.org).  All rights reserved.
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
#ifdef PLATFORM_GTK

#include <stdlib.h>
#include <stdio.h>


#include "gtkmain.h"
#include "draw.h"
#include "board.h"
#include "linuxmain.h"

/* static GdkGC* newGCForColor( GdkWindow* window, XP_Color* newC ); */
static void
insetRect( XP_Rect* r, short i )
{
    r->top += i;
    r->left += i;
    i *= 2;

    r->width -= i;
    r->height -= i;
} /* insetRect */

//#define DRAW_WHAT(dc) ((dc)->pixmap)
#define DRAW_WHAT(dc) ((dc)->widget->window)

static void
eraseRect(GtkDrawCtx* dctx, XP_Rect* rect )
{
    gdk_draw_rectangle( DRAW_WHAT(dctx),
			dctx->widget->style->white_gc,
			TRUE, rect->left, rect->top, 
			rect->width, rect->height );
} /* eraseRect */

static void
frameRect( GtkDrawCtx* dctx, XP_Rect* rect )
{
    gdk_draw_rectangle( DRAW_WHAT(dctx),
                        dctx->drawGC, FALSE, rect->left, rect->top, 
                        rect->width, rect->height );
} /* frameRect */

#ifdef DRAW_WITH_PRIMITIVES

static void
gtk_prim_draw_setClip( DrawCtx* p_dctx, XP_Rect* newClip, XP_Rect* oldClip)
{
} /* gtk_prim_draw_setClip */

static void 
gtk_prim_draw_frameRect( DrawCtx* p_dctx, XP_Rect* rect )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    frameRect( dctx, rect );
} /* gtk_prim_draw_frameRect */

static void
gtk_prim_draw_invertRect( DrawCtx* p_dctx, XP_Rect* rect )
{
    /* not sure you can do this on GTK!! */
} /* gtk_prim_draw_invertRect */

static void
gtk_prim_draw_clearRect( DrawCtx* p_dctx, XP_Rect* rect )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    eraseRect( dctx, rect );
} /* gtk_prim_draw_clearRect */

static void
gtk_prim_draw_drawString( DrawCtx* p_dctx, XP_UCHAR* str,
                          XP_U16 x, XP_U16 y )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_U16 fontHeight = 10;     /* FIX ME */
    gdk_draw_string( DRAW_WHAT(dctx), dctx->gdkFont, dctx->drawGC,
                     x, y + fontHeight, str );
} /* gtk_prim_draw_drawString */

static void
gtk_prim_draw_drawBitmap( DrawCtx* p_dctx, XP_Bitmap bm, 
                          XP_U16 x, XP_U16 y )
{
} /* gtk_prim_draw_drawBitmap */

static void
gtk_prim_draw_measureText( DrawCtx* p_dctx, XP_UCHAR* str, 
                           XP_U16* widthP, XP_U16* heightP)
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    gint len = strlen(str);
    gint width = gdk_text_measure( dctx->gdkFont, str, len );

    *widthP = width;
    *heightP = 12;              /* ??? :-) */
} /* gtk_prim_draw_measureText */

#endif /* DRAW_WITH_PRIMITIVES */

static void
drawBitmapFromLBS( GtkDrawCtx* dctx, XP_Bitmap bm, XP_Rect* rect )
{
    GdkPixmap* pm;
    LinuxBMStruct* lbs = (LinuxBMStruct*)bm;
    gint x, y;
    XP_U8* bp;
    XP_U16 i;
    XP_S16 nBytes;
    XP_U16 nCols, nRows;
    
    nCols = lbs->nCols;
    nRows = lbs->nRows;
    bp = (XP_U8*)(lbs + 1);    /* point to the bitmap data */
    nBytes = lbs->nBytes;

    pm = gdk_pixmap_new( DRAW_WHAT(dctx), nCols, nRows, -1 );

    gdk_draw_rectangle( pm, dctx->widget->style->white_gc, TRUE,
                        0, 0, nCols, nRows );

    x = 0;
    y = 0;

    while ( nBytes-- ) {
        XP_U8 byte = *bp++;
        for ( i = 0; i < 8; ++i ) {
            XP_Bool draw = ((byte & 0x80) != 0);
            if ( draw ) {
                gdk_draw_point( pm, dctx->widget->style->black_gc, x, y );
            }
            byte <<= 1;
            if ( ++x == nCols ) {
                x = 0;
                if ( ++y == nRows ) {
                    break;
                }
            }
        }
    }

    XP_ASSERT( nBytes == -1 );   /* else we're out of sync */

    gdk_draw_pixmap( DRAW_WHAT(dctx),
                     dctx->drawGC,
                     (GdkDrawable*)pm, 0, 0,
                     rect->left+2,
                     rect->top+2,
                     lbs->nCols,
                     lbs->nRows );

    gdk_pixmap_unref( pm );
} /* drawBitmapFromLBS */

#if 0
static void
gtk_draw_destroyCtxt( DrawCtx* p_dctx )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    GtkAllocation* alloc = &dctx->widget->allocation;

    gdk_draw_rectangle( DRAW_WHAT(dctx),
			dctx->widget->style->white_gc,
			TRUE,
			0, 0, alloc->width, alloc->height );

} /* draw_setup */
#endif

static XP_Bool
gtk_draw_boardBegin( DrawCtx* p_dctx, XP_Rect* rect, XP_Bool hasfocus )
{
    GdkRectangle gdkrect;
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;

    gdk_gc_set_foreground( dctx->drawGC, &dctx->black );

    gdkrect = *(GdkRectangle*)rect;
    ++gdkrect.width;
    ++gdkrect.height;
    gdk_gc_set_clip_rectangle( dctx->drawGC, &gdkrect );

    return XP_TRUE;
} /* draw_finish */

static void
gtk_draw_boardFinished( DrawCtx* p_dctx )
{
    //    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
} /* draw_finished */

static XP_Bool
gtk_draw_drawCell( DrawCtx* p_dctx, XP_Rect* rect, XP_UCHAR* letter, 
                   XP_Bitmap bitmap, XP_S16 owner, XWBonusType bonus, 
                   XP_Bool isBlank, XP_Bool highlight, XP_Bool isStar )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Rect rectInset = *rect;
    XP_Bool showGrid = dctx->globals->gridOn;

    eraseRect( dctx, rect );

    insetRect( &rectInset, 1 );

    gdk_gc_set_foreground( dctx->drawGC, &dctx->black );

    if ( showGrid ) {
        gdk_draw_rectangle( DRAW_WHAT(dctx),
                            dctx->drawGC,
                            FALSE,
                            rect->left, rect->top, rect->width, 
                            rect->height );
    }

    /* draw the bonus colors only if we're not putting a "tile" there */
    if ( !!letter ) {
        if ( *letter == LETTER_NONE && bonus != BONUS_NONE ) {
            XP_ASSERT( bonus <= 4 );
            gdk_gc_set_foreground( dctx->drawGC, &dctx->bonusColors[bonus-1] );
            gdk_draw_rectangle( DRAW_WHAT(dctx),
                                dctx->drawGC,
                                TRUE,
                                rectInset.left, rectInset.top,
                                rectInset.width+1, rectInset.height+1 );

        } else {
            gint len = strlen(letter);
            gint width = gdk_text_measure( dctx->gdkFont, letter, len );
            gint x = rect->left;
            x += (rect->width - width) / 2;

            if ( highlight ) {
                gdk_gc_set_foreground( dctx->drawGC, &dctx->red );
            } else {
                gdk_gc_set_foreground( dctx->drawGC, 
                                       owner >= 0? &dctx->playerColors[owner]:
                                       &dctx->black );
            }
            gdk_draw_text( DRAW_WHAT(dctx), dctx->gdkFont, dctx->drawGC,
                           x, rect->top+rect->height-1, letter, len );

            if ( isBlank ) {
                gdk_draw_arc( DRAW_WHAT(dctx), dctx->drawGC,
                              0,	/* filled */
                              rect->left, /* x */
                              rect->top, /* y */
                              rect->width,/*width, */
                              rect->height,/*width, */
                              0, 360*64 );
            }
        } 
    } else if ( !!bitmap ) {
        drawBitmapFromLBS( dctx, bitmap, rect );
    }

    if ( isStar ) {
        letter = "*";
        gdk_gc_set_foreground( dctx->drawGC, &dctx->black );
        gdk_draw_text( DRAW_WHAT(dctx), dctx->gdkFont, dctx->drawGC,
                       rect->left+3, rect->top+rect->height-1,
                       letter, 1 );
    }

    return XP_TRUE;
} /* gtk_draw_drawCell */

static void
gtk_draw_invertCell( DrawCtx* p_dctx, XP_Rect* rect )
{
/*     GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx; */
/*     (void)gtk_draw_drawMiniWindow( p_dctx, "f", rect); */

/*     GdkGCValues values; */

/*     gdk_gc_get_values( dctx->drawGC, &values ); */

/*     gdk_gc_set_function( dctx->drawGC, GDK_INVERT ); */

/*     gdk_gc_set_clip_rectangle( dctx->drawGC, (GdkRectangle*)rect ); */
/*     gdk_draw_rectangle( DRAW_WHAT(dctx), dctx->drawGC, */
/* 			TRUE, rect->left, rect->top,  */
/* 			rect->width, rect->height ); */

/*     gdk_gc_set_function( dctx->drawGC, values.function ); */
} /* gtk_draw_invertCell */

static void
gtk_draw_trayBegin( DrawCtx* p_dctx, XP_Rect* rect, XP_U16 owner, 
                    XP_Bool hasfocus )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Rect clip = *rect;
    insetRect( &clip, -1 );
    gdk_gc_set_clip_rectangle( dctx->drawGC, (GdkRectangle*)&clip );
/*     eraseRect( dctx, rect ); */
} /* gtk_draw_trayBegin */

static void
gtk_draw_drawTile( DrawCtx* p_dctx, XP_Rect* rect, XP_UCHAR* textP,
                   XP_Bitmap bitmap, XP_S16 val, XP_Bool highlighted )
{
    unsigned char numbuf[3];
    gint len; 
    gint width;
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Rect insetR = *rect;

    gdk_gc_set_foreground( dctx->drawGC, &dctx->black );

    /*     insetRect( &insetR, 1 ); */	/* to draw inside the thing */
    /*     ++insetR.left; */
    /*     --insetR.width; */
    eraseRect( dctx, &insetR );

    if ( val < 0 ) {
    } else {
        insetRect( &insetR, 1 );

        if ( !!textP ) {
            if ( *textP != LETTER_NONE ) { /* blank */
                gdk_draw_text( DRAW_WHAT(dctx), dctx->gdkTrayFont, dctx->drawGC,
                               insetR.left + 2, 
                               insetR.top + dctx->trayFontHeight,
                               textP, 1 );
            }
        } else if ( !!bitmap ) {
            drawBitmapFromLBS( dctx, bitmap, &insetR );
        }
    
        sprintf( numbuf, "%d", val );
        len = strlen( numbuf );
        width = gdk_text_measure( dctx->gdkFont, numbuf, len );

        gdk_draw_text( DRAW_WHAT(dctx), dctx->gdkFont, dctx->drawGC,
                       insetR.left+insetR.width - width - 1, 
                       insetR.top + insetR.height - 2,
                       numbuf, len );
    
        /* frame the tile */
        gdk_draw_rectangle( DRAW_WHAT(dctx),
                            dctx->drawGC,
                            FALSE,
                            insetR.left, insetR.top, insetR.width, 
                            insetR.height );

        if ( highlighted ) {
            insetRect( &insetR, 1 );
            gdk_draw_rectangle( DRAW_WHAT(dctx),
                                dctx->drawGC,
                                FALSE, insetR.left, insetR.top, 
                                insetR.width, insetR.height);
        }
    }
} /* gtk_draw_drawTile */

static void
gtk_draw_drawTileBack( DrawCtx* p_dctx, XP_Rect* rect )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Rect r = *rect;

    insetRect( &r, 1 );

    gdk_gc_set_foreground( dctx->drawGC, &dctx->black );
    gdk_draw_rectangle( DRAW_WHAT(dctx),
			dctx->drawGC, TRUE, 
			r.left, r.top, r.width, r.height );
} /* gtk_draw_drawTileBack */

static void
gtk_draw_drawTrayDivider( DrawCtx* p_dctx, XP_Rect* rect, XP_Bool selected )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Rect r = *rect;

    eraseRect( dctx, &r );

    ++r.left;
    r.width -= selected? 2:1;
    if ( selected ) {
	--r.height;
    }

    gdk_gc_set_foreground( dctx->drawGC, &dctx->black );
    gdk_draw_rectangle( DRAW_WHAT(dctx),
			dctx->drawGC,
			!selected, 
			r.left, r.top, r.width, r.height);
    
} /* gtk_draw_drawTrayDivider */

#if 0
static void 
gtk_draw_frameBoard( DrawCtx* p_dctx, XP_Rect* rect )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;

    gdk_gc_set_foreground( dctx->drawGC, &dctx->black );
    gdk_draw_rectangle( DRAW_WHAT(dctx),
			dctx->drawGC, FALSE, 
			rect->left, rect->top, rect->width, rect->height );

} /* gtk_draw_frameBoard */

static void 
gtk_draw_frameTray( DrawCtx* p_dctx, XP_Rect* rect )
{
    //    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
} /* gtk_draw_frameBoard */
#endif

static void 
gtk_draw_clearRect( DrawCtx* p_dctx, XP_Rect* rectP )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Rect rect = *rectP;

    ++rect.width;
    ++rect.top;

    eraseRect( dctx, &rect );

} /* gtk_draw_clearRect */

static void
gtk_draw_drawBoardArrow( DrawCtx* p_dctx, XP_Rect* rectP, 
                         XWBonusType cursorBonus, XP_Bool vertical )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
/*     XP_Rect rect = *rectP; */
    char curs = vertical? '|':'-';
    
    gdk_gc_set_foreground( dctx->drawGC, &dctx->black );
    gdk_draw_text( DRAW_WHAT(dctx), dctx->gdkFont, dctx->drawGC,
		   rectP->left+3, rectP->top+rectP->height-1,
		   &curs, 1 );

} /* gtk_draw_drawBoardCursor */

static void
gtk_draw_scoreBegin( DrawCtx* p_dctx, XP_Rect* rect, XP_U16 numPlayers, 
		     XP_Bool hasfocus )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;

    gdk_gc_set_clip_rectangle( dctx->drawGC, (GdkRectangle*)rect );
    eraseRect( dctx, rect );
} /* gtk_draw_scoreBegin */

static void
gtkDrawDrawRemText( DrawCtx* p_dctx, XP_Rect* r, XP_U16 nTilesLeft,
		    XP_U16* widthP, XP_U16* heightP )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    GtkAppGlobals* globals = dctx->globals;
    XP_Bool isVertical = globals->cGlobals.params->verticalScore;
    char buf[10];
    char* bufp = buf;
    XP_U16 height, width;
    XP_U16 left = r->left;
    XP_U16 top = r->top;
    XP_Bool draw = !widthP;
    
    gdk_gc_set_foreground( dctx->drawGC, &dctx->black );
    sprintf( buf, "rem:%d", nTilesLeft );
    width = 2 + gdk_text_measure( dctx->gdkFont, buf, strlen(buf) );

    if ( isVertical ) {
	height = 12;
	left += 2;
	if ( width > r->width ) {
	    if ( draw ) {
		gdk_draw_string( DRAW_WHAT(dctx), dctx->gdkFont, dctx->drawGC,
				 left + 2, top + height, "rem:" );
	    }
	    bufp += 4;
	    top += 12;
	}
    } else {
	height = r->height;
    }

    if ( draw ) {
	gdk_draw_string( DRAW_WHAT(dctx), dctx->gdkFont, dctx->drawGC,
			 left, top + height, bufp );
    } else {
	*widthP = width;
	*heightP = height;
    }
} /* gtkDrawDrawRemText */

static void
gtk_draw_measureRemText( DrawCtx* p_dctx, XP_Rect* r, XP_S16 nTilesLeft,
                         XP_U16* width, XP_U16* height )
{
    gtkDrawDrawRemText( p_dctx, r, nTilesLeft, width, height );
} /* gtk_draw_measureRemText */

static void
gtk_draw_drawRemText( DrawCtx* p_dctx, XP_Rect* rInner, XP_Rect* rOuter,
                      XP_S16 nTilesLeft )
{
    gtkDrawDrawRemText( p_dctx, rInner, nTilesLeft, NULL, NULL );
} /* gtk_draw_drawRemText */

static void
widthAndText( char* buf, GdkFont* font, DrawScoreInfo* dsi,
	      XP_U16* widthP, XP_U16* heightP )
{
    XP_S16 score = dsi->score;
    XP_U16 nTilesLeft = dsi->nTilesLeft;
    XP_Bool isTurn = dsi->isTurn;
    char* borders = "";
    if ( isTurn ) {
	borders = "*";
    }

    sprintf( buf, "%s%.3d", borders, score );
    if ( nTilesLeft < MAX_TRAY_TILES ) {
	char nbuf[10];
	sprintf( nbuf, ":%d", nTilesLeft );
	(void)strcat( buf, nbuf );
    }
    (void)strcat( buf, borders );

    if ( !!widthP ) {
	*widthP = gdk_string_measure( font, buf );
	*heightP = HOR_SCORE_HEIGHT;		/* a wild-ass guess */
    }
} /* widthAndText */

static void
gtk_draw_measureScoreText( DrawCtx* p_dctx, XP_Rect* r, 
			   DrawScoreInfo* dsi,
			   XP_U16* width, XP_U16* height )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    char buf[20];
    GdkFont* font = /* dsi->selected? dctx->gdkBoldFont :  */dctx->gdkFont;

    widthAndText( buf, font, dsi, width, height );
} /* gtk_draw_measureScoreText */

static void
gtk_draw_score_drawPlayer( DrawCtx* p_dctx, XP_S16 playerNum,
			   XP_Rect* rInner, XP_Rect* rOuter, 
			   DrawScoreInfo* dsi )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    char scoreBuf[20];
    XP_U16 x;
    GdkFont* font = /* dsi->selected? dctx->gdkBoldFont :  */dctx->gdkFont;


    widthAndText( scoreBuf, font, dsi, NULL, NULL );
    x = rInner->left;// + ((rect->width - width) /2);

    gdk_gc_set_foreground( dctx->drawGC, 
			   playerNum >= 0? &dctx->playerColors[playerNum]:
			   &dctx->black
			   /*selected? &dctx->red:&dctx->black*/ );

    if ( dsi->selected ) {
	gdk_draw_rectangle( DRAW_WHAT(dctx), dctx->drawGC,
			    TRUE, rOuter->left, rOuter->top, 
			    rOuter->width, rOuter->height );
	eraseRect( dctx, rInner );
    }

    gdk_draw_string( DRAW_WHAT(dctx), font, dctx->drawGC,
		     x, rInner->top + rInner->height, scoreBuf );
} /* gtk_draw_score_drawPlayer */

static void
gtk_draw_score_pendingScore( DrawCtx* p_dctx, XP_Rect* rect, XP_S16 score,
			     XP_U16 playerNum )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    char buf[5];
    XP_U16 left;
    XP_Rect localR;

    if ( score >= 0 ) {
	sprintf( buf, "%.3d", score );
    } else {
	strcpy( buf, "???" );
    }

    gdk_gc_set_foreground( dctx->drawGC, &dctx->black );
    gdk_gc_set_clip_rectangle( dctx->drawGC, (GdkRectangle*)rect );

    localR = *rect;
    rect = &localR;
    insetRect( rect, 1 );
    eraseRect( dctx, rect );

    left = rect->left + 1;
    gdk_draw_string( DRAW_WHAT(dctx), dctx->gdkFont, dctx->drawGC,
		     left, rect->top + (rect->height/2), "Pts:" );
    gdk_draw_string( DRAW_WHAT(dctx), dctx->gdkFont, dctx->drawGC,
		     left, rect->top + rect->height, buf );
} /* gtk_draw_score_pendingScore */

static void
gtk_draw_scoreFinished( DrawCtx* p_dctx )
{
/*     GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx; */

} /* gtk_draw_scoreFinished */

static void
gtkFormatTimerText( XP_UCHAR* buf, XP_S16 secondsLeft )
{
    XP_U16 minutes, seconds;

    if ( secondsLeft < 0 ) {
	*buf++ = '-';
	secondsLeft *= -1;
    }

    minutes = secondsLeft / 60;
    seconds = secondsLeft % 60;
    sprintf( buf, "% 1d:%02d", minutes, seconds );
} /* gtkFormatTimerText */

static void
gtk_draw_drawTimer( DrawCtx* p_dctx, XP_Rect* rInner, XP_Rect* rOuter,
		    XP_U16 player, XP_S16 secondsLeft )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_UCHAR buf[10];

    gtkFormatTimerText( buf, secondsLeft );

    gdk_gc_set_clip_rectangle( dctx->drawGC, (GdkRectangle*)rInner );
    eraseRect( dctx, rInner );
    gdk_draw_string( DRAW_WHAT(dctx), dctx->gdkFont, dctx->drawGC,
		     rInner->left, rInner->top + rInner->height, buf );
} /* gtk_draw_drawTimer */

#define MINI_LINE_HT 12
#define MINI_V_PADDING 6
#define MINI_H_PADDING 8

static unsigned char*
gtk_draw_getMiniWText( DrawCtx* p_dctx, XWMiniTextType textHint )
{
/*     GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx; */
    unsigned char* str;

    switch( textHint ) {
    case BONUS_DOUBLE_LETTER:
        str = "Double letter"; break;
    case BONUS_DOUBLE_WORD:
        str = "Double word"; break;
    case BONUS_TRIPLE_LETTER:
        str = "Triple letter"; break;
    case BONUS_TRIPLE_WORD:
        str = "Triple word"; break;
    case INTRADE_MW_TEXT:	
        str = "Trading tiles;\nclick D when done"; break;
    default:
        XP_ASSERT( XP_FALSE );
    }
    return str;
} /* gtk_draw_getMiniWText */

static void
gtk_draw_measureMiniWText( DrawCtx* p_dctx, unsigned char* str, 
			   XP_U16* widthP, XP_U16* heightP )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_U16 height, maxWidth;

    for ( height = MINI_V_PADDING, maxWidth = 0; ; ) {
        unsigned char* nextStr = strstr( str, "\n" );
        XP_U16 len = nextStr==NULL? strlen(str): nextStr - str;

        XP_U16 width = gdk_text_measure( dctx->gdkFont, str, len );
        maxWidth = XP_MAX( maxWidth, width );
        height += MINI_LINE_HT;

        if ( nextStr == NULL ) {
            break;
        }
        str = nextStr+1;	/* skip '\n' */
    }

    *widthP = maxWidth + MINI_H_PADDING;
    *heightP = height;
} /* gtk_draw_measureMiniWText */

static void
gtk_draw_drawMiniWindow( DrawCtx* p_dctx, unsigned char* text, XP_Rect* rect,
			 void** closureP )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Rect localR = *rect;

    gdk_gc_set_foreground( dctx->drawGC, &dctx->black );
    gdk_gc_set_clip_rectangle( dctx->drawGC, (GdkRectangle*)&localR );

    /* play some skanky games to get the shadow drawn under and to the
       right... */
    eraseRect( dctx, &localR );

    insetRect( &localR, 1 );
    --localR.width;
    --localR.height;
    frameRect( dctx, &localR );

    --localR.top;
    --localR.left;
    eraseRect( dctx, &localR );
    frameRect( dctx, &localR );

    for ( ; ; ) { /* draw up to the '\n' each time */
        unsigned char* nextStr = strstr( text, "\n" );
        XP_U16 len, width, left;
        if ( nextStr == NULL ) {
            len = strlen(text);
        } else {
            len = nextStr - text;
        }

        localR.top += MINI_LINE_HT;
        width = gdk_text_measure( dctx->gdkFont, text, len );
        left = localR.left + ((localR.width - width) / 2);
        gdk_draw_text( DRAW_WHAT(dctx), dctx->gdkFont, dctx->drawGC,
                       left, localR.top, text, len );

        if ( nextStr == NULL ) {
            break;
        }
        text = nextStr+1;	/* skip the CR */
    }
} /* gtk_draw_drawMiniWindow */

static void
gtk_draw_eraseMiniWindow( DrawCtx* p_dctx, XP_Rect* rect, XP_Bool lastTime,
			  void** closure, XP_Bool* invalUnder )
{
/*     GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx; */
    *invalUnder = XP_TRUE;
} /* gtk_draw_eraseMiniWindow */

#define SET_GDK_COLOR( c, r, g, b ) { \
     c.red = (r); \
     c.green = (g); \
     c.blue = (b); \
}
static void
draw_doNothing( DrawCtx* dctx, ... )
{
} /* draw_doNothing */

static void
allocAndSet( GdkColormap* map, GdkColor* color, unsigned short red,
	     unsigned short green, unsigned short blue )

{
    gboolean success;

    color->red = red;
    color->green = green;
    color->blue = blue;

    success = gdk_color_alloc( map, color );
    if ( !success ) {
	XP_WARNF( "unable to alloc color" );
    }
} /* allocAndSet */

DrawCtx* 
gtkDrawCtxtMake( GtkWidget *widget, GtkAppGlobals* globals )
{
    GtkDrawCtx* dctx = g_malloc( sizeof(GtkDrawCtx) );
    GdkFont* font;
    short i;

    dctx->vtable = g_malloc( sizeof(*(((GtkDrawCtx*)dctx)->vtable)) );

    for ( i = 0; i < sizeof(*dctx->vtable)/4; ++i ) {
        ((void**)(dctx->vtable))[i] = draw_doNothing;
    }

    SET_VTABLE_ENTRY( dctx->vtable, draw_clearRect, gtk );

#ifdef DRAW_WITH_PRIMITIVES
    SET_VTABLE_ENTRY( dctx->vtable, draw_setClip, gtk_prim );
    SET_VTABLE_ENTRY( dctx->vtable, draw_frameRect, gtk_prim );
    SET_VTABLE_ENTRY( dctx->vtable, draw_invertRect, gtk_prim );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawString, gtk_prim );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawBitmap, gtk_prim );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureText, gtk_prim );

    InitDrawDefaults( dctx->vtable );
#else

    SET_VTABLE_ENTRY( dctx->vtable, draw_boardBegin, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawCell, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_invertCell, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_boardFinished, gtk );

    SET_VTABLE_ENTRY( dctx->vtable, draw_trayBegin, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTile, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTileBack, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTrayDivider, gtk );

    SET_VTABLE_ENTRY( dctx->vtable, draw_drawBoardArrow, gtk );

    SET_VTABLE_ENTRY( dctx->vtable, draw_scoreBegin, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureRemText, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawRemText, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureScoreText, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_score_drawPlayer, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_score_pendingScore, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_scoreFinished, gtk );

    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTimer, gtk );

    SET_VTABLE_ENTRY( dctx->vtable, draw_getMiniWText, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureMiniWText, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawMiniWindow, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_eraseMiniWindow, gtk );

#endif

/*     SET_VTABLE_ENTRY( dctx, draw_frameBoard, gtk_ ); */
/*     SET_VTABLE_ENTRY( dctx, draw_frameTray, gtk_ ); */

    dctx->widget = widget;
    dctx->globals = globals;

/*     font = gdk_font_load( "-*-new century schoolbook-medium-r-normal-" */
/*                           "-14-100-100-100-p-82-iso8859-1" ); */
    font = gdk_font_load( "-adobe-new century schoolbook-medium-r-normal-"
			  "-10-100-75-75-p-60-iso8859-1" );

    if ( font == NULL ) {
        font = gdk_font_load( "-misc-fixed-medium-r-*-*-*-140-*-*-*-*-*-*" );
    }    
    dctx->gdkFont = font;

    font = gdk_font_load( "-adobe-new century schoolbook-bold-r-normal-"
			  "-10-100-75-75-p-66-iso8859-1" );
    dctx->gdkBoldFont = font? font:dctx->gdkFont;

/*     font = gdk_font_load( "-*-new century schoolbook-bold-i-normal-" */
/* 			  "-24-240-75-75-p-148-iso8859-1" ); */
    font = gdk_font_load( "-adobe-new century schoolbook-bold-r-normal-"
			  "-12-120-75-75-p-77-iso8859-1" );

    if ( font == NULL ) {
        font = dctx->gdkFont;
    }
    dctx->gdkTrayFont = font;

    dctx->drawGC = gdk_gc_new( widget->window );
    dctx->trayFontHeight = 13;

    dctx->black.red = 0x0000;
    dctx->black.pixel = 0x0000;
    dctx->black.green = 0x0000;
    dctx->black.blue = 0x0000;

    dctx->white.red = 0xFFFF;
    dctx->white.pixel = 0x0000;
    dctx->white.green = 0xFFFF;
    dctx->white.blue = 0xFFFF;

    {
        GdkColormap* map = gdk_colormap_get_system();

        allocAndSet( map, &dctx->bonusColors[0], 0xFFFF, 0xAFFF, 0xAFFF );
        allocAndSet( map, &dctx->bonusColors[1], 0xAFFF, 0xFFFF, 0xAFFF );
        allocAndSet( map, &dctx->bonusColors[2], 0xAFFF, 0xAFFF, 0xFFFF );
        allocAndSet( map, &dctx->bonusColors[3], 0xFFFF, 0xAFFF, 0xFFFF );

        allocAndSet( map, &dctx->playerColors[0], 0x0000, 0x0000, 0xAFFF );
        allocAndSet( map, &dctx->playerColors[1], 0xAFFF, 0x0000, 0x0000 );
        allocAndSet( map, &dctx->playerColors[2], 0x0000, 0xAFFF, 0x0000 );
        allocAndSet( map, &dctx->playerColors[3], 0xAFFF, 0x0000, 0xAFFF );

        allocAndSet( map, &dctx->red, 0xFFFF, 0x0000, 0x0000 );
    }

    return (DrawCtx*)dctx;
} /* gtk_drawctxt_init */

#endif /* PLATFORM_GTK */

