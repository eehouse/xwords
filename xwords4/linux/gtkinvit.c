/* -*- compile-command: "make MEMDEBUG=TRUE -j5"; -*- */
/* 
 * Copyright 2001-2014 by Eric House (xwords@eehouse.org).  All rights
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

#include "knownplyr.h"
#include "gtkinvit.h"
#include "gtkutils.h"
#include "linuxbt.h"
#include "comtypes.h"
#include "mqttcon.h"
#include "strutils.h"

typedef struct _PageData {
    CommsConnType pageType;
    gboolean doUse;
    const char* labelText;
    GtkWidget* label;
    const char* okButtonTxt;
} PageData;

#ifdef XWFEATURE_RELAY
static XP_UCHAR s_devIDBuf[32] = {0};
#endif

typedef struct _GtkInviteState {
    GtkGameGlobals* globals;
    XW_DUtilCtxt* dutil;        /* hang onto as optimization */
    CommsAddrRec* addr;
    gint* nPlayersP;
    XP_U32* relayDevIDp;
    gint maxPlayers;

    GtkWidget* nPlayersCombo;
    /* relay */
    GtkWidget* devID;
    /* BT */
    GtkWidget* bthost;
    /* SMS */
    GtkWidget* smsphone;
    GtkWidget* smsport;

    GtkWidget* mqttDevID;

#ifdef XWFEATURE_KNOWNPLAYERS
    GtkWidget* knownsCombo;
#endif

    GtkWidget* bgScanButton;
    GtkWidget* okButton;

    GtkWidget* notebook;
    guint curPage;

    XP_U16 nTypes;
    PageData pageData[COMMS_CONN_NTYPES];
    
    gboolean cancelled;
} GtkInviteState;

/* Make it static so we remember user's late entry */

static gint
conTypeToPageNum( const GtkInviteState* state, CommsConnType conType )
{
    gint pageNum = 0;           /* default */
    int ii;
    for ( ii = 0; ; ++ii ) {
        const PageData* pageData = &state->pageData[ii];
        CommsConnType thisType = pageData->pageType;
        if ( thisType == COMMS_CONN_NONE || thisType == conType ) {
            pageNum = ii;
            break;
        }
        XP_ASSERT( ii < VSIZE(state->pageData) );
    }
    return pageNum;
}

static void
handle_ok( GtkWidget* XP_UNUSED(widget), gpointer closure )
{
    GtkInviteState* state = (GtkInviteState*)closure;
    const gchar* txt;
    guint curPage = state->curPage;
    PageData* data = &state->pageData[curPage];
    CommsConnType conType = data->pageType;

    if ( 0 ) {
#ifdef XWFEATURE_KNOWNPLAYERS
    } else if ( COMMS_CONN_NONE == conType ) {
        gchar* name =
            gtk_combo_box_text_get_active_text( GTK_COMBO_BOX_TEXT(state->knownsCombo) );
        kplr_getAddr( state->dutil, NULL_XWE, name, state->addr, NULL );
#endif
    } else {
        addr_addType( state->addr, conType );
        switch ( conType ) {
#ifdef XWFEATURE_RELAY
        case COMMS_CONN_RELAY:
            txt = gtk_entry_get_text( GTK_ENTRY(state->devID) );
            snprintf( s_devIDBuf, sizeof(s_devIDBuf), "%s", txt );
            *state->relayDevIDp = atoi( txt );
            break;
#endif
#ifdef XWFEATURE_BLUETOOTH
        case COMMS_CONN_BT:
            txt = gtk_entry_get_text( GTK_ENTRY(state->bthost) );
            XP_STRNCPY( state->addr->u.bt.hostName, txt,
                        sizeof(state->addr->u.bt.hostName) );
            break;
#endif
        case COMMS_CONN_SMS:
            txt = gtk_entry_get_text( GTK_ENTRY(state->smsport) );
            state->addr->u.sms.port = atoi( txt );
            break;
        case COMMS_CONN_MQTT:
            txt = gtk_entry_get_text( GTK_ENTRY(state->mqttDevID) );
            if ( !strToMQTTCDevID( txt, &state->addr->u.mqtt.devID ) ) {
                XP_ASSERT(0);
            }
            break;
        default:
            XP_ASSERT( 0 );     /* keep compiler happy */
            break;
        }
    }

    /* get the number to invite */
    gchar* num = 
        gtk_combo_box_text_get_active_text( GTK_COMBO_BOX_TEXT(state->nPlayersCombo) );
    *(state->nPlayersP) = atoi( num );
        
    state->cancelled = XP_FALSE;
    gtk_main_quit();
} /* handle_ok */

static void
handle_scan( GtkWidget* XP_UNUSED(widget), gpointer closure )
{
    GtkInviteState* state = (GtkInviteState*)closure;
    XP_USE(state);
    LOG_FUNC();

    GSList* devNames = linux_bt_scan();
    if ( !devNames ) {
        XP_LOGF( "%s: got nothing", __func__ );
    } else {
        GSList* iter;
        for ( iter = devNames; !!iter; iter = iter->next ) {
#ifdef DEBUG
            gchar* name = iter->data;
            XP_LOGF( "%s: got %s", __func__, name );
#endif
        }
    }
}

static void
handle_cancel( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkInviteState* state = (GtkInviteState*)closure;
    state->cancelled = XP_TRUE;
    gtk_main_quit();
}

/*
 * Invite: _____
 * Relay:  _____
 * Port:   _____
 * Cancel OK
 */

#ifdef XWFEATURE_RELAY
static GtkWidget*
makeRelayPage( GtkInviteState* state, PageData* data )
{
    data->okButtonTxt = "Invite via Relay";

    GtkWidget* vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    GtkWidget* hbox;

    hbox = makeLabeledField( "Invitee DeviceID", &state->devID, NULL );
    gtk_entry_set_text( GTK_ENTRY(state->devID), s_devIDBuf );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );

    gtk_widget_show( vbox );
    
    return vbox;
} /* makeRelayPage */
#endif

static GtkWidget*
makeBTPage( GtkInviteState* state, PageData* data )
{
    data->okButtonTxt = "Invite via Bluetooth";

    GtkWidget* vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );

    GtkWidget* hbox = makeLabeledField( "Invitee device", &state->bthost, NULL );
    if ( addr_hasType( state->addr, data->pageType ) ) {
        gtk_entry_set_text( GTK_ENTRY(state->bthost), state->addr->u.bt.hostName );
    }
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );

    state->bgScanButton = makeButton( "Scan", (GCallback)handle_scan,
                                      state );
    gtk_box_pack_start( GTK_BOX(vbox), state->bgScanButton, FALSE, TRUE, 0 );

    gtk_widget_show( vbox );

    return vbox;
} /* makeBTPage */

/* #ifdef XWFEATURE_DIRECTIP */
/* static GtkWidget* */
/* makeIPDirPage( GtkInviteState* state, PageData* data ) */
/* { */
/*     GtkWidget* vbox = boxWithUseCheck( state, data ); */

/*     /\* XP_UCHAR hostName_ip[MAX_HOSTNAME_LEN + 1]; *\/ */
/*     /\* XP_U16 port_ip; *\/ */

/*     XP_Bool hasIP = addr_hasType( state->addr, data->pageType ); */
/*     const gchar* name = hasIP ? */
/*         state->addr->u.ip.hostName_ip : state->globals->cGlobals.params->connInfo.ip.hostName; */
/*     GtkWidget* hbox = makeLabeledField( "Hostname", &state->iphost, name ); */
/*     gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 ); */
    
/*     hbox = makeLabeledField( "Relay port", &state->ipport, NULL ); */
/*     if ( hasIP ) { */
/*         char buf[16]; */
/*         snprintf( buf, sizeof(buf), "%d", state->addr->u.ip.port_ip ); */
/*         gtk_entry_set_text( GTK_ENTRY(state->ipport), buf ); */
/*     } */
/*     gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 ); */

/*     return vbox; */
/* } */
/* #endif */

static GtkWidget*
makeSMSPage( GtkInviteState* state, PageData* data )
{
    data->okButtonTxt = "Invite via SMS";

    GtkWidget* vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    XP_Bool hasSMS = addr_hasType( state->addr, data->pageType );
    const gchar* phone = hasSMS ?
        state->addr->u.sms.phone : state->globals->cGlobals.params->connInfo.sms.myPhone;
    GtkWidget* hbox = makeLabeledField( "Invitee phone", &state->smsphone, phone );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );

    int portVal = hasSMS ? state->addr->u.sms.port
        : state->globals->cGlobals.params->connInfo.sms.port;
    gchar port[32];
    snprintf( port, sizeof(port), "%d", portVal );
    hbox = makeLabeledField( "Invitee port", &state->smsport, port );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );

    gtk_widget_show( vbox );

    return vbox;
} /* makeBTPage */

#ifdef XWFEATURE_KNOWNPLAYERS
static GtkWidget*
makeKnownsPage( GtkInviteState* state, PageData* data )
{
    data->okButtonTxt = "Invite Known Player";

    GtkWidget* hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
    GtkWidget* label = gtk_label_new( "Invite which player:" );
    gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, TRUE, 0 );

    XP_U16 nFound = 0;
    kplr_getNames( state->dutil, NULL_XWE, NULL, &nFound );
    XP_ASSERT( nFound > 0 );
    const XP_UCHAR* names[nFound];
    kplr_getNames( state->dutil, NULL_XWE, names, &nFound );

    GtkWidget* combo = state->knownsCombo = gtk_combo_box_text_new();
    for ( int ii = 0; ii < nFound; ++ii ) {
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(combo), names[ii] );
    }
    gtk_combo_box_set_active( GTK_COMBO_BOX(combo), 0 );
    gtk_box_pack_start( GTK_BOX(hbox), combo, FALSE, TRUE, 0 );
    state->knownsCombo = combo;

    GtkWidget* vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
    gtk_widget_show( vbox );

    return vbox;
}
#endif

static GtkWidget*
makeMQTTPage( GtkInviteState* state, PageData* data )
{
    data->okButtonTxt = "Invite via MQTT";

    GtkWidget* vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    GtkWidget* hbox;

    hbox = makeLabeledField( "Invitee MQTT DevID", &state->mqttDevID, NULL );
    // gtk_entry_set_text( GTK_ENTRY(state->mqttDevID), s_mqttIDBuf );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );

    gtk_widget_show( vbox );

    return vbox;
}

static PageData*
getNextData( GtkInviteState* state, CommsConnType typ, gchar* label )
{
    PageData* result = &state->pageData[state->nTypes++];
    result->pageType = typ;
    result->label = gtk_label_new( label );
    result->labelText = label;
    return result;
}

static void
onPageChanged( GtkNotebook* XP_UNUSED(notebook), gpointer XP_UNUSED(arg1), 
               guint arg2, gpointer data )
{
    GtkInviteState* state = (GtkInviteState*)data;
    state->curPage = arg2;
    PageData* pageData = &state->pageData[arg2];
    gtk_button_set_label(GTK_BUTTON(state->okButton), pageData->okButtonTxt );
}

XP_Bool
gtkInviteDlg( GtkGameGlobals* globals, CommsAddrRec* addr, 
              gint* nPlayersP, XP_U32* relayDevIDp )
{
    GtkInviteState state = {
        .globals = globals,
        .addr = addr,
        .nPlayersP = nPlayersP,
        .relayDevIDp = relayDevIDp,
        .maxPlayers = *nPlayersP,
        .dutil = globals->cGlobals.params->dutil,
    };

    GtkWidget* dialog;
    GtkWidget* hbox;
    GtkWidget* vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );

    hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
    GtkWidget* label = gtk_label_new( "Invite how many:" );
    gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, TRUE, 0 );

    state.nPlayersCombo = gtk_combo_box_text_new();
    for ( int ii = 1; ii <= state.maxPlayers; ++ii ) {
        gchar buf[8];
        sprintf( buf, "%d", ii );
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(state.nPlayersCombo), buf );
    }
    gtk_combo_box_set_active( GTK_COMBO_BOX(state.nPlayersCombo), 0 );
    gtk_box_pack_start( GTK_BOX(hbox), state.nPlayersCombo, FALSE, TRUE, 0 );
    gtk_widget_show( state.nPlayersCombo );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );

    state.notebook = gtk_notebook_new();
    g_signal_connect( state.notebook, "switch-page",
                      G_CALLBACK(onPageChanged), &state );

    PageData* data;
#ifdef XWFEATURE_KNOWNPLAYERS
    if ( kplr_havePlayers( state.dutil, NULL_XWE ) ) {
        data = getNextData( &state, COMMS_CONN_NONE, "Knowns" );
        (void)gtk_notebook_append_page( GTK_NOTEBOOK(state.notebook),
                                        makeKnownsPage( &state, data ),
                                        data->label );
    }
#endif
    data = getNextData( &state, COMMS_CONN_MQTT, "MQTT" );
    (void)gtk_notebook_append_page( GTK_NOTEBOOK(state.notebook),
                                    makeMQTTPage( &state, data ),
                                    data->label );
#ifdef XWFEATURE_RELAY
    data = getNextData( &state, COMMS_CONN_RELAY, "Relay" );
    (void)gtk_notebook_append_page( GTK_NOTEBOOK(state.notebook), 
                                    makeRelayPage( &state, data ),
                                    data->label );
#endif
#ifdef XWFEATURE_BLUETOOTH
    data = getNextData( &state, COMMS_CONN_BT, "Bluetooth" );
    (void)gtk_notebook_append_page( GTK_NOTEBOOK(state.notebook),
                                    makeBTPage( &state, data ),
                                    data->label );
#endif
/* #ifdef XWFEATURE_DIRECTIP */
/*     data = getNextData( &state, COMMS_CONN_IP_DIRECT, "Direct" ); */
/*     (void)gtk_notebook_append_page( GTK_NOTEBOOK(state.notebook), */
/*                                     makeIPDirPage(&state, data), */
/*                                     data->label ); */
/* #endif */
#ifdef XWFEATURE_SMS
    data = getNextData( &state, COMMS_CONN_SMS, "SMS" );
    (void)gtk_notebook_append_page( GTK_NOTEBOOK(state.notebook),
                                    makeSMSPage( &state, data ),
                                    data->label );
#endif

    gtk_box_pack_start( GTK_BOX(vbox), state.notebook, FALSE, TRUE, 0 );

    /* Set page to the first we actually have */
    XP_U32 st = 0;
    CommsConnType firstType;
    if ( addr_iter( addr, &firstType, &st ) ) {
        gint pageNo = conTypeToPageNum( &state, firstType );
        gtk_notebook_set_current_page( GTK_NOTEBOOK(state.notebook), pageNo );
    }

    gtk_widget_show( state.notebook );

    /* buttons at the bottom */
    hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
    state.okButton = makeButton( state.pageData[0].okButtonTxt, 
                                 (GCallback)handle_ok, &state );
    gtk_box_pack_start( GTK_BOX(hbox), state.okButton, FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX(hbox),
                        makeButton( "Cancel", (GCallback)handle_cancel,
                                    &state ),
                        FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );

    gtk_widget_show( vbox );

    dialog = gtk_dialog_new();
    gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );
    gtk_dialog_add_action_widget( GTK_DIALOG(dialog), vbox, 0 );

    gtk_widget_show_all( dialog );
    gtk_main();
    gtk_widget_destroy( dialog );

    return !state.cancelled;
} /* gtkInviteDlg */
#endif
