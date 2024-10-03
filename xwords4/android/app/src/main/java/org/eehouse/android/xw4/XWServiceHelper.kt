/*
 * Copyright 2018 - 2024 by Eric House (xwords@eehouse.org).  All
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

import android.content.Context
import org.eehouse.android.xw4.DBUtils.getRowIDsFor
import org.eehouse.android.xw4.jni.CommsAddrRec
import org.eehouse.android.xw4.jni.JNIThread
import org.eehouse.android.xw4.jni.UtilCtxt
import org.eehouse.android.xw4.jni.UtilCtxtImpl
import org.eehouse.android.xw4.loc.LocUtils


internal abstract class XWServiceHelper(private val mContext: Context) {
    enum class ReceiveResult {
        OK, GAME_GONE, UNCONSUMED
    }

    open fun getSink(rowid: Long): MultiMsgSink {
        return MultiMsgSink(mContext, rowid)
    }

    open fun postNotification(device: String?, gameID: Int, rowid: Long)
    {
        val body = LocUtils.getString(mContext, R.string.new_game_body)
        GameUtils.postInvitedNotification(mContext, gameID, body, rowid)
    }

    fun receiveMessage(
        gameID: Int,
        sink: MultiMsgSink?, msg: ByteArray,
        addr: CommsAddrRec
    ): ReceiveResult {
        var result: ReceiveResult
        val rowids = DBUtils.getRowIDsFor(mContext, gameID)
        if (0 == rowids.size) {
            result = ReceiveResult.GAME_GONE
        } else {
            result = ReceiveResult.UNCONSUMED
            rowids.map{
                if (receiveMessage(it, sink, msg, addr)) {
                    result = ReceiveResult.OK
                }
            }
        }
        return result
    }

    protected fun receiveMessage(
        rowid: Long, sink: MultiMsgSink?,
        msg: ByteArray, addr: CommsAddrRec
    ): Boolean {
        var sink = sink
        var allConsumed = true
        val isLocalP = BooleanArray(1)
        var consumed = false

        JNIThread.getRetained(rowid).use { jniThread ->
            if (null != jniThread) {
                jniThread.receive(msg, addr)
                consumed = true
            } else {
                if (null == sink) {
                    sink = getSink(rowid)
                }
                val bmr = GameUtils.BackMoveResult()
                if (GameUtils.feedMessage(
                        mContext, rowid, msg, addr,
                        sink, bmr, isLocalP
                    )
                ) {
                    GameUtils.postMoveNotification(
                        mContext, rowid, bmr,
                        isLocalP[0]
                    )
                    consumed = true
                }
            }
        }
        if (allConsumed && !consumed) {
            allConsumed = false
        }
        return allConsumed
    }

    fun postEvent(event: MultiService.MultiEvent, vararg args: Any?) {
        if (0 == s_srcMgr.postEvent(event, *args)) {
            Log.d(
                TAG, "postEvent(): dropping %s event",
                event.toString()
            )
        }
    }

    fun handleInvitation(nli: NetLaunchInfo,
        device: String?, dfo: MultiService.DictFetchOwner?
    ): Boolean {
        // PENDING: get the test for dicts back in
        if (DictLangCache.haveDict(mContext, nli.isoCode(), nli.dict!!)) {
            GameUtils.handleInvitation(mContext, nli, getSink(0))
        } else {
            val intent = MultiService.makeMissingDictIntent(mContext, nli, dfo!!)
            MultiService.postMissingDictNotification(mContext, intent, nli.gameID())
        }
        return true
    }

    private var m_utilCtxt: UtilCtxt? = null
    val utilCtxt: UtilCtxt
        get() {
            if (null == m_utilCtxt) {
                m_utilCtxt = UtilCtxtImpl()
            }
            return m_utilCtxt!!
        }

    private fun checkNotInFlight(nli: NetLaunchInfo): Boolean {
        var inProcess: Boolean
        val inviteID = nli.inviteID()
        synchronized(s_seen) {
            val now = System.currentTimeMillis()
            val lastSeen = s_seen[inviteID]
            inProcess = null != lastSeen && lastSeen + SEEN_INTERVAL_MS > now
            if (!inProcess) {
                s_seen[inviteID] = now
            }
        }
        Log.d(TAG, "checkNotInFlight('%s') => %b", inviteID, !inProcess)
        return !inProcess
    }

    companion object {
        private val TAG: String = XWServiceHelper::class.java.simpleName
        private val s_srcMgr = MultiService()

        fun setListener(li: MultiService.MultiEventListener) {
            s_srcMgr.setListener(li)
        }

        fun clearListener(li: MultiService.MultiEventListener) {
            s_srcMgr.clearListener(li)
        }

        // Check that we aren't already processing an invitation with this
        // inviteID.
        private const val SEEN_INTERVAL_MS = (1000 * 2).toLong()
        private val s_seen: MutableMap<String, Long> = HashMap()
    }
}
