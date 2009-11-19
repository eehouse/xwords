/* -*- mode: C; fill-column: 78; c-basic-offset: 4; compile-command: "make MEMDEBUG=TRUE"; -*- */ 
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
#ifdef PLATFORM_GTK

#include <stdlib.h>
#include <stdio.h>

#include <gdk/gdkdrawable.h>

#include "gtkmain.h"
#include "draw.h"
#include "board.h"
#include "linuxmain.h"

typedef enum {
    XP_GTK_JUST_NONE
    ,XP_GTK_JUST_CENTER
    ,XP_GTK_JUST_TOPLEFT
    ,XP_GTK_JUST_BOTTOMRIGHT
} XP_GTK_JUST;

typedef struct FontPerSize {
	unsigned int ht;
	PangoFontDescription* fontdesc;
	PangoLayout* layout;
} FontPerSize;

/* static GdkGC* newGCForColor( GdkWindow* window, XP_Color* newC ); */
static void
gtkInsetRect( XP_Rect* r, short i )
{
    r->top += i;
    r->left += i;
    i *= 2;

    r->width -= i;
    r->height -= i;
} /* gtkInsetRect */

#if 0
#define DRAW_WHAT(dc) ((dc)->globals->pixmap)
#else
#define DRAW_WHAT(dc) ((dc)->drawing_area->window)
#endif

#define GTKMIN_W_HT 12

static void
gtkFillRect( GtkDrawCtx* dctx, const XP_Rect* rect, const GdkColor* color )
{
    gdk_gc_set_foreground( dctx->drawGC, color );
    gdk_draw_rectangle( DRAW_WHAT(dctx), dctx->drawGC,
                        TRUE,
                        rect->left, rect->top, rect->width, 
                        rect->height );
}

static void
gtkEraseRect( const GtkDrawCtx* dctx, const XP_Rect* rect )
{
    gdk_draw_rectangle( DRAW_WHAT(dctx),
                        dctx->drawing_area->style->white_gc,
                        TRUE, rect->left, rect->top, 
                        rect->width, rect->height );
} /* gtkEraseRect */

static void
frameRect( GtkDrawCtx* dctx, const XP_Rect* rect )
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
    gtkEraseRect( dctx, rect );
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
                           XP_U16* widthP, XP_U16* heightP )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    gint len = strlen(str);
    gint width = gdk_text_measure( dctx->gdkFont, str, len );

    *widthP = width;
    *heightP = 12;              /* ??? :-) */
} /* gtk_prim_draw_measureText */

#endif /* DRAW_WITH_PRIMITIVES */

static gint
compForHt( gconstpointer  a,
		   gconstpointer  b )
{
	FontPerSize* fps1 = (FontPerSize*)a;
	FontPerSize* fps2 = (FontPerSize*)b;
	return fps1->ht - fps2->ht;
}

static PangoLayout*
layout_for_ht( GtkDrawCtx* dctx, XP_U16 ht )
{
	PangoLayout* layout = NULL;

	/* Try to find a cached layout.  Otherwise create a new one. */
	FontPerSize fps = { .ht = ht };
	GList* gl = g_list_find_custom( dctx->fontsPerSize, &fps,
									compForHt );
	if ( NULL != gl ) {
        layout = ((FontPerSize*)gl->data)->layout;
	}

	if ( NULL == layout ) {
		FontPerSize* fps = g_malloc( sizeof(*fps) );
		dctx->fontsPerSize = g_list_insert( dctx->fontsPerSize, fps, 0 );

		char font[32];
		snprintf( font, sizeof(font), "helvetica normal %dpx", ht );

        layout = pango_layout_new( dctx->pangoContext );
        fps->fontdesc = pango_font_description_from_string( font );
        pango_layout_set_font_description( layout, fps->fontdesc );
        fps->layout = layout;

		/* This only happens first time??? */
		pango_layout_set_alignment( layout, PANGO_ALIGN_CENTER );
		fps->ht = ht;
	}

	return layout;
} /* layout_for_ht */

static void
draw_string_at( GtkDrawCtx* dctx, PangoLayout* layout,
                const XP_UCHAR* str, XP_U16 fontHt,
                const XP_Rect* where, XP_GTK_JUST just,
                const GdkColor* frground, const GdkColor* bkgrnd )
{
    gint xx = where->left;
    gint yy = where->top;

    if ( !layout ) {
        layout = layout_for_ht( dctx, fontHt );
    }

    pango_layout_set_text( layout, (char*)str, XP_STRLEN(str) );

    if ( just != XP_GTK_JUST_NONE ) {
        int width, height;
        pango_layout_get_pixel_size( layout, &width, &height );

        switch( just ) {
        case XP_GTK_JUST_CENTER:
            xx += (where->width - width) / 2;
            yy += (where->height - height) / 2;
            break;
        case XP_GTK_JUST_BOTTOMRIGHT:
            xx += where->width - width;
            yy += where->height - height;
            break;
        case XP_GTK_JUST_TOPLEFT:
        default:
            /* nothing to do?? */
            break;
        }
    }

    gdk_draw_layout_with_colors( DRAW_WHAT(dctx), dctx->drawGC,
                                 xx, yy, layout,
                                 frground, bkgrnd );
} /* draw_string_at */

static void
drawBitmapFromLBS( GtkDrawCtx* dctx, const XP_Bitmap bm, const XP_Rect* rect )
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

    gdk_draw_rectangle( pm, dctx->drawing_area->style->white_gc, TRUE,
                        0, 0, nCols, nRows );

    x = 0;
    y = 0;

    while ( nBytes-- ) {
        XP_U8 byte = *bp++;
        for ( i = 0; i < 8; ++i ) {
            XP_Bool draw = ((byte & 0x80) != 0);
            if ( draw ) {
                gdk_draw_point( pm, dctx->drawing_area->style->black_gc, x, y );
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

    gdk_draw_drawable( DRAW_WHAT(dctx),
                       dctx->drawGC,
                       (GdkDrawable*)pm, 0, 0,
                       rect->left+2,
                       rect->top+2,
                       lbs->nCols,
                       lbs->nRows );

    g_object_unref( pm );
} /* drawBitmapFromLBS */

static void
freer( gpointer data, gpointer XP_UNUSED(user_data) )
{
	FontPerSize* fps = (FontPerSize*)data;
	pango_font_description_free( fps->fontdesc );
	g_object_unref( fps->layout );
	g_free( fps );
}

static void
gtk_draw_destroyCtxt( DrawCtx* p_dctx )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    GtkAllocation* alloc = &dctx->drawing_area->allocation;

    gdk_draw_rectangle( DRAW_WHAT(dctx),
			dctx->drawing_area->style->white_gc,
			TRUE,
			0, 0, alloc->width, alloc->height );

	g_list_foreach( dctx->fontsPerSize, freer, NULL );
	g_list_free( dctx->fontsPerSize );

    g_object_unref( dctx->pangoContext );

} /* gtk_draw_destroyCtxt */


static void
gtk_draw_dictChanged( DrawCtx* XP_UNUSED(p_dctx), 
                      const DictionaryCtxt* XP_UNUSED(dict) )
{
}

static XP_Bool
gtk_draw_boardBegin( DrawCtx* p_dctx, const XP_Rect* rect, 
                     DrawFocusState XP_UNUSED(dfs) )
{
    GdkRectangle gdkrect;
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;

    gdk_gc_set_foreground( dctx->drawGC, &dctx->black );

    gdkrect = *(GdkRectangle*)rect;
    ++gdkrect.width;
    ++gdkrect.height;
/*     gdk_gc_set_clip_rectangle( dctx->drawGC, &gdkrect ); */

    return XP_TRUE;
} /* draw_finish */

static void
gtk_draw_objFinished( DrawCtx* XP_UNUSED(p_dctx), 
                      BoardObjectType XP_UNUSED(typ),
                      const XP_Rect* XP_UNUSED(rect), 
                      DrawFocusState XP_UNUSED(dfs) )
{
} /* draw_finished */


static XP_Bool
gtk_draw_vertScrollBoard( DrawCtx* p_dctx, XP_Rect* rect,
                          XP_S16 dist, DrawFocusState XP_UNUSED(dfs) )
{
    /* Turn this on to mimic what palm does, but need to figure out some gtk
       analog to copybits for it to actually work. */
#if 1
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Bool down = dist <= 0;
    gint ysrc, ydest;

    if ( down ) {
        ysrc = rect->top;
        dist = -dist;           /* make it positive */
        ydest = ysrc + dist;
    } else {
        ydest = rect->top;
        ysrc = ydest + dist;
    }

    gdk_draw_drawable( DRAW_WHAT(dctx),
                       dctx->drawGC,
                       DRAW_WHAT(dctx),
                       rect->left,
                       ysrc,
                       rect->left,
                       ydest,
                       rect->width,
                       rect->height - dist );

    if ( !down ) {
        rect->top += rect->height - dist;
    }
    rect->height = dist;
#endif
    return XP_TRUE;
}


static void
drawHintBorders( GtkDrawCtx* dctx, const XP_Rect* rect, HintAtts hintAtts)
{
    if ( hintAtts != HINT_BORDER_NONE && hintAtts != HINT_BORDER_CENTER ) {
        XP_Rect lrect = *rect;
        gtkInsetRect( &lrect, 1 );

        gdk_gc_set_foreground( dctx->drawGC, &dctx->black );

        if ( (hintAtts & HINT_BORDER_LEFT) != 0 ) {
            gdk_draw_rectangle( DRAW_WHAT(dctx),
                                dctx->drawGC,
                                FALSE, lrect.left, lrect.top, 
                                0, lrect.height);
        }
        if ( (hintAtts & HINT_BORDER_TOP) != 0 ) {
            gdk_draw_rectangle( DRAW_WHAT(dctx),
                                dctx->drawGC,
                                FALSE, lrect.left, lrect.top, 
                                lrect.width, 0/*rectInset.height*/);
        }
        if ( (hintAtts & HINT_BORDER_RIGHT) != 0 ) {
            gdk_draw_rectangle( DRAW_WHAT(dctx),
                                dctx->drawGC,
                                FALSE, lrect.left+lrect.width, 
                                lrect.top, 
                                0, lrect.height);
        }
        if ( (hintAtts & HINT_BORDER_BOTTOM) != 0 ) {
            gdk_draw_rectangle( DRAW_WHAT(dctx),
                                dctx->drawGC,
                                FALSE, lrect.left, 
                                lrect.top+lrect.height, 
                                lrect.width, 0 );
        }
    }
}

static XP_Bool
gtk_draw_drawCell( DrawCtx* p_dctx, const XP_Rect* rect, const XP_UCHAR* letter,
                   const XP_Bitmaps* bitmaps, Tile XP_UNUSED(tile), 
                   XP_S16 owner, XWBonusType bonus, HintAtts hintAtts, 
                   CellFlags flags )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Rect rectInset = *rect;
    XP_Bool showGrid = dctx->globals->gridOn;
    XP_Bool highlight = (flags & CELL_HIGHLIGHT) != 0;
    GdkColor* cursor = 
        ((flags & CELL_ISCURSOR) != 0) ? &dctx->cursor : NULL;

    gtkEraseRect( dctx, rect );

    gtkInsetRect( &rectInset, 1 );

    if ( showGrid ) {
        gdk_gc_set_foreground( dctx->drawGC, &dctx->black );
        gdk_draw_rectangle( DRAW_WHAT(dctx),
                            dctx->drawGC,
                            FALSE,
                            rect->left, rect->top, rect->width, 
                            rect->height );
    }

    /* We draw just an empty, potentially colored, square IFF there's nothing
       in the cell or if CELL_DRAGSRC is set */
    if ( (flags & (CELL_DRAGSRC|CELL_ISEMPTY)) != 0 ) {
        if ( !!cursor || bonus != BONUS_NONE ) {
            GdkColor* foreground;
            if ( !!cursor ) {
                foreground = cursor;
            } else if ( bonus != BONUS_NONE ) {
                foreground = &dctx->bonusColors[bonus-1];
            } else {
                foreground = NULL;
            }
            if ( !!foreground ) {
                gdk_gc_set_foreground( dctx->drawGC, foreground );
                gdk_draw_rectangle( DRAW_WHAT(dctx), dctx->drawGC, TRUE,
                                    rectInset.left, rectInset.top,
                                    rectInset.width+1, rectInset.height+1 );
            }
        }
        if ( (flags & CELL_ISSTAR) != 0 ) {
            draw_string_at( dctx, NULL, "*", rect->height, rect, 
                            XP_GTK_JUST_CENTER, &dctx->black, NULL );
        }
    } else if ( !!bitmaps ) {
        drawBitmapFromLBS( dctx, bitmaps->bmps[0], rect );
    } else if ( !!letter ) {
        GdkColor* foreground;
        if ( cursor ) {
            gdk_gc_set_foreground( dctx->drawGC, cursor );
        } else if ( !highlight ) {
            gdk_gc_set_foreground( dctx->drawGC, &dctx->tileBack );
        }
        gdk_draw_rectangle( DRAW_WHAT(dctx), dctx->drawGC, TRUE,
                            rectInset.left, rectInset.top,
                            rectInset.width+1, rectInset.height+1 );

        foreground = highlight? &dctx->white : &dctx->playerColors[owner];
        draw_string_at( dctx, NULL, letter, rectInset.height-2, &rectInset, 
                        XP_GTK_JUST_CENTER, foreground, cursor );

        if ( (flags & CELL_ISBLANK) != 0 ) {
            gdk_draw_arc( DRAW_WHAT(dctx), dctx->drawGC,
                          0,	/* filled */
                          rect->left, /* x */
                          rect->top, /* y */
                          rect->width,/*width, */
                          rect->height,/*width, */
                          0, 360*64 );
        }
    }

    drawHintBorders( dctx, rect, hintAtts );

    return XP_TRUE;
} /* gtk_draw_drawCell */

static void
gtk_draw_invertCell( DrawCtx* XP_UNUSED(p_dctx), 
                     const XP_Rect* XP_UNUSED(rect) )
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

static XP_Bool
gtk_draw_trayBegin( DrawCtx* p_dctx, const XP_Rect* XP_UNUSED(rect),
                    XP_U16 owner, DrawFocusState dfs )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    dctx->trayOwner = owner;
    dctx->topFocus = dfs == DFS_TOP;
    return XP_TRUE;
} /* gtk_draw_trayBegin */

static void
gtkDrawTileImpl( DrawCtx* p_dctx, const XP_Rect* rect, const XP_UCHAR* textP,
                 const XP_Bitmaps* bitmaps, XP_U16 val, CellFlags flags, 
                 XP_Bool clearBack )
{
    XP_UCHAR numbuf[3];
    gint len; 
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Rect insetR = *rect;
    XP_Bool isCursor = (flags & CELL_ISCURSOR) != 0;
    XP_Bool valHidden = (flags & CELL_VALHIDDEN) != 0;
    XP_Bool notEmpty = (flags & CELL_ISEMPTY) == 0;

    if ( clearBack ) {
        gtkEraseRect( dctx, &insetR );
    }

    if ( isCursor || notEmpty ) {
        GdkColor* foreground = &dctx->playerColors[dctx->trayOwner];
        XP_Rect formatRect = insetR;

        gtkInsetRect( &insetR, 1 );

        if ( clearBack ) {
            gtkFillRect( dctx, &insetR, 
                         isCursor ? &dctx->cursor : &dctx->tileBack );
        }

        formatRect.left += 3;
        formatRect.width -= 6;

        if ( notEmpty ) {
            if ( !!bitmaps ) {
                drawBitmapFromLBS( dctx, bitmaps->bmps[1], &insetR );
            } else if ( !!textP ) {
                if ( *textP != LETTER_NONE ) { /* blank */
                    draw_string_at( dctx, NULL, textP, formatRect.height>>1,
                                    &formatRect, XP_GTK_JUST_TOPLEFT,
                                    foreground, NULL );

                }
            }
    
            if ( !valHidden ) {
                XP_SNPRINTF( numbuf, VSIZE(numbuf), "%d", val );
                len = XP_STRLEN( numbuf );

                draw_string_at( dctx, NULL, numbuf, formatRect.height>>2,
                                &formatRect, XP_GTK_JUST_BOTTOMRIGHT,
                                foreground, NULL );
            }
        }

        /* frame the tile */
        gdk_gc_set_foreground( dctx->drawGC, &dctx->black );
        gdk_draw_rectangle( DRAW_WHAT(dctx),
                            dctx->drawGC,
                            FALSE,
                            insetR.left, insetR.top, insetR.width, 
                            insetR.height );

        if ( (flags & CELL_HIGHLIGHT) != 0 ) {
            gtkInsetRect( &insetR, 1 );
            gdk_draw_rectangle( DRAW_WHAT(dctx),
                                dctx->drawGC,
                                FALSE, insetR.left, insetR.top, 
                                insetR.width, insetR.height);
        }
    }
} /* gtkDrawTileImpl */

static void
gtk_draw_drawTile( DrawCtx* p_dctx, const XP_Rect* rect, const XP_UCHAR* textP,
                   const XP_Bitmaps* bitmaps, XP_U16 val, CellFlags flags )
{
    gtkDrawTileImpl( p_dctx, rect, textP, bitmaps, val, flags, XP_TRUE );
}

#ifdef POINTER_SUPPORT
static void
gtk_draw_drawTileMidDrag( DrawCtx* p_dctx, const XP_Rect* rect, 
                          const XP_UCHAR* textP, const XP_Bitmaps* bitmaps, 
                          XP_U16 val, XP_U16 owner, CellFlags flags )
{
    gtk_draw_trayBegin( p_dctx, rect, owner, DFS_NONE );
    gtkDrawTileImpl( p_dctx, rect, textP, bitmaps, val, 
                     flags | CELL_HIGHLIGHT, XP_FALSE );
}
#endif

static void
gtk_draw_drawTileBack( DrawCtx* p_dctx, const XP_Rect* rect, 
                       CellFlags flags )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Bool hasCursor = (flags & CELL_ISCURSOR) != 0;
    XP_Rect r = *rect;

    gtkInsetRect( &r, 1 );

    gtkFillRect( dctx, &r, &dctx->playerColors[dctx->trayOwner] );

    gtkInsetRect( &r, 1 );
    gtkFillRect( dctx, &r, hasCursor? &dctx->cursor : &dctx->tileBack );

    draw_string_at( dctx, NULL, "?", r.height,
                    &r, XP_GTK_JUST_CENTER,
                    &dctx->playerColors[dctx->trayOwner], NULL );

} /* gtk_draw_drawTileBack */

static void
gtk_draw_drawTrayDivider( DrawCtx* p_dctx, const XP_Rect* rect, 
                          CellFlags flags )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Rect r = *rect;
    XP_Bool selected = 0 != (flags & CELL_HIGHLIGHT);
    XP_Bool isCursor = 0 != (flags & CELL_ISCURSOR);

    gtkEraseRect( dctx, &r );

    gtkFillRect( dctx, &r, isCursor? &dctx->cursor:&dctx->white );

    r.left += 2;
    r.width -= 4;
    if ( selected ) {
        --r.height;
    }

    gdk_gc_set_foreground( dctx->drawGC, &dctx->black );
    gdk_draw_rectangle( DRAW_WHAT(dctx),
			dctx->drawGC,
			!selected, 
			r.left, r.top, r.width, r.height);
    
} /* gtk_draw_drawTrayDivider */

static void 
gtk_draw_clearRect( DrawCtx* p_dctx, const XP_Rect* rectP )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Rect rect = *rectP;

    ++rect.width;
    ++rect.top;

    gtkEraseRect( dctx, &rect );

} /* gtk_draw_clearRect */

static void
gtk_draw_drawBoardArrow( DrawCtx* p_dctx, const XP_Rect* rectP, 
                         XWBonusType XP_UNUSED(cursorBonus), XP_Bool vertical,
                         HintAtts hintAtts, CellFlags XP_UNUSED(flags) )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    const XP_UCHAR* curs = vertical? "|":"-";

    /* font needs to be small enough that "|" doesn't overwrite cell below */
    draw_string_at( dctx, NULL, curs, (rectP->height*2)/3,
                    rectP, XP_GTK_JUST_CENTER,
                    &dctx->black, NULL );
    drawHintBorders( dctx, rectP, hintAtts );
} /* gtk_draw_drawBoardCursor */

static void
gtk_draw_scoreBegin( DrawCtx* p_dctx, const XP_Rect* rect, 
                     XP_U16 XP_UNUSED(numPlayers), 
                     const XP_S16* const XP_UNUSED(scores), 
                     XP_S16 XP_UNUSED(remCount), DrawFocusState dfs )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;

/*     gdk_gc_set_clip_rectangle( dctx->drawGC, (GdkRectangle*)rect ); */
    gtkEraseRect( dctx, rect );
    dctx->topFocus = dfs == DFS_TOP;
    dctx->scoreIsVertical = rect->height > rect->width;
} /* gtk_draw_scoreBegin */

static PangoLayout*
getLayoutToFitRect( GtkDrawCtx* dctx, const XP_UCHAR* str, const XP_Rect* rect, 
                    int* heightP )
{
    PangoLayout* layout;
    float ratio, ratioH;
    int width, height;
    XP_U16 len = XP_STRLEN(str);

    /* First measure it using any font at all */
    layout = layout_for_ht( dctx, 24 );
    pango_layout_set_text( layout, (char*)str, len );
    pango_layout_get_pixel_size( layout, &width, &height );

    /* Figure the ratio of is to should-be.  The smaller of these is the one
       we must use. */
    ratio = (float)rect->width / (float)width;
    ratioH = (float)rect->height / (float)height;
    if ( ratioH < ratio ) {
        ratio = ratioH;
    }
    height = 24.0 * ratio;
    if ( !!heightP && *heightP < height ) {
        height = *heightP;
    }

    layout = layout_for_ht( dctx, height );
    pango_layout_set_text( layout, (char*)str, len );
    if ( !!heightP ) {
        *heightP = height;
    }
    return layout;
} /* getLayoutToFitRect */

static void
gtkDrawDrawRemText( DrawCtx* p_dctx, const XP_Rect* rect, XP_S16 nTilesLeft,
                    XP_U16* widthP, XP_U16* heightP, XP_Bool focussed )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_UCHAR buf[10];
    PangoLayout* layout;

    XP_SNPRINTF( buf, sizeof(buf), "rem:%d", nTilesLeft );
    layout = getLayoutToFitRect( dctx, buf, rect, NULL );

    if ( !!widthP ) {
        int width, height;
        pango_layout_get_pixel_size( layout, &width, &height );
        if ( width > rect->width ) {
            width = rect->width;
        }
        if ( height > rect->height ) {
            height = rect->height;
        }
        *widthP = width;
        *heightP = height;
    } else {
        const GdkColor* cursor = NULL;
        if ( focussed ) {
            cursor = &dctx->cursor;
            gtkFillRect( dctx, rect, cursor );
        }
        draw_string_at( dctx, layout, buf, rect->height,
                        rect, XP_GTK_JUST_TOPLEFT,
                        &dctx->black, cursor );
    }
} /* gtkDrawDrawRemText */

static void
gtk_draw_measureRemText( DrawCtx* p_dctx, const XP_Rect* rect, XP_S16 nTilesLeft,
                         XP_U16* width, XP_U16* height )
{
    if ( nTilesLeft <= 0 ) {
        *width = *height = 0;
    } else {
        gtkDrawDrawRemText( p_dctx, rect, nTilesLeft, width, height, XP_FALSE );
    }
} /* gtk_draw_measureRemText */

static void
gtk_draw_drawRemText( DrawCtx* p_dctx, const XP_Rect* rInner, 
                      const XP_Rect* XP_UNUSED(rOuter), XP_S16 nTilesLeft,
                      XP_Bool focussed )
{
    gtkDrawDrawRemText( p_dctx, rInner, nTilesLeft, NULL, NULL, focussed );
} /* gtk_draw_drawRemText */

static void
formatScoreText( PangoLayout* layout, XP_UCHAR* buf, XP_U16 bufLen, 
                 const DrawScoreInfo* dsi, const XP_Rect* bounds, 
                 XP_Bool scoreIsVertical, XP_U16* widthP, int* nLines )
{
    XP_S16 score = dsi->totalScore;
    XP_U16 nTilesLeft = dsi->nTilesLeft;
    XP_Bool isTurn = dsi->isTurn;
    XP_S16 maxWidth = bounds->width;
    XP_UCHAR numBuf[16];
    int width, height;
    *nLines = 1;

    XP_SNPRINTF( numBuf, VSIZE(numBuf), "%d", score );
    if ( (nTilesLeft < MAX_TRAY_TILES) && (nTilesLeft > 0) ) {
        XP_UCHAR tmp[10];
        XP_SNPRINTF( tmp, VSIZE(tmp), ":%d", nTilesLeft );
        (void)XP_STRCAT( numBuf, tmp );
    }

    if ( !!layout ) {
        pango_layout_set_text( layout, (char*)numBuf, XP_STRLEN(numBuf) );
        pango_layout_get_pixel_size( layout, &width, &height );
        if ( !scoreIsVertical ) {
            maxWidth -= width;
            *widthP = width;
        }
    }

    /* Reformat name + ':' until it fits */
    XP_UCHAR name[MAX_SCORE_LEN] = { 0 };
    if ( isTurn && maxWidth > 0 ) {
        XP_U16 len = 1 + XP_STRLEN( dsi->name ); /* +1 for "\0" */
        if ( scoreIsVertical ) {
            ++*nLines;
        } else {
            ++len;              /* for ':' */
        }
        if ( len >= VSIZE(name) ) {
            len = VSIZE(name) - 1;
        }
        for ( ; ; ) {
            XP_SNPRINTF( name, len-1, "%s", dsi->name );
            if ( !scoreIsVertical ) {
                name[len-2] = ':';
                name[len-1] = '\0';
            }

            pango_layout_set_text( layout, (char*)name, len );
            pango_layout_get_pixel_size( layout, &width, &height );
            if ( width <= maxWidth ) {
                if ( !scoreIsVertical ) {
                    *widthP += width;
                }
                break;
            }

            if ( --len < 2 ) {
                name[0] = '\0';
                break;
            }
        }
    }

    if ( scoreIsVertical ) {
        *widthP = bounds->width;
    }

    XP_SNPRINTF( buf, bufLen, "%s%s%s", name, (*nLines>1? XP_CR:""), numBuf );
} /* formatScoreText */

static void
gtk_draw_measureScoreText( DrawCtx* p_dctx, const XP_Rect* bounds, 
                           const DrawScoreInfo* dsi,
                           XP_U16* widthP, XP_U16* heightP )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_UCHAR buf[36];
    PangoLayout* layout;
    int lineHeight = GTK_HOR_SCORE_HEIGHT, nLines;

    layout = getLayoutToFitRect( dctx, "M", bounds, &lineHeight );
    formatScoreText( layout, buf, VSIZE(buf), dsi, bounds, 
                     dctx->scoreIsVertical, widthP, &nLines );
    *heightP = nLines * lineHeight;

    XP_U16 playerNum = dsi->playerNum;
    XP_ASSERT( playerNum < VSIZE(dctx->scoreCache) );
    XP_SNPRINTF( dctx->scoreCache[playerNum].str,
                 VSIZE(dctx->scoreCache[playerNum].str), "%s", buf );
    dctx->scoreCache[playerNum].fontHt = lineHeight;
} /* gtk_draw_measureScoreText */

static void
gtk_draw_score_drawPlayer( DrawCtx* p_dctx, const XP_Rect* rInner, 
                           const XP_Rect* rOuter, const DrawScoreInfo* dsi )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Bool hasCursor = (dsi->flags & CELL_ISCURSOR) != 0;
    GdkColor* cursor = NULL;
    XP_U16 playerNum = dsi->playerNum;
    const XP_UCHAR* scoreBuf = dctx->scoreCache[playerNum].str;
    XP_U16 fontHt = dctx->scoreCache[playerNum].fontHt;

    if ( hasCursor ) {
        cursor = &dctx->cursor;
        gtkFillRect( dctx, rOuter, cursor );
    }

    gdk_gc_set_foreground( dctx->drawGC, &dctx->playerColors[playerNum] );

    if ( dsi->selected ) {
        XP_Rect selRect = *rOuter;
        XP_S16 diff;
        if ( dctx->scoreIsVertical ) {
            diff = selRect.height - rInner->height;
        } else {
            diff = selRect.width - rInner->width;
        }
        if ( diff > 0 ) {
            if ( dctx->scoreIsVertical ) {
                selRect.height -= diff>>1;
                selRect.top += diff>>2;
            } else {
                selRect.width -= diff>>1;
                selRect.left += diff>>2;
            }
        }

        gdk_draw_rectangle( DRAW_WHAT(dctx), dctx->drawGC,
                            TRUE, selRect.left, selRect.top, 
                            selRect.width, selRect.height );
        if ( hasCursor ) {
            gtkFillRect( dctx, rInner, cursor );
        }
        gtkEraseRect( dctx, rInner );
    }

/*     XP_U16 fontHt = rInner->height; */
/*     if ( strstr( scoreBuf, "\n" ) ) { */
/*         fontHt >>= 1; */
/*     } */

    draw_string_at( dctx, NULL, scoreBuf, fontHt/*-1*/,
                    rInner, XP_GTK_JUST_CENTER,
                    &dctx->playerColors[playerNum], cursor );

} /* gtk_draw_score_drawPlayer */

static void
gtk_draw_score_pendingScore( DrawCtx* p_dctx, const XP_Rect* rect, 
                             XP_S16 score, XP_U16 XP_UNUSED(playerNum),
                             CellFlags flags )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_UCHAR buf[5];
    XP_U16 ht;
    XP_Rect localR;
    GdkColor* cursor = ((flags & CELL_ISCURSOR) != 0) 
        ? &dctx->cursor : NULL;

    if ( score >= 0 ) {
		XP_SNPRINTF( buf, VSIZE(buf), "%.3d", score );
    } else {
		XP_STRNCPY( buf, "???", VSIZE(buf)  );
    }

/*     gdk_gc_set_clip_rectangle( dctx->drawGC, (GdkRectangle*)rect ); */

    localR = *rect;
    gtkInsetRect( &localR, 1 );

    if ( !!cursor ) {
        gtkFillRect( dctx, &localR, cursor );
    } else {
        gtkEraseRect( dctx, &localR );
    }

	ht = localR.height >> 2;
    draw_string_at( dctx, NULL, "Pts:", ht,
                    &localR, XP_GTK_JUST_TOPLEFT,
                    &dctx->black, cursor );
    draw_string_at( dctx, NULL, buf, ht,
                    &localR, XP_GTK_JUST_BOTTOMRIGHT,
                    &dctx->black, cursor );

} /* gtk_draw_score_pendingScore */

static void
gtkFormatTimerText( XP_UCHAR* buf, XP_U16 bufLen, XP_S16 secondsLeft )
{
    XP_U16 minutes, seconds;

    if ( secondsLeft < 0 ) {
        *buf++ = '-';
        --bufLen;
        secondsLeft *= -1;
    }

    minutes = secondsLeft / 60;
    seconds = secondsLeft % 60;
    XP_SNPRINTF( buf, bufLen, "% 1d:%02d", minutes, seconds );
} /* gtkFormatTimerText */

static void
gtk_draw_drawTimer( DrawCtx* p_dctx, const XP_Rect* rInner, 
                    XP_U16 XP_UNUSED(player), XP_S16 secondsLeft )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_UCHAR buf[10];

    gtkFormatTimerText( buf, VSIZE(buf), secondsLeft );

/*     gdk_gc_set_clip_rectangle( dctx->drawGC, (GdkRectangle*)rInner ); */
    gtkEraseRect( dctx, rInner );
    draw_string_at( dctx, NULL, buf, rInner->height-1,
                    rInner, XP_GTK_JUST_CENTER,
                    &dctx->black, NULL );
} /* gtk_draw_drawTimer */

#define MINI_LINE_HT 12
#define MINI_V_PADDING 6
#define MINI_H_PADDING 8

static const XP_UCHAR*
gtk_draw_getMiniWText( DrawCtx* XP_UNUSED(p_dctx), XWMiniTextType textHint )
{
/*     GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx; */
    XP_UCHAR* str;

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
gtk_draw_measureMiniWText( DrawCtx* p_dctx, const XP_UCHAR* str, 
                           XP_U16* widthP, XP_U16* heightP )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    int height, width;

	PangoLayout* layout = layout_for_ht( dctx, GTKMIN_W_HT );
    pango_layout_set_text( layout, (char*)str, XP_STRLEN(str) );
    pango_layout_get_pixel_size( layout, &width, &height );
    *heightP = height;
    *widthP = width + 6;
} /* gtk_draw_measureMiniWText */

static void
gtk_draw_drawMiniWindow( DrawCtx* p_dctx, const XP_UCHAR* text, 
                         const XP_Rect* rect, void** XP_UNUSED(closureP) )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Rect localR = *rect;

    gdk_gc_set_foreground( dctx->drawGC, &dctx->black );
/*     gdk_gc_set_clip_rectangle( dctx->drawGC, (GdkRectangle*)&localR ); */

    /* play some skanky games to get the shadow drawn under and to the
       right... */
    gtkEraseRect( dctx, &localR );

    gtkInsetRect( &localR, 1 );
    --localR.width;
    --localR.height;
    frameRect( dctx, &localR );

    --localR.top;
    --localR.left;
    gtkEraseRect( dctx, &localR );
    frameRect( dctx, &localR );

    draw_string_at( dctx, NULL, text, GTKMIN_W_HT,
                    &localR, XP_GTK_JUST_CENTER,
                    &dctx->black, NULL );
} /* gtk_draw_drawMiniWindow */

#define SET_GDK_COLOR( c, r, g, b ) { \
     c.red = (r); \
     c.green = (g); \
     c.blue = (b); \
}
static void
draw_doNothing( DrawCtx* XP_UNUSED(dctx), ... )
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

    success = gdk_colormap_alloc_color( map,
                                        color,
                                        TRUE, /* writeable */
                                        TRUE ); /* best-match */
    XP_ASSERT( success );
} /* allocAndSet */

DrawCtx* 
gtkDrawCtxtMake( GtkWidget* drawing_area, GtkAppGlobals* globals )
{
    GtkDrawCtx* dctx = g_malloc0( sizeof(GtkDrawCtx) );
    GdkColormap* map;

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
    SET_VTABLE_ENTRY( dctx->vtable, draw_objFinished, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_vertScrollBoard, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_trayBegin, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTile, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTileBack, gtk );
#ifdef POINTER_SUPPORT
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTileMidDrag, gtk );
#endif
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTrayDivider, gtk );

    SET_VTABLE_ENTRY( dctx->vtable, draw_drawBoardArrow, gtk );

    SET_VTABLE_ENTRY( dctx->vtable, draw_scoreBegin, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureRemText, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawRemText, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureScoreText, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_score_drawPlayer, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_score_pendingScore, gtk );

    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTimer, gtk );

    SET_VTABLE_ENTRY( dctx->vtable, draw_getMiniWText, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureMiniWText, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawMiniWindow, gtk );

    SET_VTABLE_ENTRY( dctx->vtable, draw_destroyCtxt, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_dictChanged, gtk );
#endif

	dctx->pangoContext = gtk_widget_get_pango_context( drawing_area );
    dctx->drawing_area = drawing_area;
    dctx->globals = globals;

    map = gdk_colormap_get_system();

    allocAndSet( map, &dctx->black, 0x0000, 0x0000, 0x0000 );
    allocAndSet( map, &dctx->white, 0xFFFF, 0xFFFF, 0xFFFF );

    {
        GdkWindow *window = NULL;
        if ( GTK_WIDGET_FLAGS(GTK_WIDGET(drawing_area)) & GTK_NO_WINDOW ) {
            /* XXX I'm not sure about this function because I never used it.
             * (the name seems to indicate what you want though).
             */
            window = gtk_widget_get_parent_window( GTK_WIDGET(drawing_area) );
        } else {
            window = GTK_WIDGET(drawing_area)->window;
        }
        dctx->drawGC = gdk_gc_new( window );
    }

    map = gdk_colormap_get_system();

    allocAndSet( map, &dctx->black, 0x0000, 0x0000, 0x0000 );
    allocAndSet( map, &dctx->white, 0xFFFF, 0xFFFF, 0xFFFF );

    allocAndSet( map, &dctx->bonusColors[0], 0xFFFF, 0xAFFF, 0xAFFF );
    allocAndSet( map, &dctx->bonusColors[1], 0xAFFF, 0xFFFF, 0xAFFF );
    allocAndSet( map, &dctx->bonusColors[2], 0xAFFF, 0xAFFF, 0xFFFF );
    allocAndSet( map, &dctx->bonusColors[3], 0xFFFF, 0xAFFF, 0xFFFF );

    allocAndSet( map, &dctx->playerColors[0], 0x0000, 0x0000, 0xAFFF );
    allocAndSet( map, &dctx->playerColors[1], 0xAFFF, 0x0000, 0x0000 );
    allocAndSet( map, &dctx->playerColors[2], 0x0000, 0xAFFF, 0x0000 );
    allocAndSet( map, &dctx->playerColors[3], 0xAFFF, 0x0000, 0xAFFF );

    allocAndSet( map, &dctx->tileBack, 0xFFFF, 0xFFFF, 0x9999 );
    allocAndSet( map, &dctx->cursor, 253<<8, 12<<8, 222<<8 ); 
    allocAndSet( map, &dctx->red, 0xFFFF, 0x0000, 0x0000 );

    return (DrawCtx*)dctx;
} /* gtkDrawCtxtMake */

void
draw_gtk_status( GtkDrawCtx* dctx, char ch )
{
    GtkAppGlobals* globals = dctx->globals;

    XP_Rect rect = {
        .left = globals->netStatLeft,
        .top = globals->netStatTop,
        .width = GTK_NETSTAT_WIDTH,
        .height = GTK_HOR_SCORE_HEIGHT
    };
    gtkEraseRect( dctx, &rect );

    const XP_UCHAR str[2] = { ch, '\0' };
    draw_string_at( dctx, NULL, str, GTKMIN_W_HT,
                    &rect, XP_GTK_JUST_CENTER,
                    &dctx->black, NULL );
}

#endif /* PLATFORM_GTK */

