/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2015 by Eric House (xwords@eehouse.org).  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

package org.eehouse.android.xw4;

// Just make sure all calls to startActivityForResult are using unique codes.

public enum RequestCode {
    __UNUSED,

    // BoardDelegate
    BT_INVITE_RESULT,
    SMS_USER_INVITE_RESULT,
    SMS_DATA_INVITE_RESULT,
    RELAY_INVITE_RESULT,
    P2P_INVITE_RESULT,
    MQTT_INVITE_RESULT,

    // PermUtils
    PERM_REQUEST,

    // GameConfig
    REQUEST_LANG_GC,
    REQUEST_DICT,

    // Games list
    REQUEST_LANG_GL,
    CONFIG_GAME,
    STORE_DATA_FILE,
    LOAD_DATA_FILE,

    // SMSInviteDelegate
    GET_CONTACT,

    // HostDelegate
    HOST_DIALOG,
}
