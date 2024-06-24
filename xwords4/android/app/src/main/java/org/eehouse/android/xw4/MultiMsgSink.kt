/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2024 by Eric House (xwords@eehouse.org).  All
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
import org.eehouse.android.xw4.DBUtils.recordInviteSent
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify.InviteMeans
import org.eehouse.android.xw4.MultiMsgSink
import org.eehouse.android.xw4.NFCUtils.addMsgFor
import org.eehouse.android.xw4.jni.CommsAddrRec
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.TransportProcs
import org.eehouse.android.xw4.jni.XwJNI.TopicsAndPackets

open class MultiMsgSink @JvmOverloads constructor(
    private val m_context: Context, // rowID is used as token to identify game on relay.  Anything that
    // uniquely identifies a game on a device would work
    var rowID: Long = 0
) : TransportProcs {
    // Use set to count so message sent over both BT and Relay is counted only
    // once.
    private val m_sentSet: MutableSet<String?> = HashSet()

    fun sendViaRelay(buf: ByteArray?, msgID: String?, gameID: Int): Int {
        return -1
    }

    open fun sendViaBluetooth(
        buf: ByteArray, msgID: String?, gameID: Int,
        addr: CommsAddrRec
    ): Int {
        return BTUtils.sendPacket(m_context, buf, msgID, addr, gameID)
    }

    open fun sendViaSMS(buf: ByteArray, msgID: String?, gameID: Int, addr: CommsAddrRec): Int {
        return NBSProto.sendPacket(m_context, addr.sms_phone!!, gameID, buf, msgID)
    }

    fun sendViaP2P(buf: ByteArray, gameID: Int, addr: CommsAddrRec): Int {
        return WiDirService
            .sendPacket(m_context, addr.p2p_addr, gameID, buf)
    }

    fun sendViaNFC(buf: ByteArray?, gameID: Int): Int {
        return addMsgFor(buf!!, gameID)
    }

    fun numSent(): Int {
        return m_sentSet.size
    }

    override val flags: Int
        /***** TransportProcs interface  */
        get() = TransportProcs.COMMS_XPORT_FLAGS_HASNOCONN

    override fun transportSendInvt(
        addr: CommsAddrRec, conType: CommsConnType,
        nli: NetLaunchInfo, timestamp: Int
    ): Boolean {
        return sendInvite(m_context, rowID, addr, conType, nli, timestamp)
    }

    override fun transportSendMsg(
        buf: ByteArray, streamVers: Int, msgID: String?,
        addr: CommsAddrRec, typ: CommsConnType,
        gameID: Int, timestamp: Int
    ): Int {
        var nSent = -1
        when (typ) {
            CommsConnType.COMMS_CONN_RELAY -> nSent = sendViaRelay(buf, msgID, gameID)
            CommsConnType.COMMS_CONN_BT -> nSent = sendViaBluetooth(buf, msgID, gameID, addr)
            CommsConnType.COMMS_CONN_SMS -> nSent = sendViaSMS(buf, msgID, gameID, addr)
            CommsConnType.COMMS_CONN_P2P -> nSent = sendViaP2P(buf, gameID, addr)
            CommsConnType.COMMS_CONN_NFC -> nSent = sendViaNFC(buf, gameID)
            CommsConnType.COMMS_CONN_MQTT -> Assert.failDbg() // shouldn't come this way
            else -> Assert.failDbg()
        }
        Log.i(
            TAG, "transportSendMsg(): sent %d bytes for game %d/%x via %s",
            nSent, gameID, gameID, typ.toString()
        )
        if (0 < nSent) {
            Log.d(TAG, "transportSendMsg: adding %s", msgID)
            m_sentSet.add(msgID)
        }

        Log.d(
            TAG, "transportSendMsg(len=%d, typ=%s) => %d", buf!!.size,
            typ, nSent
        )
        return nSent
    }

    override fun transportSendMQTT(tap: TopicsAndPackets): Int {
        return MQTTUtils.send(m_context, tap)
    }

    override fun countChanged(newCount: Int, quashed: Boolean) {
        // Log.d( TAG, "countChanged(new=%d); dropping", newCount );
    }

    companion object {
        private val TAG: String = MultiMsgSink::class.java.simpleName
        fun sendInvite(
            context: Context, rowid: Long,
            addr: CommsAddrRec, typ: CommsConnType,
            nli: NetLaunchInfo, timestamp: Int
        ): Boolean {
            Log.d(TAG, "sendInvite(to=%s, typ=%s, nli=%s)", addr, typ, nli)
            var success = false
            val means: InviteMeans? = null
            val target: String? = null
            when (typ) {
                CommsConnType.COMMS_CONN_MQTT -> Assert.failDbg()
                CommsConnType.COMMS_CONN_SMS -> if (XWPrefs.getNBSEnabled(context)) {
                    NBSProto.inviteRemote(context, addr.sms_phone!!, nli)
                    success = true
                }

                CommsConnType.COMMS_CONN_BT -> success =
                    BTUtils.sendInvite(context, addr.bt_hostName!!, addr.bt_btAddr, nli)

                CommsConnType.COMMS_CONN_NFC -> {}
                else -> {
                    Log.d(TAG, "sendInvite(); not handling %s", typ)
                    Assert.failDbg()
                }
            }
            if (null != means) {
                Assert.failDbg() // shouldn't be getting called any more
                recordInviteSent(context!!, rowid, means, target, true)
            }

            Log.d(TAG, "sendInvite(%s, %s) => %b", typ, addr, success)
            return success
        }
    }
}
