/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2024 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.telephony.PhoneNumberUtils
import android.telephony.SmsManager

import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.TimeUnit

import org.eehouse.android.xw4.MultiService.DictFetchOwner
import org.eehouse.android.xw4.MultiService.MultiEvent
import org.eehouse.android.xw4.XWServiceHelper.ReceiveResult
import org.eehouse.android.xw4.jni.CommsAddrRec
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.XwJNI
import org.eehouse.android.xw4.jni.XwJNI.SMSProtoMsg
import org.eehouse.android.xw4.jni.XwJNI.SMS_CMD
import org.eehouse.android.xw4.loc.LocUtils

object NBSProto {
    private val TAG: String = NBSProto::class.java.simpleName

    private const val MSG_SENT = "MSG_SENT"
    private const val MSG_DELIVERED = "MSG_DELIVERED"
    private const val TOAST_FREQ = 5

    private var s_showToasts: Boolean? = null
    private var s_nReceived = 0
    private var s_nSent = 0

    private val s_sentDied: MutableSet<Int> = HashSet()

    fun handleFrom(
        context: Context, buffer: ByteArray,
        phone: String, port: Short
    ) {
        addPacketFrom(context, phone, port, buffer)
        Log.d(TAG, "got ${buffer.size} bytes from $phone")
        if ((0 == (++s_nReceived % TOAST_FREQ)) && showToasts(context)) {
            DbgUtils.showf(context, "Got NBS msg $s_nReceived")
        }

        ConnStatusHandler.updateStatusIn(
            context, CommsConnType.COMMS_CONN_SMS,
            true
        )
    }

    fun inviteRemote(
        context: Context, phone: String,
        nli: NetLaunchInfo
    ) {
        addInviteTo(context, phone, nli)
    }

    fun sendPacket(
        context: Context, phone: String,
        gameID: Int, binmsg: ByteArray, msgID: String?
    ): Int {
        Log.d(
            TAG, "sendPacket(phone=%s, gameID=%X, len=%d, msgID=%s)",
            phone, gameID, binmsg!!.size, msgID
        )
        addPacketTo(context, phone, gameID, binmsg)
        return binmsg.size
    }

    fun gameDied(context: Context, gameID: Int, phone: String?) {
        addGameDied(context, phone, gameID)
    }

    fun onGameDictDownload(context: Context, oldIntent: Intent?) {
        val nli = MultiService.getMissingDictData(context, oldIntent!!)
        addInviteFrom(context, nli)
    }

    fun stopThreads() {
        stopCurThreads()
    }

    fun smsToastEnable(newVal: Boolean) {
        s_showToasts = newVal
    }

    private fun addPacketFrom(
        context: Context, phone: String,
        port: Short, data: ByteArray
    ) {
        add(ReceiveElem(context, phone, port, data))
    }

    private fun addInviteFrom(context: Context, nli: NetLaunchInfo) {
        add(ReceiveElem(context, nli))
    }

    private fun addPacketTo(
        context: Context, phone: String?,
        gameID: Int, binmsg: ByteArray?
    ) {
        add(SendElem(context, phone, SMS_CMD.DATA, gameID, binmsg))
    }

    private fun addInviteTo(context: Context, phone: String, nli: NetLaunchInfo) {
        add(SendElem(context, phone, SMS_CMD.INVITE, nli))
    }

    private fun addGameDied(context: Context, phone: String?, gameID: Int) {
        add(SendElem(context, phone, SMS_CMD.DEATH, gameID, null))
    }

    private fun addAck(context: Context, phone: String, gameID: Int) {
        add(SendElem(context, phone, SMS_CMD.ACK_INVITE, gameID, null))
    }

    private fun add(elem: QueueElem) {
        if (XWPrefs.getNBSEnabled(elem.context)) {
            sQueue.add(elem)
            startThreadOnce()
        }
    }

    private val sQueue = LinkedBlockingQueue<QueueElem>()

    private val sThreadHolder = arrayOf<NBSProtoThread?>(null)

    private fun startThreadOnce() {
        synchronized(sThreadHolder) {
            if (sThreadHolder[0] == null) {
                sThreadHolder[0] = NBSProtoThread()
                sThreadHolder[0]!!.start()
            }
        }
    }

    private fun removeSelf(self: NBSProtoThread) {
        synchronized(sThreadHolder) {
            if (sThreadHolder[0] === self) {
                sThreadHolder[0] = null
            }
        }
    }

    private fun stopCurThreads() {
        synchronized(sThreadHolder) {
            val self = sThreadHolder[0]
            self?.interrupt()
        }
    }

    private var s_nbsPort: Short? = null
    private val nBSPort: Short
        get() {
            if (null == s_nbsPort) {
                val asStr = XWApp.getContext().getString(R.string.nbs_port)
                s_nbsPort = asStr.toInt().toShort()
            }
            return s_nbsPort!!
        }

    private fun showToasts(context: Context): Boolean {
        if (null == s_showToasts) {
            s_showToasts =
                XWPrefs.getPrefsBoolean(context, R.string.key_show_sms, false)
        }
        val result = s_showToasts!!
        return result
    }

    internal class NBSProtoThread : Thread("NBSProtoThread") {
        private val mWaitSecs = intArrayOf(0)
        private val mCachedDests: MutableSet<String> = HashSet()

        override fun run() {
            Log.d(TAG, "%s.run() starting", this)

            while (!isInterrupted) {
                try {
                    // We want to time out quickly IFF there's a potential
                    // message combination going on, i.e. if mWaitSecs[0] was
                    // set by smsproto_prepOutbound(). Otherwise sleep until
                    // there's something in the queue.
                    val waitSecs = (if (mWaitSecs[0] <= 0) 10 * 60 else mWaitSecs[0]).toLong()
                    val elem = sQueue.poll(waitSecs, TimeUnit.SECONDS)
                    if (!process(elem)) {
                        break
                    }
                } catch (iex: InterruptedException) {
                    Log.d(TAG, "poll() threw: $iex.message")
                    break
                }
            }

            removeSelf(this)

            Log.d(TAG, "%s.run() DONE", this)
        }

        private fun processReceive(elem: ReceiveElem): Boolean {
            if (null != elem.data) {
                val msgs = XwJNI
                    .smsproto_prepInbound(elem.data!!, elem.phone!!,
                                          elem.port.toInt())
                if (null != msgs) {
                    Log.d(TAG, "got $msgs.size msgs combined!", msgs.size)
                    for (ii in msgs.indices) {
                        Log.d(TAG, "%d: type: %s; len: %d", ii, msgs[ii].cmd, msgs[ii].data!!.size)
                    }
                    for (msg in msgs) {
                        receive(elem.context, elem.phone!!, msg)
                    }
                    getHelper().postEvent(MultiEvent.SMS_RECEIVE_OK)
                } else {
                    Log.d(
                        TAG, "processReceive(): bogus or incomplete message "
                                + "(%d bytes from %s)", elem.data!!.size, elem.phone
                    )
                }
            }
            if (null != elem.nli) {
                makeForInvite(elem.context, elem.phone!!, elem.nli)
            }
            return true
        }

        // Called when we have nothing to add, but might be sending what's
        // already waiting for possible combination with other messages.
        private fun processRetry(): Boolean {
            var handled = false

            val iter = mCachedDests.iterator()
            while (iter.hasNext()) {
                val portAndPhone = iter.next().split("\u0000".toRegex(), limit = 2).toTypedArray()
                val port = portAndPhone[0].toShort()
                val msgs = XwJNI.smsproto_prepOutbound(
                    portAndPhone[1], port.toInt(), mWaitSecs
                )
                if (null != msgs) {
                    sendBuffers(msgs, portAndPhone[1], port)
                    handled = true
                }
                val needsRetry = mWaitSecs[0] > 0
                if (!needsRetry) {
                    iter.remove()
                }
                handled = handled || needsRetry
            }
            return handled
        }

        private fun processSend(elem: SendElem): Boolean {
            val msgs = XwJNI.smsproto_prepOutbound(
                elem.cmd, elem.gameID, elem.data,
                elem.phone!!, elem.port.toInt(), mWaitSecs
            )
            if (null != msgs) {
                sendBuffers(msgs, elem.phone!!, elem.port)
            }

            val needsRetry = mWaitSecs[0] > 0
            if (needsRetry) {
                cacheForRetry(elem)
            }

            return null != msgs || needsRetry
        }

        private fun process(qelm: QueueElem?): Boolean {
            val handled = if (null == qelm) {
                processRetry()
            } else if (qelm is SendElem) {
                processSend(qelm)
            } else {
                processReceive(qelm as ReceiveElem)
            }
            Log.d(TAG, "%s.process($qelm) => $handled", this)
            return handled
        }

        private var mHelper: SMSServiceHelper? = null
        private fun getHelper(): SMSServiceHelper
        {
            if (null == mHelper) {
                mHelper = SMSServiceHelper(XWApp.getContext())
            }
            return mHelper!!
        }

        private fun receive(context: Context, phone: String, msg: SMSProtoMsg) {
            Log.i(TAG, "receive(cmd=%s)", msg.cmd)
            when (msg.cmd) {
                SMS_CMD.INVITE -> makeForInvite(
                    context, phone,
                    NetLaunchInfo.makeFrom(context, msg.data)
                )

                SMS_CMD.DATA -> if (feedMessage(
                        context, msg.gameID, msg.data,
                        CommsAddrRec(phone)
                    )
                ) {
                    SMSResendReceiver.resetTimer(context)
                }

                SMS_CMD.DEATH -> getHelper().postEvent(MultiEvent.MESSAGE_NOGAME, msg.gameID)
                SMS_CMD.ACK_INVITE -> getHelper().postEvent(MultiEvent.NEWGAME_SUCCESS, msg.gameID)
                else -> {
                    Log.w(TAG, "unexpected cmd %s", msg.cmd)
                    Assert.failDbg()
                }
            }
        }

        private fun feedMessage(
            context: Context, gameID: Int, msg: ByteArray?,
            addr: CommsAddrRec
        ): Boolean {
            val rslt = getHelper()
                .receiveMessage(gameID, null, msg!!, addr)
            if (ReceiveResult.GAME_GONE == rslt) {
                sendDiedPacket(context, addr.sms_phone, gameID)
            }
            Log.d(TAG, "feedMessage(): rslt: $rslt")
            return rslt == ReceiveResult.OK
        }

        private fun sendDiedPacket(context: Context, phone: String?, gameID: Int) {
            if (!s_sentDied.contains(gameID)) {
                addGameDied(context, phone, gameID)
                s_sentDied.add(gameID)
            }
        }

        private fun makeForInvite(context: Context, phone: String, nli: NetLaunchInfo?) {
            if (nli != null) {
                getHelper().handleInvitation(nli, phone, DictFetchOwner.OWNER_SMS)
                addAck(context, phone, nli.gameID())
            }
        }

        private fun sendBuffers(fragments: Array<ByteArray>, phone: String, port: Short) {
            val context = XWApp.getContext()
            var success = false
            if (XWPrefs.getNBSEnabled(context) && Perms23.haveNBSPerms(context)
            ) {
                // Try send-to-self

                if (XWPrefs.getSMSToSelfEnabled(context)) {
                    val myPhone = SMSPhoneInfo.get(context)!!.number
                    if (null != myPhone
                        && PhoneNumberUtils.compare(phone, myPhone)
                    ) {
                        for (fragment in fragments) {
                            handleFrom(context, fragment, phone, port)
                        }
                        success = true
                    }
                }

                if (!success) {
                    try {
                        val mgr = SmsManager.getDefault()
                        val sent = makeStatusIntent(context, MSG_SENT)
                        val delivery = makeStatusIntent(context, MSG_DELIVERED)
                        for (fragment in fragments) {
                            mgr.sendDataMessage(
                                phone, null, port, fragment,
                                sent, delivery
                            )
                        }
                        success = true
                    } catch (iae: IllegalArgumentException) {
                        Log.w(TAG, "sendBuffers(%s): %s", phone, iae.toString())
                    } catch (npe: NullPointerException) {
                        Assert.failDbg() // shouldn't be trying to do this!!!
                    } catch (se: SecurityException) {
                        getHelper().postEvent(MultiEvent.SMS_SEND_FAILED_NOPERMISSION)
                    } catch (ee: Exception) {
                        Log.ex(TAG, ee)
                    }
                }
            } else {
                Log.i(TAG, "dropping because SMS disabled")
            }

            if (success && (0 == (++s_nSent % TOAST_FREQ) && showToasts(context))) {
                DbgUtils.showf(context, "Sent msg %d", s_nSent)
            }

            ConnStatusHandler.updateStatusOut(
                context, CommsConnType.COMMS_CONN_SMS,
                success
            )
        }

        private fun makeStatusIntent(context: Context, msg: String): PendingIntent {
            val intent = Intent(msg)
            return PendingIntent.getBroadcast(
                context, 0, intent,
                PendingIntent.FLAG_IMMUTABLE
            )
        }

        private fun cacheForRetry(elem: QueueElem) {
            val dest = elem.port.toString() + "\u0000" + elem.phone
            mCachedDests.add(dest)
        }
    }

    private open class QueueElem @JvmOverloads constructor(
        var context: Context,
        var phone: String?,
        var port: Short = nBSPort
    )

    private class SendElem(
        context: Context, phone: String?, var cmd: SMS_CMD, var gameID: Int,
        var data: ByteArray?
    ) : QueueElem(context, phone) {
        constructor(context: Context, phone: String?, cmd: SMS_CMD, nli: NetLaunchInfo) : this(
            context,
            phone,
            cmd,
            0,
            nli.asByteArray()
        )

        override fun toString(): String {
            return String.format(
                "SendElem: {cmd: %s, dataLen: %d}", cmd,
                if (data == null) 0 else data!!.size
            )
        }
    }

    private class ReceiveElem : QueueElem {
        // One of these two will be set
        var data: ByteArray? = null
        var nli: NetLaunchInfo? = null

        constructor(
            context: Context,
            phone: String?,
            port: Short,
            data: ByteArray
        ) : super(context, phone, port) {
            this.data = data
        }

        constructor(context: Context, nli: NetLaunchInfo) : super(context, nli.phone) {
            this.nli = nli
        }

        override fun toString(): String {
            return String.format("ReceiveElem: {nli: %s, data: %s}", nli, data)
        }
    }

    private class NBSMsgSink(private val mContext: Context) : MultiMsgSink(mContext) {
        override fun sendViaSMS(
            buf: ByteArray,
            msgID: String?,
            gameID: Int,
            addr: CommsAddrRec
        ): Int {
            return sendPacket(mContext, addr.sms_phone!!, gameID, buf, msgID)
        }
    }

    private class SMSServiceHelper internal constructor(private val mContext: Context) :
        XWServiceHelper(mContext) {
        override fun getSink(rowid: Long): MultiMsgSink {
            return NBSMsgSink(mContext)
        }

        override fun postNotification(phone: String?, gameID: Int, rowid: Long) {
            val owner = Utils.phoneToContact(mContext, phone!!, true)
            val body = LocUtils.getString(
                mContext, R.string.new_name_body_fmt,
                owner
            )
            GameUtils.postInvitedNotification(mContext, gameID, body, rowid)
        }
    }
}
