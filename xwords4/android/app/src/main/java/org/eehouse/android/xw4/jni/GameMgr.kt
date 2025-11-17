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

import java.io.Serializable

import org.eehouse.android.xw4.Assert
import org.eehouse.android.xw4.Log
import org.eehouse.android.xw4.NetLaunchInfo

object GameMgr {
    private val TAG: String = GameMgr::class.java.simpleName

    class GLItemRef(val ir: Long) {
        fun isGame(): Boolean {
            return gmgr_isGame(ir)
        }
        fun toGame(): GameRef {
            return GameRef(gmgr_toGame(ir))
        }
        fun toGroup(): GroupRef {
            return GroupRef(gmgr_toGroup(ir))
        }
        override fun toString(): String {
            val result =
                if (isGame()) toGame().toString()
                else toGroup().toString()
            return result
        }
    }

    class GroupRef(val grp: Int): Serializable {
        override fun equals(other: Any?): Boolean {
            val result =
                if ( null != other && other is GroupRef) {
                    val gr = other as GroupRef
                    gr.grp == this.grp
                } else false
            return result
        }

        override fun toString(): String {
            return "$grp"
        }

        suspend fun getGroupCollapsed(): Boolean {
            return Device.await {
                val jniState = Device.ptrGlobals()
                gmgr_getGroupCollapsed(jniState, grp)
            } as Boolean
        }

        fun setGroupCollapsed(collapsed: Boolean) {
            Device.post {
                val jniState = Device.ptrGlobals()
                gmgr_setGroupCollapsed( jniState, grp, collapsed)
            }
        }

        suspend fun getGroupGamesCount(): Int {
            return Device.await {
                val jniState = Device.ptrGlobals()
                gmgr_getGroupGamesCount(jniState, grp)
            } as Int
        }

        suspend fun getGroupName(): String {
            return Device.await {
                val jniState = Device.ptrGlobals()
                gmgr_getGroupName(jniState, grp)
            } as String
        }

        fun setGroupName(name: String) {
            Device.post {
                val jniState = Device.ptrGlobals()
                gmgr_setGroupName(jniState, grp, name)
            }
        }

        companion object {
            // Keep in sync with values in gamemgr.h
            val GROUP_DEFAULT = GroupRef(0xFF)
            val GROUP_ARCHIVE = GroupRef(0xFE)
        }
    }

    suspend fun getPositions(): Array<GLItemRef> {
        val longs = Device.await {
            val jniState = Device.ptrGlobals()
            gmgr_getPositions(jniState)
        } as LongArray
        val refs = longs.map{ GameMgr.GLItemRef(it) }.toTypedArray()
        // Log.d(TAG, "getPositions() => {len: ${refs.size}}")
        return refs
    }

    fun deleteGame(gr: GameRef) {
        Device.post {
            val jniState = Device.ptrGlobals()
            gmgr_deleteGame(jniState, gr.gr)
        }
    }

    suspend fun newFor(gi: CurGameInfo, invitee: CommsAddrRec? = null): GameRef {
        val gr = Device.await {
            val jniState = Device.ptrGlobals()
            gmgr_newFor(jniState, gi, invitee)
        } as Long
        return GameRef(gr)
    }

    suspend fun getFor(gameID: Int): GameRef? {
        val gr = Device.await {
            val jniState = Device.ptrGlobals()
            gmgr_getFor(jniState, gameID)
        } as Long
        return if (gr == 0L) null else GameRef(gr)
    }

    suspend fun figureGR(bytes: ByteArray): GameRef {
        val grval = Device.await {
            val jniState = Device.ptrGlobals()
            gmgr_figureGR(jniState, bytes)
        } as Long
        return GameRef(grval)
    }

    suspend fun gameExists(gr: GameRef): Boolean {
        return Device.await {
            val jniState = Device.ptrGlobals()
            gmgr_gameExists(jniState, gr.gr)
        } as Boolean
    }

    // gmgr_addForInvite() returns a GameRef, but that would require this be a
    // suspend. Until we need to use the result, let's not do that.
    fun addForInvite(nli: NetLaunchInfo) {
        Device.post {
            val jniState = Device.ptrGlobals()
            gmgr_addForInvite(jniState, nli)
        }
    }

    fun clearThumbnails() {
        Device.post {
            val jniState = Device.ptrGlobals()
            gmgr_clearThumbnails(jniState)
        }
    }

    suspend fun addGroup(name: String): GroupRef {
        val grp = Device.await {
            val jniState = Device.ptrGlobals()
            gmgr_addGroup(jniState, name)
        } as Int
        return GroupRef(grp)
    }

    fun deleteGroup(grp: GroupRef) {
        Device.post {
            val jniState = Device.ptrGlobals()
            gmgr_deleteGroup(jniState, grp.grp)
        }
    }

    suspend fun getDefaultGroup() : GroupRef {
        val grp = Device.await {
            val jniState = Device.ptrGlobals()
            gmgr_getDefaultGroup(jniState)
        } as Int
        return GroupRef(grp)
    }

    suspend fun getGroup(name: String): GroupRef? {
        val grp = Device.await {
            val jniState = Device.ptrGlobals()
            gmgr_getGroup(jniState, name)
        } as Int
        val result =
            if (grp == 0) null
            else GroupRef(grp)
        return result
    }

    suspend fun convertGame(name: String?, grp: GroupRef,
                            bytes: ByteArray): GameRef? {
        val gr = Device.await {
            val jniState = Device.ptrGlobals()
            gmgr_convertGame(jniState, name, grp.grp, bytes)
        } as Long
        return if (gr == 0L) null else GameRef(gr)
    }

    fun makeGroupDefault(grp: GroupRef) {
        Device.post {
            val jniState = Device.ptrGlobals()
            gmgr_makeGroupDefault(jniState, grp.grp)
        }
    }

    class GroupsNames(val refs: IntArray, val names: Array<String>):
        Serializable {}

    suspend fun getGroupsMap(): GroupsNames {
        val refs: Array<IntArray?> = Array<IntArray?>(1, {null})
        val names: Array<Array<String>?> = Array<Array<String>?>(1, {null})
        Device.await {
            val jniState = Device.ptrGlobals()
            gmgr_getGroupsMap(jniState, refs, names)
        }
        return GroupsNames(refs[0]!!, names[0]!!)
    }

    fun moveGames(grp: GroupRef, games: Array<GameRef>) {
        Device.post {
            val jniState = Device.ptrGlobals()
            val vals: LongArray = games.map{it.gr}.toLongArray()
            gmgr_moveGames(jniState, grp.grp, vals)
        }
    }

    fun archiveGame(gr: GameRef) {
        moveGames(GroupRef.GROUP_ARCHIVE, arrayOf(gr))
    }

    fun raiseGroup(grp: GroupRef) {
        Device.post {
            val jniState = Device.ptrGlobals()
            gmgr_raiseGroup(jniState, grp.grp)
        }
    }

    fun lowerGroup(grp: GroupRef) {
        Device.post {
            val jniState = Device.ptrGlobals()
            gmgr_lowerGroup(jniState, grp.grp)
        }
    }

    @JvmStatic
    private external fun gmgr_addGroup(jniState: Long, name: String): Int
    @JvmStatic
    private external fun gmgr_deleteGroup(jniState: Long, grp: Int)
    @JvmStatic
    private external fun gmgr_getGroupCollapsed(jniState: Long, grp: Int): Boolean
    @JvmStatic
    private external fun gmgr_setGroupCollapsed( jniState: Long, grp: Int, collapsed: Boolean)
    @JvmStatic
    private external fun gmgr_getGroupGamesCount(jniState: Long, grp: Int): Int
    @JvmStatic
    private external fun gmgr_getGroupName(jniState: Long, grp: Int): String
    @JvmStatic
    private external fun gmgr_setGroupName(jniState: Long, grp: Int, name: String)
    @JvmStatic
    private external fun gmgr_getPositions(jniState: Long): LongArray
    @JvmStatic
    private external fun gmgr_deleteGame(jniState: Long, gr: Long)
    @JvmStatic
    private external fun gmgr_getGroupsMap(jniState: Long,
                                           refs: Array<IntArray?>,
                                           names: Array<Array<String>?> )
    @JvmStatic
    private external fun gmgr_newFor(jniState: Long, gi: CurGameInfo,
                                     invitee: CommsAddrRec?): Long
    @JvmStatic
    private external fun gmgr_getFor(jniState: Long, gameID: Int): Long
    @JvmStatic
    private external fun gmgr_figureGR(jniState: Long, bytes: ByteArray): Long
    @JvmStatic
    private external fun gmgr_gameExists(jniState: Long, gr: Long): Boolean
    @JvmStatic
    private external fun gmgr_addForInvite(jniState: Long, nli: NetLaunchInfo): Long
    @JvmStatic
    private external fun gmgr_clearThumbnails(jniState: Long)

    @JvmStatic
    private external fun gmgr_getDefaultGroup(jniState: Long): Int
    @JvmStatic
    private external fun gmgr_getGroup(jniState: Long, name: String): Int
    @JvmStatic
    private external fun gmgr_convertGame(jniState: Long, name: String?, grp: Int,
                                          bytes: ByteArray): Long
    @JvmStatic
    private external fun gmgr_makeGroupDefault(jniState: Long, grp: Int)
    @JvmStatic
    private external fun gmgr_moveGames(jniState: Long, grp: Int, games: LongArray)
    @JvmStatic
    private external fun gmgr_raiseGroup(jniState: Long, grp: Int)
    @JvmStatic
    private external fun gmgr_lowerGroup(jniState: Long, grp: Int)

    @JvmStatic
    private external fun gmgr_isGame(ir :Long): Boolean
    @JvmStatic
    private external fun gmgr_toGame(ir: Long): Long
    @JvmStatic
    private external fun gmgr_toGroup(ir: Long): Int
}
