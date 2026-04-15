/*
 * Copyright 2025-2026 by Eric House (xwords@eehouse.org).  All rights
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
import org.eehouse.android.xw4.Log
import org.eehouse.android.xw4.Utils

class DvcThread(): Thread() {
    private val TAG: String = DvcThread::class.java.simpleName
    private val mQueue = FIFOPriorityQueue()

    override fun run() {
        Log.d(TAG, "run starting")
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
        Log.d(TAG, "run DONE")
    }

    fun post(priority: Priority = Priority.UI, code: () -> Unit ) {
        val we = WrapElemRunning(code)
        mQueue.add(we, priority)
    }

    suspend fun await(priority: Priority = Priority.UI, code: () -> Any?): Any? {
        val we = WrapElemWaiting(code)
        mQueue.add(we, priority)
        return we.deferred.await()
    }

    fun blockFor(code: () -> Any?): Any? {
        Assert.assertFalse(this == Thread.currentThread())
        val we = WrapElemBlocking(code)
        mQueue.add(we, Priority.BLOCKING)
        while ( !we.done.get() ) {
            Thread.sleep(10)
        }
        return we.result
    }


    private abstract class WrapElem(val code: () -> Any?) {
        val mCaller = Throwable().stackTrace[4] // for logging
        abstract fun run()
    }

    // Only takes code that has no return value
    private inner class WrapElemRunning( code: () -> Unit ): WrapElem(code) {
        override fun run() {
            code()
        }
    }

    private inner class WrapElemBlocking( code: () -> Any? ): WrapElem(code) {
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

    private class WrapElemWaiting( code: () -> Any? ): WrapElem(code) {
        val deferred = CompletableDeferred<Any?>()
        override fun run() {
            deferred.complete(code())
        }
    }

    enum class Priority(val singleton: Boolean = false) {
        BLOCKING,
        UI,
        DRAW(singleton=true),
        NETWORK
    }

    private inner class FIFOPriorityQueue {
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

    private inner class Watcher(private val elem: WrapElem) {
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
}
