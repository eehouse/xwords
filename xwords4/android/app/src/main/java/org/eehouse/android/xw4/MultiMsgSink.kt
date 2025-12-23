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
import org.eehouse.android.xw4.jni.Device

open class MultiMsgSink @JvmOverloads constructor(
    private val m_context: Context, // rowID is used as token to identify game on relay.  Anything that
    // uniquely identifies a game on a device would work
    var rowID: Long = 0
) {
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
        Assert.failDbg()
        return -1
        // return NBSProto.sendPacket(m_context, addr.sms_phone!!, gameID, buf, msgID)
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

    companion object {
        private val TAG: String = MultiMsgSink::class.java.simpleName
        fun sendInvite(
            context: Context, rowid: Long,
            addr: CommsAddrRec, typ: CommsConnType,
            nli: NetLaunchInfo, timestamp: Int
        ): Boolean {
            Assert.failDbg()
            Log.d(TAG, "sendInvite(to=%s, typ=%s, nli=%s)", addr, typ, nli)
            var success = false
            val means: InviteMeans? = null
            val target: String? = null
            when (typ) {
                CommsConnType.COMMS_CONN_MQTT -> Assert.failDbg()
                CommsConnType.COMMS_CONN_SMS -> Assert.failDbg()
                //     if (XWPrefs.getNBSEnabled(context)) {
                //     NBSProto.inviteRemote(context, addr.sms_phone!!, nli)
                //     success = true
                // }

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
                recordInviteSent(context, rowid, means, target, true)
            }

            Log.d(TAG, "sendInvite(%s, %s) => %b", typ, addr, success)
            return success
        }
    }
}
