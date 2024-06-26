/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2024 by Eric House (xwords@eehouse.org).  All
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

import android.os.Handler
import org.eehouse.android.xw4.Utils.nextRandomInt
import java.util.Formatter

// Implements read-locks and write-locks per game.  A read lock is
// obtainable when other read locks are granted but not when a
// write lock is.  Write-locks are exclusive.
//
// Let's try representing a lock with something serializable. Let's not have
// one public object per game, but rather let lots of objects represent the
// same state. That way a lock can be grabbed by one thread or object (think
// GamesListDelegate) and held for as long as it takes the game that's opened
// to be closed. During that time it can be shared among objects
// (BoardDelegate and JNIThread, etc.) that know how to manage their
// interactions. (Note that I'm not doing this now -- found a way around it --
// but that the capability is still worth having, and so the change is going
// in. Having getFor() be public, and there being GameLock instances out there
// whose state was "unlocked", was just dumb.)
//
// So the class everybody sees (GameLock) is not stored. The rowid it
// holds is a key to a private Hash of state.
private val TAG: String = GameLock::class.java.simpleName

class GameLock private constructor(
    private val m_state: GameLockState,
    private var m_owner: Owner,
    val rowid: Long
) : AutoCloseable {
    private class Owner {
        var mThread: Thread = Thread.currentThread()
        var mTrace: String? = null
        var mStamp: Long = 0

        init {
            mTrace = if (GET_OWNER_STACK) {
                android.util.Log.getStackTraceString(Exception())
            } else {
                "<untracked>"
            }
            setStamp()
        }

        override fun toString(): String {
            val ageMS = System.currentTimeMillis() - mStamp
            return String.format(
                "Owner: {age: %dms (since %d); thread: {%s}; stack: {%s}}",
                ageMS, mStamp, mThread, mTrace
            )
        }

        fun setStamp() {
            mStamp = System.currentTimeMillis()
        }
    }

    private class GameLockState(var mRowid: Long) {
        val mOwners: MutableSet<Owner> = HashSet()
        private var mReadOnly = false

        fun add(owner: Owner) {
            synchronized(mOwners) {
                Assert.assertFalse(mOwners.contains(owner))
                mOwners.add(owner)
            }
        }

        fun remove(owner: Owner) {
            synchronized(mOwners) {
                Assert.assertTrue(mOwners.contains(owner))
                mOwners.remove(owner)

                if (DEBUG_LOCKS) {
                    Log.d(TAG, "remove(): %d owners left", mOwners.size)
                }
                if (0 == mOwners.size) {
                    (mOwners as Object).notifyAll()
                }
            }
        }

        // We grant a lock IFF:
        // * Count is 0
        // OR
        // * existing locks are ReadOnly and this request is readOnly
        // OR
        // * the requesting thread already holds the lock (later...)
        // // This could be written to allow multiple read locks.  Let's
        // // see if not doing that causes problems.
        private fun tryLockImpl(readOnly: Boolean): GameLock? {
            var result: GameLock? = null
            synchronized(mOwners) {
                if (DEBUG_LOCKS) {
                    Log.d(TAG, "%s.tryLockImpl(ro=%b)", this, readOnly)
                }
                // Thread thisThread = Thread.currentThread();
                var grant = false
                if (0 == mOwners.size) {
                    grant = true
                } else if (mReadOnly && readOnly) {
                    grant = true
                    // } else if ( thisThread == mOwnerThread ) {
                    //     grant = true;
                }
                if (grant) {
                    mReadOnly = readOnly
                    result = GameLock(this, mRowid)
                }
            }
            return result
        }

        fun tryLockRO(): GameLock? {
            val result = tryLockImpl(true)
            logIfNull(result, "tryLockRO()")
            return result
        }

        @Throws(InterruptedException::class)
        fun lockImpl(timeoutMS: Long, readOnly: Boolean): GameLock? {
            var result: GameLock? = null
            val startMS = System.currentTimeMillis()
            val endMS = startMS + timeoutMS
            synchronized(mOwners) {
                while (true) {
                    result = tryLockImpl(readOnly)
                    if (null != result) {
                        break
                    }
                    val now = System.currentTimeMillis()
                    if (now >= endMS) {
                        throw GameLockedException()
                    }
                    (mOwners as Object).wait(endMS - now)
                }
            }

            if (DEBUG_LOCKS) {
                val tookMS = System.currentTimeMillis() - startMS
                Log.d(TAG, "%s.lockImpl() returning after %d ms", this, tookMS)
            }
            return result
        }

        // Version that's allowed to return null -- if maxMillis > 0
        @Throws(GameLockedException::class)
        fun lock(maxMillis: Long): GameLock? {
            Assert.assertTrue(maxMillis <= THROW_TIME)
            var result: GameLock? = null
            try {
                result = lockImpl(maxMillis, false)
            } catch (ex: InterruptedException) {
                Log.d(TAG, "lock(): got %s", ex.message)
            }
            if (DEBUG_LOCKS) {
                Log.d(TAG, "%s.lock(%d) => %s", this, maxMillis, result)
            }
            logIfNull(result, "lock(maxMillis=%d)", maxMillis)
            return result
        }

        fun tryLock(): GameLock? {
            val result = tryLockImpl(false)
            logIfNull(result, "tryLock()")
            return result
        }

        @Throws(InterruptedException::class)
        fun lock(): GameLock {
            if (BuildConfig.DEBUG) {
                DbgUtils.assertOnUIThread(false)
            }
            val result = lockImpl(Long.MAX_VALUE, false)
            Assert.assertNotNull(result)
            return result!!
        }

        fun lockRO(maxMillis: Long): GameLock? {
            Assert.assertTrue(maxMillis <= THROW_TIME)
            var lock: GameLock? = null
            try {
                lock = lockImpl(maxMillis, true)
            } catch (ex: InterruptedException) {
            }

            logIfNull(lock, "lockRO(maxMillis=%d)", maxMillis)
            return lock
        }

        fun unlock(owner: Owner) {
            if (DEBUG_LOCKS) {
                Log.d(TAG, "%s.unlock()", this)
            }
            remove(owner)
            if (DEBUG_LOCKS) {
                Log.d(TAG, "%s.unlock() DONE", this)
            }
        }

        fun canWrite(): Boolean {
            val result = !mReadOnly // && 1 == mLockCount[0];
            if (!result) {
                Log.w(TAG, "%s.canWrite(): => false", this)
            }
            return result
        }

        override fun toString(): String {
            return String.format(
                "{this: %H; rowid: %d; count: %d; ro: %b}",
                this, mRowid, mOwners.size, mReadOnly
            )
        }

        private fun logIfNull(result: GameLock?, fmt: String, vararg args: Any) {
            if (BuildConfig.DEBUG && null == result) {
                val func = Formatter().format(fmt, *args).toString()
                Log.d(TAG, "%s.%s => null", this, func)

                val now = System.currentTimeMillis()
                var minStamp = now
                val iter: Iterator<Owner> = mOwners.iterator()
                while (iter.hasNext()) {
                    val owner = iter.next()
                    val stamp = owner.mStamp
                    if (stamp < minStamp) {
                        minStamp = stamp
                    }
                }

                Log.d(
                    TAG, "Unable to lock; would-be owner: %s; %s",
                    Owner(), getHolderDump(mRowid)
                )

                val heldMS = now - minStamp
                if (heldMS > (60 * 1000)) { // 1 minute's a long time
                    DbgUtils.showf("GameLock: logged owner held for %d seconds!", heldMS / 1000)
                }
            }
        }
    }

    class GameLockedException : RuntimeException()

    init {
        m_state.add(m_owner)
    }

    private constructor(state: GameLockState, rowid: Long) : this(state, Owner(), rowid)

    private fun setOwner(owner: Owner) {
        m_state.add(owner) // first so doesn't drop to 0
        m_state.remove(m_owner)
        m_owner = owner
        owner.setStamp()
    }

    fun release() {
        m_state.unlock(m_owner)
    }

    fun retain(): GameLock {
        return GameLock(m_state, rowid)
    }

    override fun close() {
        release()
    }

    interface GotLockProc {
        fun gotLock(lock: GameLock?)
    }

    // used only for asserts
    fun canWrite(): Boolean {
        return m_state.canWrite()
    }

    companion object {

        private val GET_OWNER_STACK = BuildConfig.NON_RELEASE
        private const val DEBUG_LOCKS = false

        // private static final long ASSERT_TIME = 2000;
        private const val THROW_TIME: Long = 1000
        private val sLockMap: MutableMap<Long, GameLockState> = HashMap()
        private fun getFor(rowid: Long): GameLockState? {
            var result: GameLockState? = null
            synchronized(sLockMap) {
                if (sLockMap.containsKey(rowid)) {
                    result = sLockMap[rowid]
                }
                if (null == result) {
                    result = GameLockState(rowid)
                    sLockMap[rowid] = result!!
                }
            }
            return result
        }

        fun tryLock(rowid: Long): GameLock? {
            return getFor(rowid)!!.tryLock()
        }

        fun tryLockRO(rowid: Long): GameLock? {
            return getFor(rowid)!!.tryLockRO()
        }

        @Throws(InterruptedException::class)
        fun lock(rowid: Long): GameLock {
            return getFor(rowid)!!.lock()
        }

        @Throws(GameLockedException::class)
        fun lock(rowid: Long, maxMillis: Long): GameLock? {
            return getFor(rowid)!!.lock(maxMillis)
        }

        @Throws(GameLockedException::class)
        fun lockRO(rowid: Long, maxMillis: Long): GameLock? {
            return getFor(rowid)!!.lockRO(maxMillis)
        }

        // Meant to be called from UI thread, returning immediately, but when it
        // gets the lock, or time runs out, calls the callback (using the Handler
        // passed in) with the lock or null.
        fun getLockThen(
            rowid: Long,
            maxMillis: Long,
            handler: Handler,
            proc: GotLockProc
        ) {
            // capture caller thread and stack
            val owner = Owner()

            Thread {
                var lock: GameLock? = null
                if (false && 0 == Utils.nextRandomInt() % 5) {
                    Log.d(TAG, "testing return-null case")
                    try {
                        Thread.sleep(maxMillis)
                    } catch (ex: Exception) {
                    }
                } else {
                    try {
                        lock = getFor(rowid)!!.lockImpl(maxMillis, false)
                        owner.setStamp()
                        lock!!.setOwner(owner)
                    } catch (gle: GameLockedException) {
                    } catch (gle: InterruptedException) {
                    }
                }

                val fLock = lock
                handler.post { proc.gotLock(fLock) }
            }.start()
        }

        fun getHolderDump(rowid: Long): String {
            var result: String
            val state = getFor(rowid)
            synchronized(state!!.mOwners) {
                result = String.format("Showing %d owners: ", state.mOwners.size)
                val iter: Iterator<Owner> = state.mOwners.iterator()
                while (iter.hasNext()) {
                    val owner = iter.next()
                    result += owner.toString()
                }
            }
            return result
        }
    }
}
