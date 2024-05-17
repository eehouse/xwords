/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */ /*
 * Copyright 2009-2010 by Eric House (xwords@eehouse.org).  All
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
package org.eehouse.android.xw4.jni

import org.eehouse.android.xw4.NetLaunchInfo
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.XwJNI.TopicsAndPackets

interface TransportProcs {
    val flags: Int

    fun transportSendMsg(
        buf: ByteArray?, streamVers: Int, msgNo: String?,
        addr: CommsAddrRec?, conType: CommsConnType?,
        gameID: Int, timestamp: Int
    ): Int

    fun transportSendMQTT(tap: TopicsAndPackets): Int

    fun transportSendInvt(
        addr: CommsAddrRec?, conType: CommsConnType?,
        nli: NetLaunchInfo?, timestamp: Int
    ): Boolean

    fun countChanged(newCount: Int, quashed: Boolean)

    interface TPMsgHandler {
        fun tpmCountChanged(newCount: Int, quashed: Boolean)
    }

    companion object {
        const val COMMS_XPORT_FLAGS_NONE = 0
        const val COMMS_XPORT_FLAGS_HASNOCONN = 1
    }
}
