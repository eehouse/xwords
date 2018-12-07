/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
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

import java.util.Formatter;

public class Log {
    private static final String PRE_TAG = BuildConfig.FLAVOR + ":";

    public static void d( String tag, String fmt, Object... args ) {
        String str = new Formatter().format( fmt, args ).toString();
        android.util.Log.d( PRE_TAG + tag, str );
    }
    public static void w( String tag, String fmt, Object... args ) {
        String str = new Formatter().format( fmt, args ).toString();
        android.util.Log.w( PRE_TAG + tag, str );
    }
    public static void e( String tag, String fmt, Object... args ) {
        String str = new Formatter().format( fmt, args ).toString();
        android.util.Log.e( PRE_TAG + tag, str );
    }
    public static void i( String tag, String fmt, Object... args ) {
        String str = new Formatter().format( fmt, args ).toString();
        android.util.Log.i( PRE_TAG + tag, str );
    }

    public static void ex( String tag, Exception exception )
    {
        w( tag, "Exception: %s", exception.toString() );
        DbgUtils.printStack( tag, exception.getStackTrace() );
    }
}
