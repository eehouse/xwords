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

import android.content.ContentValues
import android.content.Context
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper
import android.os.Environment
import android.os.Process

import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.io.OutputStreamWriter
import java.lang.ref.WeakReference
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Formatter
import java.util.Locale
import java.util.concurrent.LinkedBlockingQueue
import java.util.zip.GZIPOutputStream
import kotlin.concurrent.thread

object Log {
    private val TAG: String = Log::class.java.simpleName
    private const val PRE_TAG = BuildConfig.FLAVOR + "-"
    private val KEY_USE_DB = TAG + "/useDB"
    private val LOGGING_ENABLED = BuildConfig.NON_RELEASE
    private const val ERROR_LOGGING_ENABLED = true
    private const val LOGS_FILE_NAME_FMT = BuildConfig.FLAVOR + "_logsDB_%d.txt.gz"
    private const val LOGS_DB_NAME = "xwlogs_db"
    private const val LOGS_TABLE_NAME = "logs"
    private const val COL_ENTRY = "entry"
    private const val COL_THREAD = "tid"
    private const val COL_PID = "pid"
    private const val COL_ROWID = "rowid"
    private const val COL_TAG = "tag"
    private const val COL_LEVEL = "level"
    private const val COL_TIMESTAMP = "ts"

    private const val DB_VERSION = 2
    private var sEnabled = BuildConfig.NON_RELEASE
    private var sUseDB = false
    private var sContextRef: WeakReference<Context>? = null

    fun init(context: Context) {
        sContextRef = WeakReference(context)
        sUseDB = DBUtils.getBoolFor(context, KEY_USE_DB, false)
    }

    var storeLogs: Boolean
        get() = sUseDB
        set(enable) {
            val context = sContextRef!!.get()
            Assert.assertTrueNR(null != context)
            context?.let{ DBUtils.setBoolFor(it, KEY_USE_DB, enable) }
            sUseDB = enable
        }

    fun enable(newVal: Boolean) {
        sEnabled = newVal
    }

    fun pruneStored(procs: ResultProcs) = initDB().prune(procs)

    fun clearStored(procs: ResultProcs) = initDB().clear(procs)

    fun dumpStored(procs: ResultProcs) = initDB().dumpToFile(procs)

    fun enable(context: Context) {
        val on = LOGGING_ENABLED ||
                XWPrefs.getPrefsBoolean(
                    context, R.string.key_logging_on,
                    LOGGING_ENABLED
                )
        enable(on)
    }

    fun d(tag: String, fmt: String, vararg args: Any?) {
        if (sEnabled) {
            dolog(LOG_LEVEL.DEBUG, tag, fmt, args)
        }
    }

    fun w(tag: String, fmt: String, vararg args: Any?) {
        if (sEnabled) {
            dolog(LOG_LEVEL.WARN, tag, fmt, args)
        }
    }

    fun e(tag: String, fmt: String, vararg args: Any?) {
        if (ERROR_LOGGING_ENABLED) {
            dolog(LOG_LEVEL.ERROR, tag, fmt, args)
        }
    }

    fun i(tag: String, fmt: String, vararg args: Any?) {
        if (sEnabled) {
            dolog(LOG_LEVEL.INFO, tag, fmt, args)
        }
    }

    private fun dolog(level: LOG_LEVEL, tag: String, fmt: String, args: Array<out Any?>) {
        val str = Formatter().format(fmt, *args).toString()
        val fullTag = PRE_TAG + tag
        when (level) {
            LOG_LEVEL.DEBUG -> android.util.Log.d(fullTag, str)
            LOG_LEVEL.ERROR -> android.util.Log.e(fullTag, str)
            LOG_LEVEL.WARN -> android.util.Log.w(fullTag, str)
            LOG_LEVEL.INFO -> android.util.Log.e(fullTag, str)
            else -> Assert.failDbg()
        }
        store(level, fullTag, str)
    }

    fun ex(tag: String, exception: Exception) {
        if (sEnabled) {
            w(tag, "Exception: %s", exception.toString())
            DbgUtils.printStack(tag, exception.stackTrace)
        }
    }

    private fun llog(fmt: String, vararg args: Any?) {
        val str = Formatter().format(fmt, *args).toString()
        android.util.Log.d(TAG, str)
    }

    private var s_dbHelper: LogDBHelper? = null
    @Synchronized
    private fun initDB(): LogDBHelper {
        if (null == s_dbHelper) {
            val context = sContextRef!!.get()
            context?.let {
                s_dbHelper = LogDBHelper(it)
                // force any upgrade
                s_dbHelper!!.writableDatabase.close()
            }
        }
        return s_dbHelper!!
    }

    // Called from jni. Keep name and signature in sync with what's in
    // passToJava() in andutils.c
    @JvmStatic
    fun store(tag: String, msg: String) {
        store(LOG_LEVEL.DEBUG, tag, msg)
    }

    private fun store(level: LOG_LEVEL, tag: String, msg: String) {
        if (sUseDB) {
            val helper = initDB()
            helper.store(level, tag, msg)
        }
    }

    private enum class LOG_LEVEL {
        INFO,
        ERROR,
        WARN,
        DEBUG,
    }

    interface ResultProcs {
        fun onDumping(nRecords: Int)
        fun onDumped(db: File)
        fun onCleared(nCleared: Int)
    }

    private class LogDBHelper(private val mContext: Context) :
        SQLiteOpenHelper(mContext, LOGS_DB_NAME, null, DB_VERSION) {
        override fun onCreate(db: SQLiteDatabase) {
            val query = ("CREATE TABLE " + LOGS_TABLE_NAME + "("
                    + COL_ROWID + " INTEGER PRIMARY KEY AUTOINCREMENT"
                    + "," + COL_ENTRY + " TEXT"
                    + "," + COL_THREAD + " INTEGER"
                    + "," + COL_PID + " INTEGER"
                    + "," + COL_TAG + " TEXT"
                    + "," + COL_LEVEL + " INTEGER(2)"
                    + "," + COL_TIMESTAMP + " INTEGER"
                    + ");")

            db.execSQL(query)
        }

        override fun onUpgrade(db: SQLiteDatabase, oldVersion: Int, newVersion: Int) {
            val msg = String.format(
                "onUpgrade(%s): old: %d; new: %d", db,
                oldVersion, newVersion
            )
            android.util.Log.i(TAG, msg)
            when (oldVersion) {
                1 -> addColumn(db, COL_TIMESTAMP, "INTEGER DEFAULT 0")
                else -> Assert.failDbg()
            }
        }

        fun store(level: LOG_LEVEL, tag: String?, msg: String?) {
            val tid = Process.myTid()
            val pid = Process.myPid()

            val values = ContentValues()
                .putAnd(COL_ENTRY, msg)
                .putAnd(COL_THREAD, tid)
                .putAnd(COL_PID, pid)
                .putAnd(COL_TAG, tag)
                .putAnd(COL_LEVEL, level.ordinal)
                .putAnd(COL_TIMESTAMP, System.currentTimeMillis())
            enqueue {
                val row = writableDatabase.insert(LOGS_TABLE_NAME, null, values)
                if (0L == (row%10000)) {
                    prune()
                }
            }
        }

        fun dumpToFile(procs: ResultProcs) {
            enqueue {
                val formatter = SimpleDateFormat("yy/MM/dd HH:mm:ss.SSS")
                val dir = File(Environment.getExternalStorageDirectory(),
                               Environment.DIRECTORY_DOWNLOADS)
                var db: File?
                var ii = 1
                while (true) {
                    db = File(dir, String.format(LOGS_FILE_NAME_FMT, ii))
                    if (!db.exists()) {
                        break
                    }
                    ++ii
                }

                try {
                    val fos = FileOutputStream(db)
                    val gzos = GZIPOutputStream(fos)
                    val osw = OutputStreamWriter(gzos)

                    val columns = arrayOf(
                        COL_ENTRY, COL_TAG, COL_THREAD, COL_PID,
                        COL_TIMESTAMP
                    )
                    val selection: String? = null
                    val orderBy = COL_ROWID
                    val cursor = readableDatabase.query(
                        LOGS_TABLE_NAME, columns,
                        selection, null, null,
                        null, orderBy
                    )
                    procs.onDumping(cursor.count)

                    llog("dumpToFile(): db=%s; got %d results", db!!, cursor.count)
                    val indices = IntArray(columns.size)
                    for (ii in indices.indices) {
                        indices[ii] = cursor.getColumnIndex(columns[ii])
                    }
                    while (cursor.moveToNext()) {
                        val data = cursor.getString(indices[0])
                        val tag = cursor.getString(indices[1])
                        val tid = cursor.getInt(indices[2])
                        val pid = cursor.getInt(indices[3])
                        val ts = cursor.getLong(indices[4])
                        val builder = StringBuilder()
                            .append(formatter.format(Date(ts)))
                            .append(String.format(" % 5d % 5d", pid, tid)).append(":")
                            .append(tag).append(":")
                            .append(data).append("\n")

                        osw.write(builder.toString())
                    }
                    osw.close()
                } catch (ioe: IOException) {
                    llog("dumpToFile(): ioe: %s", ioe)
                    db = null
                }
                db?.let { procs.onDumped(db) }
            }
        }

        // Return the number of rows
        fun clear(procs: ResultProcs) {
            enqueue {
                val result = writableDatabase
                    .delete(LOGS_TABLE_NAME, null, null)
                procs.onCleared(result)
            }
        }

        fun prune(procs: ResultProcs? = null) {
            val context = sContextRef!!.get()!!
            val hours = LogPruneHoursPreference.getHours(context)
            val target = System.currentTimeMillis() - (hours * 60 * 60 * 1000)
            val test = "$COL_TIMESTAMP < $target"
            enqueue {
                android.util.Log.i(TAG, "prune(): test: $test")
                val result = writableDatabase
                    .delete(LOGS_TABLE_NAME, test, null)
                android.util.Log.d(TAG, "prune(): delete => $result")
                procs?.onCleared(result)
            }
        }

        private fun addColumn(db: SQLiteDatabase, colName: String, colType: String) {
            val cmd = String.format(
                "ALTER TABLE %s ADD COLUMN %s %s;",
                LOGS_TABLE_NAME, colName, colType
            )
            db.execSQL(cmd)
        }


        private var mQueue: LinkedBlockingQueue<Runnable>

        private fun enqueue(runnable: Runnable) {
            mQueue.add(runnable)
        }

        init {
            mQueue = LinkedBlockingQueue()
            thread {
                while (true) {
                    try {
                        mQueue.take().run()
                    } catch (ie: InterruptedException) {
                        break
                    }
                }
            }
        }
    }
}
