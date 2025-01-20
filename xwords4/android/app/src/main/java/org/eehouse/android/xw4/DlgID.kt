/*
 * Copyright 2014 - 2024 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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
package org.eehouse.android.xw4

enum class DlgID {
    NONE,
    CHANGE_GROUP,
    CONFIRM_CHANGE,
    CONFIRM_CHANGE_PLAY,
    CONFIRM_THEN,
    DIALOG_NOTAGAIN,
    DIALOG_OKONLY,
    DIALOG_ENABLESMS,
    DICT_OR_DECLINE,
    DLG_CONNSTAT,
    DLG_DELETED,
    DLG_INVITE(true),
    DLG_OKONLY,
    ENABLE_NFC,
    FORCE_REMOTE,
    GET_NAME,
    GET_NUMBER,
    INVITE_CHOICES_THEN,
    MOVE_DICT,
    NAME_GAME,
    NEW_GROUP,
    PLAYER_EDIT,
    ENABLE_SMS,
    QUERY_ENDGAME,
    RENAME_GAME,
    RENAME_GROUP,
    REVERT_ALL,
    REVERT_COLORS,
    SET_DEFAULT,
    SHOW_SUBST,
    WARN_NODICT_GENERIC,  // the general trying-to-open case
    WARN_NODICT_INVITED,  // when responding to invitation
    WARN_NODICT_SUBST,  // when a substitution will be possible/suggested
    DLG_BADWORDS,
    ASK_BADWORDS,
    QUERY_MOVE,
    QUERY_TRADE,
    ASK_PASSWORD,
    DLG_SCORES(true),
    DLG_USEDICT,
    DLG_GETDICT,
    GAMES_LIST_NEWGAME,
    CHANGE_CONN,
    GAMES_LIST_NAME_REMATCH,
    ASK_DUP_PAUSE,
    CHOOSE_TILES,
    SHOW_TILES,
    RENAME_PLAYER,
    BACKUP_LOADSTORE,
    GET_DEVID,
    SET_MQTTID,
    MQTT_PEERS,
    KACONFIG,
    ;

    private var m_addToStack: Boolean

    constructor(addToStack: Boolean) {
        m_addToStack = addToStack
    }

    constructor() {
        m_addToStack = false
    }

    fun belongsOnBackStack(): Boolean {
        return m_addToStack
    }
}
