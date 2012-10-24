/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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

import java.lang.Thread;
import java.util.Formatter;

import android.content.Context;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.database.DatabaseUtils;
import android.preference.PreferenceManager;
import android.text.format.Time;
import android.util.Log;
import android.widget.Toast;

public class DbgUtils {
    private static final String TAG = "XW4";
    private static boolean s_doLog = true;

    private static Time s_time = new Time();

    public static void logEnable( boolean enable )
    {
        s_doLog = enable;
    }

    public static void logEnable( Context context )
    {
        boolean on;
        if ( XWApp.DEBUG ) {
            on = true;
        } else {
            SharedPreferences sp
                = PreferenceManager.getDefaultSharedPreferences( context );
            String key = context.getString( R.string.key_logging_on );
            on = sp.getBoolean( key, false );
        }
        logEnable( on );
    }

    public static void logf( String msg ) 
    {
        if ( s_doLog ) {
            s_time.setToNow();
            String time = s_time.format("[%H:%M:%S]");
            long id = Thread.currentThread().getId();
            Log.d( TAG, time + "-" + id + "-" + msg );
        }
    } // logf

    public static void logf( String format, Object... args )
    {
        if ( s_doLog ) {
            Formatter formatter = new Formatter();
            logf( formatter.format( format, args ).toString() );
        }
    } // logf

    public static void showf( Context context, String format, Object... args )
    {
        Formatter formatter = new Formatter();
        String msg = formatter.format( format, args ).toString();
        Utils.showToast( context, msg );
    } // showf

    public static void showf( Context context, int formatid, Object... args )
    {
        showf( context, context.getString( formatid ), args );
    } // showf

    public static void loge( Exception exception )
    {
        logf( "Exception: %s", exception.toString() );
        printStack();
    }

    public static void printStack( StackTraceElement[] trace )
    {
        if ( s_doLog ) {
            if ( null == trace ) {
                DbgUtils.logf( "printStack(): null trace" );
            } else {
                for ( int ii = 0; ii < trace.length; ++ii ) {
                    DbgUtils.logf( "ste %d: %s", ii, trace[ii].toString() );
                }
            }
        }
    }

    public static void printStack()
    {
        if ( s_doLog ) {
            printStack( Thread.currentThread().getStackTrace() );
        }
    }

    public static void dumpCursor( Cursor cursor ) 
    {
        if ( s_doLog ) {
            String dump = DatabaseUtils.dumpCursorToString( cursor );
            logf( "cursor: %s", dump );
        }
    }
}
