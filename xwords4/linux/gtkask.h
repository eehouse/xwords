/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2000-2023 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _GTKASK_H_
#define _GTKASK_H_

#include "gtkboard.h"

/* Returns true for "yes" or "ok" answer, false otherwise.
 */
typedef struct _AskPair {
    const gchar* txt;
    gint result;
} AskPair;

void gtktell( GtkWidget* parent, const gchar *message );

gint gtkask( GtkWidget* parent, const gchar *message, 
             GtkButtonsType buttons, const AskPair* buttxts );
gint gtkask_timeout( GtkWidget* parent, const gchar *message, 
                     GtkButtonsType buttons, const AskPair* buttxts, 
                     uint32_t timeoutMS );
bool gtkask_confirm( GtkWidget* parent, const gchar *message );

/* Put up buttxts as radio buttons/single choice with and OK button to confirm
   and a cancel. That's later; for now just call gtkask() with a ton of
   buttons. */
bool gtkask_radios( GtkWidget* parent, const gchar *message,
                    const AskPair* buttxts, int* chosen );

gchar* gtkask_gettext( GtkWidget* parent, const gchar* message );

#endif
#endif /* PLATFORM_GTK */
