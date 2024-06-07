/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2016 - 2024 by Eric House (xwords@eehouse.org).  All rights
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

import org.json.JSONArray
import org.json.JSONObject

// This can't change after ship!!!!
private const val CMDS_AS_STRINGS = true
private const val KEY_CMD = "cmd"

class XWPacket(jobj: String) {
    private var mObj = JSONObject(jobj)
    private val TAG = XWPacket::class.java.simpleName

    enum class CMD {
        PING,
        PONG,
        MSG,
        INVITE,
        NOGAME,
    }

    constructor(cmd: CMD): this("{}") {
        if (CMDS_AS_STRINGS) {
            mObj.put(KEY_CMD, cmd.toString())
        } else {
            mObj.put(KEY_CMD, cmd.ordinal)
        }
    }

    fun getCommand(): CMD?
    {
        var cmd: CMD? = null
        if (CMDS_AS_STRINGS) {
            val str = mObj.optString(KEY_CMD)
            for (one in CMD.entries) {
                if (one.toString() == str) {
                    cmd = one
                    break
                }
            }
        } else {
            val ord = mObj.optInt(KEY_CMD, -1) // let's blow up :-)
            cmd = CMD.entries[ord]
        }
        return cmd
    }

    fun put(key: String, value: String?): XWPacket {
        mObj.put(key, value)
        return this
    }

    fun put(key: String, value: Int): XWPacket {
        mObj.put(key, value)
        return this
    }

    fun put(key: String, value: JSONArray?): XWPacket {
        mObj.put(key, value)
        return this
    }

    fun getString(key: String): String {
        val str = mObj.optString(key)
        return str
    }

    fun getInt(key: String, dflt: Int): Int {
        val ii = mObj.optInt(key, dflt)
        return ii
    }

    fun getJSONArray(key: String): JSONArray {
        var array = mObj.getJSONArray(key)
        return array
    }

    override fun toString(): String {
        return mObj.toString()
    }
}
