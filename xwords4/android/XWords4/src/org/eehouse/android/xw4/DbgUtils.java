/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009-2011 by Eric House (xwords@eehouse.org).  All
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

package org.eehouse.android.xw4;


import android.content.Context;
import android.database.Cursor;
import android.database.DatabaseUtils;
import android.os.Looper;
import android.text.format.Time;
import android.util.Log;

import junit.framework.Assert;

import org.eehouse.android.xw4.loc.LocUtils;

import java.util.Formatter;

public class DbgUtils {
    private static final String TAG = BuildConstants.DBG_TAG;
    private static boolean s_doLog = BuildConfig.DEBUG;

    private enum LogType { ERROR, WARN, DEBUG, INFO };

    private static Time s_time = new Time();

    public static void logEnable( boolean enable )
    {
        s_doLog = enable;
    }

    public static void logEnable( Context context )
    {
        boolean on = BuildConfig.DEBUG ||
            XWPrefs.getPrefsBoolean( context, R.string.key_logging_on, false );
        logEnable( on );
    }

    private static void callLog( LogType lt, Class claz, String fmt, Object... args )
    {
        if ( s_doLog ) {
            String tag = claz.getSimpleName();
            String msg = new Formatter().format( fmt, args ).toString();
            switch( lt ) {
            case DEBUG:
                Log.d( tag, msg );
                break;
            }
        }
    }

    public static void logd( Class claz, String fmt, Object... args )
    {
        callLog( LogType.DEBUG, claz, fmt, args );
    }

    public static void loge( Class claz, String fmt, Object... args )
    {
        callLog( LogType.ERROR, claz, fmt, args );
    }

    public static void logi( Class claz, String fmt, Object... args )
    {
        logi( claz, true, fmt, args );
    }

    public static void logi( Class claz, boolean persist, String fmt, Object... args )
    {
        callLog( LogType.INFO, claz, fmt, args );
    }

    public static void logw( Class claz, String fmt, Object... args )
    {
        callLog( LogType.WARN, claz, fmt, args );
    }

    public static void showf( Context context, String format, Object... args )
    {
        Formatter formatter = new Formatter();
        String msg = formatter.format( format, args ).toString();
        Utils.showToast( context, msg );
    } // showf

    public static void showf( Context context, int formatid, Object... args )
    {
        showf( context, LocUtils.getString( context, formatid ), args );
    } // showf

    public static void logex( Exception exception )
    {
        logw( DbgUtils.class, "Exception: %s", exception.toString() );
        printStack( exception.getStackTrace() );
    }

    public static void assertOnUIThread()
    {
        Assert.assertTrue( Looper.getMainLooper().equals(Looper.myLooper()) );
    }

    public static void printStack( StackTraceElement[] trace )
    {
        if ( s_doLog && null != trace ) {
            // 2: skip printStack etc.
            for ( int ii = 2; ii < trace.length; ++ii ) {
                DbgUtils.logi( DbgUtils.class, "ste %d: %s", ii, trace[ii].toString() );
            }
        }
    }

    public static void printStack()
    {
        if ( s_doLog ) {
            printStack( Thread.currentThread().getStackTrace() );
        }
    }

    // public static void printIntent( Intent intent )
    // {
    //     if ( s_doLog ) {
    //         Bundle bundle = intent.getExtras();
    //         ArrayList<String> al = new ArrayList<String>();
    //         if ( null != bundle ) {
    //             Set<String> keys = bundle.keySet();
    //             Iterator<String> iter = keys.iterator();
    //             while ( iter.hasNext() ) {
    //                 al.add( iter.next() );
    //             }
    //         }
    //         DbgUtils.logf( "intent extras: %s", TextUtils.join( ", ", al ) );
    //     }
    // }

    public static void dumpCursor( Cursor cursor )
    {
        if ( s_doLog ) {
            String dump = DatabaseUtils.dumpCursorToString( cursor );
            logi( DbgUtils.class, "cursor: %s", dump );
        }
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
    //     String[] asStrs = new String[longs.length];
    //     for ( int ii = 0; ii < longs.length; ++ii ) {
    //         asStrs[ii] = String.format("%d", longs[ii] );
    //     }
    //     return TextUtils.join( ", ", asStrs );
    // }

    // public static String toString( Object[] objs )
    // {
    //     String[] asStrs = new String[objs.length];
    //     for ( int ii = 0; ii < objs.length; ++ii ) {
    //         asStrs[ii] = objs[ii].toString();
    //     }
    //     return TextUtils.join( ", ", asStrs );
    // }

    // public static String hexDump( byte[] bytes )
    // {
    //     StringBuilder dump = new StringBuilder();
    //     for ( byte byt : bytes ) {
    //         dump.append( String.format( "%02x ", byt ) );
    //     }
    //     return dump.toString();
    // }

}
