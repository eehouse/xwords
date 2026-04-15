/*
 * Copyright 2025 - 2026 by Eric House (xwords@eehouse.org).  All rights
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
package org.eehouse.android.xw4.jni

import android.content.Context
import android.net.Uri

import java.util.UUID

import org.eehouse.android.xw4.Assert
import org.eehouse.android.xw4.BuildConfig
import org.eehouse.android.xw4.Log
import org.eehouse.android.xw4.NetUtils
import org.eehouse.android.xw4.Utils
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.DvcThread.Priority

object Device {
    private val TAG: String = Device::class.java.simpleName

    private var mPtrGlobals: Long
    private val mDvcThread: DvcThread

    init {
        System.loadLibrary(BuildConfig.JNI_LIB_NAME)

        // JNI thread must exist before initJNIState() is called in case
        // anything in it results in jni calls being enqueued. But we can't
        // *start* processing the queue until mPtrGlobals is set and the jni
        // world's been initialized.
        mDvcThread = DvcThread()

        var seed = Utils.nextRandomInt().toLong()
        seed = seed shl 32
        seed = seed or Utils.nextRandomInt().toLong()
        seed = seed xor System.currentTimeMillis()
        mPtrGlobals = initJNIState(DUtilCtxt(), JNIUtilsImpl.get(), seed)

        // Start *after* initJNIState()
        mDvcThread.start()
    }

    fun ptrGlobals(): Long { return mPtrGlobals }

    fun cleanGlobalsEmu() {
        cleanGlobals()
    }

    private fun cleanGlobals() {
        synchronized(Device::class.java) {
            // let's be safe here
            cleanupJNIState(mPtrGlobals) // tests for 0
            mPtrGlobals = 0
        }
    }

    fun post(priority: Priority = Priority.UI, code: () -> Unit ) {
        mDvcThread.post(priority, code)
    }

    suspend fun await(priority: Priority = Priority.UI, code: () -> Any?): Any? {
        return mDvcThread.await(priority, code)
    }

    fun blockFor(code: () -> Any?): Any? {
        return mDvcThread.blockFor(code)
    }

    fun getUUID(): UUID {
        return UUID.fromString(dvc_getUUID())
    }

    fun setInForeground(inForeground: Boolean) {
        post {
            dvc_setInForeground(mPtrGlobals, inForeground)
        }
    }

    fun setNeedsReg() {
        post {
            dvc_setNeedsReg(mPtrGlobals)
        }
    }

    fun onWakeReceived(key: Int) {
        post {
            dvc_onWakeReceived(mPtrGlobals, key)
        }
    }

    fun pingAll(gr: GameRef) {
        post( Priority.NETWORK ) {
            dvc_pingAll(mPtrGlobals, gr.gr)
        }
    }

    fun pingMQTTBroker() {
        post(Priority.NETWORK) {
            dvc_pingMQTTBroker(mPtrGlobals)
        }
    }

    fun parseMQTTPacket(topic: String, packet: ByteArray) {
        post( Priority.NETWORK ) {
            dvc_parseMQTTPacket(mPtrGlobals, topic, packet)
        }
    }

    fun parseBTPacket(fromName: String?, fromMac: String?, packet: ByteArray) {
        post( Priority.NETWORK ) {
            dvc_parseBTPacket(mPtrGlobals, fromName, fromMac, packet)
        }
    }

    fun parseNFCPacket(gameID: Int, packet: ByteArray) {
        val from = CommsAddrRec(CommsConnType.COMMS_CONN_NFC)
        post( Priority.NETWORK ) {
            dvc_parsePacketFor(mPtrGlobals, gameID, packet, from)
        }
    }

    suspend fun parseUrl(context: Context, uri: Uri): Boolean {
        val (host, prefix) = NetUtils.getHostAndPrefix(context)
        val result = await {
            dvc_parseUrl(mPtrGlobals, uri.toString(), host, prefix)
        } as Boolean
        return result
    }

    fun onBLEMtuChanged(addr: String, newBTU: Int) {
        post( Priority.NETWORK ) {
            dvc_onBLEMtuChanged(mPtrGlobals, addr, newBTU)
        }
    }

    fun parseSMSPacket(fromPhone: String, packet: ByteArray) {
        if ( BuildConfig.XWFEATURE_SMS ) {
            post( Priority.NETWORK ) {
                dvc_parseSMSPacket(mPtrGlobals, fromPhone, packet)
            }
        }
    }

    fun onTimerFired(key: Int) {
        post {
            dvc_onTimerFired(mPtrGlobals, key);
        }
    }

    fun onWebSendResult(resultKey: Int, succeeded: Boolean, result: String?) {
        post {
            dvc_onWebSendResult(mPtrGlobals, resultKey, succeeded, result);
        }
    }

    suspend fun setMQTTDevID(newID: String): Boolean {
        return await {
            dvc_setMQTTDevID(mPtrGlobals, newID)
        } as Boolean
    }

    fun onDictAdded(dictName: String) {
        Log.d(TAG, "onDictAdded($dictName)")
        post {
            dvc_onDictAdded(mPtrGlobals, dictName )
        }
    }

    fun onDictRemoved(dictName: String) {
        Log.d(TAG, "onDictRemoved($dictName)")
        post {
            dvc_onDictRemoved(mPtrGlobals, dictName )
        }
    }

    suspend fun getLegalPhonyCodes(): Array<ISOCode> {
        val codes = ArrayList<String>()
        await {
            dvc_getLegalPhonyCodes(mPtrGlobals, codes)
        }
        val result = codes.map{ISOCode(it)}
        return result.toTypedArray()
    }

    suspend fun getLegalPhoniesFor(code: ISOCode): Array<String> {
        val list = ArrayList<String>()
        await {
            dvc_getLegalPhoniesFor(mPtrGlobals, code.toString(), list)
        }
        return list.toTypedArray<String>()
    }

    fun clearLegalPhony(isoCode: Utils.ISOCode, phony: String) {
        post {
            dvc_clearLegalPhony(mPtrGlobals, isoCode.toString(), phony)
        }
    }

    fun lcToLocale(lc: Int): String? {
        return blockFor {
            dvc_lcToLocale(mPtrGlobals, lc)
        } as String?
    }

    fun haveLocaleToLc(isoCodeStr: String?, lang: IntArray): Boolean {
        // This just calls C code that doesn't interact/depend on anything
        // else, so doesn't need to be on the Device thread.
        return dvc_haveLocaleToLc(isoCodeStr, lang)
    }

    // Ok, so for now I can't figure out how to call the real constructor from
    // jni (javac -h and kotlin files are beyond me still) so I'm putting back
    // the nullable fields and an empty constructor. PENDING()...
    class TopicsAndPackets(val topics: Array<String>?,
                           val packets: Array<ByteArray>?,
                           val qos: Int)
    {
        constructor(topic: String, packet: ByteArray, qos: Int):
            this(arrayOf(topic), arrayOf(packet), qos)
        constructor(): this(null, null, 0) // PENDING Remove me later

        fun iterator(): Iterator<Pair<String, ByteArray>>
        {
            val lst = ArrayList<Pair<String, ByteArray>>()
            for (ii in 0 ..< topics!!.size) {
                lst.add(Pair<String, ByteArray>(topics[ii], packets!![ii]))
            }
            return lst.iterator()
        }

        fun qosInt(): Int { return this.qos }

        override fun hashCode(): Int {
            val topCode = topics.contentDeepHashCode()
            // Log.d(TAG, "hashCode(): topic: ${topics!!.get(0)}, code: $topCode")
            val packCode = packets.contentDeepHashCode()
            // Log.d(TAG, "hashCode(): buffer: ${packets!!.get(0).size}, code: $packCode")
            return topCode xor packCode
        }

        override fun equals(other: Any?): Boolean {
            val tmp = other as? TopicsAndPackets
            val result = this === tmp
                || (tmp?.packets.contentDeepEquals(packets)
                        && tmp?.topics.contentDeepEquals(topics))
            // Log.d(TAG, "equals($this, $tmp) => $result")
            return result
        }
    }

	@JvmStatic
    private external fun initJNIState(dutil: DUtilCtxt, jniu: JNIUtils, seed: Long): Long
	@JvmStatic
    private external fun cleanupJNIState(jniState: Long)
	@JvmStatic
    private external fun dvc_getUUID(): String
    @JvmStatic
    private external fun dvc_setInForeground(jniState: Long, inForeground: Boolean)
    @JvmStatic
    private external fun dvc_setNeedsReg(jniState: Long)
    @JvmStatic
    private external fun dvc_onWakeReceived(jniState: Long, key: Int)
    @JvmStatic
    private external fun dvc_pingAll(jniState: Long, gr: Long)
    @JvmStatic
    private external fun dvc_pingMQTTBroker(jniState: Long)
    @JvmStatic
    private external fun dvc_parseMQTTPacket(jniState: Long, topic: String, packet: ByteArray)
    @JvmStatic
    private external fun dvc_parseBTPacket(jniState: Long, fromName: String?,
                                           fromMac: String?, packet: ByteArray)
    @JvmStatic
    private external fun dvc_parsePacketFor(jniState: Long, gameID: Int,
                                            packet: ByteArray, from: CommsAddrRec)
    @JvmStatic
    private external fun dvc_parseUrl(jniState: Long, url: String,
                                      host: String, prefix: String): Boolean
    @JvmStatic
    private external fun dvc_onBLEMtuChanged(jniState: Long, addr: String,
                                             newBTU:Int )
    @JvmStatic
    private external fun dvc_parseSMSPacket(jniState: Long, fromPhone: String, packet: ByteArray)
    @JvmStatic
    private external fun dvc_onTimerFired(jniState: Long, key: Int)
    @JvmStatic
    private external fun dvc_onWebSendResult(jniState: Long, resultKey: Int,
                                             succeeded: Boolean, result: String?)
    @JvmStatic
    private external fun dvc_setMQTTDevID(jniState: Long, newID: String): Boolean
	@JvmStatic
    private external fun dvc_getLegalPhonyCodes(
        jniState: Long, list: ArrayList<String>
    )
    @JvmStatic
    private external fun dvc_getLegalPhoniesFor(
        jniState: Long, code: String, list: ArrayList<String>
    )
	@JvmStatic
    private external fun dvc_clearLegalPhony(jniState: Long, code: String, phony: String)
    @JvmStatic
    private external fun dvc_onDictAdded(jniState: Long, dictName: String )
    @JvmStatic
    private external fun dvc_onDictRemoved(jniState: Long, dictName: String )
    @JvmStatic
    private external fun dvc_lcToLocale(jniState: Long, lc: Int): String?
    @JvmStatic
    private external fun dvc_haveLocaleToLc(isoCodeStr: String?, lc: IntArray): Boolean
}
