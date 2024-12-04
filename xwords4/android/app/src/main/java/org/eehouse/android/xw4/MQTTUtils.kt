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
import android.os.Build
import com.hivemq.client.internal.mqtt.lifecycle.MqttClientAutoReconnectImpl
import com.hivemq.client.mqtt.MqttClient
import com.hivemq.client.mqtt.datatypes.MqttQos
import com.hivemq.client.mqtt.lifecycle.MqttClientConnectedContext
import com.hivemq.client.mqtt.lifecycle.MqttClientConnectedListener
import com.hivemq.client.mqtt.lifecycle.MqttClientDisconnectedContext
import com.hivemq.client.mqtt.lifecycle.MqttClientDisconnectedListener
import com.hivemq.client.mqtt.mqtt3.Mqtt3BlockingClient
import com.hivemq.client.mqtt.mqtt3.message.publish.Mqtt3Publish

import org.json.JSONException
import org.json.JSONObject
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.locks.ReentrantLock
import java.util.function.Consumer
import kotlin.concurrent.thread
import kotlin.concurrent.withLock

import org.eehouse.android.xw4.jni.CommsAddrRec
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.CommsAddrRec.ConnExpl
import org.eehouse.android.xw4.jni.XwJNI
import org.eehouse.android.xw4.jni.XwJNI.STAT
import org.eehouse.android.xw4.jni.XwJNI.TopicsAndPackets
import org.eehouse.android.xw4.loc.LocUtils
import org.eehouse.android.xw4.TimerReceiver.TimerCallback

private const val PONG_PREFIX = "xw4/pong/"
private const val MIN_BACKOFF = (1000 * 60 * 2 // 2 minutes
).toLong()
private const val MAX_BACKOFF = (1000 * 60 * 60 * 4 // 4 hours, to test
).toLong()

object MQTTUtils {
    private var sEnabled = false

    private val sStateChangedIf = object:NetStateCache.StateChangedIf {
        override fun onNetAvail(context: Context, nowAvailable: Boolean) {
            Log.d(TAG, "onNetAvail(avail=$nowAvailable)")
            DbgUtils.assertOnUIThread()
            if (nowAvailable) {
                GameUtils.resendAllIf(context, CommsConnType.COMMS_CONN_MQTT)
                getConn(context)?.reconnect()
            }
        }
    }

    fun init(context: Context) {
        NetStateCache.register(context, sStateChangedIf)
        val enabled = XWPrefs.getMQTTEnabled(context)
        setEnabled(context, enabled)
    }

    // HiveMQ's version requires 24 or better, not to compile but to run
    // without NoSuchClass exception
    fun MQTTSupported(): Boolean { return Build.VERSION.SDK_INT >= 24 }

    fun setEnabled(context: Context, enabled: Boolean) {
        sEnabled = enabled
        if ( enabled ) {
            getConn(context)
        } else {
            killConn()
        }
    }

    interface PingResult {
        fun onSuccess(host: String, elapsed: Long)
    }

    fun ping(context: Context, pr: PingResult) {
        getConn(context)?.ping(pr)
    }

    fun getStats( context: Context ): String?
    {
        return getConn(context)?.getStats()
    }

    private val sOutboundQueue = LinkedBlockingQueue<TopicsAndPackets>()
    fun send(context: Context, tap: TopicsAndPackets): Int {
        if ( !sOutboundQueue.contains(tap)) {
            sOutboundQueue.add(tap)
            getConn(context)?.ensureSending()
        } else {
            Log.d(TAG, "not adding duplicate packet")
        }
        return -1
    }

    fun onResume(context: Context) = getConn(context)?.reconnect()

    fun onDestroy(context: Context)
    {
        Log.d(TAG, "onDestroy()")
        NetStateCache.unregister(sStateChangedIf)
    }

    fun makeOrNotify(context: Context, nli: NetLaunchInfo) {
        MQTTServiceHelper(context).handleInvitation(nli)
    }

    fun handleMessage(context: Context, from: CommsAddrRec,
                      gameID: Int, msg: ByteArray)
    {
        val rowids = DBUtils.getRowIDsFor(context, gameID)
        Log.d(TAG, "handleMessage(): got %d rows for gameID %X", rowids.size, gameID)
        if (0 == rowids.size) {
            notifyNotHere(context, from.mqtt_devID!!, gameID)
        } else {
            val helper = MQTTServiceHelper(context, from)
            for (rowid in rowids) {
                val sink = MultiMsgSink(context, rowid)
                helper.receiveMessage(rowid, sink, msg)
            }
        }
    }

    fun handleGameGone(context: Context, from: CommsAddrRec, gameID: Int)
    {
        val player = XwJNI.kplr_nameForMqttDev(from.mqtt_devID)
        val args =
            if (null == player) null
            else arrayOf(ConnExpl(CommsConnType.COMMS_CONN_MQTT, player))
        MQTTServiceHelper(context, from)
            .postEvent(MultiService.MultiEvent.MESSAGE_NOGAME, gameID, *args.orEmpty())
    }

    fun handleCtrlReceived(context: Context, msg: ByteArray) {
        try {
            val obj = JSONObject(String(msg))
            val msg = obj.optString("msg", null)
            if (null != msg) {
                var title = obj.optString("title", null)
                if (null == title) {
                    title = LocUtils.getString(context, R.string.remote_msg_title)
                }
                val alertIntent = GamesListDelegate.makeAlertIntent(context, msg)
                val code = msg.hashCode() xor title.hashCode()
                Utils.postNotification(context, alertIntent, title, msg, code)
            }
        } catch (je: JSONException) {
            Log.e(TAG, "handleCtrlReceived() ex: %s", je)
        }
    }

    fun gameDied(context: Context, devID: String, gameID: Int) {
        val tap = XwJNI.dvc_makeMQTTNoSuchGames(devID, gameID)
        send(context, tap)
    }

    fun onConfigChanged(context: Context)
    {
        killConn()
        getConn(context)
    }

    private class MQTTServiceHelper(val mContext: Context) : XWServiceHelper(mContext) {
        private var mReturnAddr: CommsAddrRec? = null

        constructor(context: Context, from: CommsAddrRec?) : this(context) {
            mReturnAddr = from
        }

        fun handleInvitation(nli: NetLaunchInfo) {
            handleInvitation(nli, null, MultiService.DictFetchOwner.OWNER_MQTT)
            // Now nuke the invitation so we don't keep getting it, e.g. if
            // the sender deletes the game
            val tap = XwJNI.dvc_makeMQTTNukeInvite(nli)
            send(mContext, tap)
        }

        fun receiveMessage(rowid: Long, sink: MultiMsgSink, msg: ByteArray) {
            // Log.d( TAG, "receiveMessage(rowid=%d, len=%d)", rowid, msg.length )
            receiveMessage(rowid, sink, msg, mReturnAddr!!)
        }
    }

    private val TAG = MQTTUtils::class.java.simpleName

    private fun notifyNotHere(context: Context, addressee: String, gameID: Int)
    {
        val tap = XwJNI.dvc_makeMQTTNoSuchGames(addressee, gameID)
        send(context, tap)
    }

    private val sWrapper: Array<Conn?> = arrayOf(null)
    private fun getConn(context: Context): Conn?
    {
        synchronized (sWrapper) {
            if ( null == sWrapper[0] ) {
                if (sEnabled && MQTTSupported()) {
                    sWrapper[0] = Conn(context)
                    sWrapper[0]!!.start()
                }
            }
            return sWrapper[0]
        }
    }

    private fun amConn(conn: Conn): Boolean
    {
        synchronized (sWrapper) {
            return sWrapper[0] === conn
        }
    }

    private fun killConn()
    {
        synchronized (sWrapper) {
            sWrapper[0]?.stop()
            sWrapper[0] = null
        }
    }

    private fun chooseQOS(context: Context, qosInt: Int): MqttQos
    {
        val asStr = XWPrefs.getPrefsString(context, R.string.key_mqtt_qos)
        val qos = try {
            MqttQos.valueOf(asStr)
        } catch (ex: Exception) {
            MqttQos.entries[qosInt]
        }
        // Log.d(TAG, "chooseQOS($qosInt) => $qos")
        return qos
    }
    
    private class Conn(val mContext: Context): MqttClientConnectedListener,
                                               MqttClientDisconnectedListener,
                                               Consumer<Mqtt3Publish>
    {
        private val mStart = System.currentTimeMillis()
        private val mDevID = XwJNI.dvc_getMQTTDevID()
        private val mTaskQueue = LinkedBlockingQueue<Task>()
        private val mHost: String
        private val mClient: Mqtt3BlockingClient
        private val mTaskThread: Thread
        private val mStats = Stats()

        init {
            Log.d(TAG, "initing conn() %H", this)
            mHost = XWPrefs.getPrefsString(mContext, R.string.key_mqtt_host)!!
                .trim { it <= ' ' } // in case some idiot adds whitespace. Ahem.
            val port = XWPrefs.getPrefsInt(mContext, R.string.key_mqtt_port, 1883)

            val payload = JSONObject()
                .putAnd("devid", mDevID)
                .putAnd("ts", Utils.getCurSeconds())

            mClient = MqttClient.builder()
                .useMqttVersion3()
                .identifier(mDevID)
                .automaticReconnect(MqttClientAutoReconnectImpl.DEFAULT)
                .addConnectedListener(this)
                .serverHost(mHost)
                .serverPort(port)
                .simpleAuth().username("xwuser").password("xw4r0cks".toByteArray())
                .applySimpleAuth()
                .willPublish()
                .topic("xw4/device/LWT")
                .payload(payload.toString().toByteArray())
                .applyWillPublish()
                .buildBlocking()

            mTaskThread = thread(start=false) {
                while ( true ) {
                    try {
                        val task = mTaskQueue.take()
                        // Log.d(TAG, "took task: $task")
                        task.run()
                        task.isLast && break
                    } catch (ie: InterruptedException) {
                        Log.ex(TAG, ie)
                    }
                }
                Log.d(TAG, "exiting mTaskThread")
            }
            add(ConnectTask())
        } // init

        internal fun start()
        {
            Assert.assertTrueNR(amConn(this))
            mTaskThread.start()
        }

        internal fun stop()
        {
            synchronized(this) {
                mSendThread?.interrupt()
            }

            mTaskQueue.clear()
            Log.d(TAG, "stop(): interrupting mTaskThread")
            mTaskThread.interrupt() // in case stalled connecting
            add(DisconnectTask(isLast=true))
        }

        private fun add(task: Task) = mTaskQueue.add(task)

        internal fun reconnect() = add(ConnectTask())

        internal fun timerFired()
        {
            Log.d(TAG, "$this.timerFired()")
            reconnect()
        }

        private val mConnectedLock = ReentrantLock()
        private val mConnectedCondition = mConnectedLock.newCondition()

        private var mSendThread: Thread? = null
        internal fun ensureSending()
        {
            synchronized ( this ) {
                if ( null == mSendThread ) {
                    mSendThread = thread {
                        val thread = Thread.currentThread()
                        while (!thread.isInterrupted()) {
                            try {
                                val msg = sOutboundQueue.take()
                                mConnectedLock.withLock {
                                    while (!mClient.state.isConnected) {
                                        mConnectedCondition.await()
                                    }
                                }
                                add(SendTask(msg))
                            } catch (ex: InterruptedException) {
                                Log.ex(TAG, ex)
                                break
                            }
                        }
                        Log.d(TAG, "exiting mSendThread")
                    }
                } else {
                    Assert.assertTrueNR(!mSendThread!!.isInterrupted())
                }
            }
        }

        val mPingProcs = HashMap<Int, PingResult>()
        private fun setPingProc(id: Int, pr: PingResult)
            = mPingProcs.put(id, pr)

        internal fun ping(pr: PingResult)
        {
            val id = Math.abs(Utils.nextRandomInt())
            setPingProc(id, pr)

            val packet = JSONObject()
                .putAnd("devid", mDevID)
                .putAnd("time", System.currentTimeMillis())
                .putAnd("id", id)
                .toString()

            val qos = chooseQOS(mContext, XwJNI.dvc_getQOS())
            val tap = TopicsAndPackets("xw4/ping/" + mDevID,
                                       packet.toByteArray(), qos.ordinal)
            add(SendTask(tap))
        }

        internal fun getStats(): String
        {
            return mStats.toString()
        }

        override fun onConnected(context: MqttClientConnectedContext) {
            Log.d(TAG, "$this.onConnected($context)")
            mConnectedLock.withLock {
                Log.d(TAG, "signaling connected")
                mConnectedCondition.signal()
            }
            add(SubscribeAllTask())
            updateStatus(true)
            mStats.updateState(true)
        }

        override fun onDisconnected(context: MqttClientDisconnectedContext) {
            Log.d(TAG, "onDisconnected(cause=${context.getCause()})")
            // try: context.getCause().printStackTrace()
            updateStatus(false)
            mStats.updateState(false)
        }

        private fun updateStatus(success: Boolean)
            = ConnStatusHandler.updateStatus(mContext, null,
                                             CommsAddrRec.CommsConnType.COMMS_CONN_MQTT,
                                             success)

        override fun accept(pub: Mqtt3Publish) {
            val payload = pub.payload
            val topic = pub.topic

            if (pub.payload.isPresent()) {
                Log.d(TAG, "accept($pub)")
                val byteBuf = pub.payload.get()
                val packet = ByteArray(byteBuf.capacity())
                byteBuf.get(packet)
                add(IncomingTask(topic.toString(), packet))
                XwJNI.sts_increment(STAT.STAT_MQTT_RCVD)
            } else {
                // Unretain message posted by server; no need to log!! In fact
                // it'd be nice if it weren't transmitted at all
                // Log.d( TAG, "no message found!!")
            }

            TimerReceiver.setBackoff(mContext, sTimerCallbacks, MIN_BACKOFF)
        }

        private abstract inner class Task(val isLast: Boolean = false): Runnable

        private inner class ConnectTask(): Task() {
            override fun run() {
                try {
                    if ( !mClient.state.isConnected ) {
                        Log.d( TAG, "calling connectWith()...")
                        mClient.connectWith()
                            .cleanSession(true)
                            .send()
                            .also{ ack -> Log.d(TAG, "connect.also($ack)") }
                    } else {
                        Log.d(TAG, "$this: already connected")
                    }
                } catch (ex: Exception) {
                    Log.ex(TAG, ex)
                }
            }
        }

        private inner class DisconnectTask(isLast: Boolean = false): Task(isLast) {
            override fun run() {
                try {
                    mClient
                        .disconnect()
                        .also{ ack -> Log.d(TAG, "disconnect.also($ack)") }
                } catch (ex: Exception) {
                    Log.ex(TAG, ex)
                }
            }
        }

        private inner class SubscribeAllTask(): Task() {
            override fun run() {
                val qosArray = intArrayOf(0)
                val topics = XwJNI.dvc_getMQTTSubTopics(qosArray) + arrayOf(PONG_PREFIX + mDevID)
                val qos = chooseQOS(mContext, qosArray[0])
                topics.map{ add(SubscribeTask(it, qos)) }
            }
        }

        private inner class SubscribeTask(val mTopic: String, val mQos: MqttQos): Task() {
            override fun run() {
                mClient.toAsync()
                    .subscribeWith()
                    .topicFilter(mTopic)
                    .qos(mQos)
                    .callback(this@Conn)
                    .send()
                    .whenComplete{ ack, throwable ->
                        Log.d( TAG, "$this.whenComplete(); topic=$mTopic, "
                               + "ack=$ack, err=$throwable")
                    }
            }
        }

        private inner class SendTask(val tap: TopicsAndPackets): Task()
        {
            override fun run() {
                val qos = chooseQOS(mContext, tap.qosInt())
                for (pr in tap.iterator()) {
                    mClient.toAsync()
                        .publishWith()
                        .topic(pr.first)
                        .payload(pr.second)
                        .qos(qos)
                        .retain(true)
                        .send()
                        .whenComplete { mqtt3Publish, throwable ->
                            // Log.d(TAG, "whenComplete(): $mqtt3Publish")
                            mStats.addSend(throwable == null)
                            if (throwable != null) {
                                // Handle failure to publish
                            } else {
                                // Handle successful publish, e.g. logging or incrementing a metric
                                TimerReceiver.setBackoff(mContext, sTimerCallbacks, MIN_BACKOFF)
                                XwJNI.sts_increment(STAT.STAT_MQTT_SENT)
                            }
                        }
                }
            }
        }

        private inner class IncomingTask(val mTopic: String,
                                         val mPacket: ByteArray): Task()
        {
            override fun run() {
                Log.d(TAG, "got msg; topic: ${mTopic}, len: ${mPacket.size}")
                if (mTopic.startsWith(PONG_PREFIX)) {
                    handlePong(mPacket)
                } else {
                    XwJNI.dvc_parseMQTTPacket(mTopic, mPacket)
                }
                ConnStatusHandler
                    .updateStatusIn(mContext, CommsAddrRec.CommsConnType.COMMS_CONN_MQTT,
                                    true)

            }
        }

        private inner class Stats {
            private var mConndTime = System.currentTimeMillis()
            private var mDisconndTime = mConndTime
            private var mDisconReps: Int = 0
            private var mSendOks: Int = 0
            private var mSendFails: Int = 0

            fun updateState(connected: Boolean)
            {
                val now = System.currentTimeMillis()
                if (connected) {
                    if ( mConndTime <= mDisconndTime) {
                        mConndTime = now
                    }
                } else if (!connected) {
                    if (mDisconndTime <= mConndTime) {
                        mDisconndTime = now
                    } else {
                        ++mDisconReps
                    }
                }
            }

            fun addSend(success: Boolean)
                = if ( success) ++mSendOks else ++mSendFails

            override fun toString(): String
            {
                val now = System.currentTimeMillis()
                val connected = mConndTime < mDisconndTime
                val secsInState = (now - (if (connected) mConndTime
                                          else mDisconndTime)) / 1000
                val age = (now - this@Conn.mStart) / 1000
                return StringBuilder()
                    .append("age: ${age}s\n")
                    .append("state: ${if (mClient.state.isConnected) "Connected" else "Unconnected"}")
                    .append(" (for the last ${secsInState} seconds)")
                    .append("\n").append("failed conns: $mDisconReps")
                    .append("\nSuccessful sends: $mSendOks")
                    .append("\nFailed sends: $mSendFails")
                    .toString()
            }
        }

        private fun handlePong(packet: ByteArray)
        {
            val payload = JSONObject(String(packet))
            val proc = mPingProcs.remove(payload.getInt("id"))
            proc?.onSuccess(mHost, System.currentTimeMillis() - payload.getLong("time"))
        }

        override fun toString(): String
        {
            val age = (System.currentTimeMillis() - mStart) / 1000
            return "client: {connected: ${mClient.state.isConnected}; age: ${age}s"
        }
    }

    private val sTimerCallbacks: TimerCallback = object : TimerCallback
    {
        override fun timerFired(context: Context) {
            getConn(context)?.timerFired()
        }

        override fun incrementBackoff(backoff: Long): Long {
            var backoff = backoff
            backoff = if (backoff < MIN_BACKOFF) {
                MIN_BACKOFF
            } else {
                backoff * 150 / 100
            }
            if (MAX_BACKOFF < backoff) {
                backoff = MAX_BACKOFF
            }
            return backoff
        }
    }
}
