/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */ /*
 * Copyright 2009-2020 by Eric House (xwords@eehouse.org).  All rights
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
import java.io.Serializable

object Quarantine {
    private val TAG: String = Quarantine::class.java.simpleName
    private val DATA_KEY = TAG + "/key"
    private val sDataRef = arrayOf<QData?>(null)

    fun getCount(rowid: Long): Int {
        val result =
            synchronized(sDataRef) {
                get().countFor(rowid)
            }
        return result
    }

    private val sLogged: MutableSet<Long> = HashSet()
    @Synchronized
    fun safeToOpen(rowid: Long): Boolean {
        val count = getCount(rowid)
        val result = count < BuildConfig.BAD_COUNT
        if (!result) {
            Log.d(TAG, "safeToOpen(%d) => %b (count=%d)", rowid, result, count)
            if (BuildConfig.NON_RELEASE && !sLogged.contains(rowid)) {
                sLogged.add(rowid)
                Log.d(TAG, "printing calling stack:")
                DbgUtils.printStack(TAG)
                val list = get()
                    .listFor(rowid)
                for (ii in list!!.indices) {
                    val trace = list[ii]
                    Log.d(TAG, "printing saved stack %d (of %d):", ii, list.size)
                    DbgUtils.printStack(TAG, trace)
                }
            }
        }
        return result
    }

    fun clear(rowid: Long) {
        synchronized(sDataRef) {
            get().clear(rowid)
            store()
        }
    }

    fun recordOpened(rowid: Long) {
        synchronized(sDataRef) {
            val newCount = get().increment(rowid)
            store()
            Log.d(
                TAG, "recordOpened(%d): %s (count now %d)", rowid,
                sDataRef[0].toString(), newCount
            )
        }
    }

    fun recordClosed(rowid: Long) {
        synchronized(sDataRef) {
            get().clear(rowid)
            store()
            Log.d(
                TAG, "recordClosed(%d): %s (count now 0)", rowid,
                sDataRef[0].toString()
            )
        }
    }

    fun markBad(rowid: Long) {
        synchronized(sDataRef) {
            for (ii in 0 until BuildConfig.BAD_COUNT) {
                get().increment(rowid)
            }
            store()
            Log.d(TAG, "markBad(%d): %s", rowid, sDataRef[0].toString())
        }
        GameListItem.inval(rowid)
    }

    private fun store() {
        synchronized(sDataRef) {
            DBUtils.setSerializableFor(context, DATA_KEY, sDataRef[0])
        }
    }

    private fun get(): QData {
        var data: QData?
        synchronized(sDataRef) {
            data = sDataRef[0]
            if (null == data) {
                data = DBUtils.getSerializableFor(context, DATA_KEY) as QData?
                if (null == data) {
                    data = QData()
                } else {
                    Log.d(TAG, "loading existing: %s", data)
                    data!!.removeZeros()
                }
                sDataRef[0] = data
            }
        }
        return data!!
    }

    private val context: Context
        get() = XWApp.getContext()

    private class QData : Serializable {
        private val mCounts = HashMap<Long, MutableList<Array<StackTraceElement>?>>()

        @Synchronized
        fun increment(rowid: Long): Int {
            if (!mCounts.containsKey(rowid)) {
                mCounts[rowid] = ArrayList()
            }
            // null: in release case, we just need size() to work
            val stack = if (BuildConfig.NON_RELEASE
            ) Thread.currentThread().stackTrace else null
            val list = mCounts[rowid]!!
            list.add(stack)
            return list.size
        }

        @Synchronized
        fun countFor(rowid: Long): Int {
            val list = listFor(rowid)
            val result = list?.size ?: 0
            return result
        }

        @Synchronized
        fun listFor(rowid: Long): List<Array<StackTraceElement>?>? {
            val result =
                if (mCounts.containsKey(rowid)) mCounts[rowid]
                else null
            return result
        }

        @Synchronized
        fun clear(rowid: Long) {
            mCounts.remove(rowid)
        }

        @Synchronized
        fun removeZeros() {
            val iter: MutableIterator<List<Array<StackTraceElement>?>> = mCounts.values.iterator()
            while (iter.hasNext()) {
                if (0 == iter.next().size) {
                    iter.remove()
                }
            }
        }

        @Synchronized
        override fun toString(): String {
            val sb = StringBuilder()
            synchronized(mCounts) {
                sb.append("{len:").append(mCounts.size)
                    .append(", data:[")
                for (rowid in mCounts.keys) {
                    val count = mCounts[rowid]!!.size
                    sb.append(String.format("{%d: %d}", rowid, count))
                }
            }
            return sb.append("]}").toString()
        }
    }
}
