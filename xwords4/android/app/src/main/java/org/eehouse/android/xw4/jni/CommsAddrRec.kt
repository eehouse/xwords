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
package org.eehouse.android.xw4.jni

import android.content.Context
import android.text.TextUtils

import java.io.Serializable
import java.net.InetAddress

import org.eehouse.android.xw4.Assert
import org.eehouse.android.xw4.BTUtils
import org.eehouse.android.xw4.BuildConfig
import org.eehouse.android.xw4.Log
import org.eehouse.android.xw4.MQTTUtils
import org.eehouse.android.xw4.NFCUtils
import org.eehouse.android.xw4.R
import org.eehouse.android.xw4.SMSPhoneInfo
import org.eehouse.android.xw4.Utils
import org.eehouse.android.xw4.WiDirService
import org.eehouse.android.xw4.WiDirWrapper
import org.eehouse.android.xw4.XWPrefs
import org.eehouse.android.xw4.loc.LocUtils

class CommsAddrRec : Serializable {
    enum class CommsConnType(val isSelectable: Boolean = true) {
        _COMMS_CONN_NONE,
        COMMS_CONN_IR,
        COMMS_CONN_IP_DIRECT,
        COMMS_CONN_RELAY(!BuildConfig.NO_NEW_RELAY),
        COMMS_CONN_BT,
        COMMS_CONN_SMS,
        COMMS_CONN_P2P,
        COMMS_CONN_NFC(false),
        COMMS_CONN_MQTT(BuildConfig.NO_NEW_RELAY||MQTTUtils.MQTTSupported());

        fun longName(context: Context): String {
            val id = when (this) {
                COMMS_CONN_RELAY -> R.string.connstat_relay
                COMMS_CONN_BT -> R.string.invite_choice_bt
                COMMS_CONN_SMS -> R.string.invite_choice_data_sms
                COMMS_CONN_P2P -> R.string.invite_choice_p2p
                COMMS_CONN_NFC -> R.string.invite_choice_nfc
                COMMS_CONN_MQTT -> R.string.invite_choice_mqtt
                else -> { Assert.failDbg(); 0 }
            }
            return if (0 == id) toString() else LocUtils.getString(context, id)
        }

        fun shortName(): String {
            val parts = TextUtils.split(toString(), "_")
            return parts[parts.size - 1]
        }
    }

    // Pairs how and name of device in that context
    class ConnExpl(val mType: CommsConnType, val mName: String) : Serializable {
        fun getUserExpl(context: Context): String {
            Assert.assertTrueNR(BuildConfig.NON_RELEASE)
            return String.format("(Msg src: {%s: %s})", mType, mName)
        }
    }

    class CommsConnTypeSet @JvmOverloads constructor(inBits: Int = BIT_VECTOR_MASK) :
        HashSet<CommsConnType>() {
        fun toInt(): Int {
            var result = BIT_VECTOR_MASK
            val iter: Iterator<CommsConnType?> = iterator()
            while (iter.hasNext()) {
                val typ = iter.next()
                result = result or (1 shl typ!!.ordinal - 1)
            }
            return result
        }

        val types: Array<CommsConnType>
            // Called from jni world, where making and using an iterator is too
            get() = toArray(s_hint)

        override fun add(typ: CommsConnType): Boolean {
            // DbgUtils.logf( "CommsConnTypeSet.add(%s)", typ.toString() );
            // Assert.assertFalse( CommsConnType._COMMS_CONN_NONE == typ );
            return if (CommsConnType._COMMS_CONN_NONE == typ) true else super.add(typ)
        }

        override fun toString(): String {
            val result =
                if (BuildConfig.NON_RELEASE) {
                    val tmp = types.map{ "$it" }
                    "[" + TextUtils.join(",", tmp) + "]"
                } else {
                    super.toString()
                }
            return result
        }

        fun toString(context: Context, longVersion: Boolean): String {
            val result: String
            val types = types
            result = if (0 == types.size) {
                LocUtils.getString(context, R.string.note_none)
            } else {
                val strs: MutableList<String?> = ArrayList()
                for (typ in types) {
                    if (typ.isSelectable) {
                        val str = if (longVersion) typ.longName(context) else typ.shortName()
                        strs.add(str)
                    }
                }
                val sep = if (longVersion) " + " else ","
                TextUtils.join(sep, strs)
            }
            return result
        }

        init {
            var isVector = 0 != BIT_VECTOR_MASK and inBits
            val bits = inBits and BIT_VECTOR_MASK.inv()
            val values = CommsConnType.entries.toTypedArray()
            // Deal with games saved before I added the BIT_VECTOR_MASK back
            // in. This should be removable before ship. Or later of course.
            if (!isVector && bits >= values.size) {
                isVector = true
            }
            if (isVector) {
                for (value in values) {
                    val ord = value.ordinal
                    if (0 != bits and (1 shl ord - 1)) {
                        add(value)
                        if (BuildConfig.NON_RELEASE
                            && CommsConnType.COMMS_CONN_RELAY == value
                        ) {
                            // I've seen this....
                            Log.e(TAG, "still have RELAY bit")
                            // DbgUtils.printStack( TAG );
                        }
                    }
                }
            } else if (bits < values.size) { // don't crash
                add(values[bits])
            } else {
                Log.e(TAG, "<init>: bad bits value: 0x%x", inBits)
            }
        }

        companion object {
            private const val BIT_VECTOR_MASK = 0x8000

            /**
             * Return supported types in display order, i.e. with the easiest to
             * use or most broadly useful first. DATA_SMS comes last because it
             * depends on permissions that are banned on PlayStore variants of the
             * game.
             *
             * @return ordered list of types supported by this device as
             * configured.
             */
            fun getSupported(context: Context): List<CommsConnType> {
                val supported: MutableList<CommsConnType> = ArrayList()
                supported.add(CommsConnType.COMMS_CONN_RELAY)
                if ( MQTTUtils.MQTTSupported()) {
                    supported.add(CommsConnType.COMMS_CONN_MQTT)
                }
                if (BTUtils.BTAvailable()) {
                    supported.add(CommsConnType.COMMS_CONN_BT)
                }
                if (WiDirWrapper.enabled()) {
                    supported.add(CommsConnType.COMMS_CONN_P2P)
                }
                if (Utils.isGSMPhone(context)) {
                    supported.add(CommsConnType.COMMS_CONN_SMS)
                }
                if (NFCUtils.nfcAvail(context).get(0)) {
                    supported.add(CommsConnType.COMMS_CONN_NFC)
                }
                return supported
            }

            fun removeUnsupported(
                context: Context,
                set: CommsConnTypeSet
            ) {
                // Remove anything no longer supported. This probably only
                // happens when key_force_radio is being messed with
                val supported = getSupported(context)
                for (typ in set.types) {
                    if (!supported.contains(typ)) {
                        set.remove(typ)
                    }
                }
            }

            private val s_hint = arrayOfNulls<CommsConnType>(0)
        }
    }

    @JvmField
    var conTypes: CommsConnTypeSet? = null

    // relay case
    var ip_relay_invite: String? = null
    var ip_relay_hostName: String? = null
    var ip_relay_ipAddr: InetAddress? = null // a cache, maybe unused in java
    var ip_relay_port = 0
    var ip_relay_seeksPublicRoom = false
    var ip_relay_advertiseRoom = false

    // bt case
    @JvmField
    var bt_hostName: String? = null
    @JvmField
    var bt_btAddr: String? = null

    // sms case
    @JvmField
    var sms_phone: String? = null
    var sms_port = 0 // SMS port, if they still use those

    // wifi-direct
    @JvmField
    var p2p_addr: String? = null

    // mqtt
    @JvmField
    var mqtt_devID: String? = null

    constructor(cTyp: CommsConnType) : this() {
        conTypes!!.add(cTyp)
    }

    constructor() {
        conTypes = CommsConnTypeSet()
    }

    constructor(types: CommsConnTypeSet) : this() {
        conTypes!!.addAll(types)
    }

    constructor(host: String?, port: Int) : this(CommsConnType.COMMS_CONN_RELAY) {
        setRelayParams(host, port)
    }

    constructor(btName: String?, btAddr: String?) : this(CommsConnType.COMMS_CONN_BT) {
        setBTParams(btAddr, btName)
    }

    constructor(phone: String?) : this(CommsConnType.COMMS_CONN_SMS) {
        sms_phone = phone
        sms_port = 2 // something other that 0 (need to fix comms)
    }

    constructor(src: CommsAddrRec) {
        copyFrom(src)
    }

    operator fun contains(typ: CommsConnType): Boolean {
        return null != conTypes && conTypes!!.contains(typ)
    }

    fun setRelayParams(host: String?, port: Int, room: String?) {
        setRelayParams(host, port)
        ip_relay_invite = room
    }

    fun setRelayParams(host: String?, port: Int) {
        ip_relay_hostName = host
        ip_relay_port = port
        ip_relay_seeksPublicRoom = false
        ip_relay_advertiseRoom = false
    }

    fun setBTParams(btAddr: String?, btName: String?): CommsAddrRec {
        bt_hostName = btName
        if (!BTUtils.isBogusAddr(btAddr)) {
            bt_btAddr = btAddr
        }
        return this
    }

    fun setSMSParams(phone: String?): CommsAddrRec {
        sms_phone = phone
        sms_port = 1 // so don't assert in comms....
        return this
    }

    fun setP2PParams(macAddress: String?): CommsAddrRec {
        p2p_addr = macAddress
        return this
    }

    fun setMQTTParams(devID: String?): CommsAddrRec {
        mqtt_devID = devID
        return this
    }

    fun populate(context: Context, newTypes: CommsConnTypeSet): CommsAddrRec {
        val supported = CommsConnTypeSet.getSupported(context)
        for (typ in newTypes.types) {
            if (supported.contains(typ) && !conTypes!!.contains(typ)) {
                conTypes!!.add(typ)
                addTypeDefaults(context, typ)
            }
        }
        return this
    }

    fun populate(context: Context): CommsAddrRec {
        for (typ in conTypes!!.types) {
            addTypeDefaults(context, typ)
        }
        return this
    }

    fun remove(typ: CommsConnType) {
        conTypes!!.remove(typ)
    }

    fun changesMatter(other: CommsAddrRec): Boolean {
        var matter = conTypes != other.conTypes
        val iter: Iterator<CommsConnType?> = conTypes!!.iterator()
        while (!matter && iter.hasNext()) {
            val conType = iter.next()
            when (conType) {
                CommsConnType.COMMS_CONN_RELAY -> matter =
                    (null == ip_relay_invite || ip_relay_invite != other.ip_relay_invite
                            || ip_relay_hostName != other.ip_relay_hostName || ip_relay_port != other.ip_relay_port)

                else -> Log.w(
                    TAG, "changesMatter: not handling case: %s",
                    conType.toString()
                )
            }
        }
        return matter
    }

    override fun toString(): String {
        return if (BuildConfig.NON_RELEASE) {
            val list = conTypes!!.map { typ ->
                when (typ) {
                    CommsConnType.COMMS_CONN_MQTT -> String.format("%s: %s", typ, mqtt_devID)
                    CommsConnType.COMMS_CONN_SMS ->
                        String.format(
                            "%s: {phone: %s, port: %d}",
                            typ, sms_phone, sms_port
                        )
                    else -> typ.toString()
                }
            }
            "{" + TextUtils.join(",", list) + "}"
        } else {
            super.toString()
        }
    }

    private fun copyFrom(src: CommsAddrRec) {
        conTypes = src.conTypes
        ip_relay_invite = src.ip_relay_invite
        ip_relay_hostName = src.ip_relay_hostName
        ip_relay_port = src.ip_relay_port
        ip_relay_seeksPublicRoom = src.ip_relay_seeksPublicRoom
        ip_relay_advertiseRoom = src.ip_relay_advertiseRoom
        bt_hostName = src.bt_hostName
        bt_btAddr = src.bt_btAddr
        sms_phone = src.sms_phone
        sms_port = src.sms_port
        p2p_addr = src.p2p_addr
        mqtt_devID = src.mqtt_devID
    }

    private fun addTypeDefaults(context: Context, typ: CommsConnType) {
        when (typ) {
            CommsConnType.COMMS_CONN_RELAY -> Assert.failDbg()
            CommsConnType.COMMS_CONN_BT -> {
                val strs: Array<String?>? = BTUtils.getBTNameAndAddress(context)
                if (null != strs) {
                    bt_hostName = strs[0]
                    bt_btAddr = strs[1]
                }
            }

            CommsConnType.COMMS_CONN_SMS -> {
                val pi: SMSPhoneInfo? = SMSPhoneInfo.get(context)
                // Do we have phone permission? If not, shouldn't be set at all!
                if (null != pi) {
                    sms_phone = pi.number
                    sms_port = 3 // fix comms already...
                }
            }

            CommsConnType.COMMS_CONN_P2P -> p2p_addr = WiDirService.getMyMacAddress(context)
            CommsConnType.COMMS_CONN_MQTT -> mqtt_devID = MQTTUtils.getMQTTDevID()
            CommsConnType.COMMS_CONN_NFC -> {}
            else -> Assert.failDbg()
        }
    }

    companion object {
        private val TAG = CommsAddrRec::class.java.getSimpleName()
        fun getSelfAddr(context: Context, types: CommsConnTypeSet): CommsAddrRec {
            return CommsAddrRec()
                .populate(context, types)
        }

        fun getSelfAddr(context: Context): CommsAddrRec {
            val types: CommsConnTypeSet = XWPrefs.getAddrTypes(context)
            val result = getSelfAddr(context, types)
            Log.d(TAG, "getSelfAddr() => %s", result)
            return result
        }

        fun getSelfAddr(context: Context, gi: CurGameInfo): CommsAddrRec? {
            val addrRec =
                if (CurGameInfo.DeviceRole.ROLE_STANDALONE == gi.deviceRole) null as CommsAddrRec?
                else getSelfAddr(context)
            return addrRec
        }
    }
}
