/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
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
import android.content.Intent;
import android.database.Cursor;
import android.database.DatabaseUtils;
import android.os.Bundle;
import android.text.TextUtils;
import android.text.format.Time;

import java.util.ArrayList;
import java.util.Formatter;
import java.util.List;
import java.util.Set;

import org.eehouse.android.xw4.loc.LocUtils;

public class DbgUtils {
    private static final String TAG = DbgUtils.class.getSimpleName();

    private static Time s_time = new Time();

    public static void showf( String format, Object... args )
    {
        showf( XWApp.getContext(), format, args );
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

    public static void toastNoLock( String tag, Context context, long rowid,
                                    String format, Object... args )
    {
        format = "Unable to lock game; " + format;
        if ( BuildConfig.DEBUG ) {
            showf( context, format, args );
        }
        Log.w( tag, format, args );
        Log.w( tag, "stack for lock owner for %d", rowid );
        Log.w( tag, GameLock.getHolderStack( rowid ) );
    }

    public static void assertOnUIThread()
    {
        assertOnUIThread( true );
    }

    public static void assertOnUIThread( boolean isOnThread )
    {
        Assert.assertTrue( isOnThread == Utils.isOnUIThread() );
    }

    public static void printStack( String tag, StackTraceElement[] trace )
    {
        if ( null != trace ) {
            // 1: skip printStack etc.
            for ( int ii = 1; ii < trace.length; ++ii ) {
                Log.d( tag, "ste %d: %s", ii, trace[ii].toString() );
            }
        }
    }

    public static void printStack( String tag )
    {
        printStack( tag, Thread.currentThread().getStackTrace() );
    }

    public static void printStack( String tag, Exception ex )
    {
        String stackTrace = android.util.Log.getStackTraceString(ex);
        Log.d( tag, stackTrace );
    }

    static String extrasToString( Intent intent )
    {
        Bundle bundle = intent.getExtras();
        ArrayList<String> al = new ArrayList<String>();
        if ( null != bundle ) {
            for ( String key : bundle.keySet() ) {
                al.add( key + ":" + bundle.get(key) );
            }
        }
        return TextUtils.join( ", ", al );
    }

    public static void dumpCursor( Cursor cursor )
    {
        String dump = DatabaseUtils.dumpCursorToString( cursor );
        Log.i( TAG, "cursor: %s", dump );
    }

    public static String toStr( Object[] params )
    {
        List<String> strs = new ArrayList<>();
        if ( null != params ) {
            for ( Object obj : params ) {
                strs.add( obj.toString() );
            }
        }
        return TextUtils.join( ", ", strs );
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
