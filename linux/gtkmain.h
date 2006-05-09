/* -*- mode: C; fill-column: 78; c-basic-offset: 4; -*- */ 
/* Copyright 1997 - 2005 by Eric House (xwords@eehouse.org) (fixin@peak.org).  All rights reserved.
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

#ifndef _GTKMAIN_H_
#define _GTKMAIN_H_

#ifdef PLATFORM_GTK
#include <gtk/gtk.h>
#include <sys/time.h>
#include <pango/pango-font.h>

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

typedef struct GtkDrawCtx {
    DrawCtxVTable* vtable;

/*     GdkDrawable* pixmap; */
    GtkWidget* drawing_area;
    struct GtkAppGlobals* globals;

    GdkGC* drawGC;

    GdkColor black;
    GdkColor white;
    GdkColor red;		/* for pending tiles */
    GdkColor tileBack;	/* for pending tiles */
    GdkColor bonusColors[4];
    GdkColor playerColors[MAX_NUM_PLAYERS];

    /* new for gtk 2.0 */
    PangoContext* pangoContext;
    PangoFontDescription* fontdesc[LAYOUT_NLAYOUTS];
    PangoLayout* layout[LAYOUT_NLAYOUTS]; 
    
    XP_U16 trayOwner;
} GtkDrawCtx;

typedef struct ClientStreamRec {
    XWStreamCtxt* stream;
    guint key;
    int sock;
} ClientStreamRec;

typedef struct GtkAppGlobals {
    CommonGlobals cGlobals;
    GtkWidget* window;
    GtkDrawCtx* draw;

/*     GdkPixmap* pixmap; */
    GtkWidget* drawing_area;

    EngineCtxt* engine;

    guint idleID;

    struct timeval penTv;		/* for timer */
    XP_U32 penTimerInterval;
    struct timeval scoreTv;		/* for timer */
    XP_U32 scoreTimerInterval;

    GtkAdjustment* adjustment;

    ClientStreamRec clientRecs[MAX_NUM_PLAYERS];

    guint timerSources[NUM_TIMERS_PLUS_ONE - 1];

    CommonPrefs cp;

    XP_Bool gridOn;
    XP_Bool dropIncommingMsgs;
    XP_Bool mouseDown;
} GtkAppGlobals;

/* DictionaryCtxt* gtk_dictionary_make(); */
int gtkmain( XP_Bool isServer, LaunchParams* params, int argc, char *argv[] );

#define NUM_COLS 15
#define NUM_ROWS 15
#define MIN_SCALE 12		/* was 14 */

#define MIN_TRAY_SCALEH 24
#define MIN_TRAY_SCALEV MIN_TRAY_SCALEH
#define GTK_TRAYPAD_WIDTH 2

#define TOP_MARGIN 0		/* was 2 */
#define BOARD_LEFT_MARGIN 2
#define TRAY_LEFT_MARGIN 2
#define SCORE_BOARD_PADDING 0

#define HOR_SCORE_LEFT (BOARD_LEFT_MARGIN)
#define HOR_SCORE_HEIGHT 12
#define TIMER_HEIGHT HOR_SCORE_HEIGHT
#define HOR_SCORE_TOP (TOP_MARGIN)
#define TIMER_PAD 10
#define VERT_SCORE_TOP (TIMER_HEIGHT + TIMER_PAD)
#define VERT_SCORE_HEIGHT ((MIN_SCALE*MAX_COLS) - TIMER_HEIGHT - TIMER_PAD)
#define TIMER_WIDTH 40
#define TIMER_TOP HOR_SCORE_TOP
#define HOR_SCORE_WIDTH ((MIN_SCALE*MAX_COLS)-TIMER_PAD)
#define VERT_SCORE_WIDTH 40

#define BOARD_TOP (SCORE_TOP + SCORE_HEIGHT + SCORE_BOARD_PADDING )
#define BOARD_LEFT (BOARD_LEFT_MARGIN)

#define TRAY_LEFT TRAY_LEFT_MARGIN

#define GTK_DIVIDER_WIDTH 5

#define BOTTOM_MARGIN TOP_MARGIN
#define RIGHT_MARGIN BOARD_LEFT_MARGIN

#endif /* PLATFORM_GTK */

#endif
