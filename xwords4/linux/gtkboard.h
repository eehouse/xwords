/* -*- mode: C; fill-column: 78; c-basic-offset: 4; -*- */ 
/* Copyright 1997 - 2005 by Eric House (xwords@eehouse.org) All rights
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

#ifndef _GTKBOARD_H_
#define _GTKBOARD_H_

#ifdef PLATFORM_GTK
#include <gtk/gtk.h>
#include <sys/time.h>
#include <pango/pango-font.h>
#include <glib.h>

#include "draw.h"
#include "main.h"
#include "game.h"
#include "dictnry.h"

enum {
    LAYOUT_BOARD
    ,LAYOUT_SMALL
    ,LAYOUT_LARGE
    ,LAYOUT_NLAYOUTS
};

#define MAX_SCORE_LEN 31

typedef struct GtkDrawCtx {
    DrawCtxVTable* vtable;

/*     GdkDrawable* pixmap; */
    GtkWidget* drawing_area;
    cairo_surface_t* surface;
    GdkDrawingContext* dc;

    struct GtkGameGlobals* globals;

    cairo_t* _cairo;

    GdkRGBA black;
    GdkRGBA white;
    GdkRGBA grey;
    GdkRGBA red;		/* for pending tiles */
    GdkRGBA tileBack;	/* for pending tiles */
    GdkRGBA cursor;
    GdkRGBA bonusColors[4];
    GdkRGBA playerColors[MAX_NUM_PLAYERS];

	GList* fontsPerSize;

    struct {
        XP_UCHAR str[MAX_SCORE_LEN+1];
        XP_U16 fontHt;
    } scoreCache[MAX_NUM_PLAYERS];
    
    XP_U16 trayOwner;
    XP_U16 cellHeight;
    TileValueType tvType;
    XP_Bool scoreIsVertical;
} GtkDrawCtx;

typedef struct ClientStreamRec {
    XWStreamCtxt* stream;
    guint key;
    int sock;
} ClientStreamRec;

typedef struct _DropTypeData {
    CommsCtxt* comms;
    CommsConnType typ;
} DropTypeData;

typedef struct GtkGameGlobals {
    CommonGlobals cGlobals;
    GtkWidget* window;
    GtkAppGlobals* apg;
/*     GdkPixmap* pixmap; */
    GtkWidget* drawing_area;

    GtkWidget* flip_button;
    GtkWidget* zoomin_button;
    GtkWidget* zoomout_button;
    GtkWidget* toggle_undo_button;
    GtkWidget* prevhint_button;
    GtkWidget* nexthint_button;

    GtkWidget* commit_button;
    GtkWidget* invite_button;
    GtkWidget* buttons_hbox;
#if ! defined XWFEATURE_STANDALONE_ONLY && defined DEBUG
    GtkWidget* drop_checks_vbox;
#endif
#ifdef XWFEATURE_CHAT
    GtkWidget* chat_button;
#endif
    GtkWidget* pause_button;
    GtkWidget* unpause_button;
    GtkWidget* countLabel;

    EngineCtxt* engine;

    GtkAdjustment* adjustment;

    ClientStreamRec clientRecs[MAX_NUM_PLAYERS];
#ifndef XWFEATURE_STANDALONE_ONLY
    XP_U16 netStatLeft, netStatTop;
    XP_UCHAR stateChar;
#endif

    DropTypeData dropData[COMMS_CONN_NTYPES];

    XP_Bool gridOn;
    XP_Bool mouseDown;
    XP_Bool altKeyDown;
    XP_Bool winSizeSet;
    /* save window position */
    GdkEventConfigure lastConfigure;
#ifdef KEYBOARD_NAV
    XP_Bool keyDown;
#endif
} GtkGameGlobals;

/* DictionaryCtxt* gtk_dictionary_make(); */
#define GTK_MIN_SCALE 12		/* was 14 */

#define GTK_MIN_TRAY_SCALEH 24
#define GTK_MIN_TRAY_SCALEV GTK_MIN_TRAY_SCALEH
#define GTK_TRAYPAD_WIDTH 2

#define GTK_TOP_MARGIN 0		/* was 2 */
#define GTK_BOARD_LEFT_MARGIN 2
#define GTK_TRAY_LEFT_MARGIN 2
#define GTK_SCORE_BOARD_PADDING 0

#define GTK_HOR_SCORE_LEFT (GTK_BOARD_LEFT_MARGIN)
#define GTK_HOR_SCORE_HEIGHT 12
#define GTK_TIMER_HEIGHT GTK_HOR_SCORE_HEIGHT
#define GTK_HOR_SCORE_TOP (GTK_TOP_MARGIN)
#define GTK_TIMER_PAD 10
#define GTK_VERT_SCORE_TOP (GTK_TIMER_HEIGHT + GTK_TIMER_PAD)
#define GTK_VERT_SCORE_HEIGHT ((MIN_SCALE*MAX_COLS) - GTK_TIMER_HEIGHT - \
                               GTK_TIMER_PAD)
#define GTK_TIMER_WIDTH 40
#define GTK_NETSTAT_WIDTH 20
#define GTK_TIMER_TOP GTK_HOR_SCORE_TOP
#define GTK_HOR_SCORE_WIDTH ((GTK_MIN_SCALE*20)-GTK_TIMER_PAD)
#define GTK_VERT_SCORE_WIDTH 40

#define GTK_BOARD_TOP (GTK_SCORE_TOP + GTK_SCORE_HEIGHT \
 + GTK_SCORE_BOARD_PADDING )
#define GTK_BOARD_LEFT (GTK_BOARD_LEFT_MARGIN)

#define GTK_TRAY_LEFT GTK_TRAY_LEFT_MARGIN

#define GTK_DIVIDER_WIDTH 5

#define GTK_BOTTOM_MARGIN GTK_TOP_MARGIN
#define GTK_RIGHT_MARGIN GTK_BOARD_LEFT_MARGIN

void initBoardGlobalsGtk( GtkGameGlobals* globals, LaunchParams* params,
                          const CurGameInfo* gi );
void freeGlobals( GtkGameGlobals* globals );
XP_Bool loadGameNoDraw( GtkGameGlobals* globals, LaunchParams* params, 
                        sqlite3_int64 rowid );
void destroy_board_window( GtkWidget* widget, GtkGameGlobals* globals );

GtkWidget* makeAddSubmenu( GtkWidget* menubar, gchar* label );
GtkWidget* createAddItem( GtkWidget* parent, gchar* label,
                          GCallback handlerFunc, gpointer closure );

#endif /* PLATFORM_GTK */

#endif
