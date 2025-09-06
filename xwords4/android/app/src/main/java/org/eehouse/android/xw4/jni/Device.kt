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

import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.locks.ReentrantLock
import kotlin.concurrent.thread
import kotlin.concurrent.withLock
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

import org.eehouse.android.xw4.Assert
import org.eehouse.android.xw4.Log
import org.eehouse.android.xw4.Utils

object Device {
    private val TAG: String = Device::class.java.simpleName

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
            Log.d(TAG, "blocking task finished $runtime ms after creation")
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
                        Log.d(TAG, "Watcher: $elem still running")
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
                Log.d(TAG, "thread: running $elem")
                elem.run()
                watcher.done()
                Log.d(TAG, "thread: $elem done")
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

    fun parseMQTTPacket(topic: String, packet: ByteArray) {
        post( Priority.NETWORK ) {
            val jniState = XwJNI.getJNIState()
            dvc_parseMQTTPacket(jniState, topic, packet)
        }
    }

    fun parseSMSPacket(fromPhone: String, packet: ByteArray) {
        post( Priority.NETWORK ) {
            val jniState = XwJNI.getJNIState()
            dvc_parseSMSPacket(jniState, fromPhone, packet)
        }
    }

    fun onTimerFired(key: Int) {
        post {
            val jniState = XwJNI.getJNIState()
            dvc_onTimerFired(jniState, key);
        }
    }

    fun onWebSendResult(resultKey: Int, succeeded: Boolean, result: String?) {
        post {
            val jniState = XwJNI.getJNIState()
            dvc_onWebSendResult(jniState, resultKey, succeeded, result);
        }
    }

    suspend fun setMQTTDevID(newID: String): Boolean {
        return await {
            val jniState = XwJNI.getJNIState()
            dvc_setMQTTDevID(jniState, newID)
        } as Boolean
    }

    fun onDictAdded(dictName: String) {
        Log.d(TAG, "onDictAdded($dictName)")
        post {
            val jniState = XwJNI.getJNIState()
            dvc_onDictAdded(jniState, dictName )
        }
    }

    fun onDictRemoved(dictName: String) {
        Log.d(TAG, "onDictRemoved($dictName)")
        post {
            val jniState = XwJNI.getJNIState()
            dvc_onDictRemoved(jniState, dictName )
        }
    }

    fun lcToLocale(lc: Int): String? {
        return blockFor {
            val jniState = XwJNI.getJNIState()
            dvc_lcToLocale(jniState, lc)
        } as String?
    }

    @JvmStatic
    private external fun dvc_parseMQTTPacket(jniState: Long, topic: String, packet: ByteArray)
    @JvmStatic
    private external fun dvc_parseSMSPacket(jniState: Long, fronPhone: String, packet: ByteArray)
    @JvmStatic
    private external fun dvc_onTimerFired(jniState: Long, key: Int)
    @JvmStatic
    private external fun dvc_onWebSendResult(jniState: Long, resultKey: Int,
                                             succeeded: Boolean, result: String?)
    @JvmStatic
    private external fun dvc_setMQTTDevID(jniState: Long, newID: String): Boolean
    @JvmStatic
    private external fun dvc_onDictAdded(jniState: Long, dictName: String )
    @JvmStatic
    private external fun dvc_onDictRemoved(jniState: Long, dictName: String )
    @JvmStatic
    external fun dvc_lcToLocale(jniState: Long, lc: Int): String?
}
