/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2016 - 2024 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.Activity
import android.app.Service
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.NetworkInfo
import android.net.wifi.WpsInfo
import android.net.wifi.p2p.WifiP2pConfig
import android.net.wifi.p2p.WifiP2pDevice
import android.net.wifi.p2p.WifiP2pDeviceList
import android.net.wifi.p2p.WifiP2pInfo
import android.net.wifi.p2p.WifiP2pManager
import android.net.wifi.p2p.WifiP2pManager.ConnectionInfoListener
import android.net.wifi.p2p.WifiP2pManager.DnsSdServiceResponseListener
import android.net.wifi.p2p.WifiP2pManager.DnsSdTxtRecordListener
import android.net.wifi.p2p.WifiP2pManager.GroupInfoListener
import android.net.wifi.p2p.WifiP2pManager.PeerListListener
import android.net.wifi.p2p.nsd.WifiP2pDnsSdServiceInfo
import android.net.wifi.p2p.nsd.WifiP2pDnsSdServiceRequest
import android.os.Handler
import android.os.Looper
import android.os.Parcelable
import android.text.TextUtils

import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.io.IOException
import java.net.InetAddress
import java.net.ServerSocket

import org.eehouse.android.xw4.BiDiSockWrap.Iface
import org.eehouse.android.xw4.MultiService.DictFetchOwner
import org.eehouse.android.xw4.MultiService.MultiEvent
import org.eehouse.android.xw4.XWPacket.CMD
import org.eehouse.android.xw4.XWServiceHelper.ReceiveResult
import org.eehouse.android.xw4.jni.CommsAddrRec
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.loc.LocUtils

class WiDirService : XWService() {
    private enum class P2PAction {
        _NONE,
        GOT_MSG,
        GOT_INVITE,
        GAME_GONE,
    }

    private var m_sink: P2pMsgSink? = null
    private var mHelper: WiDirServiceHelper? = null

    interface DevSetListener {
        fun setChanged(macToName: Map<String, String>)
    }

    override fun onCreate() {
        m_sink = P2pMsgSink()
        mHelper = WiDirServiceHelper(this)
    }

    override fun onStartCommand(intent: Intent, flags: Int, startId: Int): Int {
        val result: Int

        if (enabled() && null != intent) {
            result = START_STICKY

            val ordinal = intent.getIntExtra(KEY_CMD, -1)
            if (-1 != ordinal) {
                val cmd = P2PAction.entries[ordinal]
                when (cmd) {
                    P2PAction.GOT_MSG -> {
                        handleGotMessage(intent)
                        updateStatusIn(true)
                    }

                    P2PAction.GOT_INVITE -> handleGotInvite(intent)
                    P2PAction.GAME_GONE -> handleGameGone(intent)
                    else -> {Log.d(TAG, "unexpected cmd $cmd"); Assert.failDbg()}
                }
            }
        } else {
            result = START_NOT_STICKY
            stopSelf(startId)
        }

        return result
    }

    // See: http://stackoverflow.com/questions/26300889/wifi-p2p-service-discovery-works-intermittently
    private class ServiceDiscoverer : Runnable, WifiP2pManager.ActionListener {
        private var m_curState = State.START
        private var m_lastGoodState = State.START
        private var m_lastBadState = State.START
        private val m_channel = sChannel
        private val m_handler = Handler()
        private val m_mgr = mgr
        private var m_failures: IntArray? = null
        private var m_lastSucceeded = false

        enum class State {
            START,
            CLEAR_LOCAL_SERVICES,
            ADD_LOCAL_SERVICES,
            CLEAR_SERVICES_REQUESTS,
            ADD_SERVICE_REQUEST,
            DISCOVER_PEERS,
            PEER_DISCOVERY_STARTED,
            DONE,
        }

        fun stateToString(): String {
            return String.format(
                "last good: %s(%d); last bad: %s(%d); success last: %b",
                m_lastGoodState.toString(), m_lastGoodState.ordinal,
                m_lastBadState.toString(), m_lastBadState.ordinal,
                m_lastSucceeded
            )
        }

        fun restart() {
            m_curState = State.START
            schedule(0)
        }

        private fun schedule(waitSeconds: Int) {
            Log.d(
                TAG, "scheduling %s in %d seconds",
                m_curState.toString(), waitSeconds
            )
            m_handler.removeCallbacks(this) // remove any others
            m_handler.postDelayed(this, (waitSeconds * 1000).toLong())
        }

        // ActionListener interface
        override fun onSuccess() {
            m_lastSucceeded = true
            m_lastGoodState = m_curState
            Log.d(TAG, "onSuccess(): state %s done", m_curState.toString())
            m_curState = State.entries[m_curState.ordinal + 1]
            schedule(0)
        }

        override fun onFailure(code: Int) {
            m_lastSucceeded = false
            m_lastBadState = m_curState

            val count = ++m_failures!![m_curState.ordinal]
            var codeStr: String? = null
            when (code) {
                WifiP2pManager.ERROR -> {
                    m_curState = State.START
                    schedule(10)
                    codeStr = "ERROR"
                }

                WifiP2pManager.P2P_UNSUPPORTED ->                 // Assert.fail();  // fires on emulator
                    codeStr = "UNSUPPORTED"

                WifiP2pManager.BUSY -> {
                    if (!sEnabled) {
                        Log.d(TAG, "onFailure(): no wifi, so stopping machine")
                    } else {
                        if (8 < count) {
                            Log.d(TAG, "too many errors; restarting machine")
                            m_curState = State.START
                        }
                        schedule(10)
                        codeStr = "BUSY"
                    }
                }
            }
            Log.d(
                TAG, "onFailure(%s): state %s failed (count=%d)",
                codeStr, m_curState.toString(), count
            )
        }

        override fun run() {
            when (m_curState) {
                State.START -> {
                    m_failures = IntArray(State.entries.size)
                    onSuccess() // move to next state
                }

                State.CLEAR_LOCAL_SERVICES -> m_mgr.clearLocalServices(m_channel, this)
                State.ADD_LOCAL_SERVICES -> {
                    val record: MutableMap<String, String?> = HashMap()
                    record["AVAILABLE"] = "visible"
                    record["PORT"] = "" + OWNER_PORT
                    record["NAME"] = sDeviceName
                    val service = WifiP2pDnsSdServiceInfo
                        .newInstance(SERVICE_NAME, SERVICE_REG_TYPE, record)

                    val mgr = mgr
                    m_mgr.addLocalService(m_channel, service, this)
                }

                State.CLEAR_SERVICES_REQUESTS -> {
                    setDiscoveryListeners(m_mgr)
                    m_mgr.clearServiceRequests(m_channel, this)
                }

                State.ADD_SERVICE_REQUEST -> m_mgr.addServiceRequest(
                    m_channel,
                    WifiP2pDnsSdServiceRequest.newInstance(),
                    this
                )

                State.DISCOVER_PEERS -> m_mgr.discoverPeers(m_channel, this)
                State.PEER_DISCOVERY_STARTED -> m_mgr.discoverServices(m_channel, this)
                State.DONE -> Log.d(
                    TAG, "machine done; should I try connecting to: %s?",
                    s_peersSet.toString()
                )

                else -> Assert.failDbg()
            }
        }
    }

    private fun handleGotMessage(intent: Intent) {
        Log.d(TAG, "handleGotMessage(%s)", intent.toString())
        val gameID = intent.getIntExtra(KEY_GAMEID, 0)
        val data = Utils.base64Decode(intent.getStringExtra(KEY_DATA))
        val macAddress = intent.getStringExtra(KEY_RETADDR)

        val addr = CommsAddrRec(CommsConnType.COMMS_CONN_P2P)
            .setP2PParams(macAddress)

        val rslt = mHelper!!.receiveMessage(gameID, m_sink, data, addr)
        if (ReceiveResult.GAME_GONE == rslt) {
            sendNoGame(null, macAddress, gameID)
        }
    }

    private fun handleGotInvite(intent: Intent) {
        Log.d(TAG, "handleGotInvite()")
        val nliData = intent.getStringExtra(KEY_NLI)!!
        val nli = NetLaunchInfo.makeFrom(this, nliData)
        val returnMac = intent.getStringExtra(KEY_SRC)

        if (null == nli
            || !mHelper!!.handleInvitation(nli, returnMac, DictFetchOwner.OWNER_P2P)) {
            Log.d(TAG, "handleInvitation() failed")
        }
    }

    private fun handleGameGone(intent: Intent) {
        val gameID = intent.getIntExtra(KEY_GAMEID, 0)
        mHelper!!.postEvent(MultiEvent.MESSAGE_NOGAME, gameID)
    }

    private fun makeGame(nli: NetLaunchInfo, senderMac: String) {
        val rowids = DBUtils.getRowIDsFor(this, nli.gameID())
        if (0 == rowids.size) {
            val addr = nli.makeAddrRec(this)
            val rowid = GameUtils.makeNewMultiGame2(
                this, nli,
                m_sink,
                mHelper!!.utilCtxt
            )
            if (DBUtils.ROWID_NOTFOUND != rowid) {
                val gameName = nli.gameName
                if ( 0 < (gameName?.length ?: 0) ) {
                    DBUtils.setName(this, rowid, gameName!!)
                }
                val body = LocUtils.getString(
                    this, R.string.new_bt_body_fmt,
                    senderMac
                )
                GameUtils.postInvitedNotification(
                    this, nli.gameID(), body,
                    rowid
                )
            }
        }
    }

    private class WFDBroadcastReceiver(
        private val mManager: WifiP2pManager,
        private val mChannel: WifiP2pManager.Channel?
    ) : BroadcastReceiver(), ConnectionInfoListener, PeerListListener {
        override fun onReceive(context: Context, intent: Intent) {
            if (enabled()) {
                val action = intent.action
                Log.d(TAG, "got intent: $intent")

                if (WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION == action) {
                    val state = intent.getIntExtra(WifiP2pManager.EXTRA_WIFI_STATE, -1)
                    sEnabled = state == WifiP2pManager.WIFI_P2P_STATE_ENABLED
                    Log.d(TAG, "WifiP2PEnabled: %b", sEnabled)
                    if (sEnabled) {
                        startDiscovery()
                    }
                } else if (WifiP2pManager.WIFI_P2P_PEERS_CHANGED_ACTION == action) {
                    mManager.requestPeers(mChannel, this)
                } else if (WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION == action) {
                    // Assert.fail();
                    val networkInfo = intent
                        .getParcelableExtra<Parcelable>(WifiP2pManager.EXTRA_NETWORK_INFO) as NetworkInfo?
                    if (networkInfo!!.isConnected) {
                        Log.d(
                            TAG, "network %s connected",
                            networkInfo.toString()
                        )
                        mManager.requestConnectionInfo(mChannel, this)
                    } else {
                        // here
                        Log.d(
                            TAG, "network %s NOT connected",
                            networkInfo.toString()
                        )
                    }
                } else if (WifiP2pManager.WIFI_P2P_THIS_DEVICE_CHANGED_ACTION == action) {
                    // Respond to this device's wifi state changing
                    val device = intent
                        .getParcelableExtra<Parcelable>(WifiP2pManager.EXTRA_WIFI_P2P_DEVICE) as WifiP2pDevice?
                    sMacAddress = device!!.deviceAddress
                    sDeviceName = device.deviceName
                    synchronized(sUserMap) {
                        sUserMap.put(sMacAddress!!, sDeviceName!!)
                    }
                    Log.d(
                        TAG, "Got my MAC Address: %s and name: %s", sMacAddress,
                        sDeviceName
                    )

                    val stored = DBUtils.getStringFor(context, MAC_ADDR_KEY)
                    Assert.assertTrue(null == stored || stored == sMacAddress)
                    if (null == stored) {
                        DBUtils.setStringFor(context, MAC_ADDR_KEY, sMacAddress)
                    }
                } else if (WifiP2pManager.WIFI_P2P_DISCOVERY_CHANGED_ACTION == action) {
                    val running = intent
                        .getIntExtra(WifiP2pManager.EXTRA_DISCOVERY_STATE, -1)
                    Assert.assertTrue(running == 2 || running == 1) // remove soon
                    sDiscoveryRunning = 2 == running
                    Log.d(TAG, "discovery changed: running: %b", sDiscoveryRunning)
                }
            }
        }

        // WifiP2pManager.ConnectionInfoListener interface
        override fun onConnectionInfoAvailable(info: WifiP2pInfo) {
            // InetAddress from WifiP2pInfo struct.
            val groupOwnerAddress = info.groupOwnerAddress
            val hostAddress = groupOwnerAddress.hostAddress
            Log.d(
                TAG, "onConnectionInfoAvailable(%s); addr: %s",
                info.toString(), hostAddress
            )

            // After the group negotiation, we can determine the group owner.
            if (info.groupFormed) {
                sAmGroupOwner = info.isGroupOwner
                Log.d(TAG, "am %sgroup owner", if (sAmGroupOwner) "" else "NOT ")
                DbgUtils.showf(
                    "Joining WiFi P2p group as %s",
                    if (sAmGroupOwner) "owner" else "guest"
                )
                if (info.isGroupOwner) {
                    startAcceptThread()
                } else {
                    stopAcceptThread()
                    connectToOwner(info.groupOwnerAddress)
                    // The other device acts as the client. In this case,
                    // you'll want to create a client thread that connects to the group
                    // owner.
                }
                mgr.requestGroupInfo(sChannel, sGroupListener)
            } else {
                Assert.failDbg()
            }
        }

        override fun onPeersAvailable(peerList: WifiP2pDeviceList) {
            Log.d(
                TAG, "got list of %d peers",
                peerList.deviceList.size
            )

            updatePeersList(peerList)

            for (device in peerList.deviceList) {
                // Log.d( TAG, "not connecting to: %s", device.toString() );
                tryConnect(device)
            }

            // Out with the old, in with the new.
            // peers.clear();
            // peers.addAll(peerList.getDeviceList());

            // // If an AdapterView is backed by this data, notify it
            // // of the change.  For instance, if you have a ListView of available
            // // peers, trigger an update.
            // ((WiFiPeerListAdapter) getListAdapter()).notifyDataSetChanged();
            // if (peers.size() == 0) {
            //     Log.d(WiFiDirectActivity.TAG, "No devices found");
            //     return;
            // }
        }
    }

    private inner class P2pMsgSink : MultiMsgSink(this@WiDirService)

    private inner class WiDirServiceHelper(service: Service?) : XWServiceHelper(
        service!!
    ) {
        override fun getSink(rowid: Long): MultiMsgSink {
            return m_sink!!
        }

        override fun postNotification(device: String?, gameID: Int, rowid: Long) {
            Log.e(TAG, "postNotification() doing nothing")
        }
    }

    companion object {
        private val TAG: String = WiDirService::class.java.simpleName
        private val CLAZZ: Class<*> = WiDirService::class.java
        private const val MAC_ADDR_KEY = "p2p_mac_addr"
        private const val SERVICE_NAME = "srvc_" + BuildConfig.FLAVOR
        private const val SERVICE_REG_TYPE = "_presence._tcp"
        private const val OWNER_PORT = 5432
        private val PEERS_LIST_KEY = TAG + ".peers_key"

        private const val KEY_CMD = "cmd"
        private const val KEY_SRC = "src"
        private const val KEY_NLI = "nli"
        private const val KEY_DEST = "dest"

        private const val KEY_GAMEID = "gmid"
        private const val KEY_DATA = "data"
        private const val KEY_MAC = "mac"
        private const val KEY_NAME = "name"
        private const val KEY_MAP = "map"
        private const val KEY_RETADDR = "raddr"

        private var s_enabled = false
        private var sChannel: WifiP2pManager.Channel? = null
        private var s_discoverer: ServiceDiscoverer? = null
        private var sIntentFilter: IntentFilter? = null
        private var sGroupListener: GroupInfoListener? = null
        private var sReceiver: WFDBroadcastReceiver? = null

        // private static boolean sDiscoveryStarted;
        private var sDiscoveryRunning = false // set via broadcast; unused
        private var sEnabled = false
        private var sHavePermission = false

        // These two kinda overlap...
        private var sAmServer = false
        private var sAmGroupOwner = false
        private var sAcceptThread: Thread? = null
        private var sServerSock: ServerSocket? = null
        private var sIface: Iface? = null
        private val sSocketWrapMap: MutableMap<String?, BiDiSockWrap> = HashMap()
        private val sUserMap: MutableMap<String, String> = HashMap()
        private val sPendingDevs: MutableMap<String, Long> = HashMap()
        private var sMacAddress: String? = null
        private var sDeviceName: String? = null
        private val s_devListeners: MutableSet<DevSetListener> = HashSet()
        private var s_peersSet: MutableSet<String?>? = null

        private fun updateStatusOut(success: Boolean) {
            ConnStatusHandler.updateStatusOut(
                XWApp.getContext(),
                CommsConnType.COMMS_CONN_P2P, success
            )
        }

        private fun updateStatusIn(success: Boolean) {
            ConnStatusHandler.updateStatusIn(
                XWApp.getContext(), CommsConnType.COMMS_CONN_P2P,
                success
            )
        }

        fun init(context: Context) {
            Log.d(TAG, "init()")
            s_enabled = XWPrefs.getPrefsBoolean(
                context, R.string.key_enable_p2p,
                false
            )

            Assert.assertNull(s_peersSet)
            s_peersSet = HashSet()
            val peers = DBUtils.getStringFor(context, PEERS_LIST_KEY)
            if (null != peers) {
                val macs = TextUtils.split(peers, ",")
                for (mac in macs) {
                    s_peersSet!!.add(mac)
                }
            }
            Log.d(TAG, "loaded saved peers: %s", s_peersSet!!.toString())

            try {
                val listener =
                    WifiP2pManager.ChannelListener { Log.d(TAG, "onChannelDisconnected()") }
                sChannel = mgr.initialize(
                    context, Looper.getMainLooper(),
                    listener
                )
                s_discoverer = ServiceDiscoverer()
                sHavePermission = true
            } catch (ndf: NoClassDefFoundError) { // old os version
                sHavePermission = false
            } catch (se: SecurityException) {               // perm not in manifest
                sHavePermission = false
            } catch (npe: NullPointerException) { // Seeing this on Oreo emulator
                sHavePermission = false
            }
        }

        fun reset(context: Context) {
            // Put experimental stuff here that might help get a connection

            if (null != s_discoverer) {
                s_discoverer!!.restart()
            }
        }

        fun enabled(): Boolean {
            val result = BuildConfig.WIDIR_ENABLED && s_enabled
            return result
        }

        fun connecting(): Boolean {
            return enabled() && 0 < sSocketWrapMap.size && sSocketWrapMap.values.iterator()
                .next().isConnected
        }

        fun getMyMacAddress(context: Context?): String? {
            if (enabled()) {
                if (null == sMacAddress && null != context) {
                    sMacAddress = DBUtils.getStringFor(context, MAC_ADDR_KEY)
                }
            }
            Log.d(TAG, "getMyMacAddress() => %s", sMacAddress)
            // Assert.assertNotNull(sMacAddress);
            return sMacAddress
        }

        fun formatNetStateInfo(): String {
            var connectState = ""
            if (null != s_discoverer) {
                connectState = s_discoverer!!.stateToString()
            }

            val map = mapToString(copyUserMap())
            connectState += String.format(
                "; role: %s; map: %s nThreads: %d",
                if (sAmGroupOwner) "owner" else "guest", map,
                Thread.activeCount()
            )
            return connectState
        }

        private val myMacAddress: String?
            get() = getMyMacAddress(null)

        fun registerDevSetListener(dsl: DevSetListener) {
            synchronized(s_devListeners) {
                s_devListeners.add(dsl)
            }
            updateListeners()
        }

        fun unregisterDevSetListener(dsl: DevSetListener) {
            synchronized(s_devListeners) {
                s_devListeners.remove(dsl)
            }
        }

        fun inviteRemote(
            context: Context, macAddr: String?,
            nli: NetLaunchInfo
        ) {
            Log.d(TAG, "inviteRemote(%s)", macAddr)
            Assert.assertNotNull(macAddr)
            val nliString = nli.toString()
            Log.d(TAG, "inviteRemote(%s)", nliString)

            val forwarding = booleanArrayOf(false)
            val wrap = getForSend(macAddr, forwarding)

            if (null == wrap) {
                Log.e(TAG, "inviteRemote: no socket for %s", macAddr)
            } else {
                val packet = XWPacket(CMD.INVITE)
                    .put(KEY_SRC, myMacAddress)
                    .put(KEY_NLI, nliString)

                if (forwarding[0]) {
                    packet.put(KEY_DEST, macAddr)
                }
                wrap.send(packet)
            }
        }

        fun sendPacket(
            context: Context, macAddr: String?, gameID: Int,
            buf: ByteArray
        ): Int {
            Log.d(TAG, "sendPacket(len=%d,addr=%s)", buf.size, macAddr)
            var nSent = -1

            val forwarding = booleanArrayOf(false)
            val wrap = getForSend(macAddr, forwarding)

            if (null != wrap) {
                val packet = XWPacket(CMD.MSG)
                    .put(KEY_SRC, myMacAddress)
                    .put(KEY_DATA, Utils.base64Encode(buf))
                    .put(KEY_GAMEID, gameID)

                if (forwarding[0]) {
                    packet.put(KEY_DEST, macAddr)
                }
                wrap.send(packet)
                nSent = buf.size
            } else {
                Log.d(TAG, "sendPacket: no socket for %s", macAddr)
            }
            return nSent
        }

        fun activityResumed(activity: Activity) {
            if (enabled() && sHavePermission) {
                if (initListeners(activity)) {
                    activity.registerReceiver(sReceiver, sIntentFilter)
                    Log.d(TAG, "activityResumed() done")
                    startDiscovery()
                }
            }
        }

        fun activityPaused(activity: Activity) {
            if (enabled() && sHavePermission) {
                Assert.assertNotNull(sReceiver)
                // No idea why I'm seeing this exception...
                try {
                    activity.unregisterReceiver(sReceiver)
                } catch (iae: IllegalArgumentException) {
                    Log.ex(TAG, iae)
                }
                Log.d(TAG, "activityPaused() done")

                // Examples seem to kick discovery off once and that's it
                // sDiscoveryStarted = false;
            }
        }

        fun gameDied(macAddr: String?, gameID: Int) {
            // FIX ME when can test; probably just this:
            // sendNoGame( null, macAddr, gameID );

            val iter: Iterator<BiDiSockWrap> = sSocketWrapMap.values.iterator()
            while (iter.hasNext()) {
                sendNoGame(iter.next(), null, gameID)
            }
        }

        private fun initListeners(context: Context): Boolean {
            var succeeded = false
            if (enabled()) {
                if (null == sIface) {
                    try {
                        val mgr = mgr
                        Assert.assertNotNull(sChannel)

                        sIface = object : Iface {
                            override fun gotPacket(socket: BiDiSockWrap, bytes: ByteArray) {
                                Log.d(TAG, "wrapper got packet!!!")
                                updateStatusIn(true)
                                processPacket(socket, bytes)
                            }

                            override fun connectStateChanged(
                                wrap: BiDiSockWrap,
                                nowConnected: Boolean
                            ) {
                                Log.d(
                                    TAG, "connectStateChanged(connected=%b)",
                                    nowConnected
                                )
                                if (nowConnected) {
                                    wrap.send(
                                        XWPacket(CMD.PING)
                                            .put(KEY_NAME, sDeviceName)
                                            .put(KEY_MAC, getMyMacAddress(context))
                                    )
                                } else {
                                    val sizeBefore = sSocketWrapMap.size
                                    sSocketWrapMap.values.remove(wrap)
                                    Log.d(
                                        TAG, "removed wrap; had %d, now have %d",
                                        sizeBefore, sSocketWrapMap.size
                                    )
                                    if (0 == sSocketWrapMap.size) {
                                        updateStatusIn(false)
                                        updateStatusOut(false)
                                    }
                                }
                            }

                            override fun onWriteSuccess(wrap: BiDiSockWrap) {
                                Log.d(TAG, "onWriteSuccess()")
                                updateStatusOut(true)
                            }
                        }

                        sGroupListener = GroupInfoListener { group ->
                            if (null == group) {
                                Log.d(TAG, "onGroupInfoAvailable(null)!")
                            } else {
                                Log.d(
                                    TAG, "onGroupInfoAvailable(owner: %b)!",
                                    group.isGroupOwner
                                )
                                Assert.assertTrue(sAmGroupOwner == group.isGroupOwner)
                                if (sAmGroupOwner) {
                                    val devs = group.clientList
                                    synchronized(sUserMap) {
                                        for (dev in devs) {
                                            val macAddr = dev.deviceAddress
                                            sUserMap[macAddr] = dev.deviceName
                                            val wrap = sSocketWrapMap[macAddr]
                                            if (null == wrap) {
                                                Log.d(
                                                    TAG,
                                                    "groupListener: no socket for %s",
                                                    macAddr
                                                )
                                            } else {
                                                Log.d(
                                                    TAG, "socket for %s connected: %b",
                                                    macAddr, wrap.isConnected
                                                )
                                            }
                                        }
                                    }
                                }
                            }
                            Log.d(TAG, "thread count: %d", Thread.activeCount())
                            Handler().postDelayed({
                                Companion.mgr.requestGroupInfo(
                                    sChannel,
                                    sGroupListener
                                )
                            }, (60 * 1000).toLong())
                        }

                        sIntentFilter = IntentFilter()
                        sIntentFilter!!.addAction(WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION)
                        sIntentFilter!!.addAction(WifiP2pManager.WIFI_P2P_PEERS_CHANGED_ACTION)
                        sIntentFilter!!.addAction(WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION)
                        sIntentFilter!!.addAction(WifiP2pManager.WIFI_P2P_THIS_DEVICE_CHANGED_ACTION)
                        sIntentFilter!!.addAction(WifiP2pManager.WIFI_P2P_DISCOVERY_CHANGED_ACTION)

                        sReceiver = WFDBroadcastReceiver(mgr, sChannel)
                        succeeded = true
                    } catch (se: SecurityException) {
                        Log.d(TAG, "disabling wifi; no permissions")
                        sEnabled = false
                    }
                } else {
                    succeeded = true
                }
            }
            return succeeded
        }

        // See: http://stackoverflow.com/questions/26300889/wifi-p2p-service-discovery-works-intermittently
        private fun startDiscovery() {
            if (null != s_discoverer) {
                s_discoverer!!.restart()
            }
        }

        private val mgr: WifiP2pManager
            get() {
                val context = XWApp.getContext()
                return context.getSystemService(WIFI_P2P_SERVICE) as WifiP2pManager
            }

        private fun setDiscoveryListeners(mgr: WifiP2pManager) {
            if (enabled()) {
                val srl =
                    DnsSdServiceResponseListener { instanceName, registrationType, srcDevice -> // Service has been discovered. My app?
                        if (instanceName.equals(SERVICE_NAME, ignoreCase = true)) {
                            Log.d(
                                TAG, "onDnsSdServiceAvailable: %s with name %s",
                                instanceName, srcDevice.deviceName
                            )
                            tryConnect(srcDevice)
                        }
                    }

                val trl = DnsSdTxtRecordListener { domainName, map, device ->

                    // This doesn't seen to be getting called
                    Log.d(
                        TAG,
                        "onDnsSdTxtRecordAvailable(avail: %s, port: %s; name: %s)",
                        map["AVAILABLE"], map["PORT"], map["NAME"]
                    )
                }
                mgr.setDnsSdResponseListeners(sChannel, srl, trl)
                Log.d(TAG, "setDiscoveryListeners done")
            }
        }

        private fun connectPending(macAddress: String): Boolean {
            var result = false
            if (sPendingDevs.containsKey(macAddress)) {
                val `when` = sPendingDevs[macAddress]!!
                val now = Utils.getCurSeconds()
                result = 3 >= now - `when`
            }
            Log.d(TAG, "connectPending(%s)=>%b", macAddress, result)
            return result
        }

        private fun notePending(macAddress: String) {
            sPendingDevs[macAddress] = Utils.getCurSeconds()
        }

        private fun tryConnect(device: WifiP2pDevice) {
            val macAddress = device.deviceAddress
            if (sAmGroupOwner) {
                Log.d(
                    TAG, "tryConnect(%s): dropping because group owner",
                    macAddress
                )
            } else if (sSocketWrapMap.containsKey(macAddress)
                && sSocketWrapMap[macAddress]!!.isConnected
            ) {
                Log.d(TAG, "tryConnect(%s): already connected", macAddress)
            } else if (connectPending(macAddress)) {
                // Do nothing
            } else {
                Log.d(TAG, "trying to connect to %s", macAddress)
                val config = WifiP2pConfig()
                config.deviceAddress = macAddress
                config.wps.setup = WpsInfo.PBC

                mgr.connect(sChannel, config, object : WifiP2pManager.ActionListener {
                    override fun onSuccess() {
                        Log.d(TAG, "onSuccess(): %s", "connect_xx")
                        notePending(macAddress)
                    }

                    override fun onFailure(reason: Int) {
                        Log.d(TAG, "onFailure(%d): %s", reason, "connect_xx")
                    }
                })
            }
        }

        private fun connectToOwner(addr: InetAddress) {
            val wrap = BiDiSockWrap(addr, OWNER_PORT, sIface!!)
            Log.d(TAG, "connectToOwner(%s)", addr.toString())
            wrap.connect()
        }

        private fun storeByAddress(wrap: BiDiSockWrap, packet: XWPacket) {
            val macAddress = packet.getString(KEY_MAC)
            // Assert.assertNotNull( macAddress );
            if (null != macAddress) {
                // this has fired. Sockets close and re-open?
                sSocketWrapMap[macAddress] = wrap
                Log.d(
                    TAG, "storeByAddress(); storing wrap for %s",
                    macAddress
                )

                GameUtils.resendAllIf(
                    XWApp.getContext(),
                    CommsConnType.COMMS_CONN_P2P
                )
            }
        }

        private fun processPacket(wrap: BiDiSockWrap, bytes: ByteArray) {
            val context = XWApp.getContext()
            var intent: Intent? = null
            val asStr = String(bytes)
            Log.d(TAG, "got string: %s", asStr)
            val packet = XWPacket(asStr)
            // JSONObject asObj = new JSONObject( asStr );
            Log.d(TAG, "got packet: %s", packet.toString())
            val cmd = packet.getCommand()
            when (cmd) {
                CMD.PING -> {
                    storeByAddress(wrap, packet)
                    val reply = XWPacket(CMD.PONG)
                        .put(KEY_MAC, myMacAddress)
                    addMappings(reply)
                    wrap.send(reply)
                }

                CMD.PONG -> {
                    storeByAddress(wrap, packet)
                    readMappings(packet)
                }

                CMD.INVITE -> if (!forwardedPacket(packet, bytes)) {
                    intent = getIntentTo(P2PAction.GOT_INVITE)
                    intent.putExtra(KEY_NLI, packet.getString(KEY_NLI))
                    intent.putExtra(KEY_SRC, packet.getString(KEY_SRC))
                }

                CMD.MSG -> if (!forwardedPacket(packet, bytes)) {
                    val gameID = packet.getInt(KEY_GAMEID, 0)
                    if (0 != gameID) {
                        if (GameUtils.haveWithGameID(context, gameID)) {
                            intent = getIntentTo(P2PAction.GOT_MSG)
                            intent.putExtra(KEY_GAMEID, gameID)
                            intent.putExtra(KEY_DATA, packet.getString(KEY_DATA))
                            intent.putExtra(KEY_RETADDR, packet.getString(KEY_SRC))
                        } else {
                            sendNoGame(wrap, null, gameID)
                        }
                    }
                }

                CMD.NOGAME -> if (!forwardedPacket(packet, bytes)) {
                    val gameID = packet.getInt(KEY_GAMEID, 0)
                    intent = getIntentTo(P2PAction.GAME_GONE)
                    intent.putExtra(KEY_GAMEID, gameID)
                }
                else -> {Log.d(TAG, "unexpected cmd $cmd"); Assert.failDbg()}
            }
            if (null != intent) {
                context.startService(intent)
            }
        }

        private fun sendNoGame(
            wrap: BiDiSockWrap?, macAddress: String?,
            gameID: Int
        ) {
            var wrap = wrap
            val forwarding = booleanArrayOf(false)
            if (null == wrap) {
                wrap = getForSend(macAddress, forwarding)
            }

            if (null != wrap) {
                val packet = XWPacket(CMD.NOGAME)
                    .put(KEY_GAMEID, gameID)
                if (forwarding[0]) {
                    packet.put(KEY_DEST, macAddress)
                }
                wrap.send(packet)
            }
        }

        private fun addMappings(packet: XWPacket) {
            synchronized(sUserMap) {
                try {
                    val array = JSONArray()
                    for (mac in sUserMap.keys) {
                        val map = JSONObject()
                            .put(KEY_MAC, mac)
                            .put(KEY_NAME, sUserMap[mac])
                        array.put(map)
                    }
                    packet.put(KEY_MAP, array)
                } catch (ex: JSONException) {
                    Log.ex(TAG, ex)
                }
            }
        }

        private fun readMappings(packet: XWPacket) {
            synchronized(sUserMap) {
                try {
                    val array = packet.getJSONArray(KEY_MAP)
                    for (ii in 0 until array.length()) {
                        val map = array.getJSONObject(ii)
                        val name = map.getString(KEY_NAME)
                        val mac = map.getString(KEY_MAC)
                        sUserMap[mac] = name
                    }
                } catch (ex: JSONException) {
                    Log.ex(TAG, ex)
                }
            }
            updateListeners()
        }

        private fun updateListeners() {
            var listeners: Array<DevSetListener?>? = null
            synchronized(s_devListeners) {
                if (0 < s_devListeners.size) {
                    listeners = arrayOfNulls(s_devListeners.size)
                    val iter: Iterator<DevSetListener> = s_devListeners.iterator()
                    for (ii in listeners!!.indices) {
                        listeners!![ii] = iter.next()
                    }
                }
            }

            if (null != listeners) {
                val macToName = copyUserMap()
                macToName.remove(myMacAddress)

                for (listener in listeners!!) {
                    listener!!.setChanged(macToName)
                }
            }
        }

        private fun copyUserMap(): MutableMap<String, String> {
            var macToName: MutableMap<String, String>
            synchronized(sUserMap) {
                macToName = HashMap(sUserMap)
            }
            return macToName
        }

        private fun mapToString(macToName: Map<String, String>): String {
            var result = ""
            val iter = macToName.keys.iterator()
            var ii = 0
            while (iter.hasNext()) {
                val mac = iter.next()
                result += String.format(
                    "%d: %s=>%s; ", ii, mac,
                    macToName[mac]
                )
                ++ii
            }
            return result
        }

        private fun forwardedPacket(packet: XWPacket, bytes: ByteArray): Boolean {
            var forwarded = false
            val destAddr = packet.getString(KEY_DEST)
            if (null != destAddr && 0 < destAddr.length) {
                forwarded = destAddr == sMacAddress
                if (forwarded) {
                    forwardPacket(bytes, destAddr)
                } else {
                    Log.d(TAG, "addr mismatch: %s vs %s", destAddr, sMacAddress)
                }
            }
            return forwarded
        }

        private fun forwardPacket(bytes: ByteArray, destAddr: String) {
            Log.d(TAG, "forwardPacket(mac=%s)", destAddr)
            if (sAmGroupOwner) {
                val wrap = sSocketWrapMap[destAddr]
                if (null != wrap && wrap.isConnected) {
                    wrap.send(bytes)
                } else {
                    Log.e(TAG, "no working socket for %s", destAddr)
                }
            } else {
                Log.e(TAG, "can't forward; not group owner (any more?)")
            }
        }

        private fun startAcceptThread() {
            sAmServer = true
            sAcceptThread = Thread {
                Log.d(TAG, "accept thread starting")
                var done = false
                try {
                    sServerSock = ServerSocket(OWNER_PORT)
                    while (!done) {
                        Log.d(TAG, "calling accept()")
                        val socket = sServerSock!!.accept()
                        Log.d(TAG, "accept() returned!!")
                        BiDiSockWrap(socket, sIface!!)
                    }
                } catch (ioe: IOException) {
                    Log.e(TAG, ioe.toString())
                    sAmServer = false
                    done = true
                }
                Log.d(TAG, "accept thread exiting")
            }
            sAcceptThread!!.start()
        }

        private fun stopAcceptThread() {
            Log.d(TAG, "stopAcceptThread()")
            if (null != sAcceptThread) {
                if (null != sServerSock) {
                    try {
                        sServerSock!!.close()
                    } catch (ioe: IOException) {
                        Log.ex(TAG, ioe)
                    }
                    sServerSock = null
                }
                sAcceptThread = null
            }
        }

        private fun getIntentTo(cmd: P2PAction): Intent {
            val context = XWApp.getContext()
            val intent = Intent(context, WiDirService::class.java)
            intent.putExtra(KEY_CMD, cmd.ordinal)
            return intent
        }

        private fun getForSend(macAddr: String?, forwarding: BooleanArray): BiDiSockWrap? {
            var wrap = sSocketWrapMap[macAddr]

            // See if we need to forward through group owner instead
            if (null == wrap && !sAmGroupOwner && 1 == sSocketWrapMap.size) {
                wrap = sSocketWrapMap.values.iterator().next()
                Log.d(TAG, "forwarding to %s through group owner", macAddr)
                forwarding[0] = true
            }

            if (null == wrap) {
                updateStatusOut(false)
            }
            return wrap
        }

        private fun updatePeersList(peerList: WifiP2pDeviceList) {
            val newSet: MutableSet<String?> = HashSet()
            for (device in peerList.deviceList) {
                val macAddress = device.deviceAddress
                newSet.add(macAddress)
            }

            Log.d(
                TAG, "updatePeersList(): old set: %s; new set: %s",
                s_peersSet.toString(), newSet.toString()
            )
            s_peersSet = newSet

            DBUtils.setStringFor(
                XWApp.getContext(), PEERS_LIST_KEY,
                TextUtils.join(",", s_peersSet!!)
            )
        }
    }
}
