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

import android.content.Context
import android.content.Intent
import android.database.Cursor
import android.database.DatabaseUtils
import android.os.Bundle
import android.text.TextUtils
import org.eehouse.android.xw4.loc.LocUtils
import java.util.Formatter
import kotlin.jvm.optionals.getOrNull

object DbgUtils {
    private val TAG = DbgUtils::class.java.getSimpleName()

    @JvmStatic
    fun showf(format: String?, vararg args: Any?) {
        Assert.assertVarargsNotNullNR(args)
        showf(XWApp.getContext(), format, *args)
    }

    @JvmStatic
    fun showf(context: Context?, format: String?, vararg args: Any?) {
        Assert.assertVarargsNotNullNR(args)
        val formatter = Formatter()
        val msg = formatter.format(format, *args).toString()
        Utils.showToast(context, msg)
    } // showf

    @JvmStatic
    fun showf(context: Context?, formatid: Int, vararg args: Any?) {
        Assert.assertVarargsNotNullNR(args)
        showf(context, LocUtils.getString(context, formatid), *args)
    } // showf

    @JvmStatic
    fun toastNoLock(
        tag: String, context: Context?, rowid: Long,
        format: String, vararg args: Any?
    ) {
        var format = format
        Assert.assertVarargsNotNullNR(args)
        format = "Unable to lock game; $format"
        if (BuildConfig.DEBUG) {
            showf(context, format, *args)
        }
        Log.w(tag, format, *args)
        Log.w(tag, "stack for lock owner for %d", rowid)
        Log.w(tag, GameLock.getHolderDump(rowid))
    }

    @JvmStatic
    @JvmOverloads
    fun assertOnUIThread(isOnThread: Boolean = true) {
        Assert.assertTrue(isOnThread == Utils.isOnUIThread())
    }

    @JvmStatic
    @JvmOverloads
    fun printStack(
        tag: String,
        trace: Array<StackTraceElement>? = Thread.currentThread().getStackTrace()
    ) {
        if (null != trace) {
            // 1: skip printStack etc.
            for (ii in 1 until trace.size) {
                Log.d(tag, "ste %d: %s", ii, trace[ii].toString())
            }
        }
    }

	@JvmStatic
    fun printStack(tag: String, ex: Exception?) {
        val stackTrace = android.util.Log.getStackTraceString(ex)
        Log.d(tag, stackTrace)
    }

    fun extrasToString(extras: Bundle?): String {
        val al = ArrayList<String?>()
        if (null != extras) {
            for (key in extras.keySet()) {
                al.add(key + ":" + extras.get(key))
            }
        }
        return TextUtils.join(", ", al)
    }

    @JvmStatic
    fun extrasToString(intent: Intent): String {
        val bundle = intent.extras
        return extrasToString(bundle)
    }

    fun dumpCursor(cursor: Cursor?) {
        val dump = DatabaseUtils.dumpCursorToString(cursor)
        Log.i(TAG, "cursor: %s", dump)
    }

    @JvmStatic
    fun toStr(params: Array<Any?>): String {
        Assert.assertVarargsNotNullNR(params)
        return TextUtils.join(", ", params.map{obj -> "$obj"})
    }

    // public static String secondsToDateStr( long seconds )
    // {
    //     return millisToDateStr( seconds * 1000 );
    // }
    // public static String millisToDateStr( long millis )
    // {
    //     Time tim = new Time();
    //     tim.set( millis );
    //     return tim.format2445();
    // }
    // public static String toString( long[] longs )
    // {
    //     String result = "";
    //     if ( null != longs && 0 < longs.length ) {
    //         String[] asStrs = new String[longs.length];
    //         for ( int ii = 0; ii < longs.length; ++ii ) {
    //             asStrs[ii] = String.format("%d", longs[ii] );
    //         }
    //         result = TextUtils.join( ", ", asStrs );
    //     }
    //     return result;
    // }
    // public static String toString( Object[] objs )
    // {
    //     String[] asStrs = new String[objs.length];
    //     for ( int ii = 0; ii < objs.length; ++ii ) {
    //         asStrs[ii] = objs[ii].toString();
    //     }
    //     return TextUtils.join( ", ", asStrs );
    // }
    @JvmStatic
    fun hexDump(bytes: ByteArray?): String {
        var result = "<null>"
        if (null != bytes) {
            val dump = StringBuilder()
            for (byt in bytes) {
                dump.append(String.format("%02x ", byt))
            }
            result = dump.toString()
        }
        return result
    }

    private val sLockHolders: MutableList<DeadlockWatch> = ArrayList()

	private const val DEFAULT_SLEEP_MS = (10 * 1000).toLong()

    class DeadlockWatch internal constructor(syncObj: Any?) : Thread(), AutoCloseable {
        private val mOwner: Any?
        private var mStartStamp: Long = 0

        // private long mGotItTime = 0;
        private var mCloseFired = false
        private var mStartStack: String? = null

        // There's a race between this constructor and the synchronized()
        // block that follows its try-with-resources.  Oh well.
        init {
            mOwner = if (BuildConfig.DEBUG) syncObj else null
            if (BuildConfig.DEBUG) {
                mStartStack = android.util.Log.getStackTraceString(Exception())
                // Log.d( TAG, "__init(owner=%d): %s", mOwner.hashCode(), mStartStack );
                mStartStamp = System.currentTimeMillis()
                synchronized(sLockHolders) { sLockHolders.add(this) }
                start()
            }
        }

        // public void gotIt( Object obj )
        // {
        //     if ( BuildConfig.DEBUG ) {
        //         Assert.assertTrue( obj == mOwner );
        //         mGotItTime = System.currentTimeMillis();
        //         // Log.d( TAG, "%s got lock after %dms", obj, mGotItTime - mStartStamp );
        //     }
        // }
        override fun close() {
            if (BuildConfig.DEBUG) {
                mCloseFired = true
                // Assert.assertTrue( 0 < mGotItTime ); // did you forget to call gotIt? :-)
            }
        }

        override fun run() {
            if (BuildConfig.DEBUG) {
                val sleepMS = DEFAULT_SLEEP_MS
                try {
                    sleep(sleepMS)
                    if (!mCloseFired) {
                        var likelyCulprit: DeadlockWatch? = null
						Log.d( TAG, "looking for likelyCulprit" )
                        synchronized(sLockHolders) {
							likelyCulprit =
								sLockHolders.stream()
								.filter({it.mOwner === mOwner && it !== this})
								.findFirst()
								.getOrNull()
						}

                        val msg = StringBuilder()
                            .append("timer fired!!!!")
                            .append("lock sought by: ")
                            .append(mStartStack)
                            .append("lock likely held by: ")
                            .append(likelyCulprit!!.mStartStack)
                            .toString()
                        Log.e(TAG, msg)
                    }
                    removeSelf()
                } catch (ie: InterruptedException) {
                }
            }
        }

        private fun removeSelf() {
            if (BuildConfig.DEBUG) {
                synchronized(sLockHolders) {
                    val start = sLockHolders.size
                    // Log.d( TAG, "removing for owner %d", mOwner.hashCode() );
                    sLockHolders.remove(this)
                    Assert.assertTrue(start - 1 == sLockHolders.size)
                }
            }
        }

        override fun toString(): String
             = super<Thread>.toString() + "; startStack: " + mStartStack
    }
}
