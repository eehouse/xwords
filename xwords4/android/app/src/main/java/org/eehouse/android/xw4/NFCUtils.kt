/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */ /*
 * Copyright 2013 by Eric House (xwords@eehouse.org).  All rights
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
import android.app.Dialog
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.nfc.NfcAdapter
import android.nfc.NfcAdapter.ReaderCallback
import android.nfc.NfcManager
import android.nfc.Tag
import android.nfc.tech.IsoDep
import android.os.Build
import android.text.TextUtils
import org.eehouse.android.xw4.DBUtils.getIntFor
import org.eehouse.android.xw4.DBUtils.getRowIDsFor
import org.eehouse.android.xw4.DBUtils.setIntFor
import org.eehouse.android.xw4.DbgUtils.assertOnUIThread
import org.eehouse.android.xw4.DbgUtils.hexDump
import org.eehouse.android.xw4.NFCUtils.Wrapper.Procs
import org.eehouse.android.xw4.jni.CommsAddrRec
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.loc.LocUtils
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.DataInputStream
import java.io.DataOutputStream
import java.io.IOException
import java.lang.ref.WeakReference
import java.math.BigInteger
import java.util.Arrays
import java.util.Random
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.atomic.AtomicInteger
import kotlin.math.abs
import kotlin.math.min

object NFCUtils {
    private val TAG = NFCUtils::class.java.getSimpleName()
    private const val USE_BIGINTEGER = true
    const val VERSION_1 = 0x01.toByte()
    private const val MESSAGE: Byte = 0x01
    private const val INVITE: Byte = 0x02
    private const val REPLY: Byte = 0x03
    private const val REPLY_NOGAME: Byte = 0x00
    private val sInSDK = 19 <= Build.VERSION.SDK_INT
    private var sNfcAvail: BooleanArray? = null

    // Return array of two booleans, the first indicating whether the
    // device supports NFC and the second whether it's on.  Only the
    // second can change.
    @JvmStatic
    fun nfcAvail(context: Context): BooleanArray {
        if (null == sNfcAvail) {
            sNfcAvail = booleanArrayOf(
                sInSDK && null != getNFCAdapter(context),
                false
            )
        }
        if (sNfcAvail!![0]) {
            sNfcAvail!![1] = getNFCAdapter(context)!!.isEnabled
        }
        // Log.d( TAG, "nfcAvail() => {%b,%b}", s_nfcAvail[0], s_nfcAvail[1] );
        return sNfcAvail!!
    }

    @JvmStatic
    fun makeEnableNFCDialog(activity: Activity): Dialog {
        val lstnr = DialogInterface.OnClickListener { dialog, item ->
            activity.startActivity( Intent( "android.settings.NFC_SETTINGS" ) )
        }
        return LocUtils.makeAlertBuilder(activity)
            .setTitle(R.string.info_title)
            .setMessage(R.string.enable_nfc)
            .setPositiveButton(android.R.string.cancel, null)
            .setNegativeButton(R.string.button_go_settings, lstnr)
            .create()
    }

    private fun getNFCAdapter(context: Context): NfcAdapter? {
        val manager = context.getSystemService(Context.NFC_SERVICE) as NfcManager
        return manager.defaultAdapter
    }

    private fun formatMsgs(gameID: Int, msgs: List<ByteArray>): ByteArray? {
        return formatMsgs(gameID, msgs.toTypedArray<ByteArray>())
    }

    private fun formatMsgs(gameID: Int, msgs: Array<ByteArray>?): ByteArray? {
        var result: ByteArray? = null
        if (null != msgs && 0 < msgs.size) {
            try {
                val baos = ByteArrayOutputStream()
                val dos = DataOutputStream(baos)
                dos.writeInt(gameID)
                Log.d(TAG, "formatMsgs(): wrote gameID: %d", gameID)
                dos.flush()
                baos.write(msgs.size)
                for (ii in msgs.indices) {
                    val msg = msgs[ii]
                    val len = msg.size.toShort()
                    baos.write(len.toInt() and 0xFF)
                    baos.write(len.toInt() shr 8 and 0xFF)
                    baos.write(msg)
                }
                result = baos.toByteArray()
            } catch (ioe: IOException) {
                Assert.failDbg()
            }
        }
        Log.d(TAG, "formatMsgs(gameID=%d) => %s", gameID, hexDump(result))
        return result
    }

    private fun unformatMsgs(data: ByteArray, start: Int, gameID: IntArray): ArrayList<ByteArray> {
        val result = ArrayList<ByteArray>()
        try {
            val bais = ByteArrayInputStream(data, start, data.size)
            val dis = DataInputStream(bais)
            gameID[0] = dis.readInt()
            Log.d(TAG, "unformatMsgs(): read gameID: %d", gameID[0])
            val count = bais.read()
            Log.d(TAG, "unformatMsgs(): read count: %d", count)
            for (ii in 0 until count) {
                var len = bais.read().toShort()
                len = (len.toInt() or (bais.read() shl 8)).toShort()
                Log.d(TAG, "unformatMsgs(): read len %d for msg %d", len, ii)
                val msg = ByteArray(len.toInt())
                val nRead = bais.read(msg)
                Assert.assertTrue(nRead == msg.size)
                result.add(msg)
            }
        } catch (ex: IOException) {
            Log.d(TAG, "ex: %s: %s", ex, ex.message)
            result.clear()
            gameID[0] = 0
        }
        Log.d(
            TAG, "unformatMsgs() => %s (len=%d)", result,
            result?.size ?: 0
        )
        return result
    }

    private val sMsgsStore = MsgsStore()
    fun setHaveDataListener(gameID: Int, listener: HaveDataListener) {
        sMsgsStore.setHaveDataListener(gameID, listener)
    }

    @JvmStatic
    fun addMsgFor(msg: ByteArray, gameID: Int): Int {
        return sMsgsStore.addMsgFor(gameID, MESSAGE, msg)
    }

    @JvmStatic
    fun addInvitationFor(msg: ByteArray, gameID: Int): Int {
        return sMsgsStore.addMsgFor(gameID, INVITE, msg)
    }

    fun addReplyFor(msg: ByteArray, gameID: Int): Int {
        return sMsgsStore.addMsgFor(gameID, REPLY, msg)
    }

    fun getMsgsFor(gameID: Int): MsgToken {
        return MsgToken(sMsgsStore, gameID)
    }

    @JvmOverloads
    fun receiveMsgs(context: Context, data: ByteArray, offset: Int = 0) {
        // Log.d( TAG, "receiveMsgs(gameID=%d, %s, offset=%d)", gameID,
        //        DbgUtils.hexDump(data), offset );
        assertOnUIThread(false)
        val gameID = intArrayOf(0)
        val msgs = unformatMsgs(data, offset, gameID)
        val helper = if ( 0 == msgs.size ) null else NFCServiceHelper(context)
        for (msg in msgs) {
            val typ = byteArrayOf(0)
            val body = MsgsStore.split(msg, typ)
            when (typ[0]) {
                MESSAGE -> {
                    val rowids = getRowIDsFor(context, gameID[0])
                    if (0 == rowids.size) {
                        addReplyFor(byteArrayOf(REPLY_NOGAME), gameID[0])
                    } else {
                        for (rowid in rowids) {
                            val sink = MultiMsgSink(context, rowid)
                            helper!!.receiveMessage(rowid, sink, body)
                        }
                    }
                }

                INVITE -> GamesListDelegate.postReceivedInvite(context, body)
                REPLY -> when (body[0]) {
                             // PENDING Don't enable this until deviceID is being
                             REPLY_NOGAME ->
                                 // checked. Otherwise it'll happen every time I tap my
                                 // device against another that doesn't have my game,
                                 // which could be common.
                                 // helper.postEvent( MultiEvent.MESSAGE_NOGAME, gameID );
                                 Log.e(
                                     TAG, "receiveMsgs(): not calling helper.postEvent( "
                                     + "MultiEvent.MESSAGE_NOGAME, gameID );"
                                 )

                             else -> {
                                 Log.e(TAG, "unexpected reply %d", body[0])
                                 Assert.failDbg()
                             }
                         }

                else -> Assert.failDbg()
            }
        }
    }

    private var sNextMsgID = 0

    @get:Synchronized
    private val nextMsgID: Int
        private get() = ++sNextMsgID

    fun numTo(num: Int): ByteArray {
        val result: ByteArray
        if (USE_BIGINTEGER) {
            val bi = BigInteger.valueOf(num.toLong())
            val bibytes = bi.toByteArray()
            result = ByteArray(1 + bibytes.size)
            result[0] = bibytes.size.toByte()
            System.arraycopy(bibytes, 0, result, 1, bibytes.size)
        } else {
            val baos = ByteArrayOutputStream()
            val dos = DataOutputStream(baos)
            try {
                dos.writeInt(num)
                dos.flush()
            } catch (ioe: IOException) {
                Assert.failDbg()
            }
            result = baos.toByteArray()
        }
        // Log.d( TAG, "numTo(%d) => %s", num, DbgUtils.hexDump(result) );
        return result
    }

    @Throws(IOException::class)
    fun numFrom(bais: ByteArrayInputStream): Int {
        val biLen = bais.read()
        // Log.d( TAG, "numFrom(): read biLen: %d", biLen );
        val bytes = ByteArray(biLen)
        bais.read(bytes)
        val bi = BigInteger(bytes)

        // Log.d( TAG, "numFrom() => %d", result );
        return bi.toInt()
    }

    fun numFrom(bytes: ByteArray, start: Int, out: IntArray): Int {
        val result: Int
        if (USE_BIGINTEGER) {
            val biLen = bytes[start]
            val rest = Arrays.copyOfRange(bytes, start + 1, start + 1 + biLen)
            val bi = BigInteger(rest)
            out[0] = bi.toInt()
            result = biLen + 1
        } else {
            val bais = ByteArrayInputStream(
                bytes, start,
                bytes.size - start
            )
            val dis = DataInputStream(bais)
            try {
                out[0] = dis.readInt()
            } catch (ioe: IOException) {
                Log.e(TAG, "from readInt(): %s", ioe.message)
            }
            result = bais.available() - start
        }
        return result
    }

    // private static void testNumThing()
    // {
    //     Log.d( TAG, "testNumThing() starting" );
    //     int[] out = {0};
    //     for ( int ii = 1; ii > 0 && ii < Integer.MAX_VALUE; ii *= 2 ) {
    //         byte[] tmp = numTo( ii );
    //         numFrom( tmp, 0, out );
    //         if ( ii != out[0] ) {
    //             Log.d( TAG, "testNumThing(): %d failed; got %d", ii, out[0] );
    //             break;
    //         } else {
    //             Log.d( TAG, "testNumThing(): %d ok", ii );
    //         }
    //     }
    //     Log.d( TAG, "testNumThing() DONE" );
    // }
    private val sLatestAck = AtomicInteger(0)
    var latestAck: Int
        get() {
            val result = sLatestAck.getAndSet(0)
            if (0 != result) {
                Log.d(TAG, "getLatestAck() => %d", result)
            }
            return result
        }
        set(ack) {
            if (0 != ack) {
                Log.e(TAG, "setLatestAck(%d)", ack)
            }
            val oldVal = sLatestAck.getAndSet(ack)
            if (0 != oldVal) {
                Log.e(TAG, "setLatestAck(%d): dropping ack msgID %d", ack, oldVal)
            }
        }

    private fun updateStatus(context: Context, incoming: Boolean) {
        if (incoming) {
            ConnStatusHandler
                .updateStatusIn(context, CommsConnType.COMMS_CONN_NFC, true)
        } else {
            ConnStatusHandler
                .updateStatusOut(context, CommsConnType.COMMS_CONN_NFC, true)
        }
    }

    private val sSentTokens: MutableMap<Int?, MsgToken> = HashMap()
    private fun removeSentMsgs(context: Context, ack: Int) {
        var msgs: MsgToken? = null
        if (0 != ack) {
            Log.d(TAG, "removeSentMsgs(msgID=%d)", ack)
            synchronized(sSentTokens) {
                msgs = sSentTokens.remove(ack)
                Log.d(TAG, "removeSentMsgs(): removed %s, now have %s", msgs, keysFor())
            }
            updateStatus(context, false)
        }
        if (null != msgs) {
            msgs!!.removeSentMsgs()
        }
    }

    private fun remember(msgID: Int, msgs: MsgToken) {
        if (0 != msgID) {
            Log.d(TAG, "remember(msgID=%d)", msgID)
            synchronized(sSentTokens) {
                sSentTokens[msgID] = msgs
                Log.d(TAG, "remember(): now have %s", keysFor())
            }
        }
    }

    private fun keysFor(): String {
        var result = ""
        if (BuildConfig.DEBUG) {
            result = TextUtils.join(",", sSentTokens.keys)
        }
        return result
    }

    private var sParts: Array<ByteArray?>? = null
    private var sMsgID = 0
    @Synchronized
    fun reassemble(
        context: Context, part: ByteArray,
        cmd: HEX_STR
    ): ByteArray? {
        return reassemble(context, part, cmd.length())
    }

    @Synchronized
    fun reassemble(
        context: Context, part: ByteArray,
        offset: Int
    ): ByteArray? {
        var part = part
        part = Arrays.copyOfRange(part, offset, part.size)
        return reassemble(context, part)
    }

    @Synchronized
    fun reassemble(context: Context, part: ByteArray?): ByteArray? {
        var result: ByteArray? = null
        try {
            val bais = ByteArrayInputStream(part)
            val cur = bais.read()
            val count = bais.read()
            if (0 == cur) {
                sMsgID = numFrom(bais)
                val ack = numFrom(bais)
                removeSentMsgs(context, ack)
            }
            var inSequence = true
            if (sParts == null) {
                if (0 == cur) {
                    sParts = arrayOfNulls(count)
                } else {
                    Log.e(TAG, "reassemble(): out-of-order message 1")
                    inSequence = false
                }
            } else if (cur >= count || count != sParts!!.size || null != sParts!![cur]) {
                // result = HEX_STR.STATUS_FAILED;
                inSequence = false
                Log.e(TAG, "reassemble(): out-of-order message 2")
            }
            if (!inSequence) {
                sParts = null // so we can try again later
            } else {
                // write rest into array
                val rest = ByteArray(bais.available())
                bais.read(rest, 0, rest.size)
                sParts!![cur] = rest
                // Log.d( TAG, "addOrProcess(): added elem %d: %s", cur, DbgUtils.hexDump( rest ) );

                // Done? Process!!
                if (cur + 1 == count) {
                    val baos = ByteArrayOutputStream()
                    for (ii in sParts!!.indices) {
                        baos.write(sParts!![ii])
                    }
                    sParts = null
                    result = baos.toByteArray()
                    latestAck = sMsgID
                    if (0 != sMsgID) {
                        Log.d(
                            TAG, "reassemble(): done reassembling msgID=%d: %s",
                            sMsgID, hexDump(result)
                        )
                    }
                }
            }
        } catch (ioe: IOException) {
            Assert.failDbg()
        }
        return result
    }

    private const val HEADER_SIZE = 10
    fun wrapMsg(token: MsgToken, maxLen: Int): Array<ByteArray?> {
        val msg = token.msgs
        val length = msg?.size ?: 0
        val msgID = if (0 == length) 0 else nextMsgID
        if (0 < msgID) {
            Log.d(TAG, "wrapMsg(%s); msgID=%d", hexDump(msg), msgID)
        }
        val count = 1 + length / (maxLen - HEADER_SIZE)
        val result = arrayOfNulls<ByteArray>(count)
        try {
            var offset = 0
            for (ii in 0 until count) {
                val baos = ByteArrayOutputStream()
                baos.write(HEX_STR.CMD_MSG_PART.asBA())
                baos.write(ii.toByte().toInt())
                baos.write(count.toByte().toInt())
                if (0 == ii) {
                    baos.write(numTo(msgID))
                    val latestAck = latestAck
                    baos.write(numTo(latestAck))
                }
                Assert.assertTrue(
                    HEADER_SIZE >= baos.toByteArray().size
                            || !BuildConfig.DEBUG
                )
                val thisLen = min((maxLen - HEADER_SIZE).toDouble(), (length - offset).toDouble())
                    .toInt()
                if (0 < thisLen) {
                    // Log.d( TAG, "writing %d bytes starting from offset %d",
                    //        thisLen, offset );
                    baos.write(msg, offset, thisLen)
                    offset += thisLen
                }
                val tmp = baos.toByteArray()
                // Log.d( TAG, "wrapMsg(): adding res[%d]: %s", ii, DbgUtils.hexDump(tmp) );
                result[ii] = tmp
            }
            remember(msgID, token)
        } catch (ioe: IOException) {
            Assert.failDbg()
        }
        return result
    }

    private var sQueue: LinkedBlockingQueue<QueueElem>? = null
    @Synchronized
    fun addToMsgThread(context: Context, msg: ByteArray) {
        if (0 < msg.size) {
            if (null == sQueue) {
                sQueue = LinkedBlockingQueue()
                Thread {
                    Log.d(TAG, "addToMsgThread(): run starting")
                    while (true) {
                        try {
                            val elem = sQueue!!.take()
                            receiveMsgs(elem.context, elem.msg)
                            updateStatus(elem.context, true)
                        } catch (ie: InterruptedException) {
                            break
                        }
                    }
                    Log.d(TAG, "addToMsgThread(): run exiting")
                }.start()
            }

            sQueue!!.add(QueueElem(context, msg))
        }
    }

    private const val NFC_DEVID_KEY = "key_nfc_devid"
    private val sNFCDevID = intArrayOf(0)
    @JvmStatic
    fun getNFCDevID(context: Context): Int {
        synchronized(sNFCDevID) {
            if (0 == sNFCDevID[0]) {
                var devid = getIntFor(context!!, NFC_DEVID_KEY, 0)
                while (0 == devid) {
                    devid = Utils.nextRandomInt()
                    setIntFor(context, NFC_DEVID_KEY, devid)
                }
                sNFCDevID[0] = devid
            }
            // Log.d( TAG, "getNFCDevID() => %d", sNFCDevID[0] );
            return sNFCDevID[0]
        }
    }

    interface HaveDataListener {
        fun onHaveDataChanged(nowHaveData: Boolean)
    }

    class MsgToken (private val mStore: MsgsStore, private val mGameID: Int) {
        private val mMsgs: Array<ByteArray>?

        init {
            mMsgs = mStore.getMsgsFor(mGameID)
        }

        val msgs: ByteArray?
            get() = formatMsgs(mGameID, mMsgs)

        fun removeSentMsgs() {
            mStore.removeSentMsgs(mGameID, mMsgs)
        }
    }

    class MsgsStore {
        private val mListeners: MutableMap<Int, WeakReference<HaveDataListener>> = HashMap()
        fun setHaveDataListener(gameID: Int, listener: HaveDataListener) {
            Assert.assertFalse(gameID == 0)
            val ref = WeakReference(listener)
            synchronized(mListeners) { mListeners.put(gameID, ref) }
            val msgs = getMsgsFor(gameID)
            listener.onHaveDataChanged(null != msgs && 0 < msgs.size)
        }

        fun addMsgFor(gameID: Int, typ: Byte, msg: ByteArray): Int {
            var nowHaveData: Boolean? = null
            synchronized(mMsgMap) {
                if (!mMsgMap.containsKey(gameID)) {
                    mMsgMap[gameID] = ArrayList()
                }
                val msgs = mMsgMap[gameID]!!
                val full = ByteArray(msg.size + 1)
                full[0] = typ
                System.arraycopy(msg, 0, full, 1, msg.size)

                // Can't use msgs.contains() because it uses equals()
                var isDuplicate = false
                for (curMsg in msgs) {
                    if (curMsg.contentEquals(full)) {
                        isDuplicate = true
                        break
                    }
                }
                if (!isDuplicate) {
                    msgs.add(full)
                    nowHaveData = 0 < msgs.size
                    Log.d(
                        TAG, "addMsgFor(gameID=%d): added %s; now have %d msgs",
                        gameID, hexDump(msg), msgs.size
                    )
                }
            }
            reportHaveData(gameID, nowHaveData)
            return msg.size
        }

        internal fun getMsgsFor(gameID: Int): Array<ByteArray>? {
            Assert.assertFalse(gameID == 0)
            var result: Array<ByteArray>? = null
            synchronized(mMsgMap) {
                if (mMsgMap.containsKey(gameID)) {
                    val msgs: List<ByteArray> = mMsgMap[gameID]!!
                    result = msgs.toTypedArray<ByteArray>()
                }
            }
            Log.d(
                TAG, "getMsgsFor(gameID=%d) => %d msgs", gameID,
                if (result == null) 0 else result!!.size
            )
            return result
        }

        fun removeSentMsgs(gameID: Int, msgs: Array<ByteArray>?) {
            var nowHaveData: Boolean? = null
            if (null != msgs) {
                synchronized(mMsgMap) {
                    if (mMsgMap.containsKey(gameID)) {
                        val list = mMsgMap[gameID]!!
                        // Log.d( TAG, "removeSentMsgs(%d): size before: %d", gameID,
                        //        list.size() );
                        val origSize = list.size
                        for (msg in msgs) {
                            list.remove(msg)
                        }
                        if (0 < origSize) {
                            Log.d(
                                TAG, "removeSentMsgs(%d): size was %d, now %d", gameID,
                                origSize, list.size
                            )
                        }
                        nowHaveData = 0 < list.size
                    }
                }
            }
            reportHaveData(gameID, nowHaveData)
        }

        private fun reportHaveData(gameID: Int, nowHaveData: Boolean?) {
            Log.d(TAG, "reportHaveData($nowHaveData)")
            if (null != nowHaveData) {
                var proc: HaveDataListener? = null
                synchronized(mListeners) {
                    val ref = mListeners[gameID]
                    if (null != ref) {
                        proc = ref.get()
                        if (null == proc) {
                            mListeners.remove(gameID)
                        }
                    } else {
                        Log.d(TAG, "reportHaveData(): no listener for %d", gameID)
                    }
                }
                if (null != proc) {
                    proc!!.onHaveDataChanged(nowHaveData)
                }
            }
        }

        companion object {
            private val mMsgMap: MutableMap<Int, MutableList<ByteArray>> = HashMap()
            fun split(msg: ByteArray?, headerOut: ByteArray): ByteArray {
                headerOut[0] = msg!![0]
                val result = Arrays.copyOfRange(msg, 1, msg.size)
                Log.d(
                    TAG, "split(%s) => %d/%s", hexDump(msg),
                    headerOut[0], hexDump(result)
                )
                return result
            }
        }
    }

    enum class HEX_STR(hex: String) {
        DEFAULT_CLA("00"),
        SELECT_INS("A4"),
        STATUS_FAILED("6F00"),
        CLA_NOT_SUPPORTED("6E00"),
        INS_NOT_SUPPORTED("6D00"),
        STATUS_SUCCESS("9000"),
        CMD_MSG_PART("70FC");

        private val mBytes: ByteArray

        init {
            mBytes = Utils.hexStr2ba(hex)
        }

        fun asBA(): ByteArray {
            return mBytes
        }

        @JvmOverloads
        fun matchesFrom(src: ByteArray, offset: Int = 0): Boolean {
            var result = offset + mBytes.size <= src.size
            var ii = 0
            while (result && ii < mBytes.size) {
                result = src[offset + ii] == mBytes[ii]
                ++ii
            }
            // Log.d( TAG, "%s.matchesFrom(%s) => %b", this, src, result );
            return result
        }

        fun length(): Int {
            return asBA().size
        }
    }

    private class QueueElem internal constructor(val context: Context, val msg: ByteArray)

    class Wrapper private constructor(activity: Activity, procs: Procs, devID: Int) {
        private val mReader: Reader

        interface Procs {
            fun onReadingChange(nowReading: Boolean)
        }

        init {
            mReader = Reader(activity, procs, devID)
        }

        companion object {
            @JvmStatic
            fun init(activity: Activity, procs: Procs, devID: Int): Wrapper? {
                var instance: Wrapper? = null
                if (nfcAvail(activity)!![1]) {
                    instance = Wrapper(activity, procs, devID)
                }
                Log.d(TAG, "Wrapper.init(devID=%d) => %s", devID, instance)
                return instance
            }

            @JvmStatic
            fun setResumed(instance: Wrapper?, resumed: Boolean) {
                instance?.mReader?.setResumed(resumed)
            }

            @JvmStatic
            fun setGameID(instance: Wrapper?, gameID: Int) {
                instance?.mReader?.setGameID(gameID)
            }
        }
    }

    private class Reader constructor(
        private val mActivity: Activity,
        private val mProcs: Procs,
        private val mMyDevID: Int
    ) : ReaderCallback, HaveDataListener {
        private var mHaveData = false
        private val mAdapter: NfcAdapter
        private val mMinMS = 300
        private val mMaxMS = 500
        private var mConnected = false
        fun setResumed(resumed: Boolean) {
            if (resumed) {
                startReadModeThread()
            } else {
                stopReadModeThread()
            }
        }

        override fun onHaveDataChanged(haveData: Boolean) {
            if (mHaveData != haveData) {
                mHaveData = haveData
                Log.d(TAG, "onHaveDataChanged(): mHaveData now %b", mHaveData)
                interruptThread()
            }
        }

        private fun haveData(): Boolean {
            // Log.d( TAG, "haveData() => %b", result );
            return mHaveData
        }

        private var mGameID = 0
        fun setGameID(gameID: Int) {
            Log.d(TAG, "setGameID(%d)", gameID)
            mGameID = gameID
            setHaveDataListener(gameID, this)
            interruptThread()
        }

        private fun interruptThread() {
            synchronized(mThreadRef) {
                if (null != mThreadRef[0]) {
                    mThreadRef[0]!!.interrupt()
                }
            }
        }

        override fun onTagDiscovered(tag: Tag) {
            mConnected = true
            val isoDep = IsoDep.get(tag)
            try {
                isoDep.connect()
                val maxLen = isoDep.maxTransceiveLength
                Log.d(TAG, "onTagDiscovered() connected; max len: %d", maxLen)
                val aidBytes = Utils.hexStr2ba(BuildConfig.NFC_AID)
                val baos = ByteArrayOutputStream()
                baos.write(Utils.hexStr2ba("00A40400"))
                baos.write(aidBytes.size.toByte().toInt())
                baos.write(aidBytes)
                baos.write(VERSION_1.toInt()) // min
                baos.write(VERSION_1.toInt()) // max
                baos.write(numTo(mMyDevID))
                baos.write(numTo(mGameID))
                val msg = baos.toByteArray()
                Assert.assertTrue(msg.size < maxLen || !BuildConfig.DEBUG)
                val response = isoDep.transceive(msg)

                // The first reply from transceive() is special. If it starts
                // with STATUS_SUCCESS then it also includes the version we'll
                // be using to communicate, either what we sent over or
                // something lower (for older code on the other side), and the
                // remote's deviceID
                if (HEX_STR.STATUS_SUCCESS.matchesFrom(response)) {
                    var offset = HEX_STR.STATUS_SUCCESS.length()
                    val version = response[offset++]
                    if (version == VERSION_1) {
                        val out = intArrayOf(0)
                        offset += numFrom(response, offset, out)
                        Log.d(
                            TAG, "onTagDiscovered(): read remote devID: %d",
                            out[0]
                        )
                        runMessageLoop(isoDep, maxLen)
                    } else {
                        Log.e(
                            TAG, "onTagDiscovered(): remote sent version %d, "
                                    + "not %d; exiting", version, VERSION_1
                        )
                    }
                }
                isoDep.close()
            } catch (ioe: IOException) {
                Log.e(TAG, "got ioe: " + ioe.message)
            }
            mConnected = false
            interruptThread() // make sure we leave read mode!
            Log.d(TAG, "onTagDiscovered() DONE")
        }

        @Throws(IOException::class)
        private fun runMessageLoop(isoDep: IsoDep, maxLen: Int) {
            outer@ while (true) {
                val token = getMsgsFor(mGameID)
                // PENDING: no need for this Math.min thing once well tested
                val toFit = wrapMsg(
                    token, min(50.0, maxLen.toDouble())
                        .toInt()
                )
                for (ii in toFit.indices) {
                    val one = toFit[ii]
                    Assert.assertTrue(one!!.size < maxLen || !BuildConfig.DEBUG)
                    val response = isoDep.transceive(one)
                    if (!receiveAny(response)) {
                        break@outer
                    }
                }
            }
        }

        private fun receiveAny(response: ByteArray): Boolean {
            val statusOK = HEX_STR.STATUS_SUCCESS.matchesFrom(response)
            if (statusOK) {
                val offset = HEX_STR.STATUS_SUCCESS.length()
                if (HEX_STR.CMD_MSG_PART.matchesFrom(response, offset)) {
                    val all = reassemble(
                        mActivity, response,
                        offset + HEX_STR.CMD_MSG_PART.length()
                    )
                    Log.d(TAG, "receiveAny(%s) => %b", hexDump(response), statusOK)
                    if (null != all) {
                        addToMsgThread(mActivity, all)
                    }
                }
            }
            if (!statusOK) {
                Log.d(TAG, "receiveAny(%s) => %b", hexDump(response), statusOK)
            }
            return statusOK
        }

        private inner class ReadModeThread : Thread() {
            private var mShouldStop = false
            private var mInReadMode = false
            private val mFlags = (NfcAdapter.FLAG_READER_NFC_A
                    or NfcAdapter.FLAG_READER_SKIP_NDEF_CHECK)

            override fun run() {
                Log.d(TAG, "ReadModeThread.run() starting")
                val random = Random()
                while (!mShouldStop) {
                    val wantReadMode = mConnected || !mInReadMode && haveData()
                    if (wantReadMode && !mInReadMode) {
                        mAdapter.enableReaderMode(mActivity, this@Reader, mFlags, null)
                    } else if (mInReadMode && !wantReadMode) {
                        mAdapter.disableReaderMode(mActivity)
                    }
                    mInReadMode = wantReadMode
                    // Log.d( TAG, "run(): inReadMode now: %b", mInReadMode );

                    // Now sleep. If we aren't going to want to toggle read
                    // mode soon, sleep until interrupted by a state change,
                    // e.g. getting data or losing connection.
                    var intervalMS = Long.MAX_VALUE
                    if (mInReadMode && !mConnected || haveData()) {
                        intervalMS =
                            (mMinMS + abs(random.nextInt().toDouble()) % (mMaxMS - mMinMS)).toLong()
                    }
                    try {
                        sleep(intervalMS)
                    } catch (ie: InterruptedException) {
                        Log.d(TAG, "run interrupted")
                    }
                }

                // Kill read mode on the way out
                if (mInReadMode) {
                    mAdapter.disableReaderMode(mActivity)
                    mInReadMode = false
                }

                // Clear the reference only if it's me
                synchronized(mThreadRef) {
                    if (mThreadRef[0] === this) {
                        mThreadRef[0] = null
                    }
                }
                Log.d(TAG, "ReadModeThread.run() exiting")
            }

            fun doStop() {
                mShouldStop = true
                interrupt()
            }
        }

        private val mThreadRef = arrayOf<ReadModeThread?>(null)

        init {
            mAdapter = NfcAdapter.getDefaultAdapter(mActivity)
        }

        private fun startReadModeThread() {
            synchronized(mThreadRef) {
                if (null == mThreadRef[0]) {
                    mThreadRef[0] = ReadModeThread()
                    mThreadRef[0]!!.start()
                }
            }
        }

        private fun stopReadModeThread() {
            var thread: ReadModeThread?
            synchronized(mThreadRef) {
                thread = mThreadRef[0]
                mThreadRef[0] = null
            }
            if (null != thread) {
                thread!!.doStop()
                try {
                    thread!!.join()
                } catch (ex: InterruptedException) {
                    Log.d(TAG, "stopReadModeThread(): %s", ex)
                }
            }
        }
    }

    private class NFCServiceHelper internal constructor(context: Context) :
        XWServiceHelper(context) {
        private val mAddr = CommsAddrRec(CommsConnType.COMMS_CONN_NFC)
        public override fun postNotification(device: String, gameID: Int, rowid: Long) {
            val context = context
            val body = LocUtils.getString(context, R.string.new_game_body)
            GameUtils.postInvitedNotification(context, gameID, body, rowid)
        }

        fun receiveMessage(rowid: Long, sink: MultiMsgSink, msg: ByteArray) {
            Log.d(TAG, "receiveMessage(rowid=%d, len=%d)", rowid, msg.size)
            receiveMessage(rowid, sink, msg, mAddr)
        }
    }
}
