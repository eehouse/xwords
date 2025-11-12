/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2000 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _GTKDRAW_H_
#define _GTKDRAW_H_

#include "draw.h"

#define THUMB_WIDTH 150
#define THUMB_HEIGHT 150

DrawCtx* gtkDrawCtxtMake( GtkWidget *widget, GtkGameGlobals* globals,
                          DrawTarget dt );
void draw_gtk_status( GtkDrawCtx* draw, char ch );
void frame_active_rect( GtkDrawCtx* dctx, const XP_Rect* rect );
void addSurface( DrawCtx* dctx, int width, int height );
XP_Bool gtk_draw_does_offscreen(GtkDrawCtx* draw);
cairo_surface_t* gtk_draw_get_surface( GtkDrawCtx* draw );

#endif
#endif
