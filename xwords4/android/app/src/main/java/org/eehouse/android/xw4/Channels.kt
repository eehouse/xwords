/*
 * Copyright 2010 - 2024 by Eric House (xwords@eehouse.org).  All
 * rights reserved.
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

import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import android.os.Build
import java.io.Serializable

object Channels {
    private val TAG: String = Channels::class.java.simpleName

    private val sChannelsMade: MutableSet<ID> = HashSet()
    fun getChannelID(context: Context, id: ID): String {
        val name = id.toString()
        if (!sChannelsMade.contains(id)) {
            sChannelsMade.add(id)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                val notMgr =
                    context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager

                var channel = notMgr.getNotificationChannel(name)
                if (channel == null) {
                    val channelDescription = context.getString(id.getDesc())
                    channel = NotificationChannel(
                        name, channelDescription,
                        id.getImportance()
                    )
                    channel.enableVibration(true)
                    notMgr.createNotificationChannel(channel)
                }
            }
        }
        return name
    }

    private val IDS_KEY = TAG + "/ids_key"

    private var sData: IdsData? = null

    // I want each rowid to be able to have a notification active for it for
    // each channel. So let's try generating and storing random ints.
    private fun notificationId(rowid: Long, channel: ID): Int {
        val context = XWApp.getContext()
        var result: Int
        synchronized(Channels::class.java) {
            if (null == sData) {
                sData = DBUtils.getSerializableFor(context, IDS_KEY) as IdsData?
                if (null == sData) {
                    sData = IdsData()
                }
            }
        }

        synchronized(sData!!) {
            var dirty = false
            if (!sData!!.mMap.containsKey(channel)) {
                sData!!.mMap[channel] =
                    HashMap()
                dirty = true
            }
            val map: MutableMap<Long, Int> = sData!!.mMap[channel]!!
            if (!map.containsKey(rowid)) {
                map[rowid] = sData!!.newID()
                dirty = true
            }

            if (dirty) {
                DBUtils.setSerializableFor(context, IDS_KEY, sData)
            }
            result = map[rowid]!!
        }
        Log.d(TAG, "notificationId(%s, %d) => %d", channel, rowid, result)
        return result
    }

    enum class ID(private val mExpl: Int,
                  private val mImp: Int = NotificationManager.IMPORTANCE_LOW)
    {
        NBSPROXY(R.string.nbsproxy_channel_expl),
        GAME_EVENT(R.string.gameevent_channel_expl,
                   // HIGH seems to be required for sound
                   NotificationManager.IMPORTANCE_HIGH),
        DUP_TIMER_RUNNING(R.string.dup_timer_expl),
        DUP_PAUSED(R.string.dup_paused_expl);

        fun idFor(rowid: Long): Int {
            return notificationId(rowid, this)
        }

        fun getDesc(): Int { return mExpl }
        fun getImportance(): Int { return mImp }
    }

    private class IdsData : Serializable {
        var mMap: HashMap<ID, HashMap<Long, Int>> = HashMap()
        var mInts: HashSet<Int> = HashSet()

        fun newID(): Int {
            val result: Int
            while (true) {
                val one = Utils.nextRandomInt()
                if (!mInts.contains(one)) {
                    mInts.add(one)
                    result = one
                    break
                }
            }
            return result
        }
    }
}
