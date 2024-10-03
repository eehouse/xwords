/*
 * Copyright 2010 - 2024 by Eric House (xwords@eehouse.org).  All
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

import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothClass.Device.Major
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothServerSocket
import android.bluetooth.BluetoothSocket
import android.content.Context
import android.content.Intent
import android.os.Build
import android.provider.Settings
import android.text.TextUtils

import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.Closeable
import java.io.DataInputStream
import java.io.DataOutputStream
import java.io.IOException
import java.util.UUID
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicReference
import kotlin.concurrent.Volatile
import kotlin.math.min

import org.eehouse.android.xw4.DbgUtils.DeadlockWatch
import org.eehouse.android.xw4.MultiService.DictFetchOwner
import org.eehouse.android.xw4.MultiService.MultiEvent
import org.eehouse.android.xw4.Perms23.Perm
import org.eehouse.android.xw4.TimerReceiver.TimerCallback
import org.eehouse.android.xw4.XWServiceHelper.ReceiveResult
import org.eehouse.android.xw4.jni.CommsAddrRec
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.CommsAddrRec.ConnExpl
import org.eehouse.android.xw4.jni.XwJNI
import org.eehouse.android.xw4.jni.XwJNI.STAT

object BTUtils {
    private val TAG: String = BTUtils::class.java.simpleName
    private const val BOGUS_MARSHMALLOW_ADDR = "02:00:00:00:00:00"
    private const val MAX_PACKET_LEN = 4 * 1024
    private const val CONNECT_SLEEP_MS = 2500
    private val KEY_OWN_MAC = TAG + ":own_mac"
    private const val MIN_BACKOFF = (1000 * 60 * 2 // 2 minutes
            ).toLong()
    private const val MAX_BACKOFF = (1000 * 60 * 60 * 4 // 4 hours, to test
            ).toLong()

    private val sListeners: MutableSet<ScanListener> = HashSet()
    private val sSenders: MutableMap<String, PacketAccumulator> = HashMap()
    private var s_namesToAddrs: MutableMap<String?, String?>? = null
    private var sMyMacAddr: String? = null

    private const val BT_PROTO_JSONS = 1 // using jsons instead of lots of fields
    private const val BT_PROTO_BATCH = 2
    private const val BT_PROTO = BT_PROTO_BATCH
    private fun IS_BATCH_PROTO(): Boolean {
        return BT_PROTO_BATCH == BT_PROTO
    }

    private val sBackUser = AtomicBoolean(false)
    private var sAppName: String? = null
    private var sUUID: UUID? = null

    private val sTimerCallbacks
            : TimerCallback = object : TimerCallback {
        override fun timerFired(context: Context) {
            Log.d(TAG, "timerFired()")
            BTUtils.timerFired(context)
        }

        override fun incrementBackoff(backoff: Long): Long {
            var backoff = backoff
            backoff = if (backoff < MIN_BACKOFF) {
                MIN_BACKOFF
            } else {
                backoff * 150 / 100
            }
            if (MAX_BACKOFF <= backoff) {
                backoff = MAX_BACKOFF
            }
            return backoff
        }
    }

    val BTPerms: Array<Perm> = arrayOf(
        Perm.BLUETOOTH_CONNECT,
        Perm.BLUETOOTH_SCAN,
    )

    private fun getDefaultAdapter(context: Context = XWApp.getContext()): BluetoothAdapter?
    {
        val mgr = context.getSystemService(Context. BLUETOOTH_SERVICE) as BluetoothManager
        return mgr?.getAdapter() ?: null
    }

    fun BTAvailable(): Boolean {
        val adapter = getDefaultAdapter()
        return null != adapter
    }

    fun BTEnabled(): Boolean {
        val adapter = getAdapterIf()
        return null != adapter && adapter.isEnabled
    }

    fun enable(context: Context) {
        val adapter = getAdapterIf()
        if (null != adapter && havePermissions(context)) {
            // Only do this after explicit action from user -- Android guidelines
            adapter.enable()
        }
        XWPrefs.setBTDisabled(context, false)
    }

    fun setEnabled(context: Context, enabled: Boolean) {
        if (enabled) {
            onResume(context)
        } else {
            stopThreads()
        }
    }

    // If I build for 31 but run on an older phone, these permissions don't
    // show up in settings->app->permissions, but I get back false from
    // havePermissions(). There seems to be no way to grant them, so let's see
    // if on older phones we can proceed without crashing. Test well....
    @JvmOverloads
    fun havePermissions(context: Context = BTUtils.context): Boolean {
        val sdk = Build.VERSION.SDK_INT
        val result = (sdk < Build.VERSION_CODES.Q
                || Perms23.havePermissions(context, *BTPerms))
        Log.d(TAG, "havePermissions(sdk=%d) => %b", sdk, result)
        return result
    }

    fun disabledChanged(context: Context) {
        val disabled = XWPrefs.getBTDisabled(context)
        setEnabled(context, !disabled)
    }

    fun getAdapterIf(): BluetoothAdapter?
    {
        // BT crashes a lot inside the OS when running on behalf of a
        // background user account. We catch exceptions that indicate that's
        // going on and set this flag.
        val result =
            if (!XWPrefs.getBTDisabled(context) && !sBackUser.get()) {
                getDefaultAdapter(context)
            } else null
        return result
    }

    fun init(context: Context, appName: String?, uuid: UUID?) {
        Log.d(TAG, "init()")
        sAppName = appName
        sUUID = uuid
        loadOwnMac(context)
        onResume(context)
    }

    private fun timerFired(context: Context) {
        onResume(context)
    }

    fun onResume(context: Context) {
        Log.d(TAG, "onResume()")
        // Should only run this in the background if we have BT games
        // going. In the foreground we want to
        SecureListenThread.getOrStart()
        InsecureListenThread.getOrStart()
    }

    fun onStop(context: Context) {
        Log.d(TAG, "onStop(): doing nothing for now")
    }

    fun openBTSettings(activity: Activity) {
        val intent = Intent()
        intent.setAction(Settings.ACTION_BLUETOOTH_SETTINGS)
        activity.startActivity(intent)
    }

    fun isBogusAddr(addr: String?): Boolean {
        val result = null != addr && BOGUS_MARSHMALLOW_ADDR == addr
        // Log.d( TAG, "isBogusAddr(%s) => %b", addr, result );
        return result
    }

    fun getBTNameAndAddress(context: Context): Array<String?>? {
        var result: Array<String?>? = null
        if (havePermissions(context)) {
            val adapter = getAdapterIf()
            if (null != adapter) {
                result = arrayOf(adapter.name, sMyMacAddr)
            }
        }
        return result
    }

    fun nameForAddr(btAddr: String?): String? {
        val adapter = getAdapterIf()
        val result =
            if (null != adapter && null != btAddr) {
                nameForAddr(adapter, btAddr)
            } else null as String?
        return result
    }

    fun setAmForeground() {
        sBackUser.set(false)
    }

    private fun stopThreads() {
        SecureListenThread.stopSelf()
        InsecureListenThread.stopSelf()
        ReadThread.stopSelf()
    }

    private fun nameForAddr(adapter: BluetoothAdapter?, btAddr: String): String? {
        var result: String? = null
        if (null != adapter) {
            result = adapter.getRemoteDevice(btAddr).name
        }
        return result
    }

    private fun loadOwnMac(context: Context) {
        sMyMacAddr = DBUtils.getStringFor(context, KEY_OWN_MAC)
    }

    private fun storeOwnMac(macAddr: String?) {
        val context = context
        DBUtils.setStringFor(context, KEY_OWN_MAC, macAddr)
    }

    fun sendPacket(
        context: Context, buf: ByteArray, msgID: String?,
        targetAddr: CommsAddrRec, gameID: Int
    ): Int {
        Log.d(
            TAG, "sendPacket(%s): name: %s; addr: %s", targetAddr,
            targetAddr.bt_hostName, targetAddr.bt_btAddr
        )
        val nSent = -1
        val name = targetAddr.bt_hostName!!
        if (!havePermissions(context)) {
            Log.d(TAG, "sendPacket(): no BT permissions available")
        } else if (isActivePeer(name)) {
            getPA(name, getSafeAddr(targetAddr)).addMsg(gameID, buf, msgID)
        } else {
            Log.d(TAG, "sendPacket(): addressee %s unknown so dropping", name)
        }
        return nSent
    }

    fun gameDied(context: Context, btName: String, btAddr: String?, gameID: Int) {
        if (!TextUtils.isEmpty(btName)) {
            getPA(btName, btAddr).addDied(gameID)
        }
    }

    fun pingHost(context: Context, btName: String, btAddr: String?, gameID: Int) {
        // Log.d( TAG, "pingHost(host=%s, gameID=%X)", btAddr, gameID );
        getPA(btName, btAddr).addPing(gameID)
    }

    fun sendInvite(
        context: Context, btName: String,
        btAddr: String?, nli: NetLaunchInfo
    ): Boolean {
        var success = false
        Log.d(TAG, "sendInvite(name=%s, addr=%s, nli=%s)", btName, btAddr, nli)
        if (!TextUtils.isEmpty(btName)) {
            getPA(btName, btAddr).addInvite(nli)
            success = true
        }
        return success
    }

    fun addScanListener(listener: ScanListener) {
        synchronized(sListeners) {
            sListeners.add(listener)
        }
    }

    fun removeScanListener(listener: ScanListener) {
        synchronized(sListeners) {
            sListeners.remove(listener)
        }
    }

    private fun callListeners(dev: BluetoothDevice) {
        synchronized(sListeners) {
            for (listener in sListeners) {
                listener.onDeviceScanned(dev)
            }
        }
    }

    fun scan(context: Context, timeoutMS: Int): Int {
        val devs = getCandidates()
        val count = devs.size
        if (0 < count) {
            ScanThread.startOnce(timeoutMS, devs)
        }
        return count
    }

    private fun isActivePeer(devName: String?): Boolean {
        var result = false

        val devs = getCandidates()
        for (dev in devs) {
            if (dev.name == devName) {
                result = true
                break
            }
        }
        if (!result) {
            Log.d(TAG, "isActivePeer(%s) => FALSE", devName)
        }
        return result
    }

    private var sHaveLogged = false
    fun getCandidates(): Set<BluetoothDevice>
    {
        val result: MutableSet<BluetoothDevice> = HashSet()
        val adapter = getAdapterIf()
        if (null != adapter && havePermissions()) {
            for (dev in adapter.bondedDevices) {
                val clazz = dev.bluetoothClass.majorDeviceClass
                when (clazz) {
                    Major.AUDIO_VIDEO, Major.HEALTH, Major.IMAGING, Major.TOY, Major.PERIPHERAL -> {}
                    else -> {
                        if (!sHaveLogged) {
                            Log.d(
                                TAG, "getCandidates(): adding %s of type %d",
                                dev.name, clazz
                            )
                        }
                        result.add(dev)
                    }
                }
            }
            sHaveLogged = true
        }
        return result
    }

    private fun updateStatusIn(success: Boolean) {
        val context = context
        ConnStatusHandler.updateStatusIn(context, CommsConnType.COMMS_CONN_BT, success)
    }

    private fun updateStatusOut(success: Boolean) {
        val context = context
        ConnStatusHandler.updateStatusOut(
            context, CommsConnType.COMMS_CONN_BT, success)
    }

    private fun getPA(btName: String, btAddr: String?): PacketAccumulator {
        Assert.assertTrue(!TextUtils.isEmpty(btName))
        val pa = getSenderFor(btName, btAddr).wake()
        return pa
    }

    private fun removeSenderFor(pa: PacketAccumulator) {
        DeadlockWatch(sSenders).use { dw ->
            synchronized(sSenders) {
                if (pa === sSenders[pa.name]) {
                    sSenders.remove(pa.name)
                } else {
                    Log.e(TAG, "race? There's a different PA for %s", pa.name)
                }
            }
        }
    }

    private fun getSenderFor(btName: String, btAddr: String?): PacketAccumulator {
        return getSenderFor(btName, btAddr, true)
    }

    private fun getSenderFor(
        btName: String,
        btAddr: String?,
        create: Boolean
    ): PacketAccumulator {
        var result: PacketAccumulator?
        DeadlockWatch(sSenders).use { dw ->
            synchronized(sSenders) {
                if (create && !sSenders.containsKey(btName)) {
                    sSenders[btName] = PacketAccumulator(btName, btAddr)
                }
                result = sSenders[btName]
            }
        }
        return result!!
    }

    private fun getSafeAddr(addr: CommsAddrRec): String? {
        var btAddr = addr.bt_btAddr
        if (TextUtils.isEmpty(btAddr) || BOGUS_MARSHMALLOW_ADDR == btAddr) {
            val original = btAddr
            val btName = addr.bt_hostName
            if (null == s_namesToAddrs) {
                s_namesToAddrs = HashMap()
            }

            btAddr = if (s_namesToAddrs!!.containsKey(btName)) {
                s_namesToAddrs!![btName]
            } else {
                null
            }
            if (null == btAddr) {
                val devs = getCandidates()
                for (dev in devs) {
                    // Log.d( TAG, "%s => %s", dev.getName(), dev.getAddress() );
                    if (btName == dev.name) {
                        btAddr = dev.address
                        s_namesToAddrs!![btName] = btAddr
                        break
                    }
                }
            }
            Log.d(TAG, "getSafeAddr(\"%s\") => %s", original, btAddr)
        }
        return btAddr
    }

    private fun clearInstance(
        holder: AtomicReference<Thread?>,
        instance: Thread
    ) {
        synchronized(holder) {
            val curThread = holder.get()
            if (null == curThread) {
                // nothing to do
            } else if (instance === curThread) {
                holder.set(null)
            } else {
                Log.e(
                    TAG, "clearInstance(): cur instance %s not == %s",
                    curThread, instance
                )
            }
        }
    }

    private val context: Context
        // Save a few keystrokes...
        get() = XWApp.getContext()

    private enum class BTCmd {
        BAD_PROTO,
        PING,
        PONG,
        SCAN,
        INVITE,
        INVITE_ACCPT,
        _INVITE_DECL,  // unused
        INVITE_DUPID,
        _INVITE_FAILED,  // generic error, and unused
        MESG_SEND,
        MESG_ACCPT,
        _MESG_DECL,  // unused
        MESG_GAMEGONE,
        _REMOVE_FOR,  // unused
        INVITE_DUP_INVITE,
        MAC_ASK,  // ask peer what my mac address is
        MAC_REPLY,  // reply to above
    }

    interface ScanListener {
        fun onDeviceScanned(dev: BluetoothDevice)
        fun onScanDone()
    }

    private class ScanThread private constructor(
        private val mTimeoutMS: Int,
        private val mDevs: Set<BluetoothDevice>
    ) : Thread() {
        init {
            sInstance.set(this)
        }

        override fun run() {
            Assert.assertTrueNR(this === sInstance.get())
            val pas: MutableMap<BluetoothDevice, PacketAccumulator> = HashMap()

            for (dev in mDevs) {
                val pa =
                    PacketAccumulator(dev.name, dev.address, mTimeoutMS)
                        .addPing(0)
                        .setExitWhenEmpty()
                        .setLifetimeMS(mTimeoutMS.toLong())
                        .wake()

                pas[dev] = pa
            }

            // PENDING: figure out how to let these send results the minute
            // they have one!!!
            for (dev in pas.keys) {
                val pa = pas[dev]
                try {
                    pa!!.join()
                } catch (ex: InterruptedException) {
                    Assert.failDbg()
                }
            }

            synchronized(sListeners) {
                for (listener in sListeners) {
                    listener.onScanDone()
                }
            }

            clearInstance(sInstance, this)
        }

        companion object {
            private val sInstance = AtomicReference<Thread?>()
            fun startOnce(timeoutMS: Int, devs: Set<BluetoothDevice>) {
                synchronized(sInstance) {
                    if (null == sInstance.get()) {
                        val thread = ScanThread(timeoutMS, devs)
                        Assert.assertTrueNR(thread === sInstance.get())
                        thread.start()
                    }
                }
            }
        }
    }

    private class PacketAccumulator @JvmOverloads constructor(
        val mName: String,
        val mAddr: String?,
        timeoutMS: Int = 20000
    ) : Thread() {
        private class OutputPair {
            var bos: ByteArrayOutputStream = ByteArrayOutputStream()
            var dos: DataOutputStream = DataOutputStream(bos)

            fun length(): Int {
                return bos.toByteArray().size
            }
        }

        private class MsgElem(
            var mCmd: BTCmd,
            var mGameID: Int,
            var mMsgID: String?,
            op: OutputPair
        ) {
            var mStamp: Long = System.currentTimeMillis()
            var mData: ByteArray?
            var mLocalID: Int = 0

            init {
                val tmpOp = OutputPair()
                mData = try {
                    tmpOp.dos.writeByte(mCmd.ordinal)
                    val data = op.bos.toByteArray()
                    if (IS_BATCH_PROTO()) {
                        tmpOp.dos.writeShort(data.size)
                    }
                    tmpOp.dos.write(data, 0, data.size)
                    tmpOp.bos.toByteArray()
                } catch (ioe: IOException) {
                    // With memory-backed IO this should be impossible
                    Log.e(
                        TAG, "MsgElem.__init(): got ioe!: %s",
                        ioe.message
                    )
                    null
                }
            }

            fun setLocalID(id: Int) {
                mLocalID = id
            }

            fun isSameAs(other: MsgElem): Boolean {
                val result =
                    mCmd == other.mCmd && mGameID == other.mGameID && mData.contentEquals(other.mData)
                if (result) {
                    if (null != mMsgID && mMsgID != other.mMsgID) {
                        Log.d(
                            TAG, "hmmm: identical but msgIDs differ: new %s vs old %s",
                            mMsgID, other.mMsgID
                        )
                        // new 0:0 vs old 2:0 is ok!! since 0: is replaced by
                        // 2 or more when device becomes a client
                        // Assert.assertFalse( BuildConfig.DEBUG ); // fired!!!
                    }
                }
                return result
            }

            fun size(): Int {
                return mData!!.size
            }

            override fun toString(): String {
                return String.format("{cmd: %s, msgID: %s}", mCmd, mMsgID)
            }
        }

        private val mElems: MutableList<MsgElem>
        private var mLastFailTime: Long = 0
        private var mFailCount: Int
        private var mLength: Int
        private var mCounter = 0
        private var mDieTimeMS = Long.MAX_VALUE
        private var mResponseCount = 0
        private val mTimeoutMS: Int

        @Volatile
        private var mExitWhenEmpty = false
        private val mAdapter: BluetoothAdapter
        private val mHelper: BTHelper
        private val mPostOnResponse: Boolean

        // Ping case -- used only once
        init {
            Assert.assertTrue(!TextUtils.isEmpty(mName))
            Log.d(TAG, "PacketAccumulator(name=%s, addr=%s)", mName, mAddr)
            mElems = ArrayList()
            mFailCount = 0
            mLength = 0
            mTimeoutMS = timeoutMS
            val adapter = getAdapterIf()
            if (null == adapter) {
                Log.d(TAG, "adapter null; is BT on?")
            }
            mAdapter = adapter!!
            mHelper = BTHelper(mName, mAddr)
            mPostOnResponse = true

            if (null == sMyMacAddr) {
                addGetMac()
            }

            start()
        }

        @Synchronized
        fun wake(): PacketAccumulator {
            (this as Object).notifyAll()
            return this
        }

        fun setExitWhenEmpty(): PacketAccumulator {
            mExitWhenEmpty = true
            return this
        }

        fun setLifetimeMS(msToLive: Long): PacketAccumulator {
            mDieTimeMS = System.currentTimeMillis() + msToLive
            return this
        }

        fun addInvite(nli: NetLaunchInfo) {
            try {
                val op = OutputPair()
                if (IS_BATCH_PROTO()) {
                    val nliData = XwJNI.nliToStream(nli)
                    op.dos.writeShort(nliData.size)
                    op.dos.write(nliData, 0, nliData.size)
                } else {
                    op.dos.writeUTF(nli.toString())
                }
                append(BTCmd.INVITE, op)
            } catch (ioe: IOException) {
                Assert.failDbg()
            }
        }

        fun addMsg(gameID: Int, buf: ByteArray, msgID: String?) {
            try {
                val op = OutputPair()
                op.dos.writeInt(gameID)
                op.dos.writeShort(buf.size)
                op.dos.write(buf, 0, buf.size)
                append(BTCmd.MESG_SEND, gameID, msgID, op)
            } catch (ioe: IOException) {
                Assert.failDbg()
            }
        }

        fun addPing(gameID: Int): PacketAccumulator {
            try {
                val op = OutputPair()
                op.dos.writeInt(gameID)
                append(BTCmd.PING, gameID, op)
            } catch (ioe: IOException) {
                Assert.failDbg()
            }
            return this
        }

        fun addDied(gameID: Int) {
            try {
                val op = OutputPair()
                op.dos.writeInt(gameID)
                append(BTCmd.MESG_GAMEGONE, gameID, op)
            } catch (ioe: IOException) {
                Assert.failDbg()
            }
        }

        private fun addGetMac() {
            try {
                append(BTCmd.MAC_ASK, OutputPair())
            } catch (ioe: IOException) {
                Assert.failDbg()
            }
        }

        @Throws(IOException::class)
        private fun append(cmd: BTCmd, op: OutputPair) {
            append(cmd, 0, null, op)
        }

        @Throws(IOException::class)
        private fun append(cmd: BTCmd, gameID: Int, op: OutputPair) {
            append(cmd, gameID, null, op)
        }

        @Throws(IOException::class)
        private fun append(
            cmd: BTCmd, gameID: Int, msgID: String?,
            op: OutputPair
        ): Boolean {
            var haveSpace: Boolean
            DeadlockWatch(this).use { dw ->
                synchronized(this) {
                    val newElem = MsgElem(cmd, gameID, msgID, op)
                    haveSpace = mLength + newElem.size() < MAX_PACKET_LEN
                    if (haveSpace) {
                        // Let's check for duplicates....
                        var dupFound = false
                        for (elem in mElems) {
                            if (elem.isSameAs(newElem)) {
                                dupFound = true
                                break
                            }
                        }

                        if (dupFound) {
                            Log.d(TAG, "append(): dropping dupe: %s", newElem)
                        } else {
                            newElem.setLocalID(mCounter++)
                            mElems.add(newElem)
                            mLength += newElem.size()
                        }
                        // for now, we restart timer on new data, even if a dupe
                        mFailCount = 0
                        (this as Object).notifyAll()
                    }
                }
            }
            // Log.d( TAG, "append(%s): now %s", cmd, this );
            return haveSpace
        }

        private fun unappend(nToRemove: Int) {
            Assert.assertTrue(nToRemove <= mElems.size)
            DeadlockWatch(this).use { dw ->
                synchronized(this) {
                    for (ii in 0 until nToRemove) {
                        val elem = mElems.removeAt(0)
                        mLength -= elem.size()
                    }
                    Log.d(
                        TAG, "unappend(): after removing %d, have %d left for size %d",
                        nToRemove, mElems.size, mLength
                    )

                    resetBackoff() // we were successful sending, so should retry immediately
                }
            }
        }

        fun resetBackoff() {
            // Log.d( TAG, "resetBackoff() IN" );
            DeadlockWatch(this).use { dw ->
                synchronized(this) {
                    mFailCount = 0
                }
            }            // Log.d( TAG, "resetBackoff() OUT" );
        }

        override fun run() {
            Log.d(TAG, "PacketAccumulator.run() starting for %s", this)
            // Run as long as I have something to send. Sleep for as long as
            // appropriate based on backoff logic, and be awakened when
            // something new comes in or there's reason to hope a send try
            // will succeed.
            while (BTEnabled() && havePermissions()) {
                val doBreak = synchronized(this) {
                    if (mExitWhenEmpty && 0 == mElems.size) {
                        "BREAK"
                    } else if (System.currentTimeMillis() >= mDieTimeMS) {
                        "BREAK"
                    } else {
                        val waitTimeMS = figureWait()
                        if (waitTimeMS > 0) {
                            Log.d(TAG, "%s: waiting %dms", this, waitTimeMS)
                            try {
                                (this as Object).wait(waitTimeMS)
                                Log.d(TAG, "%s: done waiting", this)
                                "CONTINUE"
                            } catch (ie: InterruptedException) {
                                Log.d(TAG, "ie inside wait: %s", ie.message)
                                null
                            }
                        }
                    }
                }
                when (doBreak) {
                    "CONTINUE" -> continue
                    "BREAK" -> break
                    else -> {}
                }
                mResponseCount += trySend()
            }
            Log.d(
                TAG, "PacketAccumulator.run finishing for %s"
                        + " after sending %d packets", this, mResponseCount
            )

            // A hack: mExitWhenEmpty only set in the ping case
            if (!mExitWhenEmpty) {
                removeSenderFor(this)
            }
        }

        private fun getBTName(): String { return mName }
        private fun getBTAddr(): String? { return mAddr }

        private fun figureWait(): Long {
            var result = Long.MAX_VALUE

            DeadlockWatch(this).use { dw ->
                synchronized(this) {
                    if (0 < mElems.size) { // something to send
                        if (0 == mFailCount) {
                            result = 0
                        } else {
                            // If we're failing, use a backoff.
                            val wait = (1000 * mFailCount * mFailCount).toLong()
                            result = wait - (System.currentTimeMillis() - mLastFailTime)
                        }
                    }
                }
            }
            // Log.d( TAG, "%s.figureWait() => %dms", this, result );
            return result
        }

        private fun getRemoteDevice(btName: String?, btAddr: String?): BluetoothDevice? {
            var result: BluetoothDevice? = null
            if (!TextUtils.isEmpty(btAddr)) {
                result = mAdapter!!.getRemoteDevice(btAddr)
            }
            if (null == result || TextUtils.isEmpty(result.name)) {
                result = null
                Log.d(TAG, "getRemoteDevice(%s); no name; trying again", btAddr)
                Assert.assertTrueNR(!TextUtils.isEmpty(btName))
                for (dev in mAdapter!!.bondedDevices) {
                    if (dev.name == btName) {
                        result = dev
                        break
                    }
                }
            }
            Log.d(TAG, "getRemoteDevice(%s) => %s", btAddr, result)
            return result
        }

        private fun trySend(): Int {
            var nDone = 0
            var socket: BluetoothSocket? = null
            try {
                Log.d(TAG, "trySend(): attempting to connect to %s", mName)
                val btAddr = getBTAddr()
                val dev = getRemoteDevice(getBTName(), btAddr)
                if (null == dev) {
                    Log.d(TAG, "unable to find dev for %s", btAddr)
                    sleep(mTimeoutMS.toLong())
                } else {
                    socket = connect(dev, mTimeoutMS)
                    if (null == socket) {
                        setNoHost()
                        updateStatusOut(false)
                    } else {
                        Log.d(
                            TAG, "PacketAccumulator.run(): connect(%s) => %s",
                            mName, socket
                        )
                        nDone += writeAndCheck(socket)
                        updateStatusOut(true)
                        if (mPostOnResponse) {
                            callListeners(socket.remoteDevice)
                        }
                    }
                }
            } catch (ioe: IOException) {
                Log.e(
                    TAG, "PacketAccumulator.run(): ioe: %s",
                    ioe.message
                )
            } catch (ioe: InterruptedException) {
            } finally {
                if (null != socket) {
                    try {
                        socket.close()
                    } catch (ex: Exception) {
                    }
                }
            }
            return nDone
        }

        @Throws(IOException::class)
        private fun writeAndCheck(socket: BluetoothSocket): Int {
            val dos = DataOutputStream(socket.outputStream)
            Log.d(TAG, "%s.writeAndCheck() IN", this)
            dos.writeByte(BT_PROTO)

            var localElems: MutableList<MsgElem>? = ArrayList()
            DeadlockWatch(this).use { dw ->
                synchronized(this) {
                    if (0 < mLength) {
                        try {
                            // Format is <proto><len-of-rest><msgCount><msg1>..<msgN> To
                            // insert len-of-rest at the beginning we have to create a
                            // tmp byte array then append it after writing its length.

                            val tmpOP = OutputPair()
                            val msgCount = if (IS_BATCH_PROTO()) mElems.size else 1
                            if (IS_BATCH_PROTO()) {
                                tmpOP.dos.writeByte(msgCount)
                            }

                            for (ii in 0 until msgCount) {
                                val elem = mElems[ii]
                                val elemData = elem.mData!!
                                tmpOP.dos.write(elemData, 0, elemData.size)
                                localElems!!.add(elem)
                            }
                            val data = tmpOP.bos.toByteArray()

                            // now write to the socket. Note that connect()
                            // writes BT_PROTO as the first byte.
                            if (IS_BATCH_PROTO()) {
                                dos.writeShort(data.size)
                            }
                            dos.write(data, 0, data.size)
                            dos.flush()
                            Log.d(
                                TAG, "writeAndCheck(): wrote %d msgs as"
                                        + " %d-byte payload with sum %s (for %s)",
                                msgCount, data.size, Utils.getMD5SumFor(data),
                                this
                            )
                            XwJNI.sts_increment(STAT.STAT_BT_SENT);
                        } catch (ioe: IOException) {
                            Log.e(TAG, "writeAndCheck(): ioe: %s", ioe.message)
                            localElems = null
                        }
                    }
                } // synchronized
            }
            var nDone = 0
            if (null != localElems) {
                Log.d(TAG, "writeAndCheck(): reading %d replies", localElems!!.size)
                try {
                    KillerIn(socket, 30).use { ki ->
                        val inStream =
                            DataInputStream(socket.inputStream)
                        for (ii in localElems!!.indices) {
                            val elem = localElems!![ii]
                            val cmd = elem.mCmd
                            val gameID = elem.mGameID
                            val cmdOrd = inStream.readByte()
                            if (cmdOrd >= BTCmd.entries.size) {
                                break // SNAFU!!!
                            }
                            val reply = BTCmd.entries[cmdOrd.toInt()]
                            Log.d(
                                TAG, "writeAndCheck() %s: got response %s to cmd[%d] %s",
                                this, reply, ii, cmd
                            )

                            if (reply == BTCmd.BAD_PROTO) {
                                mHelper.postEvent(
                                    MultiEvent.BAD_PROTO_BT,
                                    socket.remoteDevice.name
                                )
                            } else {
                                val remoteName = socket.remoteDevice.name
                                handleReply(inStream, cmd, gameID, remoteName, reply)
                            }
                            ++nDone
                        }
                    }
                } catch (ioe: IOException) {
                    Log.d(TAG, "failed reading replies for %s: %s", this, ioe.message)
                }
            }
            unappend(nDone)
            Log.d(TAG, "writeAndCheck() => %d", nDone)
            if (nDone > 0) {
                updateStatusOut(true)
            }
            return nDone
        } // writeAndCheck()

        @Throws(IOException::class)
        private fun handleReply(
            inStream: DataInputStream, cmd: BTCmd, gameID: Int,
            remoteName: String, reply: BTCmd
        ) {
            when (cmd) {
                BTCmd.MESG_SEND, BTCmd.MESG_GAMEGONE -> when (reply) {
                    BTCmd.MESG_ACCPT -> mHelper.postEvent(
                        MultiEvent.MESSAGE_ACCEPTED,
                        gameID, 0, mName
                    )

                    BTCmd.MESG_GAMEGONE -> {
                        val expl = ConnExpl(
                            CommsConnType.COMMS_CONN_BT,
                            remoteName
                        )
                        mHelper.postEvent(MultiEvent.MESSAGE_NOGAME, gameID, expl)
                    }
                    else -> {Log.d(TAG, "Unexpected reply $reply")}
                }

                BTCmd.INVITE -> when (reply) {
                    BTCmd.INVITE_ACCPT -> mHelper.postEvent(MultiEvent.NEWGAME_SUCCESS, gameID)
                    BTCmd.INVITE_DUPID -> mHelper.postEvent(MultiEvent.NEWGAME_DUP_REJECTED, mName)
                    else -> mHelper.postEvent(MultiEvent.NEWGAME_FAILURE, gameID)
                }

                BTCmd.PING -> if (BTCmd.PONG == reply && inStream.readBoolean()) {
                    mHelper.postEvent(MultiEvent.MESSAGE_NOGAME, gameID)
                }

                BTCmd.MAC_ASK -> if (BTCmd.MAC_REPLY == reply) {
                    val mac = inStream.readUTF()
                    Assert.assertTrueNR(null == sMyMacAddr || sMyMacAddr == mac)
                    sMyMacAddr = mac
                    Log.d(TAG, "got %s as my mac addr", sMyMacAddr)
                    storeOwnMac(sMyMacAddr)
                }

                else -> {
                    Log.e(TAG, "handleReply(cmd=%s) case not handled", cmd)
                    Assert.failDbg() // fired
                }
            }
        }

        private fun connect(remote: BluetoothDevice, timeout: Int): BluetoothSocket? {
            var socket: BluetoothSocket? = null
            val name = remote.name
            Assert.assertTrueNR(null != name)
            val addr = remote.address
            Log.w(TAG, "connect(%s/%s, timeout=%d) starting", name, addr, timeout)
            // DbgUtils.logf( "connecting to %s to send cmd %s", name, cmd.toString() );
            // Docs say always call cancelDiscovery before trying to connect
            mAdapter!!.cancelDiscovery()

            // Retry for some time. Some devices take a long time to generate and
            // broadcast ACL conn ACTION
            var nTries = 0
            val end = timeout + System.currentTimeMillis()
            while (true) {
                try {
                    val useInsecure = 0 == nTries++ % 2
                    socket = if (useInsecure
                    ) remote.createInsecureRfcommSocketToServiceRecord(sUUID)
                    else remote.createRfcommSocketToServiceRecord(sUUID)
                    socket.connect()
                    Log.i(
                        TAG, "connect(%s/%s/useInsecure=%b) succeeded after %d tries",
                        name, addr, useInsecure, nTries
                    )
                    break // success!!!
                } catch (ioe: IOException) {
                    socket = null
                    // Log.d( TAG, "connect(): %s", ioe.getMessage() );
                    val msLeft = end - System.currentTimeMillis()
                    if (msLeft <= 0) {
                        break
                    }
                    try {
                        sleep(
                            min(CONNECT_SLEEP_MS.toDouble(), msLeft.toDouble()).toLong()
                        )
                    } catch (ex: InterruptedException) {
                        break
                    }
                } catch (ioe: SecurityException) {
                    socket = null
                    val msLeft = end - System.currentTimeMillis()
                    if (msLeft <= 0) {
                        break
                    }
                    try {
                        sleep(
                            min(CONNECT_SLEEP_MS.toDouble(), msLeft.toDouble()).toLong()
                        )
                    } catch (ex: InterruptedException) {
                        break
                    }
                }
            }
            Log.e(TAG, "connect(%s/%s) => %s", name, addr, socket)
            return socket
        }

        private fun setNoHost() {
            DeadlockWatch(this).use { dw ->
                synchronized(this) {
                    mLastFailTime = System.currentTimeMillis()
                    ++mFailCount
                }
            }
        }

        @Synchronized
        override fun toString(): String {
            val sb = StringBuilder("{")
                .append("name: ").append(mName)
                .append(", addr: ").append(mAddr)
                .append(", failCount: ").append(mFailCount)
                .append(", len: ").append(mLength)

            if (0 < mElems.size) {
                val age = System.currentTimeMillis() - mElems[0].mStamp
                val lowID = mElems[0].mLocalID
                val highID = mElems[mElems.size - 1].mLocalID
                val cmds: MutableList<BTCmd?> = ArrayList()
                for (elem in mElems) {
                    cmds.add(elem.mCmd)
                }
                sb.append(", age: ").append(age)
                    .append(", ids: ").append(lowID).append('-').append(highID)
                    .append(", cmds: ").append(TextUtils.join(",", cmds))
            }

            return sb.append('}').toString()
        }
    } // class PacketAccumulator


    private abstract class ListenThread protected constructor
        (private val mAdapter: BluetoothAdapter) :
        Thread()
    {
        private var mServerSocket: BluetoothServerSocket? = null

        @Throws(IOException::class)
        abstract fun openListener(adapter: BluetoothAdapter): BluetoothServerSocket?

        override fun run() {
            val simpleName = javaClass.simpleName
            Log.d(TAG, "%s.run() starting", simpleName)

            try {
                Assert.assertTrueNR(null != sAppName && null != sUUID)
                mServerSocket = openListener(mAdapter)
                Log.d(TAG, "%s.openListener(uuid=%s) succeeded", simpleName, sUUID)
            } catch (ioe: IOException) {
                Log.ex(TAG, ioe)
                mServerSocket = null
            } catch (ex: SecurityException) {
                // Got this with a message saying not allowed to call
                // listenUsingRfcommWithServiceRecord() in background (on
                // Android 9)
                sBackUser.set(true) // two-user system: disable BT
                Log.d(TAG, "set sBackUser; outta here (first case)")
                mServerSocket = null
            }

            val wrapper: AtomicReference<Thread?> = getWrapper()
            while (null != mServerSocket && this === wrapper.get()) {
                Log.d(TAG, "%s.run(): calling accept()", simpleName)
                try {
                    val socket = mServerSocket!!.accept() // blocks
                    Log.d(TAG, "%s.run(): accept() returned", simpleName)
                    ReadThread.handle(socket)
                } catch (ioe: IOException) {
                    Log.ex(TAG, ioe)
                    mServerSocket = null
                }
            }

            clearInstance(wrapper, this)
            Log.d(TAG, "%s.run() exiting", simpleName)
        }

        fun closeListener() {
            val serverSocket = mServerSocket
            if (null != serverSocket) {
                try {
                    serverSocket.close()
                } catch (ioe: IOException) {
                    Log.ex(TAG, ioe)
                }
            }
        }

        abstract fun getWrapper(): AtomicReference<Thread?>
    }

    private class SecureListenThread private constructor(adapter: BluetoothAdapter) :
        ListenThread(adapter) {
        init {
            Assert.assertTrueNR(null == getWrapper().get())
            getWrapper().set(this)
        }

        @Throws(IOException::class)
        override fun openListener(adapter: BluetoothAdapter): BluetoothServerSocket? {
            return adapter.listenUsingRfcommWithServiceRecord(sAppName, sUUID)
        }

        override fun getWrapper(): AtomicReference<Thread?> { return sInstance }

        companion object {
            private val sInstance = AtomicReference<Thread?>()

            internal fun getOrStart(): Unit
            {
                val adapter = getAdapterIf()
                if (null != adapter) {
                    synchronized(sInstance) {
                        var thread = sInstance.get() as SecureListenThread?
                        if (null == thread) {
                            thread = SecureListenThread(adapter)
                            Assert.assertTrueNR(thread === sInstance.get())
                            thread.start()
                        }
                    }
                }
            }

            fun stopSelf() {
                synchronized(sInstance) {
                    val self = sInstance.get() as SecureListenThread?
                    Log.d(TAG, "SecureListenThread.stopSelf(): self: %s", self)
                    if (null != self) {
                        sInstance.set(null)
                        self.closeListener()
                    }
                }
            }
        }
    }

    private class InsecureListenThread private constructor(adapter: BluetoothAdapter) :
        ListenThread(adapter) {
        init {
            Assert.assertTrueNR(null == sInstance.get())
            sInstance.set(this)
        }


        @Throws(IOException::class)
        override fun openListener(adapter: BluetoothAdapter): BluetoothServerSocket? {
            return adapter
                .listenUsingInsecureRfcommWithServiceRecord(sAppName, sUUID)
        }

        override fun getWrapper(): AtomicReference<Thread?> { return sInstance }

        companion object {
            private val sInstance = AtomicReference<Thread?>()


            internal fun getOrStart(): Unit
            {
                val adapter = getAdapterIf()
                if (null != adapter) {
                    synchronized(sInstance) {
                        var thread = sInstance.get() as InsecureListenThread?
                        if (null == thread) {
                            thread = InsecureListenThread(adapter)
                            Assert.assertTrueNR(thread === sInstance.get())
                            thread.start()
                        }
                    }
                }
            }

            fun stopSelf() {
                synchronized(sInstance) {
                    val self = sInstance.get() as InsecureListenThread?
                    Log.d(TAG, "InsecureListenThread.stopSelf(): self: %s", self)
                    if (null != self) {
                        sInstance.set(null)
                        self.closeListener()
                    }
                }
            }
        }
    }

    private class ReadThread private constructor() : Thread() {
        private val mQueue = LinkedBlockingQueue<BluetoothSocket>()
        private val mBTMsgSink = BTMsgSink()

        init {
            sInstance.set(this)
        }

        override fun run() {
            Log.d(TAG, "ReadThread: %s.run() starting", this)
            while (this === sInstance.get()) {
                try {
                    val socket = mQueue.take()
                    val inStream =
                        DataInputStream(socket.inputStream)
                    val proto = inStream.readByte()
                    if (proto.toInt() == BT_PROTO_BATCH || proto.toInt() == BT_PROTO_JSONS) {
                        BTInviteDelegate.onHeardFromDev(socket.remoteDevice)
                        parsePacket(proto, inStream, socket)
                        updateStatusIn(true)
                        TimerReceiver.setBackoff(context, sTimerCallbacks, MIN_BACKOFF)
                        // nBadCount = 0;
                    } else {
                        writeBack(socket, BTCmd.BAD_PROTO)
                    }
                    Log.d(TAG, "%s.run(): closing %s", this, socket)
                    socket.close()
                } catch (ie: InterruptedException) {
                    break
                } catch (ioe: IOException) {
                    Log.ex(TAG, ioe)
                }
            }

            clearInstance(sInstance, this)
            Log.d(TAG, "ReadThread: %s.run() exiting", this)
        }

        private fun writeBack(socket: BluetoothSocket, cmd: BTCmd) {
            try {
                val os = DataOutputStream(socket.outputStream)
                os.writeByte(cmd.ordinal)
                os.flush()
            } catch (ex: IOException) {
                Log.ex(TAG, ex)
            }
            Log.d(TAG, "writeBack(%s) DONE", cmd)
        }

        @Throws(IOException::class)
        private fun parsePacket(
            proto: Byte, inStream: DataInputStream,
            socket: BluetoothSocket
        ) {
            Log.d(TAG, "parsePacket(socket=%s, proto=%d)", socket, proto)
            val isOldProto = proto.toInt() == BT_PROTO_JSONS
            val inLen = if (isOldProto
            ) inStream.available().toShort() else inStream.readShort()
            if (inLen >= MAX_PACKET_LEN) {
                Log.e(TAG, "packet too big; dropping!!!")
                Assert.failDbg()
            } else if (0 < inLen) {
                var data = ByteArray(inLen.toInt())
                inStream.readFully(data)

                val bis = ByteArrayInputStream(data)
                val dis = DataInputStream(bis)
                val nMessages = (if (isOldProto) 1 else dis.readByte()).toInt()

                Log.d(
                    TAG, "dispatchAll(): read %d-byte payload with sum %s containing %d messages",
                    data.size, Utils.getMD5SumFor(data), nMessages
                )

                XwJNI.sts_increment(STAT.STAT_BT_RCVD);

                for (ii in 0 until nMessages) {
                    val cmdOrd = dis.readByte()
                    val oneLen = if (isOldProto) 0 else dis.readShort() // used only to skip
                    val availableBefore = dis.available()
                    if (cmdOrd < BTCmd.entries.size) {
                        val cmd = BTCmd.entries[cmdOrd.toInt()]
                        Log.d(TAG, "parsePacket(): reading msg %d: %s", ii, cmd)
                        when (cmd) {
                            BTCmd.PING -> {
                                val gameID = dis.readInt()
                                receivePing(gameID, socket)
                            }

                            BTCmd.INVITE -> {
                                var nli: NetLaunchInfo?
                                if (isOldProto) {
                                    nli = NetLaunchInfo.makeFrom(
                                        context,
                                        dis.readUTF()
                                    )
                                } else {
                                    data = ByteArray(dis.readShort().toInt())
                                    dis.readFully(data)
                                    nli = XwJNI.nliFromStream(data)
                                }
                                receiveInvitation(nli, socket)
                            }

                            BTCmd.MESG_SEND -> {
                                val gameID = dis.readInt()
                                data = ByteArray(dis.readShort().toInt())
                                dis.readFully(data)
                                receiveMessage(gameID, data, socket)
                            }

                            BTCmd.MESG_GAMEGONE -> {
                                val gameID = dis.readInt()
                                receiveGameGone(gameID, socket)
                            }

                            BTCmd.MAC_ASK -> receiveMacAsk(socket)
                            else -> Assert.failDbg()
                        }
                    } else {
                        Log.e(
                            TAG, "unexpected command (ord: %d);"
                                    + " skipping %d bytes", cmdOrd, oneLen
                        )
                        if (oneLen <= dis.available()) {
                            dis.readFully(ByteArray(oneLen.toInt()))
                        }
                    }

                    // sanity-check based on packet length
                    val availableAfter = dis.available()
                    Assert.assertTrue(0 == oneLen.toInt() || oneLen.toInt() == availableBefore - availableAfter || !BuildConfig.DEBUG)
                }
            } else {
                Log.e(TAG, "parsePacket(): bad packet? len == 0")
            }
        }

        @Throws(IOException::class)
        private fun receivePing(gameID: Int, socket: BluetoothSocket) {
            val deleted = (0 != gameID
                    && !GameUtils.haveWithGameID(context, gameID))
            Log.d(TAG, "receivePing(gameID=%X); deleted: %b", gameID, deleted)

            val os = DataOutputStream(socket.outputStream)
            os.writeByte(BTCmd.PONG.ordinal)
            os.writeBoolean(deleted)
            os.flush()
        }

        private fun receiveInvitation(nli: NetLaunchInfo?, socket: BluetoothSocket) {
            val host = socket.remoteDevice
            val response = makeOrNotify(nli, host.name, host.address)
            Log.d(TAG, "receiveInvitation() => %s", response)
            writeBack(socket, response)
        }

        private fun makeOrNotify(nli: NetLaunchInfo?, btName: String, btAddr: String): BTCmd {
            val result: BTCmd
            val helper = BTHelper(btName, btAddr)
            result = if (helper.handleInvitation(nli!!, btName, DictFetchOwner.OWNER_BT)) {
                BTCmd.INVITE_ACCPT
            } else {
                BTCmd.INVITE_DUP_INVITE // dupe of rematch
            }
            return result
        }

        private fun receiveMessage(gameID: Int, buf: ByteArray, socket: BluetoothSocket) {
            val helper = BTHelper(socket)
            val rslt = helper.receiveMessage(gameID, mBTMsgSink, buf, helper.getAddr())

            val response =
                if (rslt == ReceiveResult.GAME_GONE) BTCmd.MESG_GAMEGONE else BTCmd.MESG_ACCPT

            writeBack(socket, response)
        }

        private fun receiveGameGone(gameID: Int, socket: BluetoothSocket) {
            val helper = BTHelper(socket)
            helper.postEvent(MultiEvent.MESSAGE_NOGAME, gameID)
            writeBack(socket, BTCmd.MESG_ACCPT)
        }

        @Throws(IOException::class)
        private fun receiveMacAsk(socket: BluetoothSocket) {
            val os = DataOutputStream(socket.outputStream)
            os.writeByte(BTCmd.MAC_REPLY.ordinal)
            val addr = socket.remoteDevice.address
            os.writeUTF(addr)
        }

        private fun enqueue(socket: BluetoothSocket) {
            mQueue.add(socket)
        }

        companion object {
            private val sInstance = AtomicReference<Thread?>()
            fun handle(incoming: BluetoothSocket) {
                Log.d(TAG, "read(from=%s)", incoming.remoteDevice.name)
                orStart!!.enqueue(incoming)
            }

            private val orStart: ReadThread?
                get() {
                    var result: ReadThread?
                    synchronized(sInstance) {
                        result = sInstance.get() as ReadThread?
                        if (null == result) {
                            result = ReadThread()
                            Assert.assertTrueNR(result === sInstance.get())
                            result!!.start()
                        }
                    }
                    return result
                }

            fun stopSelf() {
                synchronized(sInstance) {
                    val self = sInstance.get() as ReadThread?
                    if (null != self) {
                        sInstance.set(null)
                        self.interrupt()
                    }
                }
            }
        }
    }

    private class BTMsgSink : MultiMsgSink(context) {
        override fun sendViaBluetooth(
            buf: ByteArray, msgID: String?, gameID: Int,
            addr: CommsAddrRec
        ): Int {
            var nSent = -1
            val btAddr = getSafeAddr(addr)
            if (null != btAddr && 0 < btAddr.length) {
                getPA(addr.bt_hostName!!, btAddr).addMsg(gameID, buf, msgID)
                nSent = buf.size
            } else {
                Log.i(
                    TAG, "sendViaBluetooth(): no addr for dev named %s",
                    addr.bt_hostName
                )
            }
            return nSent
        }
    }

    private class BTHelper private constructor() : XWServiceHelper(context) {
        private var mReturnAddr: CommsAddrRec? = null

        constructor(from: CommsAddrRec) : this() {
            init(from)
        }

        constructor(fromName: String, fromAddr: String?) : this() {
            init(CommsAddrRec(fromName, fromAddr))
        }

        constructor(socket: BluetoothSocket) : this() {
            val host = socket.remoteDevice
            init(CommsAddrRec(host.name, host.address))
        }

        private fun init(addr: CommsAddrRec) { mReturnAddr = addr }

        fun getAddr(): CommsAddrRec { return mReturnAddr!! }

        private fun receiveMessage(rowid: Long, sink: MultiMsgSink, msg: ByteArray) {
            Log.d(TAG, "receiveMessage(rowid=%d, len=%d)", rowid, msg.size)
            receiveMessage(rowid, sink, msg, mReturnAddr!!)
        }
    }

    private class KillerIn(private val mSocket: Closeable, private val mSeconds: Int) : Thread(),
        AutoCloseable {
        init {
            start()
        }

        override fun run() {
            try {
                sleep((1000 * mSeconds).toLong())
                Log.d(TAG, "KillerIn(): time's up; closing socket")
                mSocket.close()
            } catch (ie: InterruptedException) {
                // Log.d( TAG, "KillerIn: killed by owner" );
            } catch (ioe: IOException) {
                Log.ex(TAG, ioe)
            }
        }

        override fun close() {
            interrupt()
        }
    }
}
