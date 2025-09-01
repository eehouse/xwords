/*
 * Copyright 2009 - 2022 by Eric House (xwords@eehouse.org).  All
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
import org.eehouse.android.xw4.jni.CommsAddrRec
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole
import org.eehouse.android.xw4.jni.TransportProcs
import org.eehouse.android.xw4.jni.TransportProcs.TPMsgHandler
import org.eehouse.android.xw4.jni.XwJNI.TopicsAndPackets

class CommsTransport(
    private val m_context: Context, private val m_tpHandler: TPMsgHandler,
    private val m_rowid: Long, role: DeviceRole?
) : TransportProcs {
    private val m_done = false

    override val flags: Int
        get() = if (TRANSPORT_DOES_NOCONN) TransportProcs.COMMS_XPORT_FLAGS_HASNOCONN else TransportProcs.COMMS_XPORT_FLAGS_NONE

    override fun transportSendInvt(
        addr: CommsAddrRec, conType: CommsConnType,
        nli: NetLaunchInfo, timestamp: Int
    ): Boolean {
        return MultiMsgSink.sendInvite(
            m_context, m_rowid, addr, conType,
            nli, timestamp
        )
    }

    override fun transportSendMsg(
        buf: ByteArray, streamVers: Int, msgID: String?,
        addr: CommsAddrRec, conType: CommsConnType,
        gameID: Int, timestamp: Int
    ): Int {
        Assert.assertTrueNR(addr.contains(conType)) // fired per google
        val nSent = sendForAddr(
            m_context, addr, conType, m_rowid, gameID,
            timestamp, buf, streamVers, msgID
        )

        // Keep this while debugging why the resend_all that gets fired on
        // reconnect doesn't install a game but a manual resend does.
        Log.d(
            TAG, "transportSendMsg(len=%d, typ=%s) => %d", buf.size,
            conType, nSent
        )
        return nSent
    }

    override fun transportSendMQTT(tap: TopicsAndPackets): Int {
        Log.d(TAG, "transportSendMQTT()")
        // return MQTTUtils.send(m_context, tap)
        Assert.failDbg()
        return -1
    }

    override fun countChanged(newCount: Int, quashed: Boolean) {
        m_tpHandler.tpmCountChanged(newCount, quashed)
    }

    private fun sendForAddr(
        context: Context, addr: CommsAddrRec,
        conType: CommsConnType, rowID: Long,
        gameID: Int, timestamp: Int,
        buf: ByteArray, streamVers: Int, msgID: String?
    ): Int {
        var nSent = -1
        when (conType) {
            CommsConnType.COMMS_CONN_RELAY -> Log.e(TAG, "sendForAddr(); still sending via RELAY")
            CommsConnType.COMMS_CONN_SMS -> Assert.failDbg()
            //     nSent = NBSProto.sendPacket(
            //     context, addr.sms_phone!!,
            //     gameID, buf, msgID
            // )

            CommsConnType.COMMS_CONN_BT -> nSent =
                BTUtils.sendPacket(context, buf, msgID, addr, gameID)

            CommsConnType.COMMS_CONN_P2P -> nSent = WiDirService
                .sendPacket(context, addr!!.p2p_addr, gameID, buf)

            CommsConnType.COMMS_CONN_NFC -> nSent = NFCUtils.addMsgFor(buf!!, gameID)
            CommsConnType.COMMS_CONN_MQTT -> Assert.failDbg() // comes into transportSendMQTT() now
            else -> Assert.failDbg()
        }
        Log.d(
            TAG, "sendForAddr(typ=%s, len=%d) => %d", conType,
            buf!!.size, nSent
        )
        return nSent
    } /* NPEs in m_selector calls: sometimes my Moment gets into a state
     * where after 15 or so seconds of Crosswords trying to connect to
     * the relay I get a crash.  Logs show it's inside one or both of
     * these Selector methods.  Rebooting the device gets it out of
     * that state, so I suspect it's a but in 2.1 or Samsung's build
     * of it.  Should watch crash reports at developer.android.com and
     * perhaps catch NPEs here just to be safe.  But then do what?
     * Tell user to restart device?
     */

    companion object {
        private val TAG = CommsTransport::class.java.getSimpleName()

        // TransportProcs interface
        private const val TRANSPORT_DOES_NOCONN = true
    }
}
