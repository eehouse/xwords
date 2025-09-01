/*
 * Copyright 2025 by Eric House (xwords@eehouse.org).  All rights
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

import org.eehouse.android.xw4.Assert
import org.eehouse.android.xw4.Log
import org.json.JSONObject

object Stats {
    private val TAG: String = Stats::class.java.simpleName

    enum class STAT {
        STAT_NONE,
        STAT_MQTT_RCVD,
        STAT_MQTT_SENT,

        STAT_REG_NOROOM,

        STAT_NEW_SOLO,
        STAT_NEW_TWO,
        STAT_NEW_THREE,
        STAT_NEW_FOUR,
        STAT_NEW_REMATCH,

        STAT_NBS_RCVD,
        STAT_NBS_SENT,
        STAT_BT_RCVD,
        STAT_BT_SENT,

        STAT_NSTATS,
    }

    fun increment(stat: STAT) {
        Device.post {
            val jniState = XwJNI.getJNIState()
            sts_increment( jniState, stat );
        }
    }

    fun clearAll() {
        Device.post {
            val jniState = XwJNI.getJNIState()
            sts_clearAll( jniState );
        }
    }

    suspend fun export(): JSONObject {
        val str = Device.await {
            val jniState = XwJNI.getJNIState()
            sts_export(jniState)
        } as String
        return JSONObject(str)
    }

    @JvmStatic
    private external fun sts_increment(jniState: Long, stat: STAT)
    @JvmStatic
    private external fun sts_clearAll(jniState: Long)
    @JvmStatic
    private external fun sts_export(jniState: Long): String
    
}
