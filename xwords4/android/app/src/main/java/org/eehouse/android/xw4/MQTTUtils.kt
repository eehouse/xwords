/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */ /*
 * Copyright 2009 - 2023 by Eric House (xwords@eehouse.org).  All rights
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
import org.eclipse.paho.client.mqttv3.IMqttActionListener
import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken
import org.eclipse.paho.client.mqttv3.IMqttToken
import org.eclipse.paho.client.mqttv3.MqttAsyncClient
import org.eclipse.paho.client.mqttv3.MqttCallbackExtended
import org.eclipse.paho.client.mqttv3.MqttConnectOptions
import org.eclipse.paho.client.mqttv3.MqttException
import org.eclipse.paho.client.mqttv3.MqttMessage
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence
import org.eehouse.android.xw4.DBUtils.getIntFor
import org.eehouse.android.xw4.DBUtils.getRowIDsFor
import org.eehouse.android.xw4.DBUtils.setIntFor
import org.eehouse.android.xw4.DBUtils.setLongFor
import org.eehouse.android.xw4.DbgUtils.assertOnUIThread
import org.eehouse.android.xw4.NetStateCache.StateChangedIf
import org.eehouse.android.xw4.TimerReceiver.TimerCallback
import org.eehouse.android.xw4.jni.CommsAddrRec
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.CommsAddrRec.ConnExpl
import org.eehouse.android.xw4.jni.XwJNI.Companion.dvc_getMQTTDevID
import org.eehouse.android.xw4.jni.XwJNI.Companion.dvc_getMQTTSubTopics
import org.eehouse.android.xw4.jni.XwJNI.Companion.dvc_makeMQTTNoSuchGames
import org.eehouse.android.xw4.jni.XwJNI.Companion.dvc_makeMQTTNukeInvite
import org.eehouse.android.xw4.jni.XwJNI.Companion.dvc_parseMQTTPacket
import org.eehouse.android.xw4.jni.XwJNI.Companion.kplr_nameForMqttDev
import org.eehouse.android.xw4.jni.XwJNI.TopicsAndPackets
import org.eehouse.android.xw4.loc.LocUtils
import org.json.JSONException
import org.json.JSONObject
import java.util.Locale
import java.util.concurrent.LinkedBlockingQueue
import kotlin.math.abs

class MQTTUtils private constructor(context: Context, resendOnConnect: Boolean) : Thread(),
    IMqttActionListener, MqttCallbackExtended {
    private enum class State {
        NONE, CONNECTING, CONNECTED, SUBSCRIBING, SUBSCRIBED,
        CLOSING
    }

    private var mClient: MqttAsyncClient? = null
    private val mDevID: String
    private val mSubTopics: Array<String>
    private val mContext: Context
    private val mRxMsgThread: RxMsgThread
    private val mOutboundQueue = LinkedBlockingQueue<MessagePair>()
    private var mShouldExit = false
    private var mState = State.NONE
    private var mNeedsResend: Boolean

    private class MessagePair // outgoing
        (var mTopics: Array<String>?, var mPackets: Array<ByteArray>?) {
        // incoming: only one topic
        constructor(topic: String, packet: ByteArray) : this(
            arrayOf<String>(topic),
            arrayOf<ByteArray>(packet)
        )
    }

    override fun run() {
        val startTime = Utils.getCurSeconds()
        Log.d(TAG, "%H.run() starting", this)

        setup()
        var totalSlept: Long = 0
        while (!mShouldExit && totalSlept < 10000) {
            try {
                // this thread can be fed before the connection is
                // established. Wait for that before removing packets from the
                // queue.
                if (!mClient!!.isConnected) {
                    Log.d(TAG, "%H.run(): not connected; sleeping...", this@MQTTUtils)
                    val thisSleep: Long = 1000
                    sleep(thisSleep)
                    totalSlept += thisSleep
                    continue
                }
                totalSlept = 0
                val pair = mOutboundQueue.take()
                for (ii in pair.mPackets!!.indices) {
                    val message = MqttMessage(pair.mPackets!![ii])
                    message.isRetained = true
                    mClient!!.publish(pair.mTopics!![ii], message)
                    Log.d(
                        TAG, "%H: published msg of len %d to topic %s", this@MQTTUtils,
                        pair.mPackets!![ii].size, pair.mTopics!![ii]
                    )
                }
            } catch (me: MqttException) {
                me.printStackTrace()
                break
            } catch (ie: InterruptedException) {
                // ie.printStackTrace();
                break
            }
        }
        clearInstance()

        val now = Utils.getCurSeconds()
        Log.d(
            TAG, "%H.run() exiting after %d seconds", this,
            now - startTime
        )
    }

    private val isConnected: Boolean
        get() {
            val client = mClient
            val result = null != client && client.isConnected && mState != State.CLOSING
            Log.d(TAG, "isConnected() => %b", result)
            return result
        }

    private fun enqueue(topics: Array<String>?, packets: Array<ByteArray>?) {
        mOutboundQueue.add(MessagePair(topics, packets))
    }

    private fun setState(newState: State) {
        Log.d(TAG, "%H.setState(): was %s, now %s", this, mState, newState)
        val stateOk: Boolean
        when (newState) {
            State.CONNECTED -> {
                stateOk = mState == State.CONNECTING
                if (stateOk) {
                    mState = newState
                    subscribe()
                }
            }

            State.SUBSCRIBED -> {
                stateOk = mState == State.SUBSCRIBING
                if (stateOk) {
                    mState = newState
                    mRxMsgThread.start()
                }
            }

            else -> {
                stateOk = true
                mState = newState
                Log.d(TAG, "doing nothing on %s", mState)
            }
        }
        if (!stateOk) {
            Log.e(TAG, "%H.setState(): bad state for %s: %s", this, newState, mState)
        }
    }

    // Last Will and Testimate is the way I can get the server notified of a
    // closed connection.
    private fun addLWT(mqttConnectOptions: MqttConnectOptions) {
        try {
            val payload = JSONObject()
            payload.put("devid", mDevID)
            payload.put("ts", Utils.getCurSeconds())
            mqttConnectOptions.setWill("xw4/device/LWT", payload.toString().toByteArray(), 2, false)

            // mqttConnectOptions.setKeepAliveInterval( 15 ); // seconds; for testing
        } catch (je: JSONException) {
            Log.e(TAG, "addLWT() ex: %s", je)
        }
    }

    private fun setup() {
        Log.d(TAG, "setup()")
        val mqttConnectOptions = MqttConnectOptions()
        mqttConnectOptions.isAutomaticReconnect = true
        mqttConnectOptions.isCleanSession = false
        mqttConnectOptions.userName = "xwuser"
        mqttConnectOptions.password = "xw4r0cks".toCharArray()
        addLWT(mqttConnectOptions)

        try {
            setState(State.CONNECTING)
            mClient!!.connect(mqttConnectOptions, null, this)
        } catch (ex: MqttException) {
            ex.printStackTrace()
        } catch (ise: IllegalStateException) {
            ise.printStackTrace()
        } catch (ise: Exception) {
            ise.printStackTrace()
            clearInstance()
        }
    }

    init {
        Log.d(TAG, "%H.<init>()", this)
        mContext = context
        mNeedsResend = resendOnConnect
        mDevID = dvc_getMQTTDevID()
        mSubTopics = dvc_getMQTTSubTopics()
        Assert.assertTrueNR(16 == mDevID.length)
        mRxMsgThread = RxMsgThread()

        val host = XWPrefs.getPrefsString(context, R.string.key_mqtt_host)
            .trim { it <= ' ' } // in case some idiot adds whitespace. Ahem.
        val port = XWPrefs.getPrefsInt(context, R.string.key_mqtt_port, 1883)
        val url = String.format(Locale.US, "tcp://%s:%d", host, port)
        Log.d(TAG, "Using url: %s", url)
        try {
            mClient = MqttAsyncClient(url, mDevID, MemoryPersistence())
            mClient!!.setCallback(this)
        } catch (ex: Exception) {
            Log.ex(TAG, ex)
            mClient = null
        }
    }

    private fun disconnect() {
        Log.d(TAG, "%H.disconnect()", this)

        interrupt()
        mRxMsgThread.interrupt()
        try {
            mRxMsgThread.join()
            Log.d(TAG, "%H.disconnect(); JOINED thread", this)
        } catch (ie: InterruptedException) {
            Log.e(TAG, "%H.disconnect(); got ie from join: %s", this, ie)
        }

        mShouldExit = true

        setState(State.CLOSING)

        var client: MqttAsyncClient?
        synchronized(this) {
            client = mClient
            mClient = null
        }

        // Hack. Problem is that e.g. unsubscribe will throw an exception if
        // you're not subscribed. That can't prevent us from continuing to
        // disconnect() and close. Rather than wrap each in its own try/catch,
        // run 'em in a loop in a single try/catch.
        if (null == client) {
            Log.e(TAG, "disconnect(): null client")
        } else {
            startDisconThread(client!!)
        }

        // Make sure we don't need to call clearInstance(this)
        synchronized(sInstance) {
            Assert.assertTrueNR(
                sInstance[0] !== this
            )
        }
        Log.d(TAG, "%H.disconnect() DONE", this)
    }

    private fun startDisconThread(client: MqttAsyncClient) {
        Thread(object : Runnable {
            override fun run() {
                Log.d(TAG, "startDisconThread().run() starting")
                var ii = 0
                outer@ while (true) {
                    var action: String? = null
                    var token: IMqttToken? = null
                    try {
                        when (ii) {
                            0 -> {
                                action = "unsubscribe"
                                token = client.unsubscribe(mSubTopics)
                            }

                            1 -> {
                                action = "disconnect"
                                token = client.disconnect()
                            }

                            2 -> {
                                action = "close"
                                client.close()
                            }

                            else -> break@outer
                        }
                        if (null != token) {
                            Log.d(TAG, "%H.disconnect(): %s() waiting", this, action)
                            token.waitForCompletion()
                        }
                        if (null != action) {
                            Log.d(TAG, "%H.run(): client.%s() succeeded", this, action)
                        }
                    } catch (mex: MqttException) {
                        Log.e(
                            TAG, "%H.run(): client.%s(): got mex: %s",
                            this, action, mex
                        )
                        // Assert.failDbg(); // fired, so remove for now
                    } catch (ex: Exception) {
                        Log.e(
                            TAG, "%H.run(): client.%s(): got ex %s",
                            this, action, ex
                        )
                        Assert.failDbg() // is this happening?
                    }
                    ++ii
                }
                Log.d(TAG, "startDisconThread().run() finishing")
            }
        }).start()
    }

    private fun clearInstance() {
        Log.d(TAG, "%H.clearInstance()", this)
        clearInstance(this)
    }

    // MqttCallbackExtended
    override fun connectComplete(reconnect: Boolean, serverURI: String) {
        Log.d(
            TAG, "%H.connectComplete(reconnect=%b, serverURI=%s)", this,
            reconnect, serverURI
        )
        if (mNeedsResend) {
            mNeedsResend = false
            resendAllIf(mContext)
        }
    }

    override fun connectionLost(cause: Throwable) {
        Log.d(TAG, "%H.connectionLost(cause=%s)", this, cause)
    }

    @Throws(Exception::class)
    override fun messageArrived(topic: String, message: MqttMessage) {
        val payload = message.payload
        Log.d(
            TAG, "%H.messageArrived(topic=%s, len=%d)", this, topic,
            payload.size
        )
        if (0 < payload.size) {
            mRxMsgThread.add(topic, payload)
        }
        ConnStatusHandler
            .updateStatusIn(mContext, CommsConnType.COMMS_CONN_MQTT, true)

        TimerReceiver.setBackoff(mContext, sTimerCallbacks, MIN_BACKOFF)
    }

    override fun deliveryComplete(token: IMqttDeliveryToken) {
        // Log.d( TAG, "%H.deliveryComplete(token=%s)", this, token );
        ConnStatusHandler
            .updateStatusOut(mContext, CommsConnType.COMMS_CONN_MQTT, true)
        TimerReceiver.setBackoff(mContext, sTimerCallbacks, MIN_BACKOFF)
    }

    private fun subscribe() {
        val qos = XWPrefs
            .getPrefsInt(mContext, R.string.key_mqtt_qos, 2)
        val qoss = IntArray(mSubTopics.size)
        for (ii in qoss.indices) {
            qoss[ii] = qos
        }

        setState(State.SUBSCRIBING)
        try {
            // Log.d( TAG, "subscribing to %s", TextUtils.join( ", ", mSubTopics ) );
            mClient!!.subscribe(mSubTopics, qoss, null, this)
        } catch (ex: MqttException) {
            ex.printStackTrace()
        } catch (ex: Exception) {
            ex.printStackTrace()
            clearInstance()
        }
    }

    // IMqttActionListener
    override fun onSuccess(asyncActionToken: IMqttToken) {
        Log.d(
            TAG, "%H.onSuccess(%s); cur state: %s", this, asyncActionToken,
            mState
        )
        when (mState) {
            State.CONNECTING -> setState(State.CONNECTED)
            State.SUBSCRIBING -> setState(State.SUBSCRIBED)
            else -> Log.e(TAG, "%H.onSuccess(): unexpected state %s", this, mState)
        }
    }

    override fun onFailure(asyncActionToken: IMqttToken, exception: Throwable) {
        Log.d(
            TAG, "%H.onFailure(%s, %s); cur state: %s", this,
            asyncActionToken, exception, mState
        )
        ConnStatusHandler
            .updateStatus(
                mContext, null, CommsConnType.COMMS_CONN_MQTT,
                false
            )
    }

    private inner class RxMsgThread : Thread() {
        private val mQueue = LinkedBlockingQueue<MessagePair>()

        fun add(topic: String, msg: ByteArray) {
            Log.d(
                TAG, "%H.RxMsgThread.add(topic: %s, len: %d)", this@MQTTUtils,
                topic, msg.size
            )
            mQueue.add(MessagePair(topic, msg))
        }

        override fun run() {
            val startTime = Utils.getCurSeconds()
            Log.d(TAG, "%H.RxMsgThread.run() starting", this@MQTTUtils)
            while (true) {
                try {
                    val pair = mQueue.take()
                    Assert.assertTrueNR(1 == pair.mTopics!!.size)
                    dvc_parseMQTTPacket(pair.mTopics!![0], pair.mPackets!![0])
                } catch (ie: InterruptedException) {
                    break
                }
            }
            val now = Utils.getCurSeconds()
            Log.d(
                TAG, "%H.RxMsgThread.run() exiting after %d seconds",
                this@MQTTUtils, now - startTime
            )
        }
    }

    private class MQTTServiceHelper(context: Context?) : XWServiceHelper(context) {
        private var mReturnAddr: CommsAddrRec? = null

        constructor(context: Context?, from: CommsAddrRec?) : this(context) {
            mReturnAddr = from
        }

        fun handleInvitation(nli: NetLaunchInfo) {
            handleInvitation(nli, null, MultiService.DictFetchOwner.OWNER_MQTT)
            // Now nuke the invitation so we don't keep getting it, e.g. if
            // the sender deletes the game
            val tap = dvc_makeMQTTNukeInvite(nli)
            addToSendQueue(context, tap)
        }

        fun receiveMessage(rowid: Long, sink: MultiMsgSink, msg: ByteArray) {
            // Log.d( TAG, "receiveMessage(rowid=%d, len=%d)", rowid, msg.length );
            receiveMessage(rowid, sink, msg, mReturnAddr)
        }
    }

    companion object {
        private val TAG: String = MQTTUtils::class.java.simpleName
        private val KEY_NEXT_REG = TAG + "/next_reg"
        private val KEY_LAST_WRITE = TAG + "/last_write"
        private val KEY_TMP_KEY = TAG + "/tmp_key"
        private const val MIN_BACKOFF = (1000 * 60 * 2 // 2 minutes
                ).toLong()
        private const val MAX_BACKOFF = (1000 * 60 * 60 * 4 // 4 hours, to test
                ).toLong()

        private val sInstance = arrayOf<MQTTUtils?>(null)
        private const val sNextReg: Long = 0
        private val sLastRev: String? = null

        private val sTimerCallbacks
                : TimerCallback = object : TimerCallback {
            override fun timerFired(context: Context) {
                Log.d(TAG, "timerFired()")
                Companion.timerFired(context)
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

        private val sStateChangedIf = StateChangedIf { context, nowAvailable ->
            Log.d(TAG, "onNetAvail(avail=%b)", nowAvailable)
            assertOnUIThread()
            if (nowAvailable) {
                resendAllIf(context)
            }
        }

        private fun resendAllIf(context: Context) {
            GameUtils.resendAllIf(context, CommsConnType.COMMS_CONN_MQTT)
        }

        fun init(context: Context) {
            Log.d(TAG, "init()")
            NetStateCache.register(context, sStateChangedIf)
            getOrStart(context)
        }

        fun onResume(context: Context) {
            Log.d(TAG, "onResume()")
            getOrStart(context)
            NetStateCache.register(context, sStateChangedIf)
        }

        fun onDestroy(context: Context?) {
            NetStateCache.unregister(context, sStateChangedIf)
        }

        @JvmStatic
        fun setEnabled(context: Context, enabled: Boolean) {
            Log.d(TAG, "setEnabled( %b )", enabled)
            if (enabled) {
                getOrStart(context)
            } else {
                onConfigChanged(context)
            }
        }

        private fun timerFired(context: Context) {
            var instance: MQTTUtils?
            synchronized(sInstance) {
                instance = sInstance[0]
            }

            if (null != instance && !instance!!.isConnected) {
                clearInstance(instance)
            }
            getOrStart(context) // no-op if have instance
        }

        @JvmStatic
        fun onConfigChanged(context: Context) {
            synchronized(sInstance) {
                if (null != sInstance[0]) {
                    clearInstance(sInstance[0])
                }
            }
            getOrStart(context, true)
        }

        private fun getOrStart(context: Context): MQTTUtils? {
            return getOrStart(context, false)
        }

        private fun getOrStart(context: Context, resendOnConnect: Boolean): MQTTUtils? {
            var result: MQTTUtils? = null
            if (XWPrefs.getMQTTEnabled(context)) {
                synchronized(sInstance) {
                    result = sInstance[0]
                    if (null == result) {
                        try {
                            result = MQTTUtils(context, resendOnConnect)
                            setInstance(result!!)
                            result!!.start()
                        } catch (me: MqttException) {
                            result = null
                        }
                    }
                }
            }
            return result
        }

        private fun setInstance(newInstance: MQTTUtils) {
            var oldInstance: MQTTUtils?
            synchronized(sInstance) {
                oldInstance = sInstance[0]
                Log.d(
                    TAG,
                    "setInstance(): changing sInstance[0] from %H to %H",
                    oldInstance,
                    newInstance
                )
                sInstance[0] = newInstance
            }
            if (null != oldInstance) {
                oldInstance!!.disconnect()
            }
        }

        private fun clearInstance(curInstance: MQTTUtils?) {
            synchronized(sInstance) {
                if (sInstance[0] === curInstance) {
                    sInstance[0] = null
                } else {
                    Log.e(
                        TAG, "clearInstance(): was NOT disconnecting %H because "
                                + "not current", curInstance
                    )
                    // I don't know why I was NOT disconnecting if the instance didn't match.
                    // If it was the right thing to do after all, add explanation here!!!!
                    // curInstance = null; // protect from disconnect() call -- ????? WHY DO THIS ?????
                }
            }
            curInstance!!.disconnect()
        }

        private var sTmpKey = 0
        private fun getTmpKey(context: Context): Int {
            while (0 == sTmpKey) {
                sTmpKey = getIntFor(context, KEY_TMP_KEY, 0)
                if (0 == sTmpKey) {
                    sTmpKey = abs(Utils.nextRandomInt().toDouble())
                        .toInt()
                    setIntFor(context, KEY_TMP_KEY, sTmpKey)
                }
            }
            return sTmpKey
        }

        private fun notifyNotHere(
            context: Context, addressee: String?,
            gameID: Int
        ) {
            val tap = dvc_makeMQTTNoSuchGames(addressee!!, gameID)
            addToSendQueue(context, tap)
        }

        @JvmStatic
        fun send(context: Context, tap: TopicsAndPackets): Int {
            addToSendQueue(context, tap)
            return -1
        }

        private fun addToSendQueue(context: Context, tap: TopicsAndPackets) {
            val instance = getOrStart(context)
            instance?.enqueue(tap.topics, tap.packets)
        }

        @JvmStatic
        fun gameDied(context: Context, devID: String?, gameID: Int) {
            val tap = dvc_makeMQTTNoSuchGames(devID!!, gameID)
            addToSendQueue(context, tap)
        }

        fun handleMessage(
            context: Context, from: CommsAddrRec,
            gameID: Int, data: ByteArray
        ) {
            val rowids = getRowIDsFor(context, gameID)
            Log.d(TAG, "handleMessage(): got %d rows for gameID %X", rowids.size, gameID)
            if (0 == rowids.size) {
                notifyNotHere(context, from.mqtt_devID, gameID)
            } else {
                val helper = MQTTServiceHelper(context, from)
                for (rowid in rowids) {
                    val sink = MultiMsgSink(context, rowid)
                    helper.receiveMessage(rowid, sink, data)
                }
            }
        }

        fun handleCtrlReceived(context: Context?, buf: ByteArray?) {
            try {
                val obj = JSONObject(String(buf!!))
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

        fun handleGameGone(context: Context?, from: CommsAddrRec, gameID: Int) {
            val player = kplr_nameForMqttDev(from.mqtt_devID)
            val expl = if (null == player) null
            else ConnExpl(CommsConnType.COMMS_CONN_MQTT, player)
            MQTTServiceHelper(context, from)
                .postEvent(
                    MultiService.MultiEvent.MESSAGE_NOGAME, gameID,
                    expl
                )
        }

        fun fcmConfirmed(context: Context?, working: Boolean) {
            if (working) {
                setLongFor(context!!, KEY_NEXT_REG, 0)
            }
        }

        fun makeOrNotify(context: Context?, nli: NetLaunchInfo) {
            MQTTServiceHelper(context).handleInvitation(nli)
        }
    }
}
