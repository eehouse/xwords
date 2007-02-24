/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 1999 - 2003 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _XWORDS4DEFINES_H_
#define _XWORDS4DEFINES_H_



#define RECOMMENDED_SBAR_WIDTH 7
#define SBAR_MIN 13
#define SBAR_MAX 15
#define SBAR_PAGESIZE 13
#define SBAR_START_VALUE SBAR_MIN

#define TRAY_HEIGHT_LR	21
#define TRAY_BUTTON_HEIGHT_LR 10

#define TRAY_HEIGHT_HR	16
#define TRAY_BUTTON_HEIGHT_HR 8
#define TRAY_BUTTON_WIDTH 9

#define FLIP_BUTTON_WIDTH 8
#define FLIP_BUTTON_HEIGHT FLIP_BUTTON_WIDTH
#define BOARD_TOP 8

#define TRAY_BUTTONS_Y_LR    (160-TRAY_HEIGHT_LR)
#define TRAY_BUTTONS_Y_HR    (160-TRAY_HEIGHT_HR)
#define SHOWTRAY_BUTTON_Y (160-FLIP_BUTTON_HEIGHT-5)

#define IR_STATUS_HEIGHT 12

#define PALM_FLIP_LEFT 160-FLIP_BUTTON_WIDTH
#define PALM_TRAY_BUTTON_LEFT 143

#define XW_MAIN_FORM 1000
#define XW_NEWGAMES_FORM 1001
#define XW_ERROR_ALERT_ID 1002
#define XW_DICTINFO_FORM 1003
#define XW_ASK_FORM_ID 1004
#define XW_PASSWORD_DIALOG_ID 1005
#define XW_BLANK_DIALOG_ID 1006
#define XW_COLORPREF_DIALOG_ID 1007
#define XW_PREFS_FORM 1008
#define XW_SAVEDGAMES_DIALOG_ID 1009
#define XW_HINTCONFIG_FORM_ID 1010
#define XW_CONNS_FORM 1011
#ifdef FOR_GREMLINS
# define XW_GREMLIN_WARN_FORM_ID 1012
#endif

#define XW_ASK_MENU_ID 1001
#define ASK_COPY_PULLDOWN_ID 1000
#define ASK_SELECTALL_PULLDOWN_ID 1001

#define XW_MAIN_MENU_ID 1000
#define XW_MAIN_FLIP_BUTTON_ID 1016
#define XW_MAIN_VALUE_BUTTON_ID 1017
#define XW_MAIN_TRAY_BUTTON_ID 1018
#define XW_MAIN_SCROLLBAR_ID 1019
#ifndef EIGHT_TILES
#define XW_MAIN_DONE_BUTTON_ID 1020
#define XW_MAIN_JUGGLE_BUTTON_ID 1021
#define XW_MAIN_TRADE_BUTTON_ID 1022
#define XW_MAIN_HIDE_BUTTON_ID 1023
#endif
#define XW_MAIN_HINT_BUTTON_ID 1024
#define XW_MAIN_SHOWTRAY_BUTTON_ID 1026
//#define XW_MAIN_OK_BUTTON_ID 1026

#ifdef XWFEATURE_BLUETOOTH
# define XW_BTSTATUS_GADGET_ID 1099 /* change later */
#endif

#ifdef FOR_GREMLINS
# define GREMLIN_BOARD_GADGET_IDAUTOID 1027
# define GREMLIN_TRAY_GADGET_IDAUTOID 1028
#endif

/* File menu */
#define XW_NEWGAME_PULLDOWN_ID 1050
#define XW_SAVEDGAMES_PULLDOWN_ID 1051
#define XW_BEAMDICT_PULLDOWN_ID 1052
#define XW_BEAMBOARD_PULLDOWN_ID 1053
#define XW_PREFS_PULLDOWN_ID 1054
#define XW_ABOUT_PULLDOWN_ID 1055

/* Game menu */
#define XW_TILEVALUES_PULLDOWN_ID   1056
#define XW_TILESLEFT_PULLDOWN_ID    1057
#define XW_PASSWORDS_PULLDOWN_ID    1058
#define XW_HISTORY_PULLDOWN_ID      1059
#define XW_FINISH_PULLDOWN_ID       1060
#ifndef XWFEATURE_STANDALONE_ONLY
# define XW_RESENDIR_PULLDOWN_ID    1061
#endif

/* Move menu */
#define XW_HINT_PULLDOWN_ID         1062
#define XW_NEXTHINT_PULLDOWN_ID     1063
#ifdef XWFEATURE_SEARCHLIMIT
# define XW_HINTCONFIG_PULLDOWN_ID  1064
#endif
#define XW_UNDOCUR_PULLDOWN_ID      1065
#define XW_UNDOLAST_PULLDOWN_ID     1066
#define XW_DONE_PULLDOWN_ID         1067
#define XW_JUGGLE_PULLDOWN_ID       1068
#define XW_TRADEIN_PULLDOWN_ID      1069
#define XW_HIDESHOWTRAY_PULLDOWN_ID 1070

#ifdef FEATURE_DUALCHOOSE
# define XW_RUN68K_PULLDOWN_ID      1071
# define XW_RUNARM_PULLDOWN_ID      1072
#endif

/* debug menu */
#ifdef DEBUG
# define XW_LOGFILE_PULLDOWN_ID 2000
# define XW_LOGMEMO_PULLDOWN_ID 2001
# define XW_CLEARLOGS_PULLDOWN_ID 2002
# define XW_NETSTATS_PULLDOWN_ID 2003
# define XW_MEMSTATS_PULLDOWN_ID 2004
# define XW_BTSTATS_PULLDOWN_ID  2005
#endif

#ifdef FOR_GREMLINS
#define XW_GREMLIN_DIVIDER_RIGHT 2010
#define XW_GREMLIN_DIVIDER_LEFT 2011
#endif

#define XW_DICT_SELECTOR_ID 1038
#define XW_OK_BUTTON_ID 1039
#define XW_CANCEL_BUTTON_ID 1040
/* #define XW_DICT_BUTTON_ID 1040 */

#define MAX_GAMENAME_LENGTH 32
#define MAX_PLAYERNAME_LENGTH 32

#define NUM_PLAYER_COLS 4	/* name, local, robot and passwd */
#define PALM_MAX_ROWS 15	/* max is a 15x15 grid on palm */
#define PALM_MAX_COLS 15	/* max is a 15x15 grid on palm */

#define NUM_BOARD_SIZES 3	/* 15x15, 13x13 and 11x11 */

#define XW_PLAYERNAME_1_FIELD_ID 2100
#define XW_ROBOT_1_CHECKBOX_ID 2101
#define XW_REMOTE_1_CHECKBOX_ID 2102
#define XW_PLAYERPASSWD_1_TRIGGER_ID 2103

#define XW_PLAYERNAME_2_FIELD_ID 2104
#define XW_ROBOT_2_CHECKBOX_ID 2105
#define XW_REMOTE_2_CHECKBOX_ID 2106
#define XW_PLAYERPASSWD_2_TRIGGER_ID 2107

#define XW_PLAYERNAME_3_FIELD_ID 2108
#define XW_ROBOT_3_CHECKBOX_ID 2109
#define XW_REMOTE_3_CHECKBOX_ID 2110
#define XW_PLAYERPASSWD_3_TRIGGER_ID 2111

#define XW_PLAYERNAME_4_FIELD_ID 2112
#define XW_ROBOT_4_CHECKBOX_ID 2113
#define XW_REMOTE_4_CHECKBOX_ID 2114
#define XW_PLAYERPASSWD_4_TRIGGER_ID 2115

#define XW_NPLAYERS_LIST_ID 2121
#define XW_NPLAYERS_SELECTOR_ID 2122
#define XW_PREFS_BUTTON_ID 2123
#define XW_GINFO_JUGGLE_ID 2124

#ifndef XWFEATURE_STANDALONE_ONLY
#define XW_SOLO_GADGET_ID 2125
#define XW_SERVER_GADGET_ID 2126
#define XW_CLIENT_GADGET_ID 2127
#define XW_SERVERTYPES_LIST_ID 2128
#endif

#ifdef FOR_GREMLINS
# define XW_GREMLIN_WARN_FIELD_ID 2129
#endif


/* we need to hide these labels, so no AUTOID */
#ifndef XWFEATURE_STANDALONE_ONLY
# define XW_LOCAL_LABEL_ID 2130
# define XW_TOTALP_FIELD_ID 2131
# define XW_LOCALP_LABEL_ID 2132
#endif

#define REFCON_GADGET_ID 3000

#define XW_ASK_TXT_FIELD_ID 2200
#define XW_ASK_YES_BUTTON_ID 2201
#define XW_ASK_NO_BUTTON_ID 2202
#define XW_ASK_SCROLLBAR_ID 2203

#define MAX_PASSWORD_LENGTH 4	/* server.c has no limit */
#define XW_PASSWORD_CANCEL_BUTTON 2300
#define XW_PASSWORD_NAME_LABEL 2301
#define XW_PASSWORD_NEWNAME_LABEL 2302
#define XW_PASSWORD_NAME_FIELD 2303
#define XW_PASSWORD_PASS_FIELD 2304
#define XW_PASSWORD_OK_BUTTON 2305

#define XW_BLANK_LIST_ID 2401
#define XW_BLANK_LABEL_FIELD_ID 2402
#define XW_BLANK_OK_BUTTON_ID 2403
#define XW_BLANK_PICK_BUTTON_ID 2404
#define XW_BLANK_BACKUP_BUTTON_ID 2405

#define XW_COLORS_FACTORY_BUTTON_ID 2520
#define XW_COLORS_OK_BUTTON_ID 2521
#define XW_COLORS_CANCEL_BUTTON_ID 2522

#define STRL_RES_TYPE 'StrL'
#define XW_STRL_RESOURCE_ID 1000

#define BOARD_RES_TYPE 'Xbrd'
#define BOARD_RES_ID 1000

#define COLORS_RES_TYPE 'Clrs'
#define COLORS_RES_ID 1000

#define CARD_0 0
#ifdef DEBUG
# define XW_GAMES_DBNAME "xw4games_dbg"
# define XWORDS_GAMES_TYPE 'Xwdg'
# define XW_PREFS_DBNAME "xw4prefs_dbg"
# define XWORDS_PREFS_TYPE 'Xwpd'
#else
# define XW_GAMES_DBNAME "xw4games"
# define XWORDS_GAMES_TYPE 'Xwgm'
# define XW_PREFS_DBNAME "xw4prefs"
# define XWORDS_PREFS_TYPE 'Xwpr'
#endif

#define XW_DICTINFO_LIST_ID 2601
#define XW_DICTINFO_TRIGGER_ID 2602
#define XW_PHONIES_TRIGGER_ID 2603
#define XW_PHONIES_LABLE_ID 2604
#define XW_PHONIES_LIST_ID 2605
#define XW_DICTINFO_DONE_BUTTON_ID 2606
#define XW_DICTINFO_BEAM_BUTTON_ID 2607
#define XW_DICTINFO_CANCEL_BUTTON_ID 2608

/* 
 * prefs dialog
 */
#define XW_PREFS_ALLGAMES_GADGET_ID 2700
#define XW_PREFS_ONEGAME_GADGET_ID 2701
#define XW_PREFS_TYPES_LIST_ID 2702

/* global */
#define XW_PREFS_PLAYERCOLORS_CHECKBOX_ID 2708
#define XW_PREFS_PROGRESSBAR_CHECKBOX_ID 2709
#define XW_PREFS_SHOWGRID_CHECKBOX_ID 2710
#define XW_PREFS_SHOWARROW_CHECKBOX_ID 2711
#define XW_PREFS_ROBOTSCORE_CHECKBOX_ID 2712
#define XW_PREFS_HIDETRAYVAL_CHECKBOX_ID 2713

/* per-game */
#define XW_PREFS_ROBOTSMART_CHECKBOX_ID 2715
#define XW_PREFS_PHONIES_LABEL_ID 2716
#define XW_PREFS_PHONIES_TRIGGER_ID 2717
#define XW_PREFS_BDSIZE_LABEL_ID 2718
#define XW_PREFS_BDSIZE_SELECTOR_ID 2719
#define XW_PREFS_NOHINTS_CHECKBOX_ID 2720
#define XW_PREFS_TIMERON_CHECKBOX_ID 2721
#define XW_PREFS_TIMER_FIELD_ID 2722

#ifdef FEATURE_TRAY_EDIT
# define XW_PREFS_PICKTILES_CHECKBOX_ID 2723
# ifdef XWFEATURE_SEARCHLIMIT
# define XW_PREFS_HINTRECT_CHECKBOX_ID 2724
# endif
#else
# ifdef XWFEATURE_SEARCHLIMIT
# define XW_PREFS_HINTRECT_CHECKBOX_ID 2723
# endif
#endif


#ifdef XWFEATURE_FIVEWAY
/* These should be in same order as BoardObjectType */
# define XW_BOARD_GADGET_ID 3001
# define XW_SCOREBOARD_GADGET_ID 3002
# define XW_TRAY_GADGET_ID 3003
#endif


/* These aren't part of the hide/show thing as they're displayed only
 * explicitly byother controls */
#define XW_PREFS_PHONIES_LIST_ID 2750
#define XW_PREFS_BDSIZE_LIST_ID 2751

/* These are used to set/clear the "pages" of the prefs dialog. */
#define XW_PREFS_FIRST_GLOBAL_ID XW_PREFS_PLAYERCOLORS_CHECKBOX_ID
#define XW_PREFS_LAST_GLOBAL_ID XW_PREFS_HIDETRAYVAL_CHECKBOX_ID
#define XW_PREFS_FIRST_PERGAME_ID XW_PREFS_ROBOTSMART_CHECKBOX_ID

#if defined XWFEATURE_SEARCHLIMIT
# define XW_PREFS_LAST_PERGAME_ID XW_PREFS_HINTRECT_CHECKBOX_ID
#elif defined FEATURE_TRAY_EDIT
# define XW_PREFS_LAST_PERGAME_ID XW_PREFS_PICKTILES_CHECKBOX_ID
#else
# define XW_PREFS_LAST_PERGAME_ID XW_PREFS_TIMER_FIELD_ID
#endif

#define XW_PREFS_CANCEL_BUTTON_ID 2725
#define XW_PREFS_OK_BUTTON_ID 2726

/*
 * saved games dialog
 */
#define XW_SAVEDGAMES_LIST_ID     2800
#define XW_SAVEDGAMES_NAME_FIELD  2801
#define XW_SAVEDGAMES_USE_BUTTON  2802
#define XW_SAVEDGAMES_DUPE_BUTTON 2803
#define XW_SAVEDGAMES_DELETE_BUTTON 2804
#define XW_SAVEDGAMES_OPEN_BUTTON 2805
#define XW_SAVEDGAMES_DONE_BUTTON 2806
#define MAX_GAME_NAME_LENGTH 31

/*
 * Connections dlg (XW_CONNS_FORM)
 */
#define XW_CONNS_CANCEL_BUTTON_ID 2900
#define XW_CONNS_OK_BUTTON_ID     2901
#define XW_CONNS_TYPE_TRIGGER_ID  2902
#define XW_CONNS_TYPE_LIST_ID     2903
#define XW_CONNS_RELAY_LABEL_ID   2904
#ifdef XWFEATURE_RELAY
# define XW_CONNS_COOKIE_FIELD_ID  2905
# define XW_CONNS_COOKIE_LABEL_ID  2906
# define XW_CONNS_PORT_LABEL_ID    2907
# define XW_CONNS_RELAY_FIELD_ID   2908
# define XW_CONNS_PORT_FIELD_ID    2909
#endif

#define XW_CONNS_BT_NOTSUPPORT_LABEL_ID 2910
#ifdef XWFEATURE_BLUETOOTH
# define XW_CONNS_BT_HOSTNAME_LABEL_ID   2911
# define XW_CONNS_BT_HOSTTRIGGER_ID      2912
#endif

/*
 * selector for number of tiles during hint
 */
#define XW_HINTCONFIG_MINLIST_ID 2950
#define XW_HINTCONFIG_MAXLIST_ID 2951
#define XW_HINTCONFIG_MAXSELECTOR_ID 2952
#define XW_HINTCONFIG_MINSELECTOR_ID 2953
#define XW_HINTCONFIG_OK_ID 2954
#define XW_HINTCONFIG_CANCEL_ID 2955

#define PALM_BOARD_TOP 8
#define PALM_GRIDLESS_BOARD_TOP 2

#define PALM_BOARD_SCALE 10
#define PALM_SCORE_LEFT 0
#define PALM_SCORE_TOP 0
#define PALM_SCORE_HEIGHT BOARD_TOP

#define PALM_GRIDLESS_SCORE_WIDTH 22
#define PALM_GRIDLESS_SCORE_LEFT (160-PALM_GRIDLESS_SCORE_WIDTH)
#define PALM_GRIDLESS_SCORE_TOP 42

#define PALM_TIMER_TOP    0
#define PALM_TIMER_HEIGHT PALM_SCORE_HEIGHT

/* #define PALM_TRAY_LEFT 0 */
#define PALM_TRAY_TOP (160-PALM_TRAY_SCALEV-1)
#define PALM_TRAY_TOP_MAX 144	/* the lowest we can put the top */
#define PALM_TRAY_WIDTH 143
#ifdef EIGHT_TILES
#define PALM_TRAY_SCALEV 18
#else
#define PALM_TRAY_SCALEV 20
#endif
#define PALM_DIVIDER_WIDTH 3

#define PALM_BOARD_LEFT_LH 9
#define PALM_BOARD_LEFT_RH 0
#define PALM_TRAY_LEFT_LH 17
#define PALM_TRAY_LEFT_RH 0


/* resource IDs */
#define DOWN_ARROW_RESID 1001
#define RIGHT_ARROW_RESID 1002
#define FLIP_BUTTON_BMP_RES_ID 1003
#define VALUE_BUTTON_BMP_RES_ID 1004
#define HINT_BUTTON_BMP_RES_ID 1005
#define TRAY_BUTTONS_BMP_RES_ID 1007
#define SHOWTRAY_BUTTON_BMP_RES_ID 1008
#define STAR_BMP_RES_ID 1009

#ifdef XWFEATURE_BLUETOOTH
# define BTSTATUS_NONE_RES_ID 1010
# define BTSTATUS_LISTENING_RES_ID 1011
# define BTSTATUS_SEEKING_RES_ID 1012
# define BTSTATUS_CONNECTED_RES_ID 1013
#endif

#define STRL_RES_TYPE 'StrL'
#define STRL_RES_ID 0x03e8

#if 0
# define DMFINDDATABASE(g,c,n) DmFindDatabase( (c),(n) )
# define DMOPENDATABASE(g,c,i,m) DmOpenDatabase( (c),(i),(m))
# define DMCLOSEDATABASE(d) DmCloseDatabase( d )
#else
# define DMFINDDATABASE(g,c,n) (g)->gamesDBID
# define DMOPENDATABASE(g,c,i,m) (g)->gamesDBP
# define DMCLOSEDATABASE(d)
#endif

#define	kFrmNavHeaderFlagsObjectFocusStartState  0x00000001
#define	kFrmNavHeaderFlagsAppFocusStartState     0x00000002

/* versioning stuff */
#ifdef XWFEATURE_BLUETOOTH
# define XW_PALM_VERSION_STRING "4.3a6"
#else
# define XW_PALM_VERSION_STRING "4.2b6"
#endif
#define CUR_PREFS_VERS 0x0405



#endif
