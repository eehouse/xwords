/*
 * Copyright 2009 - 2024 by Eric House (xwords@eehouse.org).  All rights
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

import java.io.Serializable
import org.json.JSONObject
import kotlin.concurrent.thread

import org.eehouse.android.xw4.Assert
import org.eehouse.android.xw4.BuildConfig
import org.eehouse.android.xw4.Log
import org.eehouse.android.xw4.NetLaunchInfo
import org.eehouse.android.xw4.R
import org.eehouse.android.xw4.Utils
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole

// Collection of native methods and a bit of state
class XwJNI private constructor() {

    // This class is dead. For now it just asserts if anybody tries to use
    // it. Remove when convenient.
    class GamePtr(ptr: Long, rowid: Long) : AutoCloseable {
        val rowid: Long

        init {
            Assert.failDbg()
            this.rowid = rowid
        }

        override fun close() {
            Assert.failDbg()
        }
    }


    companion object {
        private val TAG = XwJNI::class.java.getSimpleName()
        // private var s_JNI: XwJNI? = null

        fun dvc_makeMQTTNukeInvite(nli: NetLaunchInfo): Device.TopicsAndPackets? {
            Assert.failDbg()
            return null // dvc_makeMQTTNukeInvite(Device.ptrGlobals(), nli)
        }

        fun dvc_makeMQTTNoSuchGames(addressee: String, gameID: Int):
            Device.TopicsAndPackets? {
            Log.d(TAG, "dvc_makeMQTTNoSuchGames(to: %s, gameID: %X)",
                  addressee, gameID)
            Assert.failDbg()
            // DbgUtils.printStack( TAG );
            return null // dvc_makeMQTTNoSuchGames(Device.ptrGlobals(), addressee, gameID)
        }

        @Synchronized
        fun initNew(
            gi: CurGameInfo, selfAddr: CommsAddrRec?, hostAddr: CommsAddrRec?,
            util: UtilCtxt?, draw: DrawCtx?, cp: CommonPrefs, procs: TransportProcs?
        ): GamePtr? {
            Assert.failDbg()
            return null
            // Only standalone doesn't provide self address
            // Assert.assertTrueNR(null != selfAddr || gi.deviceRole == DeviceRole.ROLE_STANDALONE)
            // Only client should be providing host addr
            // Assert.assertTrueNR(null == hostAddr || gi.deviceRole == DeviceRole.ROLE_ISGUEST)
            // val gamePtr = initGameJNI(0)
            // game_makeNewGame(gamePtr, gi, selfAddr, hostAddr, util, draw, cp, procs)
            // return gamePtr
        }

        fun game_makeRematch(
            gamePtr: GamePtr, util: UtilCtxt,
            cp: CommonPrefs, gameName: String?,
            newOrder: Array<Int>
        ): GamePtr? {
            val noInts = IntArray(newOrder.size)
            for (ii in newOrder.indices) {
                noInts[ii] = newOrder[ii]
            }
            Assert.failDbg()
            // var gamePtrNew = initGameJNI(0)
            // if (!game_makeRematch(gamePtr, gamePtrNew, util, cp, gameName, noInts)) {
            //     // gamePtrNew!!.release()
            //     gamePtrNew = null
            // }
            return null
        }

        fun game_makeFromInvite(
            nli: NetLaunchInfo, util: UtilCtxt,
            selfAddr: CommsAddrRec,
            cp: CommonPrefs, procs: TransportProcs
        ): GamePtr? {
            Assert.failDbg()
            // var gamePtrNew = initGameJNI(0)
            // if (!game_makeFromInvite(gamePtrNew, nli, util, selfAddr, cp, procs)) {
            //     // gamePtrNew!!.release()
            //     gamePtrNew = null
            // }
            // return gamePtrNew
            return null
        }

        fun server_canOfferRematch(gamePtr: GamePtr): BooleanArray {
            Assert.failDbg()
            return BooleanArray(2)
        }
    }
}
