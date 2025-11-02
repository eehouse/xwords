/*
 * Copyright 2025 by Eric House (xwords@eehouse.org).  All rights
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
import kotlin.concurrent.thread
import kotlin.concurrent.withLock
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

import java.util.UUID
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.locks.ReentrantLock

import org.eehouse.android.xw4.Assert
import org.eehouse.android.xw4.BuildConfig
import org.eehouse.android.xw4.Log
import org.eehouse.android.xw4.NetLaunchInfo
import org.eehouse.android.xw4.NetUtils
import org.eehouse.android.xw4.Utils
import org.eehouse.android.xw4.Utils.ISOCode

object Device {
    private val TAG: String = Device::class.java.simpleName

    private var m_ptrGlobals: Long
    fun ptrGlobals(): Long { return m_ptrGlobals }

    init {
        System.loadLibrary(BuildConfig.JNI_LIB_NAME)

        var seed = Utils.nextRandomInt().toLong()
        seed = seed shl 32
        seed = seed or Utils.nextRandomInt().toLong()
        seed = seed xor System.currentTimeMillis()
        m_ptrGlobals = initJNIState(DUtilCtxt(), JNIUtilsImpl.get(), seed)
    }

    fun cleanGlobalsEmu() {
        cleanGlobals()
    }

    private fun cleanGlobals() {
        synchronized(Device::class.java) {
            // let's be safe here
            cleanupJNIState(m_ptrGlobals) // tests for 0
            m_ptrGlobals = 0
        }
    }

    enum class Priority(val singleton: Boolean = false) {
        BLOCKING,
        UI,
        DRAW(singleton=true),
        NETWORK
    }

    class FIFOPriorityQueue {
        val mLock = ReentrantLock()
        val mCondition = mLock.newCondition()
        val mQueues = Priority.entries.map {
            ArrayDeque<Any>()
        }
            .toTypedArray()

        fun add(elem: Any, priority: Priority = Priority.UI) {
            mLock.withLock {
                val queue = mQueues[priority.ordinal]
                if (priority.singleton) {
                    queue.clear()
                }
                queue.add(elem)
                mCondition.signal()
            }
        }

        fun take(): Any {
            var elem: Any? = null
            while ( null == elem ) {
                mLock.withLock {
                    elem = mQueues.firstNotNullOfOrNull{it.removeFirstOrNull()}
                    if ( null == elem ) {
                        mCondition.await()
                    }
                }
            }
            return elem!!
        }
        
    }

    abstract class WrapElem(val code: () -> Any?) {
        val mCaller = Throwable().stackTrace[4] // for logging
        abstract fun run()
    }

    class WrapElemBlocking( code: () -> Any? ): WrapElem(code) {
        val startTime: Long = System.currentTimeMillis()
        val done = AtomicBoolean(false)
        var result: Any? = Unit
        override fun run() {
            result = code()
            done.set(true)
            val runtime = System.currentTimeMillis() - startTime
            Log.d(TAG, "blocking task for $mCaller finished $runtime ms after creation")
        }
    }

    class WrapElemWaiting( code: () -> Any? ): WrapElem(code) {
        val deferred = CompletableDeferred<Any?>()
        override fun run() {
            deferred.complete(code())
        }
    }

    // Only takes code that has no return value
    class WrapElemRunning( code: () -> Unit ): WrapElem(code) {
        override fun run() {
            code()
        }
    }

    private class Watcher(private val elem: WrapElem) {
        var running = true
        fun done() { running = false }

        init {
            Utils.launch(Dispatchers.IO) {
                while (true) {
                    delay(100)
                    if (running) {
                        Log.d(TAG, "Watcher: for ${elem.mCaller} still running")
                    } else {
                        break
                    }
                }
            }
        }
    }

    private val mQueue = FIFOPriorityQueue()
    private val mThread: Thread = thread {
        while (true) {
            try {
                val elem = mQueue.take() as WrapElem
                val watcher = Watcher(elem)
                Log.d(TAG, "thread: running for ${elem.mCaller}")
                elem.run()
                watcher.done()
                Log.d(TAG, "thread: ${elem.mCaller} done")
            } catch (ie: InterruptedException) {
                Log.w(TAG, "interrupted; killing thread")
                break
            }
        }
    }

    suspend fun await(priority: Priority = Priority.UI, code: () -> Any?): Any? {
        val we = WrapElemWaiting(code)
        mQueue.add(we, priority)
        return we.deferred.await()
    }

    fun post(priority: Priority = Priority.UI, code: () -> Unit ) {
        val we = WrapElemRunning(code)
        mQueue.add(we, priority)
    }

    fun blockFor(code: () -> Any?): Any? {
        Assert.assertFalse(mThread == Thread.currentThread())
        val we = WrapElemBlocking(code)
        mQueue.add(we, Priority.BLOCKING)
        while ( !we.done.get() ) {
            Thread.sleep(10)
        }
        return we.result
    }

    fun getUUID(): UUID {
        return UUID.fromString(dvc_getUUID())
    }

    fun parseMQTTPacket(topic: String, packet: ByteArray) {
        post( Priority.NETWORK ) {
            dvc_parseMQTTPacket(m_ptrGlobals, topic, packet)
        }
    }

    fun parseBTPacket(fromName: String?, fromMac: String?, packet: ByteArray) {
        post( Priority.NETWORK ) {
            dvc_parseBTPacket(m_ptrGlobals, fromName, fromMac, packet)
        }
    }

    suspend fun parseUrl(context: Context, uri: Uri): Boolean {
        val (host, prefix) = NetUtils.getHostAndPrefix(context)
        val result = await {
            dvc_parseUrl(m_ptrGlobals, uri.toString(), host, prefix)
        } as Boolean
        return result
    }

    fun onBLEMtuChanged(addr: String, newBTU: Int) {
        post( Priority.NETWORK ) {
            dvc_onBLEMtuChanged(m_ptrGlobals, addr, newBTU)
        }
    }

    fun parseSMSPacket(fromPhone: String, packet: ByteArray) {
        post( Priority.NETWORK ) {
            dvc_parseSMSPacket(m_ptrGlobals, fromPhone, packet)
        }
    }

    fun onTimerFired(key: Int) {
        post {
            dvc_onTimerFired(m_ptrGlobals, key);
        }
    }

    fun onWebSendResult(resultKey: Int, succeeded: Boolean, result: String?) {
        post {
            dvc_onWebSendResult(m_ptrGlobals, resultKey, succeeded, result);
        }
    }

    suspend fun setMQTTDevID(newID: String): Boolean {
        return await {
            dvc_setMQTTDevID(m_ptrGlobals, newID)
        } as Boolean
    }

    fun onDictAdded(dictName: String) {
        Log.d(TAG, "onDictAdded($dictName)")
        post {
            dvc_onDictAdded(m_ptrGlobals, dictName )
        }
    }

    fun onDictRemoved(dictName: String) {
        Log.d(TAG, "onDictRemoved($dictName)")
        post {
            dvc_onDictRemoved(m_ptrGlobals, dictName )
        }
    }

    suspend fun getLegalPhonyCodes(): Array<ISOCode> {
        val codes = ArrayList<String>()
        await {
            dvc_getLegalPhonyCodes(m_ptrGlobals, codes)
        }
        val result = codes.map{ISOCode(it)}
        return result.toTypedArray()
    }

    suspend fun getLegalPhoniesFor(code: ISOCode): Array<String> {
        val list = ArrayList<String>()
        await {
            dvc_getLegalPhoniesFor(m_ptrGlobals, code.toString(), list)
        }
        return list.toTypedArray<String>()
    }

    fun clearLegalPhony(isoCode: Utils.ISOCode, phony: String) {
        post {
            dvc_clearLegalPhony(m_ptrGlobals, isoCode.toString(), phony)
        }
    }

    fun lcToLocale(lc: Int): String? {
        return blockFor {
            dvc_lcToLocale(m_ptrGlobals, lc)
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
    private external fun dvc_parseMQTTPacket(jniState: Long, topic: String, packet: ByteArray)
    @JvmStatic
    private external fun dvc_parseBTPacket(jniState: Long, fromName: String?,
                                           fromMac: String?, packet: ByteArray)
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
