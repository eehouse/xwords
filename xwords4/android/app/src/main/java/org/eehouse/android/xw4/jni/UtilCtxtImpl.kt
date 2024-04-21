/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */ /*
 * Copyright 2009 - 2022 by Eric House (xwords@eehouse.org).  All rights
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
package org.eehouse.android.xw4.jni

import android.content.Context
import org.eehouse.android.xw4.Log
import org.eehouse.android.xw4.NetUtils
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet
import org.eehouse.android.xw4.jni.CurGameInfo.XWPhoniesChoice
import org.eehouse.android.xw4.jni.JNIThread.JNICmd
import org.json.JSONArray
import org.json.JSONObject
import java.net.HttpURLConnection

private val TAG = UtilCtxtImpl::class.java.getSimpleName()
// private val TAG = BTInviteDelegate::class.java.getSimpleName()

open class UtilCtxtImpl(val m_context: Context) : UtilCtxt {
    override fun getMQTTIDsFor(relayIDs: Array<String?>?) {
        val rowid = getRowID()
        if (0L == rowid) {
            Log.d(TAG, "getMQTTIDsFor() no rowid available so dropping")
        } else {
            Thread(Runnable {
                val params = JSONObject()
                val array = JSONArray()
                try {
                    JNIThread.getRetained(rowid).use { thread ->
                        params.put("rids", array)
                        for (rid: String? in relayIDs!!) {
                            array.put(rid)
                        }
                        val conn: HttpURLConnection = NetUtils
                            .makeHttpMQTTConn(m_context, "mids4rids")
                        val resStr: String = NetUtils.runConn(conn, params, true)
                        Log.d(TAG, "mids4rids => %s", resStr)
                        val obj: JSONObject = JSONObject(resStr)
                        val keys: Iterator<String> = obj.keys()
                        while (keys.hasNext()) {
                            val key: String = keys.next()
                            val hid: Int = key.toInt()
                            thread.handle(JNICmd.CMD_SETMQTTID, hid, obj.getString(key))
                        }
                    }
                } catch (ex: Exception) {
                    Log.ex(TAG, ex)
                }
            }).start()
        }
    }

    open fun getRowID(): Long = 0L // meant to be overridden!
}
