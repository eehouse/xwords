/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2006 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifdef DEBUG

#include "palmdbg.h"
#include "palmmain.h"
#include "xwords4defines.h"

#define CASESTR(s) case s: return #s

#define FUNC(f) #f

const char* 
frmObjId_2str( XP_U16 id )
{
    switch( id ) {
        CASESTR( XW_MAIN_FORM );
        CASESTR( XW_NEWGAMES_FORM );
        CASESTR( XW_ERROR_ALERT_ID );
        CASESTR( XW_DICTINFO_FORM );
        CASESTR( XW_ASK_FORM_ID );
        CASESTR( XW_PASSWORD_DIALOG_ID );
        CASESTR( XW_BLANK_DIALOG_ID );
        CASESTR( XW_COLORPREF_DIALOG_ID );
        CASESTR( XW_PREFS_FORM );
        CASESTR( XW_SAVEDGAMES_DIALOG_ID );
        CASESTR( XW_HINTCONFIG_FORM_ID );
        CASESTR( XW_CONNS_FORM );
#ifdef FOR_GREMLINS
        CASESTR( XW_GREMLIN_WARN_FORM_ID );
        CASESTR( GREMLIN_BOARD_GADGET_IDAUTOID );
        CASESTR( GREMLIN_TRAY_GADGET_IDAUTOID );
        CASESTR( XW_GREMLIN_DIVIDER_RIGHT );
        CASESTR( XW_GREMLIN_DIVIDER_LEFT );
        CASESTR( XW_GREMLIN_WARN_FIELD_ID );
#endif
/*         CASESTR( XW_ASK_MENU_ID ); */
/*         CASESTR( ASK_COPY_PULLDOWN_ID ); */
/*         CASESTR( ASK_SELECTALL_PULLDOWN_ID ); */
/*         CASESTR( XW_MAIN_MENU_ID ); */
        CASESTR( XW_MAIN_FLIP_BUTTON_ID );
        CASESTR( XW_MAIN_VALUE_BUTTON_ID );
        CASESTR( XW_MAIN_TRAY_BUTTON_ID );
        CASESTR( XW_MAIN_SCROLLBAR_ID );
        CASESTR( XW_MAIN_DONE_BUTTON_ID );
        CASESTR( XW_MAIN_JUGGLE_BUTTON_ID );
        CASESTR( XW_MAIN_TRADE_BUTTON_ID );
        CASESTR( XW_MAIN_HIDE_BUTTON_ID );
        CASESTR( XW_MAIN_HINT_BUTTON_ID );
        CASESTR( XW_MAIN_SHOWTRAY_BUTTON_ID );
#ifdef FOR_GREMLINS
#endif
        CASESTR( XW_NEWGAME_PULLDOWN_ID );
        CASESTR( XW_SAVEDGAMES_PULLDOWN_ID );
        CASESTR( XW_BEAMDICT_PULLDOWN_ID );
        CASESTR( XW_BEAMBOARD_PULLDOWN_ID );
        CASESTR( XW_PREFS_PULLDOWN_ID );
        CASESTR( XW_ABOUT_PULLDOWN_ID );
        CASESTR( XW_TILEVALUES_PULLDOWN_ID   );
        CASESTR( XW_TILESLEFT_PULLDOWN_ID    );
        CASESTR( XW_PASSWORDS_PULLDOWN_ID    );
        CASESTR( XW_HISTORY_PULLDOWN_ID      );
        CASESTR( XW_FINISH_PULLDOWN_ID       );
        CASESTR( XW_RESENDIR_PULLDOWN_ID    );
        CASESTR( XW_HINT_PULLDOWN_ID         );
        CASESTR( XW_NEXTHINT_PULLDOWN_ID     );
        CASESTR( XW_HINTCONFIG_PULLDOWN_ID  );
        CASESTR( XW_UNDOCUR_PULLDOWN_ID      );
        CASESTR( XW_UNDOLAST_PULLDOWN_ID     );
        CASESTR( XW_DONE_PULLDOWN_ID         );
        CASESTR( XW_JUGGLE_PULLDOWN_ID       );
        CASESTR( XW_TRADEIN_PULLDOWN_ID      );
        CASESTR( XW_HIDESHOWTRAY_PULLDOWN_ID );
#ifdef FEATURE_DUALCHOOSE
        CASESTR( XW_RUN68K_PULLDOWN_ID      );
        CASESTR( XW_RUNARM_PULLDOWN_ID      );
#endif
        CASESTR( XW_LOGFILE_PULLDOWN_ID );
        CASESTR( XW_LOGMEMO_PULLDOWN_ID );
        CASESTR( XW_CLEARLOGS_PULLDOWN_ID );
        CASESTR( XW_NETSTATS_PULLDOWN_ID );
        CASESTR( XW_MEMSTATS_PULLDOWN_ID );
        CASESTR( XW_BTSTATS_PULLDOWN_ID  );
        CASESTR( XW_DICT_SELECTOR_ID );
        CASESTR( XW_OK_BUTTON_ID );
        CASESTR( XW_CANCEL_BUTTON_ID );
        CASESTR( XW_PLAYERNAME_1_FIELD_ID );
        CASESTR( XW_ROBOT_1_CHECKBOX_ID );
        CASESTR( XW_REMOTE_1_CHECKBOX_ID );
        CASESTR( XW_PLAYERPASSWD_1_TRIGGER_ID );
        CASESTR( XW_PLAYERNAME_2_FIELD_ID );
        CASESTR( XW_ROBOT_2_CHECKBOX_ID );
        CASESTR( XW_REMOTE_2_CHECKBOX_ID );
        CASESTR( XW_PLAYERPASSWD_2_TRIGGER_ID );
        CASESTR( XW_PLAYERNAME_3_FIELD_ID );
        CASESTR( XW_ROBOT_3_CHECKBOX_ID );
        CASESTR( XW_REMOTE_3_CHECKBOX_ID );
        CASESTR( XW_PLAYERPASSWD_3_TRIGGER_ID );
        CASESTR( XW_PLAYERNAME_4_FIELD_ID );
        CASESTR( XW_ROBOT_4_CHECKBOX_ID );
        CASESTR( XW_REMOTE_4_CHECKBOX_ID );
        CASESTR( XW_PLAYERPASSWD_4_TRIGGER_ID );
        CASESTR( XW_NPLAYERS_LIST_ID );
        CASESTR( XW_NPLAYERS_SELECTOR_ID );
        CASESTR( XW_PREFS_BUTTON_ID );
        CASESTR( XW_GINFO_JUGGLE_ID );
        CASESTR( XW_SOLO_GADGET_ID );
        CASESTR( XW_SERVER_GADGET_ID );
        CASESTR( XW_CLIENT_GADGET_ID );
        CASESTR( XW_SERVERTYPES_LIST_ID );
        CASESTR( XW_LOCAL_LABEL_ID );
        CASESTR( XW_TOTALP_FIELD_ID );
        CASESTR( XW_LOCALP_LABEL_ID );
        CASESTR( REFCON_GADGET_ID );
        CASESTR( XW_ASK_TXT_FIELD_ID );
        CASESTR( XW_ASK_YES_BUTTON_ID );
        CASESTR( XW_ASK_NO_BUTTON_ID );
        CASESTR( XW_ASK_SCROLLBAR_ID );
        CASESTR( XW_PASSWORD_CANCEL_BUTTON );
        CASESTR( XW_PASSWORD_NAME_LABEL );
        CASESTR( XW_PASSWORD_NEWNAME_LABEL );
        CASESTR( XW_PASSWORD_NAME_FIELD );
        CASESTR( XW_PASSWORD_PASS_FIELD );
        CASESTR( XW_PASSWORD_OK_BUTTON );
        CASESTR( XW_BLANK_LIST_ID );
        CASESTR( XW_BLANK_LABEL_FIELD_ID );
        CASESTR( XW_BLANK_OK_BUTTON_ID );
        CASESTR( XW_BLANK_PICK_BUTTON_ID );
        CASESTR( XW_BLANK_BACKUP_BUTTON_ID );
        CASESTR( XW_COLORS_FACTORY_BUTTON_ID );
        CASESTR( XW_COLORS_OK_BUTTON_ID );
        CASESTR( XW_COLORS_CANCEL_BUTTON_ID );
        CASESTR( XW_DICTINFO_LIST_ID );
        CASESTR( XW_DICTINFO_TRIGGER_ID );
        CASESTR( XW_PHONIES_TRIGGER_ID );
        CASESTR( XW_PHONIES_LABLE_ID );
        CASESTR( XW_PHONIES_LIST_ID );
        CASESTR( XW_DICTINFO_DONE_BUTTON_ID );
        CASESTR( XW_DICTINFO_BEAM_BUTTON_ID );
        CASESTR( XW_DICTINFO_CANCEL_BUTTON_ID );
        CASESTR( XW_PREFS_ALLGAMES_GADGET_ID );
        CASESTR( XW_PREFS_ONEGAME_GADGET_ID );
        CASESTR( XW_PREFS_TYPES_LIST_ID );
        CASESTR( XW_PREFS_PLAYERCOLORS_CHECKBOX_ID );
        CASESTR( XW_PREFS_PROGRESSBAR_CHECKBOX_ID );
        CASESTR( XW_PREFS_SHOWGRID_CHECKBOX_ID );
        CASESTR( XW_PREFS_SHOWARROW_CHECKBOX_ID );
        CASESTR( XW_PREFS_ROBOTSCORE_CHECKBOX_ID );
        CASESTR( XW_PREFS_HIDETRAYVAL_CHECKBOX_ID );
        CASESTR( XW_PREFS_ROBOTSMART_CHECKBOX_ID );
        CASESTR( XW_PREFS_PHONIES_LABEL_ID );
        CASESTR( XW_PREFS_PHONIES_TRIGGER_ID );
        CASESTR( XW_PREFS_BDSIZE_LABEL_ID );
        CASESTR( XW_PREFS_BDSIZE_SELECTOR_ID );
        CASESTR( XW_PREFS_NOHINTS_CHECKBOX_ID );
        CASESTR( XW_PREFS_TIMERON_CHECKBOX_ID );
        CASESTR( XW_PREFS_TIMER_FIELD_ID );
        CASESTR( XW_PREFS_PICKTILES_CHECKBOX_ID );
        CASESTR( XW_PREFS_HINTRECT_CHECKBOX_ID );
#ifdef XWFEATURE_FIVEWAY
        CASESTR( XW_BOARD_GADGET_ID );
        CASESTR( XW_SCOREBOARD_GADGET_ID );
        CASESTR( XW_TRAY_GADGET_ID );
#endif
        CASESTR( XW_PREFS_PHONIES_LIST_ID );
        CASESTR( XW_PREFS_BDSIZE_LIST_ID );
        CASESTR( XW_PREFS_CANCEL_BUTTON_ID );
        CASESTR( XW_PREFS_OK_BUTTON_ID );
        CASESTR( XW_SAVEDGAMES_LIST_ID     );
        CASESTR( XW_SAVEDGAMES_NAME_FIELD  );
        CASESTR( XW_SAVEDGAMES_USE_BUTTON  );
        CASESTR( XW_SAVEDGAMES_DUPE_BUTTON );
        CASESTR( XW_SAVEDGAMES_DELETE_BUTTON );
        CASESTR( XW_SAVEDGAMES_OPEN_BUTTON );
        CASESTR( XW_SAVEDGAMES_DONE_BUTTON );
        CASESTR( XW_CONNS_CANCEL_BUTTON_ID );
        CASESTR( XW_CONNS_OK_BUTTON_ID     );
        CASESTR( XW_CONNS_TYPE_TRIGGER_ID  );
        CASESTR( XW_CONNS_TYPE_LIST_ID     );
#ifdef XWFEATURE_RELAY
        CASESTR( XW_CONNS_RELAY_LABEL_ID   );
        CASESTR( XW_CONNS_INVITE_FIELD_ID  );
        CASESTR( XW_CONNS_INVITE_LABEL_ID  );
        CASESTR( XW_CONNS_PORT_LABEL_ID    );
        CASESTR( XW_CONNS_RELAY_FIELD_ID   );
        CASESTR( XW_CONNS_PORT_FIELD_ID    );
#endif
#ifdef XWFEATURE_BLUETOOTH
        CASESTR( XW_CONNS_BT_HOSTNAME_LABEL_ID   );
        CASESTR( XW_CONNS_BT_HOSTTRIGGER_ID      );
#endif
        CASESTR( XW_HINTCONFIG_MINLIST_ID );
        CASESTR( XW_HINTCONFIG_MAXLIST_ID );
        CASESTR( XW_HINTCONFIG_MAXSELECTOR_ID );
        CASESTR( XW_HINTCONFIG_MINSELECTOR_ID );
        CASESTR( XW_HINTCONFIG_OK_ID );
        CASESTR( XW_HINTCONFIG_CANCEL_ID );
    default: return FUNC(__func__) " unknown";
    }
}

const char* 
eType_2str( eventsEnum eType )
{
    switch( eType ) {
        CASESTR(nilEvent);
        CASESTR(penDownEvent);
        CASESTR(penUpEvent);
        CASESTR(penMoveEvent);
        CASESTR(keyDownEvent);
        CASESTR(winEnterEvent);
        CASESTR(winExitEvent);
        CASESTR(ctlEnterEvent);
        CASESTR(ctlExitEvent);
        CASESTR(ctlSelectEvent);
        CASESTR(ctlRepeatEvent);
        CASESTR(lstEnterEvent);
        CASESTR(lstSelectEvent);
        CASESTR(lstExitEvent);
        CASESTR(popSelectEvent);
        CASESTR(fldEnterEvent);
        CASESTR(fldHeightChangedEvent);
        CASESTR(fldChangedEvent);
        CASESTR(tblEnterEvent);
        CASESTR(tblSelectEvent);
        CASESTR(daySelectEvent);
        CASESTR(menuEvent);
        CASESTR(appStopEvent);
        CASESTR(frmLoadEvent);
        CASESTR(frmOpenEvent);
        CASESTR(frmGotoEvent);
        CASESTR(frmUpdateEvent);
        CASESTR(frmSaveEvent);
        CASESTR(frmCloseEvent);
        CASESTR(frmTitleEnterEvent);
        CASESTR(frmTitleSelectEvent);
        CASESTR(tblExitEvent);
        CASESTR(sclEnterEvent);
        CASESTR(sclExitEvent);
        CASESTR(sclRepeatEvent);
        CASESTR(tsmConfirmEvent);
        CASESTR(tsmFepButtonEvent);
        CASESTR(tsmFepModeEvent);
        CASESTR(attnIndicatorEnterEvent);
        CASESTR(attnIndicatorSelectEvent);
        CASESTR(menuCmdBarOpenEvent);
        CASESTR(menuOpenEvent);
        CASESTR(menuCloseEvent);
        CASESTR(frmGadgetEnterEvent);
        CASESTR(frmGadgetMiscEvent);

        CASESTR(firstINetLibEvent);
        CASESTR(firstWebLibEvent);
        CASESTR(telAsyncReplyEvent); 

        CASESTR(keyUpEvent);
        CASESTR(keyHoldEvent);
        CASESTR(frmObjectFocusTakeEvent);
        CASESTR(frmObjectFocusLostEvent);

        CASESTR(firstLicenseeEvent);
        CASESTR(lastLicenseeEvent);

        CASESTR(lastUserEvent);

        CASESTR( dictSelectedEvent );
        CASESTR( newGameOkEvent );
        CASESTR( newGameCancelEvent);
        CASESTR( loadGameEvent);
        CASESTR( prefsChangedEvent);
        CASESTR( openSavedGameEvent);
#ifdef XWFEATURE_FIVEWAY
        CASESTR( updateAfterFocusEvent);
#endif
        CASESTR( DOWN_ARROW_RESID );
        CASESTR( RIGHT_ARROW_RESID );
        CASESTR( FLIP_BUTTON_BMP_RES_ID );
        CASESTR( VALUE_BUTTON_BMP_RES_ID );
        CASESTR( HINT_BUTTON_BMP_RES_ID );
        CASESTR( TRAY_BUTTONS_BMP_RES_ID );
        CASESTR( SHOWTRAY_BUTTON_BMP_RES_ID );
        CASESTR( STAR_BMP_RES_ID );

#ifdef XWFEATURE_BLUETOOTH
        CASESTR( BTSTATUS_NONE_RES_ID );
        CASESTR( BTSTATUS_LISTENING_RES_ID );
        CASESTR( BTSTATUS_SEEKING_RES_ID );
        CASESTR( BTSTATUS_CONNECTED_RES_ID );
#endif

#ifdef FEATURE_SILK
        CASESTR( doResizeWinEvent );
#endif
    default: 
        return "<unknown>";
        break;
    }
} /* eType_2str */

#undef CASESTR
#undef FUNC

#endif
