/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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

import org.eehouse.android.xw4.loc.LocUtils;

public class DbgUtils {
    private static final String TAG = DbgUtils.class.getSimpleName();

    private static Time s_time = new Time();

    public static void showf( String format, Object... args )
    {
        Assert.assertVarargsNotNullNR(args);
        showf( XWApp.getContext(), format, args );
    }

    public static void showf( Context context, String format, Object... args )
    {
        Assert.assertVarargsNotNullNR(args);
        Formatter formatter = new Formatter();
        String msg = formatter.format( format, args ).toString();
        Utils.showToast( context, msg );
    } // showf

    public static void showf( Context context, int formatid, Object... args )
    {
        Assert.assertVarargsNotNullNR(args);
        showf( context, LocUtils.getString( context, formatid ), args );
    } // showf

    public static void toastNoLock( String tag, Context context, long rowid,
                                    String format, Object... args )
    {
        Assert.assertVarargsNotNullNR(args);
        format = "Unable to lock game; " + format;
        if ( BuildConfig.DEBUG ) {
            showf( context, format, args );
        }
        Log.w( tag, format, args );
        Log.w( tag, "stack for lock owner for %d", rowid );
        Log.w( tag, GameLock.getHolderDump( rowid ) );
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

    static String extrasToString( Bundle extras )
    {
        ArrayList<String> al = new ArrayList<>();
        if ( null != extras ) {
            for ( String key : extras.keySet() ) {
                al.add( key + ":" + extras.get(key) );
            }
        }
        return TextUtils.join( ", ", al );
    }

    static String extrasToString( Intent intent )
    {
        Bundle bundle = intent.getExtras();
        return extrasToString( bundle );
    }

    public static void dumpCursor( Cursor cursor )
    {
        String dump = DatabaseUtils.dumpCursorToString( cursor );
        Log.i( TAG, "cursor: %s", dump );
    }

    public static String toStr( Object[] params )
    {
        Assert.assertVarargsNotNullNR(params);
        List<String> strs = new ArrayList<>();
        if ( null != params ) {
            for ( Object obj : params ) {
                strs.add( String.format( "%s", obj ) );
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

    public static String hexDump( byte[] bytes )
    {
        String result = "<null>";
        if ( null != bytes ) {
            StringBuilder dump = new StringBuilder();
            for ( byte byt : bytes ) {
                dump.append( String.format( "%02x ", byt ) );
            }
            result = dump.toString();
        }
        return result;
    }

    private static List<DeadlockWatch> sLockHolders = new ArrayList<>();

    public static class DeadlockWatch extends Thread implements AutoCloseable {
        private static final long DEFAULT_SLEEP_MS = 10 * 1000;
        final private Object mOwner;
        private long mStartStamp;
        // private long mGotItTime = 0;
        private boolean mCloseFired = false;
        private String mStartStack;

        // There's a race between this constructor and the synchronized()
        // block that follows its try-with-resources.  Oh well.
        DeadlockWatch( Object syncObj )
        {
            mOwner = BuildConfig.DEBUG ? syncObj : null;
            if ( BuildConfig.DEBUG ) {
                mStartStack = android.util.Log.getStackTraceString(new Exception());
                // Log.d( TAG, "__init(owner=%d): %s", mOwner.hashCode(), mStartStack );
                mStartStamp = System.currentTimeMillis();
                synchronized ( sLockHolders ) {
                    sLockHolders.add( this );
                    // Log.d( TAG, "added for owner %d", mOwner.hashCode() );
                }
                start();
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

        @Override
        public void close()
        {
            if ( BuildConfig.DEBUG ) {
                mCloseFired = true;
                // Assert.assertTrue( 0 < mGotItTime ); // did you forget to call gotIt? :-)
            }
        }

        @Override
        public void run()
        {
            if ( BuildConfig.DEBUG ) {
                long sleepMS = DEFAULT_SLEEP_MS;
                try {
                    Thread.sleep( sleepMS );

                    if ( !mCloseFired ) {
                        DeadlockWatch likelyCulprit = null;
                        synchronized ( sLockHolders ) {
                            for ( DeadlockWatch sc : sLockHolders ) {
                                if ( sc.mOwner == mOwner && sc != this ) {
                                    likelyCulprit = sc;
                                    break;
                                }
                            }
                        }

                        String msg = new StringBuilder()
                            .append("timer fired!!!!")
                            .append( "lock sought by: " )
                            .append( mStartStack )
                            .append( "lock likely held by: " )
                            .append( likelyCulprit.mStartStack )
                            .toString();
                        Log.e( TAG, msg );
                    }

                    removeSelf();
                } catch ( InterruptedException ie ) {
                }
            }
        }

        private void removeSelf()
        {
            if ( BuildConfig.DEBUG ) {
                synchronized ( sLockHolders ) {
                    int start = sLockHolders.size();
                    // Log.d( TAG, "removing for owner %d", mOwner.hashCode() );
                    sLockHolders.remove( this );
                    Assert.assertTrue( start - 1 == sLockHolders.size() );
                }
            }
        }

        @Override
        public String toString()
        {
            return super.toString() + "; startStack: " + mStartStack;
        }
    }
}
