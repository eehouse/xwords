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


#include "gtkconnsdlg.h"
#include "gtkutils.h"
#include "linuxbt.h"

typedef struct _PageData {
    CommsConnType pageType;
    gboolean doUse;
    const char* labelText;
    GtkWidget* label;
} PageData;

typedef struct _GtkConnsState {
    LaunchParams* params;
    CommsAddrRec* addr;
    DeviceRole role;

    GtkWidget* invite;
    GtkWidget* hostName;
    GtkWidget* port;
    GtkWidget* bthost;
    GtkWidget* smsphone;
    GtkWidget* smsport;
    GtkWidget* iphost;
    GtkWidget* ipport;
    GtkWidget* bgScanButton;

    GtkWidget* notebook;

    XP_U16 nTypes;
    PageData pageData[COMMS_CONN_NTYPES];

    gboolean cancelled;
    gboolean readOnly;
} GtkConnsState;

static gint
conTypeToPageNum( const GtkConnsState* state, CommsConnType conType )
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
    GtkConnsState* state = (GtkConnsState*)closure;
    if ( !state->readOnly ) {
        const gchar* txt;

        for ( XP_U16 indx = 0; indx < state->nTypes; ++indx ) {
            PageData* data = &state->pageData[indx];
            CommsConnType conType = data->pageType;
            if ( ! data->doUse ) {
                addr_rmType( state->addr, conType );
                continue;
            }

            addr_addType( state->addr, conType );
            switch ( conType ) {
#ifdef XWFEATURE_DIRECTIP
            case COMMS_CONN_IP_DIRECT:
                txt = gtk_entry_get_text( GTK_ENTRY(state->iphost) );
                XP_STRNCPY( state->addr->u.ip.hostName_ip, txt, 
                            sizeof(state->addr->u.ip.hostName_ip) );
                txt = gtk_entry_get_text( GTK_ENTRY(state->ipport) );
                state->addr->u.ip.port_ip = atoi( txt );
                break;
#endif
#ifdef XWFEATURE_RELAY
            case COMMS_CONN_RELAY:
                txt = gtk_entry_get_text( GTK_ENTRY(state->invite) );
                XP_STRNCPY( state->addr->u.ip_relay.invite, txt, 
                            sizeof(state->addr->u.ip_relay.invite) );
                txt = gtk_entry_get_text( GTK_ENTRY(state->hostName) );
                XP_STRNCPY( state->addr->u.ip_relay.hostName, txt,
                            sizeof(state->addr->u.ip_relay.hostName) );

                txt = gtk_entry_get_text( GTK_ENTRY(state->port) );
                state->addr->u.ip_relay.port = atoi( txt );
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
                txt = gtk_entry_get_text( GTK_ENTRY(state->smsphone) );
                XP_STRNCPY( state->addr->u.sms.phone, txt, 
                            sizeof(state->addr->u.sms.phone) );
                txt = gtk_entry_get_text( GTK_ENTRY(state->smsport) );
                state->addr->u.sms.port = atoi( txt );
                break;
            case COMMS_CONN_MQTT:
                break;
            default:
                XP_ASSERT( 0 );     /* keep compiler happy */
                break;
            }
        }
    }
        
    state->cancelled = XP_FALSE;
    gtk_main_quit();
} /* handle_ok */

static void
handle_scan( GtkWidget* XP_UNUSED(widget), gpointer closure )
{
    GtkConnsState* state = (GtkConnsState*)closure;
    XP_USE(state);
    LOG_FUNC();

    GSList* devNames = lbt_scan( state->params );
    for ( GSList* iter = devNames; !!iter; iter = iter->next ) {
        gchar* name = iter->data;
        XP_LOGFF( "got %s", name );
    }
    lbt_freeScan( state->params, devNames );
}

static void
handle_cancel( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkConnsState* state = (GtkConnsState*)closure;
    state->cancelled = XP_TRUE;
    gtk_main_quit();
}

static void
useCheckToggled( GtkWidget* item, PageData* data )
{
    gboolean checked = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(item) );
    data->doUse = checked;

    gchar buf[64];
    gchar* fmt = checked ? "✓ %s" : "%s";
    snprintf( buf, sizeof(buf), fmt, data->labelText );
    gtk_label_set_text( GTK_LABEL(data->label), buf );
}

static GtkWidget*
boxWithUseCheck( GtkConnsState* state, PageData* data )
{
    XP_Bool set = addr_hasType( state->addr, data->pageType );
    data->doUse = set;

    GtkWidget* vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );

    gchar buf[32];
    snprintf( buf, sizeof(buf), "Connect via %s", data->labelText );
    GtkWidget* check = gtk_check_button_new_with_label( buf );
    g_signal_connect( check, "toggled", G_CALLBACK(useCheckToggled), data );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(check), set );
    gtk_box_pack_start( GTK_BOX(vbox), check, FALSE, TRUE, 0);

    return vbox;
}


/*
 * Invite: _____
 * Relay:  _____
 * Port:   _____
 * Cancel OK
 */
#ifdef XWFEATURE_RELAY
static GtkWidget*
makeRelayPage( GtkConnsState* state, PageData* data )
{
    GtkWidget* vbox = boxWithUseCheck( state, data );

    XP_Bool hasRelay = addr_hasType( state->addr, COMMS_CONN_RELAY );
    if ( hasRelay ) {
        gtk_entry_set_text( GTK_ENTRY(state->invite),
                            state->addr->u.ip_relay.invite );
    }
    gtk_widget_set_sensitive( state->invite, !state->readOnly );

    GtkWidget* hbox = makeLabeledField( "Relay address", &state->hostName, NULL );
    if ( hasRelay ) {
        gtk_entry_set_text( GTK_ENTRY(state->hostName),
                            state->addr->u.ip_relay.hostName );
    }
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
    gtk_widget_set_sensitive( state->hostName, !state->readOnly );

    hbox = makeLabeledField( "Relay port", &state->port, NULL );
    if ( hasRelay ) {
        char buf[16];
        snprintf( buf, sizeof(buf), "%d", state->addr->u.ip_relay.port );
        gtk_entry_set_text( GTK_ENTRY(state->port), buf );
    }
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
    gtk_widget_set_sensitive( state->port, !state->readOnly );

    gtk_widget_show( vbox );
    
    return vbox;
} /* makeRelayPage */
#endif

static GtkWidget*
makeBTPage( GtkConnsState* state, PageData* data )
{
    GtkWidget* vbox = boxWithUseCheck( state, data );

    GtkWidget* hbox = makeLabeledField( "Host device", &state->bthost, NULL );
    if ( addr_hasType( state->addr, data->pageType ) ) {
        gtk_entry_set_text( GTK_ENTRY(state->bthost), state->addr->u.bt.hostName );
    }
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
    gtk_widget_set_sensitive( state->bthost, !state->readOnly );

    state->bgScanButton = makeButton( "Scan", (GCallback)handle_scan,
                                      state );
    gtk_box_pack_start( GTK_BOX(vbox), state->bgScanButton, FALSE, TRUE, 0 );

    gtk_widget_show( vbox );

    return vbox;
} /* makeBTPage */

#ifdef XWFEATURE_DIRECTIP
static GtkWidget*
makeIPDirPage( GtkConnsState* state, PageData* data )
{
    GtkWidget* vbox = boxWithUseCheck( state, data );

    /* XP_UCHAR hostName_ip[MAX_HOSTNAME_LEN + 1]; */
    /* XP_U16 port_ip; */

    XP_Bool hasIP = addr_hasType( state->addr, data->pageType );
    const gchar* name = hasIP ?
        state->addr->u.ip.hostName_ip : state->globals->cGlobals.params->connInfo.ip.hostName;
    GtkWidget* hbox = makeLabeledField( "Hostname", &state->iphost, name );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
    
    hbox = makeLabeledField( "Relay port", &state->ipport, NULL );
    if ( hasIP ) {
        char buf[16];
        snprintf( buf, sizeof(buf), "%d", state->addr->u.ip.port_ip );
        gtk_entry_set_text( GTK_ENTRY(state->ipport), buf );
    }
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );

    return vbox;
}
#endif

#ifdef XWFEATURE_SMS
static GtkWidget*
makeSMSPage( GtkConnsState* state, PageData* data )
{
    GtkWidget* vbox = boxWithUseCheck( state, data );
    XP_Bool hasSMS = addr_hasType( state->addr, data->pageType );
    const gchar* phone = hasSMS ?
        state->addr->u.sms.phone : state->params->connInfo.sms.myPhone;
    GtkWidget* hbox = makeLabeledField( "My phone", &state->smsphone, phone );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
    gtk_widget_set_sensitive( state->smsphone, !state->readOnly );

    int portVal = hasSMS ? state->addr->u.sms.port
        : state->params->connInfo.sms.port;
    gchar port[32];
    snprintf( port, sizeof(port), "%d", portVal );
    hbox = makeLabeledField( "My port", &state->smsport, port );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
    gtk_widget_set_sensitive( state->smsport, !state->readOnly );

    gtk_widget_show( vbox );

    return vbox;
} /* makeSMSPage */
#endif

static GtkWidget*
makeMQTTPage( GtkConnsState* state, PageData* data )
{
    GtkWidget* vbox = boxWithUseCheck( state, data );
    return vbox;
} /* makeMQTTPage */

static PageData*
getNextData( GtkConnsState* state, CommsConnType typ, gchar* label )
{
    PageData* result = &state->pageData[state->nTypes++];
    result->pageType = typ;
    result->label = gtk_label_new( label );
    result->labelText = label;
    return result;
}

gboolean
gtkConnsDlg( LaunchParams* params, GtkWidget* parent, CommsAddrRec* addr,
             DeviceRole role, XP_Bool readOnly )
{
    GtkConnsState state;
    XP_MEMSET( &state, 0, sizeof(state) );

    state.readOnly = readOnly;
    state.params = params;
    state.addr = addr;
    state.role = role;

    GtkWidget* dialog;
    GtkWidget* vbox;
    GtkWidget* hbox;

    state.notebook = gtk_notebook_new();
    PageData* data;

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
#ifdef XWFEATURE_DIRECTIP
    data = getNextData( &state, COMMS_CONN_IP_DIRECT, "Direct" );
    (void)gtk_notebook_append_page( GTK_NOTEBOOK(state.notebook),
                                    makeIPDirPage(&state, data),
                                    data->label );
#endif
#ifdef XWFEATURE_SMS
    data = getNextData( &state, COMMS_CONN_SMS, "SMS" );
    (void)gtk_notebook_append_page( GTK_NOTEBOOK(state.notebook),
                                    makeSMSPage( &state, data ),
                                    data->label );
#endif

    vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
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
    gtk_box_pack_start( GTK_BOX(hbox), 
                        makeButton( "Ok", (GCallback)handle_ok, &state ),
                        FALSE, TRUE, 0 );
    if ( !readOnly ) {
        gtk_box_pack_start( GTK_BOX(hbox), 
                            makeButton( "Cancel", (GCallback)handle_cancel,
                                        &state ),
                            FALSE, TRUE, 0 );
    }
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );

    gtk_widget_show( vbox );

    dialog = gtk_dialog_new();
    gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW(dialog), GTK_WINDOW(parent) );
    gtk_dialog_add_action_widget( GTK_DIALOG(dialog), vbox, 0 );

    gtk_widget_show_all( dialog );
    gtk_main();
    gtk_widget_destroy( dialog );

    return !state.cancelled;
} /* gtkConnsDlg */
#endif
