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

object Knowns {
    private val TAG: String = Knowns::class.java.simpleName

    suspend fun hasKnownPlayers(): Boolean {
        return Device.await {
            val jniState = XwJNI.getJNIState()
            kplr_havePlayers(jniState)
        } as Boolean
    }

    suspend fun getAddr(name: String, lastMod: IntArray? = null): CommsAddrRec? {
        return Device.await {
            val jniState = XwJNI.getJNIState()
            kplr_getAddr(jniState, name, lastMod)
        } as CommsAddrRec?
    }

    suspend fun getPlayers(byDate: Boolean = false): Array<String>? {
        return Device.await {
            val jniState = XwJNI.getJNIState()
            kplr_getPlayers(jniState, byDate)
        } as Array<String>?
    }

    suspend fun nameForMqttDev(mqttID: String): String? {
        return Device.await {
            val jniState = XwJNI.getJNIState()
            kplr_nameForMqttDev(jniState, mqttID)
        } as String?
    }

    fun deletePlayer(name: String) {
        Device.post {
            val jniState = XwJNI.getJNIState()
            kplr_deletePlayer(jniState, name)
        }
    }

    suspend fun renamePlayer(oldName: String, newName: String): Boolean {
        return Device.await {
            val jniState = XwJNI.getJNIState()
            kplr_renamePlayer(jniState, oldName, newName)
        } as Boolean
    }

    @JvmStatic
    private external fun kplr_havePlayers(jniState: Long): Boolean
    @JvmStatic
    private external fun kplr_getAddr(jniState: Long, name: String,
                                      lastMod: IntArray?): CommsAddrRec?
    @JvmStatic
    private external fun kplr_getPlayers(jniState: Long, byDate: Boolean)
        : Array<String>?

    @JvmStatic
    private external fun kplr_nameForMqttDev(jniState: Long, name: String): String?
    @JvmStatic
    private external fun kplr_deletePlayer(jniState: Long, name: String)
    @JvmStatic
    private external fun kplr_renamePlayer(jniState: Long, oldName: String,
                                           newName: String): Boolean
}
