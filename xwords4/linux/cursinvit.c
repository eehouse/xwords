/*
 * Copyright 2025 by Eric House (xwords@eehouse.org).  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifdef PLATFORM_NCURSES

#include "cursinvit.h"
#include "cursesdlgutil.h"
#include "strutils.h"
#include "knownplyr.h"
#include "cursesask.h"
#include "curlistask.h"

#define KPCOLS 30

static bool
launchForKnowns( WINDOW* parent, LaunchParams* params, CommsAddrRec* addrP )
{
    bool success = false;
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)params->cag;
    XP_U16 nFound;
    kplr_getNames( params->dutil, NULL_XWE, XP_TRUE, NULL, &nFound );
    if ( nFound ) {
        const XP_UCHAR* players[nFound];
        kplr_getNames( params->dutil, NULL_XWE, XP_TRUE, players, &nFound );

        int chosen;
        if ( curAskPickList( params, parent, "Pick your player",
                             players, nFound, &chosen ) ) {
            CommsAddrRec addr;
            if ( kplr_getAddr( params->dutil, NULL_XWE, players[chosen],
                               &addr, NULL ) ) {
                *addrP = addr;
                success = true;
            }
        }

    } else {
        ca_inform2( aGlobals, parent, "There are no known players" );
    }
    return success;
}

#define INVCOLS 40
#define INVLINES 20

#define MQTT_EXPL_LINE 1
#define MQTT_DEV_LINE 2
#define SMS_EXPL_LINE 3
#define SMS_DEV_LINE 4
#define BT_EXPL_LINE 5
#define BT_DEV_LINE 6

enum {
    SEL_EDITMQTT,
    SEL_EDITSMS,
    SEL_EDITBT,
    SEL_KNOWNS,
    SEL_CANCEL,
    SEL_OK,
    SEL_NSELS,
};

typedef struct _CurInviteState {
    LaunchParams* params;
    WINDOW* win;
    bool cancelled, confirmed;
    CommsAddrRec addr;
    gint* nPlayers;
    int sel;
    EditState ess[3];
} CurInviteState;

static void
updateButtons( CurInviteState* cis )
{
    int sel;

    switch ( cis->sel ) {
    case SEL_KNOWNS:
    case SEL_CANCEL:
    case SEL_OK:
        sel = cis->sel - SEL_KNOWNS;
        break;
    default:
        sel = -1;
    }
    const char* buttons2[] = { "Knowns", "Cancel", "OK" };
    drawButtons( cis->win, 10, 8, VSIZE(buttons2), sel, buttons2 );
}

static void
drawWin( CurInviteState* cis )
{
    mvwaddstr( cis->win, MQTT_EXPL_LINE, 1, "MQTT" );
    drawEdit( &cis->ess[0], cis->sel == SEL_EDITMQTT );

    mvwaddstr( cis->win, SMS_EXPL_LINE, 1, "SMS" );
    drawEdit( &cis->ess[1], cis->sel == SEL_EDITSMS );

    mvwaddstr( cis->win, BT_EXPL_LINE, 1, "BT" );
    drawEdit( &cis->ess[2], cis->sel == SEL_EDITBT );

    updateButtons( cis );
    wrefresh( cis->win );
}

static void
updateAddr( CurInviteState* cis, const CommsAddrRec* addr )
{
    WINDOW* win = cis->win;
    XP_UCHAR buf[32] = {};
    if ( addr_hasType( &cis->addr, COMMS_CONN_MQTT ) ) {
        formatMQTTDevID( &cis->addr.u.mqtt.devID, buf, VSIZE(buf) );
    }
    initEdit( &cis->ess[0], win, MQTT_DEV_LINE, buf );

    const char* phone = "";
    if ( addr_hasType( &cis->addr, COMMS_CONN_SMS ) ) {
        phone = addr->u.sms.phone;
    }
    initEdit( &cis->ess[1], win, SMS_DEV_LINE, phone );

    const char* bt = "";
    if ( addr_hasType( &cis->addr, COMMS_CONN_BT ) ) {
        bt = addr->u.bt.btAddr.chars;
    }
    initEdit( &cis->ess[2], win, BT_DEV_LINE, bt );
}

static bool
inviteKeyProc( int key, void* closure )
{
    CurInviteState* cis = (CurInviteState*)closure;

    switch ( key ) {
    case '\r':
    case '\n':
        switch ( cis->sel ) {
        case SEL_KNOWNS:
            if ( launchForKnowns( cis->win, cis->params, &cis->addr ) ) {
                updateAddr( cis, &cis->addr );
            }
            break;
        case SEL_CANCEL: cis->cancelled = true; break;
        case SEL_OK: cis->confirmed = true; break;
        }
        break;
    case 0x161:                 /* shift-tab */
        cis->sel = (cis->sel + SEL_NSELS - 1) % SEL_NSELS;
        break;
    case '\t': cis->sel = (cis->sel + 1) % SEL_NSELS;
        break;
    default:
        switch ( cis->sel ) {
        case SEL_EDITMQTT:
        case SEL_EDITSMS:
        case SEL_EDITBT:
            handleEdit( &cis->ess[cis->sel-SEL_EDITMQTT], key );
            break;
        }
    }

    drawWin( cis );
    return cis->cancelled || cis->confirmed;
}

bool
cursesInviteDlg( CommonGlobals* cGlobals, WINDOW* parent, CommsAddrRec* addr,
                 /*inout*/ gint* nPlayers )
{
    CurInviteState cis = {
        .params = cGlobals->params,
        .win = makeCenteredBox( parent, INVCOLS, INVLINES ),
        .addr = *addr,
        .nPlayers = nPlayers,
    };

    updateAddr( &cis, addr );
    
    drawWin( &cis );
    
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)cGlobals->params->cag;
    startModalAlert( aGlobals, cis.win, XP_TRUE, inviteKeyProc, &cis );
    if ( !cis.cancelled ) {
        *addr = cis.addr;
    }

    delwin( cis.win );
    wrefresh( parent );

    return !cis.cancelled;
}

#endif
