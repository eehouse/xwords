/* -*- compile-command: "make MEMDEBUG=TRUE -j5"; -*- */
/* 
 * Copyright 1997 - 2017 by Eric House (xwords@eehouse.org).  All rights
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

#undef GDK_DISABLE_DEPRECATED

#include <gdk/gdk.h>

#include "gtkboard.h"
#include "draw.h"
#include "board.h"
#include "linuxmain.h"
#include "linuxutl.h"

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

static void gtk_draw_measureScoreText( DrawCtx* p_dctx, XWEnv xwe,
                                       const XP_Rect* bounds,
                                       const DrawScoreInfo* dsi,
                                       XP_U16* widthP, XP_U16* heightP );
static gdouble figureColor( int in );

/* static GdkGC* newGCForColor( GdkWindow* window, XP_Color* newC ); */
static void
gtkInsetRect( XP_Rect* r, short i )
{
    r->top += i;
    r->left += i;
    i *= 2;

    XP_ASSERT( r->height >= i && r->width >= i );
    r->width -= i;
    r->height -= i;
} /* gtkInsetRect */

#define GTKMIN_W_HT 12

#ifdef USE_CAIRO
# define XP_UNUSED_CAIRO(var) UNUSED__ ## var __attribute__((unused))
# define GDKDRAWABLE void
# define GDKGC void
# define GDKCOLORMAP void
#define LOG_CAIRO_PENDING() XP_LOGF( "%s(): CAIRO work pending", __func__ )

#else
# define XP_UNUSED_CAIRO(var) var
# define GDKCOLORMAP GdkColormap
#endif

static XP_Bool
initCairo( GtkDrawCtx* dctx )
{
    /* XP_LOGF( "%s(dctx=%p)", __func__, dctx ); */
    XP_ASSERT( !dctx->_cairo );
    cairo_t* cairo = NULL;

    if ( !!dctx->surface ) {  /* the thumbnail case */
        XP_LOGF( "%s(): have surface; doing nothing", __func__ );
        cairo = cairo_create( dctx->surface );
        cairo_surface_destroy( dctx->surface );
        // XP_ASSERT( 0 );
    } else if ( !!dctx->drawing_area ) {
#ifdef GDK_AVAILABLE_IN_3_22
        GdkWindow* window = gtk_widget_get_window( dctx->drawing_area );
        const cairo_region_t* region = gdk_window_get_visible_region( window );
        dctx->dc = gdk_window_begin_draw_frame( window, region );
        cairo = gdk_drawing_context_get_cairo_context( dctx->dc );
#else
        cairo = gdk_cairo_create( gtk_widget_get_window(dctx->drawing_area) );
#endif
    } else {
        XP_ASSERT( 0 );
    }
    XP_Bool inited = !!cairo;
    if ( inited ) {
        dctx->_cairo = cairo;
        if ( !!dctx->surface ) {
            cairo_set_line_width( cairo, 0.1 );
        } else {
            cairo_set_line_width( cairo, 1.0 );
        }
        cairo_set_line_cap( cairo, CAIRO_LINE_CAP_SQUARE );
    }
    return inited;
}

static void
destroyCairo( GtkDrawCtx* dctx )
{
    /* XP_LOGF( "%s(dctx=%p)", __func__, dctx ); */
    XP_ASSERT( !!dctx->_cairo );
    if ( !!dctx->surface ) {    /* the thumbnail case */
        XP_LOGF( "%s(): have surface; doing nothing", __func__ );
    } else {
#ifdef GDK_AVAILABLE_IN_3_22
        GdkWindow* window = gtk_widget_get_window( dctx->drawing_area );
        gdk_window_end_draw_frame( window, dctx->dc );
#else
        cairo_destroy( dctx->_cairo );
#endif
    }
    dctx->_cairo = NULL;
}

static XP_Bool
haveCairo( const GtkDrawCtx* dctx )
{
    return !!dctx->_cairo;
}

static cairo_t*
getCairo( const GtkDrawCtx* dctx )
{
    XP_ASSERT( !!dctx->_cairo );
    return dctx->_cairo;
}

static void 
draw_rectangle( const GtkDrawCtx* dctx, 
                gboolean fill, gint left, gint top, gint width, 
                gint height )
{
#ifdef USE_CAIRO
    cairo_t *cr = getCairo( dctx );

    cairo_rectangle( cr, left, top, width, height );
    cairo_stroke_preserve( cr );
    if ( fill ) {
        cairo_fill( cr );
    } else {
        cairo_stroke( cr );
    }
    /* } else { */
    /*     cairo_stroke( dctx->cairo ); */
    /* } */
#else
    dctx = dctx;
    gdk_draw_rectangle( drawable, gc, fill, left, top, width, height );
#endif
}

static void
gtkSetForeground( const GtkDrawCtx* dctx, const GdkRGBA* color )
{
#ifdef USE_CAIRO
    gdk_cairo_set_source_rgba( getCairo(dctx), color );
#else
    gdk_gc_set_foreground( dctx->drawGC, color );
#endif
}

static void
gtkFillRect( GtkDrawCtx* dctx, const XP_Rect* rect, const GdkRGBA* color )
{
    gtkSetForeground( dctx, color );
    draw_rectangle( dctx, TRUE, rect->left, rect->top,
                    rect->width, rect->height );
}

static void
set_color_cairo( const GtkDrawCtx* dctx, unsigned short red, 
                 unsigned short green, unsigned short blue )
{
    GdkRGBA color = { figureColor(red),
                      figureColor(green),
                      figureColor(blue),
                      1.0 };
    gdk_cairo_set_source_rgba( getCairo(dctx), &color );
}

static void
gtkEraseRect( const GtkDrawCtx* dctx, const XP_Rect* rect )
{
    set_color_cairo( dctx, 0xFFFF, 0xFFFF, 0xFFFF );
    // const GtkStyle* style = gtk_widget_get_style( dctx->drawing_area );
    draw_rectangle( dctx, TRUE, rect->left, rect->top,
                    rect->width, rect->height );
} /* gtkEraseRect */

#ifdef DRAW_WITH_PRIMITIVES

static void
gtk_prim_draw_setClip( DrawCtx* p_dctx, XWEnv xwe, XP_Rect* newClip, XP_Rect* oldClip)
{
} /* gtk_prim_draw_setClip */

static void 
gtk_prim_draw_frameRect( DrawCtx* p_dctx, XWEnv xwe, XP_Rect* rect )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    frameRect( dctx, rect );
} /* gtk_prim_draw_frameRect */

static void
gtk_prim_draw_invertRect( DrawCtx* p_dctx, XWEnv xwe, XP_Rect* rect )
{
    /* not sure you can do this on GTK!! */
} /* gtk_prim_draw_invertRect */

static void
gtk_prim_draw_clearRect( DrawCtx* p_dctx, XWEnv xwe, XP_Rect* rect )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    gtkEraseRect( dctx, rect );
} /* gtk_prim_draw_clearRect */

static void
gtk_prim_draw_drawString( DrawCtx* p_dctx, XWEnv xwe, XP_UCHAR* str,
                          XP_U16 x, XP_U16 y )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_U16 fontHeight = 10;     /* FIX ME */
    gdk_draw_string( DRAW_WHAT(dctx), dctx->gdkFont, dctx->drawGC,
                     x, y + fontHeight, str );
} /* gtk_prim_draw_drawString */

static void
gtk_prim_draw_drawBitmap( DrawCtx* p_dctx, XWEnv xwe, XP_Bitmap bm,
                          XP_U16 x, XP_U16 y )
{
} /* gtk_prim_draw_drawBitmap */

static void
gtk_prim_draw_measureText( DrawCtx* p_dctx, XWEnv xwe, XP_UCHAR* str,
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
        layout = g_object_ref(((FontPerSize*)gl->data)->layout);
    }

    if ( NULL == layout ) {
        FontPerSize* fps = g_malloc( sizeof(*fps) );
        dctx->fontsPerSize = g_list_insert( dctx->fontsPerSize, fps, 0 );

        char font[32];
        snprintf( font, sizeof(font), "helvetica normal %dpx", ht );

        PangoContext* pc = pango_cairo_create_context( getCairo(dctx) );
        layout = pango_layout_new( pc );
        g_object_unref( pc );
        fps->fontdesc = pango_font_description_from_string( font );
        pango_layout_set_font_description( layout, fps->fontdesc );
        fps->layout = g_object_ref( layout );

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
                const GdkRGBA* frground,
                const GdkRGBA* bkgrnd )
{
    // XP_LOGF( "%s(%s, %d, %d)", __func__, str, where->left, where->top );
    gint xx = where->left;
    gint yy = where->top;
#ifdef USE_CAIRO
    cairo_t* cr = getCairo( dctx );
#endif

    gdk_cairo_set_source_rgba( cr, frground );
    if ( !!layout ) {
        g_object_ref( layout );
    } else {
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

#ifdef USE_CAIRO
    XP_USE(bkgrnd);
    cairo_save( cr );
    cairo_translate( cr, xx, yy );
    pango_cairo_show_layout( cr, layout );
    cairo_restore( cr );
#else
    gdk_draw_layout_with_colors( DRAW_WHAT(dctx), dctx->drawGC,
                                 xx, yy, layout,
                                 frground, bkgrnd );
#endif
    g_object_unref(layout);
} /* draw_string_at */

static void
drawBitmapFromLBS( GtkDrawCtx* dctx, const XP_Bitmap bm, const XP_Rect* rect )
{
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

#ifdef USE_CAIRO
    draw_rectangle( dctx, TRUE, 0, 0, nCols, nRows );
#else
    const GtkStyle* style = gtk_widget_get_style( dctx->drawing_area );
    GdkPixmap* pm = gdk_pixmap_new( DRAW_WHAT(dctx), nCols, nRows, -1 );
    draw_rectangle( dctx, pm, style->white_gc, TRUE, 0, 0, nCols, nRows );
#endif

    x = 0;
    y = 0;

    while ( nBytes-- ) {
        XP_U8 byte = *bp++;
        for ( i = 0; i < 8; ++i ) {
            XP_Bool draw = ((byte & 0x80) != 0);
            if ( draw ) {
#ifdef USE_CAIRO
                LOG_CAIRO_PENDING();
#else
                gdk_draw_point( pm, style->black_gc, x, y );
#endif
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

#ifdef USE_CAIRO
    rect = rect;
    LOG_CAIRO_PENDING();
#else
    gdk_draw_drawable( DRAW_WHAT(dctx),
                       dctx->drawGC,
                       (GdkDrawable*)pm, 0, 0,
                       rect->left+2,
                       rect->top+2,
                       lbs->nCols,
                       lbs->nRows );
    g_object_unref( pm );
#endif
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
gtk_draw_destroyCtxt( DrawCtx* p_dctx, XWEnv XP_UNUSED(xwe) )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)(void*)p_dctx;
    GtkAllocation alloc;
    gtk_widget_get_allocation( dctx->drawing_area, &alloc );

#ifdef USE_CAIRO
    draw_rectangle( dctx, TRUE, 0, 0, alloc.width, alloc.height );
#else
    const GtkStyle* style = gtk_widget_get_style( dctx->drawing_area );
    draw_rectangle( dctx, DRAW_WHAT(dctx), style->white_gc, TRUE,
                    0, 0, alloc.width, alloc.height );
#endif

    g_list_foreach( dctx->fontsPerSize, freer, NULL );
    g_list_free( dctx->fontsPerSize );

} /* gtk_draw_destroyCtxt */


static void
gtk_draw_dictChanged( DrawCtx* XP_UNUSED(p_dctx), XWEnv XP_UNUSED(xwe),
                      XP_S16 XP_UNUSED(playerNum),
                      const DictionaryCtxt* XP_UNUSED(dict) )
{
}

static XP_Bool
gtk_draw_beginDraw( DrawCtx* p_dctx, XWEnv XP_UNUSED(xwe) )
{
#ifdef USE_CAIRO
    GtkDrawCtx* dctx = (GtkDrawCtx*)(void*)p_dctx;
    return initCairo( dctx );
#else
    fix this
#endif
}

static void
gtk_draw_endDraw( DrawCtx* p_dctx, XWEnv XP_UNUSED(xwe) )
{
#ifdef USE_CAIRO
    GtkDrawCtx* dctx = (GtkDrawCtx*)(void*)p_dctx;
    destroyCairo( dctx );
#endif
}

static XP_Bool
gtk_draw_boardBegin( DrawCtx* p_dctx, XWEnv XP_UNUSED(xwe), const XP_Rect* rect,
                     XP_U16 width, XP_U16 height,
                     DrawFocusState XP_UNUSED(dfs) )
{
    GdkRectangle gdkrect;
    GtkDrawCtx* dctx = (GtkDrawCtx*)(void*)p_dctx;
    dctx->cellWidth = width;
    dctx->cellHeight = height;

    gtkSetForeground( dctx, &dctx->black );

    gdkrect = *(GdkRectangle*)(void*)rect;
    ++gdkrect.width;
    ++gdkrect.height;

    return XP_TRUE;
} /* gtk_draw_boardBegin */

static void
gtk_draw_objFinished( DrawCtx* XP_UNUSED(p_dctx), XWEnv XP_UNUSED(xwe),
                      BoardObjectType XP_UNUSED(typ),
                      const XP_Rect* XP_UNUSED(rect), 
                      DrawFocusState XP_UNUSED(dfs) )
{
} /* gtk_draw_objFinished */

static XP_Bool
gtk_draw_vertScrollBoard( DrawCtx* p_dctx, XWEnv XP_UNUSED(xwe), XP_Rect* rect,
                          XP_S16 dist, DrawFocusState XP_UNUSED(dfs) )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)(void*)p_dctx;
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

#ifdef USE_CAIRO
    dctx = dctx;
#else
    gdk_draw_drawable( DRAW_WHAT(dctx),
                       dctx->drawGC,
                       DRAW_WHAT(dctx),
                       rect->left,
                       ysrc,
                       rect->left,
                       ydest,
                       rect->width,
                       rect->height - dist );
#endif
    if ( !down ) {
        rect->top += rect->height - dist;
    }
    rect->height = dist;
    /* XP_LOGF( "%s=>(%d,%d,%d,%d)", __func__, rect->left, rect->top, */
    /*          rect->width, rect->height ); */

    return XP_TRUE;
} /* gtk_draw_vertScrollBoard */

static void
drawHintBorders( GtkDrawCtx* dctx, const XP_Rect* rect, HintAtts hintAtts)
{
    if ( hintAtts != HINT_BORDER_NONE && hintAtts != HINT_BORDER_CENTER ) {
        XP_Rect lrect = *rect;
        gtkInsetRect( &lrect, 1 );

        gtkSetForeground( dctx, &dctx->black );

        if ( (hintAtts & HINT_BORDER_LEFT) != 0 ) {
            draw_rectangle( dctx, FALSE, lrect.left, lrect.top,
                            0, lrect.height);
        }
        if ( (hintAtts & HINT_BORDER_TOP) != 0 ) {
            draw_rectangle( dctx, FALSE, lrect.left, lrect.top,
                            lrect.width, 0/*rectInset.height*/);
        }
        if ( (hintAtts & HINT_BORDER_RIGHT) != 0 ) {
            draw_rectangle( dctx, FALSE, lrect.left+lrect.width,
                            lrect.top, 
                            0, lrect.height);
        }
        if ( (hintAtts & HINT_BORDER_BOTTOM) != 0 ) {
            draw_rectangle( dctx, FALSE, lrect.left,
                            lrect.top+lrect.height,
                            lrect.width, 0 );
        }
    }
}

#ifdef XWFEATURE_CROSSHAIRS
static void
drawCrosshairs( GtkDrawCtx* dctx, const XP_Rect* rect, CellFlags flags )
{
    XP_Rect hairRect;
    if ( 0 != (flags & CELL_CROSSHOR) ) {
        hairRect = *rect;
        hairRect.height /= 3;
        hairRect.top += hairRect.height;
        gtkFillRect( dctx, &hairRect,  &dctx->cursor );
    }
    if ( 0 != (flags & CELL_CROSSVERT) ) {
        hairRect = *rect;
        hairRect.width /= 3;
        hairRect.left += hairRect.width;
        gtkFillRect( dctx, &hairRect,  &dctx->cursor );
    }
}
#else
# define drawCrosshairs( a, b, c )
#endif

static XP_Bool
gtk_draw_drawCell( DrawCtx* p_dctx, XWEnv XP_UNUSED(xwe), const XP_Rect* rect,
                   const XP_UCHAR* letter,
                   const XP_Bitmaps* bitmaps, Tile XP_UNUSED(tile), 
                   const XP_UCHAR* value, XP_S16 owner, XWBonusType bonus,
                   HintAtts hintAtts, CellFlags flags )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)(void*)p_dctx;
    XP_Rect rectInset = *rect;
    GtkGameGlobals* globals = dctx->globals;
    XP_Bool showGrid = globals->gridOn;
    XP_Bool recent = (flags & CELL_RECENT) != 0;
    XP_Bool pending = (flags & CELL_PENDING) != 0;
    GdkRGBA* cursor = 
        ((flags & CELL_ISCURSOR) != 0) ? &dctx->cursor : NULL;
    GdkRGBA* foreground = &dctx->white;

    gtkEraseRect( dctx, rect );

    gtkInsetRect( &rectInset, 1 );

#ifdef USE_CAIRO
    cairo_t* cr = getCairo( dctx );
#endif

    if ( showGrid ) {
#ifdef USE_CAIRO
        cairo_set_source_rgb( cr, 0, 0, 0 );
#else
        gdk_gc_set_foreground( dctx->drawGC, &dctx->black );
#endif
        draw_rectangle( dctx, FALSE, rect->left, rect->top,
                        rect->width, rect->height );
    }

    /* We draw just an empty, potentially colored, square IFF there's nothing
       in the cell or if CELL_DRAGSRC is set */
    if ( (flags & (CELL_DRAGSRC|CELL_ISEMPTY)) != 0 ) {
        if ( !!cursor || bonus != BONUS_NONE ) {
            if ( !!cursor ) {
                foreground = cursor;
            } else if ( bonus != BONUS_NONE ) {
                foreground = &dctx->bonusColors[bonus-1];
            /* } else { */
            /*     foreground = &dctx->white; */
            }
            if ( !!foreground ) {
#ifdef USE_CAIRO
                gtkFillRect( dctx, &rectInset, foreground );
                // gdk_cairo_set_source_rgba( cr, foreground );
#else
                gdk_gc_set_foreground( dctx->drawGC, foreground );
                draw_rectangle( dctx, DRAW_WHAT(dctx), NULL, TRUE,
                                rectInset.left, rectInset.top,
                                rectInset.width+1, rectInset.height+1 );
#endif
            }
        }
        if ( (flags & CELL_ISSTAR) != 0 ) {
            draw_string_at( dctx, NULL, "*", dctx->cellHeight, rect, 
                            XP_GTK_JUST_CENTER, &dctx->black, NULL );
        }
    } else if ( !!bitmaps && !!bitmaps->bmps[0] ) {
        XP_Rect tmpRect = *rect;
        if ( !!value ) {
            tmpRect.width = tmpRect.width * 3 / 4;
            tmpRect.height = tmpRect.height * 3 / 4;
        }
        drawBitmapFromLBS( dctx, bitmaps->bmps[0], &tmpRect );
    } else if ( !!letter ) {
        XP_Bool isBlank = (flags & CELL_ISBLANK) != 0;
        if ( cursor ) {
            gtkSetForeground( dctx, cursor );
        } else if ( !recent && !pending ) {
            gtkSetForeground( dctx, &dctx->tileBack );
        }
        draw_rectangle( dctx, TRUE, rectInset.left, rectInset.top,
                        rectInset.width+1, rectInset.height+1 );

        if ( isBlank && 0 == strcmp("_",letter ) ) {
            letter = "?";
            isBlank = XP_FALSE;
        }

        if ( pending ) {
            foreground = &dctx->white;
        } else if ( recent ) {
            foreground = &dctx->grey;
        } else {
            foreground = &dctx->playerColors[owner];
        }
        XP_Rect tmpRect = rectInset;
        XP_U16 fontHt = dctx->cellHeight;
        if ( !!value ) {
            tmpRect.width = tmpRect.width * 3 / 4;
            tmpRect.height = tmpRect.height * 3 / 4;
            fontHt = fontHt * 3 / 4;
        }
        draw_string_at( dctx, NULL, letter, fontHt, &tmpRect,
                        XP_GTK_JUST_CENTER, foreground, cursor );

        if ( isBlank ) {
#ifdef USE_CAIRO
#else
            gdk_draw_arc( DRAW_WHAT(dctx), dctx->drawGC,
                          0,	/* filled */
                          rect->left, /* x */
                          rect->top, /* y */
                          rect->width,/*width, */
                          rect->height,/*width, */
                          0, 360*64 );
#endif
        }
    }

    if ( !!value ) {
        XP_Rect tmpRect = *rect;
        tmpRect.left += tmpRect.width * 3 / 4;
        tmpRect.width /= 4;
        tmpRect.top += tmpRect.height * 3 / 4;
        tmpRect.height /= 4;
        draw_string_at( dctx, NULL, value, dctx->cellHeight/4, &tmpRect,
                        XP_GTK_JUST_CENTER, foreground, cursor );
    }

    drawHintBorders( dctx, rect, hintAtts );
    drawCrosshairs( dctx, rect, flags );

    return XP_TRUE;
} /* gtk_draw_drawCell */

static void
gtk_draw_invertCell( DrawCtx* XP_UNUSED(p_dctx), XWEnv XP_UNUSED(xwe),
                     const XP_Rect* XP_UNUSED(rect) )
{
/*     GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx; */
/*     (void)gtk_draw_drawMiniWindow( p_dctx, "f", rect); */

/*     GdkGCValues values; */

/*     gdk_gc_get_values( dctx->drawGC, &values ); */

/*     gdk_gc_set_function( dctx->drawGC, GDK_INVERT ); */

/*     gdk_gc_set_clip_rectangle( dctx->drawGC, (GdkRectangle*)rect ); */
/*     draw_rectangle( DRAW_WHAT(dctx), dctx->drawGC, */
/* 			TRUE, rect->left, rect->top,  */
/* 			rect->width, rect->height ); */

/*     gdk_gc_set_function( dctx->drawGC, values.function ); */
} /* gtk_draw_invertCell */

static XP_Bool
gtk_draw_trayBegin( DrawCtx* p_dctx, XWEnv XP_UNUSED(xwe), const XP_Rect* XP_UNUSED(rect),
                    XP_U16 owner, XP_S16 XP_UNUSED(owner), 
                    DrawFocusState XP_UNUSED(dfs) )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)(void*)p_dctx;
    XP_Bool doDraw = !dctx->surface;
    if ( doDraw ) {
        dctx->trayOwner = owner;
    }
    return doDraw;
} /* gtk_draw_trayBegin */

static XP_Bool
gtkDrawTileImpl( DrawCtx* p_dctx, XWEnv XP_UNUSED(xwe), const XP_Rect* rect, const XP_UCHAR* textP,
                 const XP_Bitmaps* bitmaps, XP_U16 val, CellFlags flags, 
                 XP_Bool clearBack )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)(void*)p_dctx;
    XP_UCHAR numbuf[3];
    XP_Rect insetR = *rect;
    XP_Bool isCursor = (flags & CELL_ISCURSOR) != 0;
    XP_Bool valHidden = (flags & CELL_VALHIDDEN) != 0;
    XP_Bool notEmpty = (flags & CELL_ISEMPTY) == 0;

    if ( clearBack ) {
        gtkEraseRect( dctx, &insetR );
    }

    if ( isCursor || notEmpty ) {
        GdkRGBA* foreground = &dctx->playerColors[dctx->trayOwner];
        XP_Rect formatRect = insetR;

        gtkInsetRect( &insetR, 1 );

        if ( clearBack ) {
            gtkFillRect( dctx, &insetR, 
                         isCursor ? &dctx->cursor : &dctx->tileBack );
        }

        formatRect.left += 3;
        formatRect.width -= 6;

        if ( notEmpty ) {
            if ( !!bitmaps && !!bitmaps->bmps[1] ) {
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

                draw_string_at( dctx, NULL, numbuf, formatRect.height>>2,
                                &formatRect, XP_GTK_JUST_BOTTOMRIGHT,
                                foreground, NULL );
            }
        }

        /* frame the tile */
        gtkSetForeground( dctx, &dctx->black );
        draw_rectangle( dctx, FALSE,
                        insetR.left, insetR.top, insetR.width,
                        insetR.height );

        if ( (flags & (CELL_PENDING|CELL_RECENT)) != 0 ) {
            gtkInsetRect( &insetR, 1 );
            draw_rectangle( dctx, FALSE, insetR.left, insetR.top,
                            insetR.width, insetR.height);
        }
    }
    return XP_TRUE;
} /* gtkDrawTileImpl */

static XP_Bool
gtk_draw_drawTile( DrawCtx* p_dctx, XWEnv xwe, const XP_Rect* rect, const XP_UCHAR* textP,
                   const XP_Bitmaps* bitmaps, XP_U16 val, CellFlags flags )
{
    return gtkDrawTileImpl( p_dctx, xwe, rect, textP, bitmaps, val, flags, XP_TRUE );
}

#ifdef POINTER_SUPPORT
static XP_Bool
gtk_draw_drawTileMidDrag( DrawCtx* p_dctx, XWEnv xwe, const XP_Rect* rect,
                          const XP_UCHAR* textP, const XP_Bitmaps* bitmaps, 
                          XP_U16 val, XP_U16 owner, CellFlags flags )
{
    gtk_draw_trayBegin( p_dctx, xwe, rect, owner, 0, DFS_NONE );
    return gtkDrawTileImpl( p_dctx, xwe, rect, textP, bitmaps, val, 
                            flags | (CELL_PENDING|CELL_RECENT), XP_FALSE );
}
#endif

static XP_Bool
gtk_draw_drawTileBack( DrawCtx* p_dctx, XWEnv XP_UNUSED(xwe), const XP_Rect* rect, 
                       CellFlags flags )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)(void*)p_dctx;
    XP_Bool hasCursor = (flags & CELL_ISCURSOR) != 0;
    XP_Rect r = *rect;

    gtkInsetRect( &r, 1 );

    gtkFillRect( dctx, &r, &dctx->playerColors[dctx->trayOwner] );

    gtkInsetRect( &r, 1 );
    gtkFillRect( dctx, &r, hasCursor? &dctx->cursor : &dctx->tileBack );

    draw_string_at( dctx, NULL, "?", r.height,
                    &r, XP_GTK_JUST_CENTER,
                    &dctx->playerColors[dctx->trayOwner], NULL );
    return XP_TRUE;
} /* gtk_draw_drawTileBack */

static void
gtk_draw_drawTrayDivider( DrawCtx* p_dctx, XWEnv XP_UNUSED(xwe),
                          const XP_Rect* rect, CellFlags flags )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)(void*)p_dctx;
    XP_Rect r = *rect;
    XP_Bool selected = 0 != (flags & (CELL_RECENT|CELL_PENDING));
    XP_Bool isCursor = 0 != (flags & CELL_ISCURSOR);

    gtkEraseRect( dctx, &r );

    gtkFillRect( dctx, &r, isCursor? &dctx->cursor:&dctx->white );

    r.left += 2;
    r.width -= 4;
    if ( selected ) {
        --r.height;
    }

    gtkSetForeground( dctx, &dctx->black );
    draw_rectangle( dctx, !selected,
                    r.left, r.top, r.width, r.height);
} /* gtk_draw_drawTrayDivider */

static void 
gtk_draw_clearRect( DrawCtx* p_dctx, XWEnv XP_UNUSED(xwe), const XP_Rect* rectP )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)(void*)p_dctx;
    XP_Rect rect = *rectP;

    ++rect.width;
    ++rect.top;

    gtkEraseRect( dctx, &rect );

} /* gtk_draw_clearRect */

static void
gtk_draw_drawBoardArrow( DrawCtx* p_dctx, XWEnv XP_UNUSED(xwe), const XP_Rect* rectP,
                         XWBonusType XP_UNUSED(cursorBonus), XP_Bool vertical,
                         HintAtts hintAtts, CellFlags XP_UNUSED(flags) )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)(void*)p_dctx;
    const XP_UCHAR* curs = vertical? "|":"-";

    /* font needs to be small enough that "|" doesn't overwrite cell below */
    draw_string_at( dctx, NULL, curs, (rectP->height*2)/3,
                    rectP, XP_GTK_JUST_CENTER,
                    &dctx->black, NULL );
    drawHintBorders( dctx, rectP, hintAtts );
} /* gtk_draw_drawBoardCursor */

static XP_Bool
gtk_draw_scoreBegin( DrawCtx* p_dctx, XWEnv XP_UNUSED(xwe), const XP_Rect* rect,
                     XP_U16 XP_UNUSED(numPlayers), 
                     const XP_S16* const XP_UNUSED(scores), 
                     XP_S16 XP_UNUSED(remCount), 
                     DrawFocusState XP_UNUSED(dfs) )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)(void*)p_dctx;
    XP_Bool doDraw = !dctx->surface;
    if ( doDraw ) {
        gtkEraseRect( dctx, rect );
        dctx->scoreIsVertical = rect->height > rect->width;
    }
    return doDraw;
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
    g_object_unref( layout );

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
gtkDrawDrawRemText( DrawCtx* p_dctx, XWEnv XP_UNUSED(xwe), const XP_Rect* rect,
                    XP_S16 nTilesLeft, XP_U16* widthP, XP_U16* heightP, XP_Bool focussed )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)(void*)p_dctx;
    XP_UCHAR buf[10];
    XP_SNPRINTF( buf, sizeof(buf), "rem:%d", nTilesLeft );
    PangoLayout* layout = getLayoutToFitRect( dctx, buf, rect, NULL );

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
        const GdkRGBA* cursor = NULL;
        if ( focussed ) {
            cursor = &dctx->cursor;
            gtkFillRect( dctx, rect, cursor );
        }
        draw_string_at( dctx, layout, buf, rect->height,
                        rect, XP_GTK_JUST_TOPLEFT,
                        &dctx->black, NULL );
    }
    g_object_unref( layout );
} /* gtkDrawDrawRemText */

static void
gtk_draw_score_drawPlayer( DrawCtx* p_dctx, XWEnv XP_UNUSED(xwe), const XP_Rect* rInner,
                           const XP_Rect* rOuter, 
                           XP_U16 XP_UNUSED(gotPct), const DrawScoreInfo* dsi )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)(void*)p_dctx;
    XP_Bool hasCursor = (dsi->flags & CELL_ISCURSOR) != 0;
    GdkRGBA* cursor = NULL;
    XP_U16 playerNum = dsi->playerNum;
    const XP_UCHAR* scoreBuf = dctx->scoreCache[playerNum].str;
    XP_U16 fontHt = dctx->scoreCache[playerNum].fontHt;

    if ( hasCursor ) {
        cursor = &dctx->cursor;
        gtkFillRect( dctx, rOuter, cursor );
    }

    gtkSetForeground( dctx, &dctx->playerColors[playerNum] );

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

        draw_rectangle( dctx, TRUE, selRect.left, selRect.top,
                        selRect.width, selRect.height );
        if ( hasCursor ) {
            gtkFillRect( dctx, rInner, cursor );
        } else {
            gtkEraseRect( dctx, rInner );
        }
    }

/*     XP_U16 fontHt = rInner->height; */
/*     if ( strstr( scoreBuf, "\n" ) ) { */
/*         fontHt >>= 1; */
/*     } */

    draw_string_at( dctx, NULL, scoreBuf, fontHt/*-1*/,
                    rInner, XP_GTK_JUST_CENTER,
                    &dctx->playerColors[playerNum], cursor );

} /* gtk_draw_score_drawPlayer */

#ifdef XWFEATURE_SCOREONEPASS
static XP_Bool
gtk_draw_drawRemText( DrawCtx* p_dctx, XWEnv XP_UNUSED(xwe), XP_S16 nTilesLeft,
                      XP_Bool focussed, XP_Rect* rect )
{
    XP_Bool drawIt = 0 <= nTilesLeft;
    if ( drawIt ) {
        XP_U16 width, height;
        gtkDrawDrawRemText( p_dctx, rect, nTilesLeft, &width, &height, focussed );
        rect->width = width;
        rect->height = height;
        gtkDrawDrawRemText( p_dctx, rect, nTilesLeft, NULL, NULL, focussed );
    }
    return drawIt;
}

static void
gtk_draw_score_drawPlayers( DrawCtx* p_dctx, XWEnv xwe, const XP_Rect* scoreRect,
                            XP_U16 nPlayers, 
                            DrawScoreInfo playerData[], 
                            XP_Rect playerRects[] )
{
    XP_USE( playerData );
    XP_USE( p_dctx );

    XP_U16 ii;
    XP_Rect rect = *scoreRect;
    rect.width /= nPlayers;
    for ( ii = 0; ii < nPlayers; ++ii ) {
        XP_U16 ignoreW, ignoreH;
        XP_Rect innerR;
        gtk_draw_measureScoreText( p_dctx, xwe, &rect, &playerData[ii], &ignoreW,
                                   &ignoreH );

        innerR = rect;
        innerR.left += 4;
        innerR.width -= 8;
        gtk_draw_score_drawPlayer( p_dctx, xwe, &innerR, &rect, 0, &playerData[ii] );

        playerRects[ii] = rect;
        rect.left += rect.width;
    }

}

#else
static XP_Bool
gtk_draw_measureRemText( DrawCtx* p_dctx, XWEnv xwe, const XP_Rect* rect, XP_S16 nTilesLeft,
                         XP_U16* width, XP_U16* height )
{
    XP_Bool drawIt = 0 <= nTilesLeft;
    if ( drawIt ) {
        gtkDrawDrawRemText( p_dctx, xwe, rect, nTilesLeft, width, height, XP_FALSE );
    }
    return drawIt;
} /* gtk_draw_measureRemText */

static void
gtk_draw_drawRemText( DrawCtx* p_dctx, XWEnv xwe, const XP_Rect* rInner,
                      const XP_Rect* XP_UNUSED(rOuter), XP_S16 nTilesLeft,
                      XP_Bool focussed )
{
    gtkDrawDrawRemText( p_dctx, xwe, rInner, nTilesLeft, NULL, NULL, focussed );
} /* gtk_draw_drawRemText */

#endif
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
gtk_draw_measureScoreText( DrawCtx* p_dctx, XWEnv XP_UNUSED(xwe), const XP_Rect* bounds,
                           const DrawScoreInfo* dsi,
                           XP_U16* widthP, XP_U16* heightP )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)(void*)p_dctx;
    XP_UCHAR buf[VSIZE(dctx->scoreCache[0].str)];
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
gtk_draw_score_pendingScore( DrawCtx* p_dctx, XWEnv XP_UNUSED(xwe), const XP_Rect* rect,
                             XP_S16 score, XP_U16 XP_UNUSED(playerNum),
                             XP_Bool curTurn, CellFlags flags )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)(void*)p_dctx;
    XP_UCHAR buf[5];
    XP_U16 ht;
    XP_Rect localR;
    GdkRGBA* cursor = ((flags & CELL_ISCURSOR) != 0) 
        ? &dctx->cursor : NULL;
    GdkRGBA* txtColor;

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
    txtColor = curTurn ? &dctx->black : &dctx->grey;
    draw_string_at( dctx, NULL, "Pts:", ht,
                    &localR, XP_GTK_JUST_TOPLEFT, txtColor, cursor );
    draw_string_at( dctx, NULL, buf, ht,
                    &localR, XP_GTK_JUST_BOTTOMRIGHT, txtColor, cursor );

} /* gtk_draw_score_pendingScore */

static void
gtk_draw_drawTimer( DrawCtx* p_dctx, XWEnv XP_UNUSED(xwe), const XP_Rect* rInner,
                    XP_U16 playerNum, XP_S16 secondsLeft,
                    XP_Bool localTurnDone )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)(void*)p_dctx;
    XP_Bool hadCairo = haveCairo( dctx );
    if ( hadCairo || initCairo( dctx ) ) {
        gtkEraseRect( dctx, rInner );

        GdkRGBA* color = localTurnDone ? &dctx->grey
            : &dctx->playerColors[playerNum];

        gchar buf[16];
        formatTimerText( buf, VSIZE(buf), secondsLeft );

        draw_string_at( dctx, NULL, buf, rInner->height-1,
                        rInner, XP_GTK_JUST_CENTER, color, NULL );
        if ( !hadCairo ) {
            destroyCairo( dctx );
        }
    }
} /* gtk_draw_drawTimer */

#ifdef XWFEATURE_MINIWIN
# define MINI_LINE_HT 12
# define MINI_V_PADDING 6
# define MINI_H_PADDING 8

static void
frameRect( GtkDrawCtx* dctx, const XP_Rect* rect )
{
    draw_rectangle( dctx, DRAW_WHAT(dctx), dctx->drawGC, 
                    FALSE, rect->left, rect->top, 
                    rect->width, rect->height );
} /* frameRect */

static const XP_UCHAR*
gtk_draw_getMiniWText( DrawCtx* XP_UNUSED(p_dctx), XWEnv XP_UNUSED(xwe),
                       XWMiniTextType textHint )
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
gtk_draw_measureMiniWText( DrawCtx* p_dctx, XWEnv XP_UNUSED(xwe), const XP_UCHAR* str,
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
gtk_draw_drawMiniWindow( DrawCtx* p_dctx, XWEnv XP_UNUSED(xwe), const XP_UCHAR* text,
                         const XP_Rect* rect, void** XP_UNUSED(closureP) )
{
    GtkDrawCtx* dctx = (GtkDrawCtx*)p_dctx;
    XP_Rect localR = *rect;

    gtkSetForeground( dctx, &dctx->black );

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
#endif

#define SET_GDK_COLOR( c, r, g, b ) { \
     c.red = (r); \
     c.green = (g); \
     c.blue = (b); \
}

static gdouble
figureColor( int in )
{
    gdouble asDouble = (gdouble)in;
    gdouble result = asDouble / 0xFFFF;
    // XP_LOGF( "%s(%d): asDouble: %lf; result: %lf", __func__, in, asDouble, result );
    XP_ASSERT( result >= 0 && result <= 1.0 );
    return result;
}

static void
allocAndSet( GDKCOLORMAP* map, GdkRGBA* color, unsigned short red,
             unsigned short green, unsigned short blue )

{
#ifdef USE_CAIRO
    XP_USE( map );
    color->red = figureColor(red);
    color->green = figureColor(green);
    color->blue = figureColor(blue);
    color->alpha = 1.0;
#else

    color->red = red;
    color->green = green;
    color->blue = blue;
#ifdef DEBUG
    gboolean success = 
#endif
        gdk_colormap_alloc_color( map,
                                  color,
                                  TRUE, /* writeable */
                                  TRUE ); /* best-match */
    XP_ASSERT( success );
#endif
} /* allocAndSet */

DrawCtx* 
gtkDrawCtxtMake( GtkWidget* drawing_area, GtkGameGlobals* globals )
{
    GtkDrawCtx* dctx = g_malloc0( sizeof(*dctx) );

    size_t tableSize = sizeof(*(((GtkDrawCtx*)dctx)->vtable));
    dctx->vtable = g_malloc( tableSize );
    /* void** ptr = (void**)dctx->vtable; */
    /* void** end = (void**)(((unsigned char*)ptr) + tableSize); */
    /* while ( ptr < end ) { */
    /*     *ptr++ = draw_doNothing; */
    /* } */

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
    SET_VTABLE_ENTRY( dctx->vtable, draw_beginDraw, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_endDraw, gtk );

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

#ifdef XWFEATURE_SCOREONEPASS
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawRemText, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_score_drawPlayers, gtk );
#else
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureRemText, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawRemText, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureScoreText, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_score_drawPlayer, gtk );
#endif
    SET_VTABLE_ENTRY( dctx->vtable, draw_score_pendingScore, gtk );

    SET_VTABLE_ENTRY( dctx->vtable, draw_drawTimer, gtk );

#ifdef XWFEATURE_MINIWIN
    SET_VTABLE_ENTRY( dctx->vtable, draw_getMiniWText, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_measureMiniWText, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_drawMiniWindow, gtk );
#endif

    SET_VTABLE_ENTRY( dctx->vtable, draw_destroyCtxt, gtk );
    SET_VTABLE_ENTRY( dctx->vtable, draw_dictChanged, gtk );
#endif

    assertDrawCallbacksSet( dctx->vtable );

    dctx->drawing_area = drawing_area;
    dctx->globals = globals;

    XP_ASSERT( !!gtk_widget_get_window(drawing_area) );
#ifdef USE_CAIRO
    /* dctx->cairo = gdk_cairo_create( window ); */
    /* XP_LOGF( "dctx->cairo=%p", dctx->cairo ); */
    /* cairo_set_line_width( dctx->cairo, 1.0 ); */
    /* cairo_set_line_cap( dctx->cairo, CAIRO_LINE_CAP_SQUARE ); */
    /* cairo_set_source_rgb( dctx->cairo, 0, 0, 0 ); */
#else
    dctx->drawGC = gdk_gc_new( window );
#endif

    GDKCOLORMAP* map = NULL;
#ifndef USE_CAIRO
    map = gdk_colormap_get_system();
#endif

    allocAndSet( map, &dctx->black, 0x0000, 0x0000, 0x0000 );
    allocAndSet( map, &dctx->grey, 0x7FFF, 0x7FFF, 0x7FFF );
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
addSurface( GtkDrawCtx* dctx, int width, int height )
{
    XP_ASSERT( !dctx->surface );
    dctx->surface = cairo_image_surface_create( CAIRO_FORMAT_RGB24, width, height );
    XP_ASSERT( !!dctx->surface );
}

void
removeSurface( GtkDrawCtx* dctx )
{
    XP_ASSERT( !!dctx->surface );
    cairo_surface_destroy( dctx->surface );
    dctx->surface = NULL;
}

static cairo_status_t
write_func( void *closure, const unsigned char *data,
            unsigned int length )
{
    XWStreamCtxt* stream = (XWStreamCtxt*)closure;
    stream_putBytes( stream, data, length );
    return CAIRO_STATUS_SUCCESS;
}

void
getImage( GtkDrawCtx* dctx, XWStreamCtxt* stream )
{
    LOG_FUNC();
    XP_ASSERT( !!dctx->surface );
#ifdef DEBUG
    cairo_status_t status =
#endif
        cairo_surface_write_to_png_stream( dctx->surface,
                                           write_func, stream );
    XP_ASSERT( CAIRO_STATUS_SUCCESS == status );
}

void
draw_gtk_status( GtkDrawCtx* dctx, char ch )
{
    if ( initCairo( dctx ) ) {
        GtkGameGlobals* globals = dctx->globals;

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

        destroyCairo( dctx );
    }
}

void
frame_active_rect( GtkDrawCtx* dctx, const XP_Rect* rect )
{
    gtkFillRect( dctx, rect, &dctx->grey );
}

#endif /* PLATFORM_GTK */
