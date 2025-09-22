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
package org.eehouse.android.xw4

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.text.TextUtils

import org.json.JSONException
import org.json.JSONObject
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.DataInputStream
import java.io.DataOutputStream
import java.io.IOException
import java.io.Serializable

import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.jni.CommsAddrRec
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet
import org.eehouse.android.xw4.jni.CurGameInfo
import org.eehouse.android.xw4.jni.Device
import org.eehouse.android.xw4.jni.GameSummary
import org.eehouse.android.xw4.loc.LocUtils

class NetLaunchInfo : Serializable {
    var gameName: String? = null
    var dict: String? = null
    var isoCodeStr: String? = null // added in version 2
    var forceChannel: Int = 0
    var nPlayersT: Int = 0
    var nPlayersH: Int = 0
    var remotesAreRobots: Boolean = false
    protected var btName: String? = null
    protected var btAddress: String? = null
    protected var p2pMacAddress: String? = null

    // SMS
    @JvmField
    var phone: String? = null
    protected var isGSM: Boolean = false
    protected var osVers: Int = 0

    // MQTT
    protected var mqttDevID: String? = null

    private var _conTypes = 0
    private var gameID = 0
    private var m_valid = false
    private var inviteID: String? = null
    private var dupeMode = false

    constructor() {
        _conTypes = EMPTY_SET
        inviteID = GameUtils.formatGameID(Utils.nextRandomInt())
        forceChannel = 0        // 0 means ANY/comms picks
    }

    private constructor(context: Context, data: String) {
        init(context, data)
    }

    private constructor(bundle: Bundle) {
        isoCodeStr = bundle.getString(MultiService.ISO)
        if (null == isoCodeStr) {
            val lang = bundle.getInt(MultiService.LANG, 0)
            if (0 != lang) {
                val code = Device.lcToLocale(lang)
                isoCodeStr = ISOCode.newIf(code).toString()
            }
        }
        Assert.assertTrueNR(null != isoCodeStr)
        inviteID = bundle.getString(MultiService.INVITEID)
        forceChannel = bundle.getInt(MultiService.FORCECHANNEL)
        dict = bundle.getString(MultiService.DICT)
        gameName = bundle.getString(MultiService.GAMENAME)
        nPlayersT = bundle.getInt(MultiService.NPLAYERST)
        nPlayersH = bundle.getInt(MultiService.NPLAYERSH)
        remotesAreRobots = bundle.getBoolean(MultiService.REMOTES_ROBOTS)
        gameID = bundle.getInt(MultiService.GAMEID)
        btName = bundle.getString(MultiService.BT_NAME)
        btAddress = bundle.getString(MultiService.BT_ADDRESS)
        p2pMacAddress = bundle.getString(MultiService.P2P_MAC_ADDRESS)
        mqttDevID = bundle.getString(MultiService.MQTT_DEVID)

        _conTypes = bundle.getInt(ADDRS_KEY)

        Utils.testSerialization(this)
    }

    constructor(context: Context, data: Uri?) : this() {
        m_valid = false
        data?.let { data ->
            val scheme = data.scheme
            try {
                if ("content" == scheme || "file" == scheme) {
                    val resolver = context.contentResolver
                    val istream = resolver.openInputStream(data)
                    val len = istream!!.available()
                    val buf = ByteArray(len)
                    istream.read(buf)

                    val json = JSONObject(String(buf))
                    inviteID = json.getString(MultiService.INVITEID)
                } else {
                    scheme?.let { // if it's null, not a valid URI for us
                        var pval = data.getQueryParameter(ADDRS_KEY)
                        val hasAddrs = null != pval
                        _conTypes =
                            if (hasAddrs) {
                                Integer.decode(pval)
                            } else {
                                EMPTY_SET
                            }

                        val supported = CommsConnTypeSet.getSupported(context)
                        val addrs = CommsConnTypeSet(_conTypes)
                        for (typ in supported) {
                            if (hasAddrs && !addrs.contains(typ)) {
                                continue
                            }
                            var doAdd: Boolean
                            when (typ) {
                                CommsConnType.COMMS_CONN_BT -> {
                                    btAddress = expand(data.getQueryParameter(BTADDR_KEY))
                                    btName = data.getQueryParameter(BTNAME_KEY)
                                    doAdd = !hasAddrs && null != btAddress
                                }

                                CommsConnType.COMMS_CONN_RELAY -> {
                                    inviteID = data.getQueryParameter(ID_KEY)
                                    doAdd = !hasAddrs// && null != room
                                }

                                CommsConnType.COMMS_CONN_SMS -> {
                                    phone = data.getQueryParameter(PHONE_KEY)
                                    pval = data.getQueryParameter(GSM_KEY)
                                    isGSM = null != pval && 1 == Integer.decode(pval)
                                    pval = data.getQueryParameter(OSVERS_KEY)
                                    if (null != pval) {
                                        osVers = Integer.decode(pval)
                                    }
                                    doAdd = !hasAddrs && null != phone
                                }

                                CommsConnType.COMMS_CONN_P2P -> {
                                    p2pMacAddress = data.getQueryParameter(P2P_MAC_KEY)
                                    doAdd = !hasAddrs && null != p2pMacAddress
                                }

                                CommsConnType.COMMS_CONN_NFC -> doAdd = true
                                CommsConnType.COMMS_CONN_MQTT -> {
                                    mqttDevID = data.getQueryParameter(MQTT_DEVID_KEY)
                                    doAdd = !hasAddrs && null != mqttDevID
                                }

                                else -> {
                                    doAdd = false
                                    Log.d(TAG, "unexpected type: %s", typ)
                                    Assert.failDbg()
                                }
                            }
                            if (doAdd) {
                                addrs.add(typ)
                            }
                        }
                        _conTypes = addrs.toInt()

                        removeUnsupported(supported)

                        Log.d(TAG, "data: %s", data)
                        dict = data.getQueryParameter(WORDLIST_KEY)
                        isoCodeStr = data.getQueryParameter(ISO_KEY)
                        Log.d(TAG, "got isoCodeStr: $isoCodeStr")
                        if (null == isoCodeStr) {
                            val langStr = data.getQueryParameter(LANG_KEY)
                            Log.d(TAG, "langStr: $langStr")
                            if (null != langStr && langStr != "0") {
                                val lang = Integer.decode(langStr)
                                isoCodeStr = Device.lcToLocale(lang).toString()
                            }
                        }
                        Assert.assertTrueNR(null != isoCodeStr) // firing

                        val np = data.getQueryParameter(TOTPLAYERS_KEY)
                        nPlayersT = Integer.decode(np)
                        val nh = data.getQueryParameter(HEREPLAYERS_KEY)
                        nPlayersH = if (nh == null) 1 else Integer.decode(nh)
                        pval = data.getQueryParameter(GID_KEY)
                        gameID = if (null == pval) 0 else Integer.decode(pval)
                        pval = data.getQueryParameter(FORCECHANNEL_KEY)
                        forceChannel = if (null == pval) 0 else Integer.decode(pval)
                        gameName = data.getQueryParameter(NAME_KEY)
                        pval = data.getQueryParameter(DUPMODE_KEY)
                        dupeMode = null != pval && Integer.decode(pval) != 0
                    }
                }
                calcValid()
            } catch (ex: Exception) {
                Log.e(TAG, "%s: (in \"%s\")", ex, data.toString())
                DbgUtils.printStack(TAG, ex)
            }
        }
        calcValid()
    }

    private constructor(
        gamID: Int, gamNam: String?, isoCodeIn: ISOCode?,
        dictName: String?, nPlayers: Int, dupMode: Boolean
    ) : this() {
        Assert.assertTrueNR(null != isoCodeIn)
        gameName = gamNam
        dict = dictName
        isoCodeStr = isoCodeIn.toString()
        nPlayersT = nPlayers
        nPlayersH = 1
        gameID = gamID
        dupeMode = dupMode
    }

    constructor(
        context: Context, summary: GameSummary,
        gi: CurGameInfo, numHere: Int
    ) : this(context, summary, gi) {
        nPlayersH = numHere
        Assert.assertTrueNR( 0 == forceChannel )
    }

    constructor(gi: CurGameInfo) : this(
        gi.gameID, gi.gameName, gi.isoCode(),
        gi.dictName, gi.nPlayers, gi.inDuplicateMode
    )

    constructor(context: Context, summary: GameSummary, gi: CurGameInfo) : this(gi) {
        Log.d(TAG, "<init>(gi: $gi)")
        for (typ in gi.conTypes!!) {
            // Log.d( TAG, "NetLaunchInfo(): got type %s", typ );
            when (typ) {
                CommsConnType.COMMS_CONN_BT -> addBTInfo(context)
                CommsConnType.COMMS_CONN_SMS -> addSMSInfo(context)
                CommsConnType.COMMS_CONN_P2P -> addP2PInfo(context)
                CommsConnType.COMMS_CONN_NFC -> addNFCInfo()
                CommsConnType.COMMS_CONN_MQTT -> addMQTTInfo()
                else -> Assert.failDbg()
            }
        }
    }

    val types: CommsConnTypeSet
        get() = CommsConnTypeSet(_conTypes)

    fun contains(typ: CommsConnType?): Boolean {
        return CommsConnTypeSet(_conTypes).contains(typ)
    }

    fun removeAddress(typ: CommsConnType) {
        val addrs = CommsConnTypeSet(_conTypes)
        addrs.remove(typ)
        _conTypes = addrs.toInt()
    }

    fun isoCode(): ISOCode? {
        return ISOCode.newIf(isoCodeStr)
    }

    fun inviteID(): String {
        var result = inviteID
        if (null == result) {
            result = GameUtils.formatGameID(gameID)
            // Log.d( TAG, "inviteID(): m_inviteID null so substituting %s", result );
        }
        return result!!
    }

    fun gameID(): Int {
        var result = gameID
        if (0 == result) {
            Assert.assertNotNull(inviteID)
            Log.i(TAG, "gameID(): looking at inviteID: %s", inviteID)
            result = inviteID!!.toInt(16)
            // Log.d( TAG, "gameID(): gameID -1 so substituting %d", result );
            gameID = result
        }
        Assert.assertTrue(0 != result)
        return result
    }

    fun putSelf(bundle: Bundle) {
        bundle.putString(MultiService.INVITEID, inviteID)
        val lang = intArrayOf(0)
        if (Device.haveLocaleToLc(isoCodeStr, lang)) {
            bundle.putInt(MultiService.LANG, lang[0])
        }
        bundle.putString(MultiService.ISO, isoCodeStr)
        bundle.putString(MultiService.DICT, dict)
        bundle.putString(MultiService.GAMENAME, gameName)
        bundle.putInt(MultiService.NPLAYERST, nPlayersT)
        bundle.putInt(MultiService.NPLAYERSH, nPlayersH)
        if (remotesAreRobots) {
            bundle.putBoolean(MultiService.REMOTES_ROBOTS, true)
        }
        bundle.putInt(MultiService.GAMEID, gameID())
        bundle.putString(MultiService.BT_NAME, btName)
        bundle.putString(MultiService.BT_ADDRESS, btAddress)
        bundle.putString(MultiService.P2P_MAC_ADDRESS, p2pMacAddress)
        bundle.putInt(MultiService.FORCECHANNEL, forceChannel)
        bundle.putString(MultiService.MQTT_DEVID, mqttDevID)
        if (dupeMode) {
            bundle.putBoolean(MultiService.DUPEMODE, true)
        }

        bundle.putInt(ADDRS_KEY, _conTypes)
    }

    override fun equals(obj: Any?): Boolean {
        var result =
            if ( null != obj && obj is NetLaunchInfo ) {
                val other = obj as NetLaunchInfo
                TextUtils.equals(gameName, other.gameName)
                    && TextUtils.equals(dict, other.dict)
                    && TextUtils.equals(isoCodeStr,other.isoCodeStr)
                    && forceChannel == other.forceChannel
                    && nPlayersT == other.nPlayersT
                    && nPlayersH == other.nPlayersH
                    && dupeMode == other.dupeMode
                    && remotesAreRobots == other.remotesAreRobots
                    // && TextUtils.equals(room, other.room)
                    && TextUtils.equals(btName, other.btName)
                    && TextUtils.equals(btAddress, other.btAddress)
                    && TextUtils.equals(mqttDevID, other.mqttDevID)
                    && TextUtils.equals(p2pMacAddress, other.p2pMacAddress)
                    && TextUtils.equals(phone, other.phone)
                    && isGSM == other.isGSM
                    && osVers == other.osVers
                    && _conTypes == other._conTypes
                    && gameID == other.gameID
                    && _conTypes == other._conTypes
                    && m_valid == other.m_valid
                    && TextUtils.equals(inviteID, other.inviteID)
            } else false
        return result
    }

    fun makeLaunchJSON(): String {
        var result: String? = null
        try {
            val obj = JSONObject()
                .put(ADDRS_KEY, _conTypes)
                .put(MultiService.DICT, dict)
                .put(MultiService.GAMENAME, gameName)
                .put(MultiService.NPLAYERST, nPlayersT)
                .put(MultiService.NPLAYERSH, nPlayersH)
                .put(MultiService.REMOTES_ROBOTS, remotesAreRobots)
                .put(MultiService.GAMEID, gameID())
                .put(MultiService.FORCECHANNEL, forceChannel)

            val lang = intArrayOf(0)
            if (Device.haveLocaleToLc(isoCodeStr, lang)) {
                obj.put(MultiService.LANG, lang[0])
            }
            obj.put(MultiService.ISO, isoCodeStr)

            if (dupeMode) {
                obj.put(MultiService.DUPEMODE, dupeMode)
            }

            val addrs = CommsConnTypeSet(_conTypes)
            if (addrs.contains(CommsConnType.COMMS_CONN_RELAY)) {
                obj.put(MultiService.INVITEID, inviteID)
            }

            if (addrs.contains(CommsConnType.COMMS_CONN_BT)) {
                obj.put(MultiService.BT_NAME, btName)
                if (!BTUtils.isBogusAddr(btAddress)) {
                    obj.put(MultiService.BT_ADDRESS, btAddress)
                }
            }
            if (addrs.contains(CommsConnType.COMMS_CONN_SMS)) {
                obj.put(PHONE_KEY, phone)
                    .put(GSM_KEY, isGSM)
                    .put(OSVERS_KEY, osVers)
            }
            if (addrs.contains(CommsConnType.COMMS_CONN_P2P)) {
                obj.put(P2P_MAC_KEY, p2pMacAddress)
            }

            if (addrs.contains(CommsConnType.COMMS_CONN_MQTT)) {
                obj.put(MQTT_DEVID_KEY, mqttDevID)
            }
            result = obj.toString()
        } catch (jse: JSONException) {
            Log.ex(TAG, jse)
        }
        // Log.d( TAG, "makeLaunchJSON() => %s", result );
        return result!!
    }

    fun makeAddrRec(context: Context): CommsAddrRec {
        val result = CommsAddrRec()
        val addrs = CommsConnTypeSet(_conTypes)
        for (typ in addrs.types) {
            result.conTypes!!.add(typ)
            when (typ) {
                CommsConnType.COMMS_CONN_RELAY -> Assert.failDbg()
                CommsConnType.COMMS_CONN_BT -> result.setBTParams(btAddress, btName)
                CommsConnType.COMMS_CONN_SMS -> result.setSMSParams(phone)
                CommsConnType.COMMS_CONN_P2P -> result.setP2PParams(p2pMacAddress)
                CommsConnType.COMMS_CONN_NFC -> {}
                CommsConnType.COMMS_CONN_MQTT -> result.setMQTTParams(mqttDevID)
                else -> Assert.failDbg()
            }
        }

        return result
    }

    @Throws(JSONException::class)
    private fun init(context: Context, data: String) {
        val supported = CommsConnTypeSet.getSupported(context)
        val json = JSONObject(data)

        val flags = json.optInt(ADDRS_KEY, -1)
        val hasAddrs = -1 != flags
        _conTypes = if (hasAddrs) flags else EMPTY_SET

        isoCodeStr = json.optString(MultiService.ISO, null)
        if (null == isoCodeStr) {
            val lang = json.optInt(MultiService.LANG, 0)
            if (0 != lang) {
                isoCodeStr = Device.lcToLocale(lang)
            }
        }
        Assert.assertTrueNR(null != isoCodeStr)

        forceChannel = json.optInt(MultiService.FORCECHANNEL, 0)
        dupeMode = json.optBoolean(MultiService.DUPEMODE, false)
        dict = json.optString(MultiService.DICT)
        gameName = json.optString(MultiService.GAMENAME)
        nPlayersT = json.optInt(MultiService.NPLAYERST, -1)
        nPlayersH = json.optInt(MultiService.NPLAYERSH, 1) // absent ok
        remotesAreRobots = json.optBoolean(MultiService.REMOTES_ROBOTS, false)
        gameID = json.optInt(MultiService.GAMEID, 0)

        // Try each type
        val addrs = CommsConnTypeSet(_conTypes)
        for (typ in supported) {
            if (hasAddrs && !addrs.contains(typ)) {
                continue
            }
            val doAdd =
                when (typ) {
                    CommsConnType.COMMS_CONN_BT -> {
                        btAddress = json.optString(MultiService.BT_ADDRESS)
                        btName = json.optString(MultiService.BT_NAME)
                        !hasAddrs && !TextUtils.isEmpty(btName)
                    }

                    CommsConnType.COMMS_CONN_RELAY -> {
                        inviteID = json.optString(MultiService.INVITEID)
                        !hasAddrs//  && !TextUtils.isEmpty(room)
                    }

                    CommsConnType.COMMS_CONN_SMS -> {
                        phone = json.optString(PHONE_KEY)
                        isGSM = json.optBoolean(GSM_KEY, false)
                        osVers = json.optInt(OSVERS_KEY, 0)
                        !hasAddrs && !TextUtils.isEmpty(phone)
                    }

                    CommsConnType.COMMS_CONN_P2P -> {
                        p2pMacAddress = json.optString(P2P_MAC_KEY)
                        !hasAddrs && null != p2pMacAddress
                    }

                    CommsConnType.COMMS_CONN_NFC -> NFCUtils.nfcAvail(context)[0]
                    CommsConnType.COMMS_CONN_MQTT -> {
                        mqttDevID = json.optString(MQTT_DEVID_KEY)
                        null != mqttDevID
                    }

                    else -> {Assert.failDbg(); false}
                }
            if (doAdd) {
                addrs.add(typ)
            }
        }

        _conTypes = addrs.toInt()
        removeUnsupported(supported)

        calcValid()
    }

    private fun appendInt(ub: Uri.Builder, key: String, value: Int) {
        ub.appendQueryParameter(key, String.format("%d", value))
    }

    fun makeLaunchUri(context: Context): Uri {
        var host: String? = LocUtils.getString(context, R.string.invite_host)
        host = NetUtils.forceHost(host)
        val ub = Uri.Builder()
            .scheme("https")
            .path(
                String.format(
                    "//%s%s", host,
                    LocUtils.getString(
                        context,
                        R.string.invite_prefix
                    )
                )
            )

        // We'll use lang rather than ISO IFF we have it.
        val lang = intArrayOf(0)
        Assert.assertTrueNR(null != isoCodeStr)
        if (Device.haveLocaleToLc(isoCodeStr, lang)) {
            appendInt(ub, LANG_KEY, lang[0])
        }
        ub.appendQueryParameter(ISO_KEY, isoCodeStr)
        appendInt(ub, TOTPLAYERS_KEY, nPlayersT)
        appendInt(ub, HEREPLAYERS_KEY, nPlayersH)
        appendInt(ub, GID_KEY, gameID())
        appendInt(ub, FORCECHANNEL_KEY, forceChannel)
        appendInt(ub, ADDRS_KEY, _conTypes)
        ub.appendQueryParameter(NAME_KEY, gameName)
        if (dupeMode) {
            appendInt(ub, DUPMODE_KEY, 1)
        }

        if (null != dict) {
            ub.appendQueryParameter(WORDLIST_KEY, dict)
        }

        val addrs = CommsConnTypeSet(_conTypes)
        if (addrs.contains(CommsConnType.COMMS_CONN_RELAY)) {
            ub.appendQueryParameter(ID_KEY, inviteID)
        }
        if (addrs.contains(CommsConnType.COMMS_CONN_BT)) {
            if (null != btAddress) {
                ub.appendQueryParameter(BTADDR_KEY, shorten(btAddress!!))
            }
            ub.appendQueryParameter(BTNAME_KEY, btName)
        }
        if (addrs.contains(CommsConnType.COMMS_CONN_SMS)) {
            ub.appendQueryParameter(PHONE_KEY, phone)
            appendInt(ub, GSM_KEY, (if (isGSM) 1 else 0))
            appendInt(ub, OSVERS_KEY, osVers)
        }
        if (addrs.contains(CommsConnType.COMMS_CONN_P2P)) {
            ub.appendQueryParameter(P2P_MAC_KEY, p2pMacAddress)
        }
        if (addrs.contains(CommsConnType.COMMS_CONN_MQTT)) {
            ub.appendQueryParameter(MQTT_DEVID_KEY, mqttDevID)
        }
        val result = ub.build()

        if (BuildConfig.DEBUG) { // Test...
            Log.i(TAG, "testing %s...", result.toString())
            val instance = NetLaunchInfo(context, result)
            Assert.assertTrue(instance.isValid)
        }

        return result
    }

    private fun add(typ: CommsConnType) {
        val addrs = CommsConnTypeSet(_conTypes)
        addrs.add(typ)
        _conTypes = addrs.toInt()
    }

    fun addBTInfo(context: Context) {
        val got = BTUtils.getBTNameAndAddress(context)
        if (null != got) {
            btName = got[0]
            btAddress = got[1]
            add(CommsConnType.COMMS_CONN_BT)
        } else {
            Log.w(TAG, "addBTInfo(): no BT info available")
        }
    }

    fun addSMSInfo(context: Context) {
        val pi = SMSPhoneInfo.get(context)
        if (null != pi) {
            phone = pi.number
            isGSM = pi.isGSM

            osVers = Build.VERSION.SDK.toInt()

            add(CommsConnType.COMMS_CONN_SMS)
        }
    }

    fun addP2PInfo(context: Context) {
        p2pMacAddress = WiDirService.getMyMacAddress(context)
        add(CommsConnType.COMMS_CONN_P2P)
    }

    fun addNFCInfo() {
        add(CommsConnType.COMMS_CONN_NFC)
    }

    fun addMQTTInfo() {
        add(CommsConnType.COMMS_CONN_MQTT)
        mqttDevID = MQTTUtils.getMQTTDevID()
    }

    val isValid: Boolean
        get() {
            calcValid() // this isn't always called. Likely should
            // remove it as it's a stupid optimization
            // Log.d( TAG, "NetLaunchInfo(%s).isValid() => %b", this, m_valid );
            return m_valid
        }

    fun setRemotesAreRobots(newVal: Boolean): NetLaunchInfo {
        remotesAreRobots = newVal
        return this
    }

    override fun toString(): String {
        return makeLaunchJSON()
    }

    fun asByteArray(): ByteArray? {
        var result: ByteArray? = null
        try {
            val bas = ByteArrayOutputStream()
            val das = DataOutputStream(bas)
            das.writeUTF(makeLaunchJSON())
            result = bas.toByteArray()
        } catch (ex: IOException) {
            Assert.failDbg()
        }
        return result
    }

    private fun hasCommon(): Boolean {
        val good = null != dict && null != isoCodeStr && 0 < nPlayersT && 0 != gameID()
        // Log.d( TAG, "hasCommon() => %b", good );
        return good
    }

    private fun removeUnsupported(supported: List<CommsConnType>) {
        val addrs = CommsConnTypeSet(_conTypes) // , true );
        val iter = addrs.iterator()
        while (iter.hasNext()) {
            val typ = iter.next()
            if (!supported.contains(typ)) {
                Log.d(TAG, "removeUnsupported(): removing %s", typ)
                iter.remove()
            }
        }
        _conTypes = addrs.toInt()
    }

    private fun shorten(addr: String): String? {
        var result: String? = null
        if (!TextUtils.isEmpty(addr)) {
            val pairs = TextUtils.split(addr, ":")
            result = TextUtils.join("", pairs)
        }
        return result
    }

    private fun expand(addr: String?): String? {
        var result: String? = null
        if (null != addr && 12 == addr.length) {
            val pairs = arrayOfNulls<String>(6)
            for (ii in 0..5) {
                val start = ii * 2
                pairs[ii] = addr.substring(start, start + 2)
            }

            result = TextUtils.join(":", pairs)
        }
        return result
    }

    private fun calcValid() {
        var valid = hasCommon()
        // Log.d( TAG, "calcValid(%s); valid (so far): %b", this, valid );
        if (valid) {
            val iter
                    : Iterator<CommsConnType> = CommsConnTypeSet(_conTypes).iterator()
            while (valid && iter.hasNext()) {
                val typ = iter.next()
                valid = when (typ) {
                    CommsConnType.COMMS_CONN_RELAY -> true
                    CommsConnType.COMMS_CONN_BT -> null != btName
                    CommsConnType.COMMS_CONN_SMS -> null != phone && 0 < osVers
                    CommsConnType.COMMS_CONN_MQTT -> null != mqttDevID
                    else -> {Log.d(TAG, "calcValid(): unexpected typ $typ"); true}
                }
                if (!valid) {
                    Log.d(TAG, "valid after %s: %b", typ, valid)
                }
            }
        }
        m_valid = valid

        Utils.testSerialization(this)
    }

    companion object {
        private val TAG: String = NetLaunchInfo::class.java.simpleName
        private const val ADDRS_KEY = "ad"
        private const val PHONE_KEY = "phn"
        private const val GSM_KEY = "gsm"
        private const val OSVERS_KEY = "osv"
        private const val BTADDR_KEY = "btas"
        private const val BTNAME_KEY = "btn"
        private const val ID_KEY = "id"
        private const val WORDLIST_KEY = "wl"
        private const val LANG_KEY = "lang"
        private const val ISO_KEY = "iso"
        private const val TOTPLAYERS_KEY = "np"
        private const val HEREPLAYERS_KEY = "nh"
        private const val GID_KEY = "gid"
        private const val FORCECHANNEL_KEY = "fc"
        private const val NAME_KEY = "nm"
        private const val P2P_MAC_KEY = "p2"
        private const val MQTT_DEVID_KEY = "r2id"
        private const val DUPMODE_KEY = "du"

        private val EMPTY_SET = CommsConnTypeSet().toInt()

        fun makeFrom(bundle: Bundle): NetLaunchInfo? {
            var nli: NetLaunchInfo? = null
            if (0 != bundle.getInt(MultiService.LANG)
                || null != bundle.getString(MultiService.ISO)
            ) { // quick test: valid?
                nli = NetLaunchInfo(bundle)
                nli.calcValid()
                if (!nli.isValid) {
                    nli = null
                }
            }
            return nli
        }

        fun makeFrom(context: Context, data: String): NetLaunchInfo? {
            val nli =
                try {
                    NetLaunchInfo(context, data)
                } catch (jse: JSONException) {
                    // Log.ex(TAG, jse)
                    null
                }
            return nli
        }

        fun makeFrom(context: Context, data: Uri): NetLaunchInfo? {
            val nli =
                try {
                    val nli = NetLaunchInfo(context, data)
                    if (nli.isValid) nli else null
                } catch (jse: JSONException) {
                    Log.ex(TAG, jse)
                    null
                }
            return nli
        }

        fun makeFrom(context: Context, data: ByteArray?): NetLaunchInfo? {
            var nli: NetLaunchInfo? = null
            try {
                val bais = ByteArrayInputStream(data)
                val dis = DataInputStream(bais)
                val nliData = dis.readUTF()
                nli = makeFrom(context, nliData)
                Assert.assertTrueNR(null != nli!!.isoCodeStr)
            } catch (ex: IOException) {
                Log.d(TAG, "not an nli")
            }
            return nli
        }

        fun putExtras(intent: Intent?, gameID: Int, btAddr: String?) {
            Assert.failDbg()
        }
    }
}
