/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2017 by Eric House (xwords@eehouse.org).  All rights
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

package org.eehouse.android.xw4;

import android.content.Context;

import java.util.Formatter;

public class Log {
    private static final String PRE_TAG = BuildConfig.FLAVOR + "-";
    private static final boolean LOGGING_ENABLED
        = BuildConfig.DEBUG || !BuildConfig.IS_TAGGED_BUILD;
    private static final boolean ERROR_LOGGING_ENABLED = true;
    private static boolean sEnabled = BuildConfig.DEBUG;

    public static void enable( boolean newVal )
    {
        sEnabled = newVal;
    }

    public static void enable( Context context )
    {
        boolean on = LOGGING_ENABLED ||
            XWPrefs.getPrefsBoolean( context, R.string.key_logging_on,
                                     LOGGING_ENABLED );
        enable( on );
    }

    public static void d( String tag, String fmt, Object... args )
    {
        if ( sEnabled ) {
            String str = new Formatter().format( fmt, args ).toString();
            android.util.Log.d( PRE_TAG + tag, str );
        }
    }

    public static void w( String tag, String fmt, Object... args )
    {
        if ( sEnabled ) {
            String str = new Formatter().format( fmt, args ).toString();
            android.util.Log.w( PRE_TAG + tag, str );
        }
    }

    public static void e( String tag, String fmt, Object... args )
    {
        if ( ERROR_LOGGING_ENABLED ) {
            String str = new Formatter().format( fmt, args ).toString();
            android.util.Log.e( PRE_TAG + tag, str );
        }
    }

    public static void i( String tag, String fmt, Object... args )
    {
        if ( sEnabled ) {
            String str = new Formatter().format( fmt, args ).toString();
            android.util.Log.i( PRE_TAG + tag, str );
        }
    }

    public static void ex( String tag, Exception exception )
    {
        if ( sEnabled ) {
            w( tag, "Exception: %s", exception.toString() );
            DbgUtils.printStack( tag, exception.getStackTrace() );
        }
    }
}
