/*
 * Copyright 2024 by Eric House (xwords@eehouse.org).  All rights reserved.
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

import android.content.Context
import android.text.TextUtils
import android.util.AttributeSet
import android.view.View
import android.widget.LinearLayout
import android.widget.TextView

import org.json.JSONException
import org.json.JSONObject

import org.eehouse.android.xw4.jni.XwJNI
import org.eehouse.android.xw4.loc.LocUtils

private val TAG: String = PeerStatusView::class.java.simpleName

class PeerStatusView(private val mContext: Context, aset: AttributeSet?) :
    LinearLayout(mContext, aset)
{
    private var mFinished = false
    private var mGameID = 0
    private var mSelfDevID: String? = null

    fun configure(gameID: Int, devID: String) {
        mGameID = gameID
        mSelfDevID = devID
        startThreadOnce()
    }

    override fun onFinishInflate() {
        mFinished = true
        startThreadOnce()
    }

    private fun startThreadOnce() {
        if (mFinished && null != mSelfDevID) {
            Thread { fetchAndDisplay() }.start()
        }
    }

    private fun fetchAndDisplay() {
        var userStr: String? = null
        val params = JSONObject()
        try {
            params.put("gid16", String.format("%X", mGameID))
            params.put("devid", mSelfDevID)

            val conn = NetUtils.makeHttpMQTTConn(mContext, "peers")
            val resStr = NetUtils.runConn(conn, params, true)
            Log.d(TAG, "runConn(ack) => %s", resStr)

            resStr?.let {
                JSONObject(it).optJSONArray("results")?.let {
                    val lines: MutableList<String?> = ArrayList()
                    for (ii in 0 until it.length()) {
                        val line = it.getJSONObject(ii)
                        val mqttID = line.getString("devid")
                        val age = line.getString("age")
                        var name = XwJNI.kplr_nameForMqttDev(mqttID)
                        if (null == name) {
                            name = if (mSelfDevID == mqttID) {
                                LocUtils.getString(mContext, R.string.selfName)
                            } else {
                                mqttID
                            }
                        }
                        lines.add(String.format("%s: %s", name, age))
                    }
                    userStr = TextUtils.join("\n", lines)
                }
            }
        } catch (je: JSONException) {
            Log.ex(TAG, je)
        } catch (npe: NullPointerException) {
            Log.ex(TAG, npe)
        }

        val activity = DelegateBase.hasLooper
        if (null != activity) {
            val finalUserStr = userStr ?: LocUtils.getString(mContext, R.string.no_peers_info)
            activity.runOnUiThread(Runnable {
                val tv = findViewById<View>(R.id.status) as TextView
                tv.text = finalUserStr
            })
        } else {
            Log.d(TAG, "no activity found")
        }
    }
}
// Nothing to see here
