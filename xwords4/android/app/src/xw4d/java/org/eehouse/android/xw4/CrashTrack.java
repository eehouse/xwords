/* -*- compile-command: "find-and-gradle.sh -PuseCrashlytics insXw4dDeb"; -*- */
/*
 * Copyright 2014 by Eric House (xwords@eehouse.org).  All rights reserved.
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

import com.crashlytics.android.Crashlytics;
import io.fabric.sdk.android.Fabric;

// This class exists solely to allow crashlytics to be included in a small
// file that can be different in flavors.

public class CrashTrack {
    private static final String TAG = CrashTrack.class.getSimpleName();

    public static void init( Context context ) {
        if ( 0 < BuildConfig.FABRIC_API_KEY.length() ) {
            // Crashlytics/Fabric sample code wants this between onCreate()'s
            // super() call and the call to setContentView(). We'll see if
            // this works.
            try {
                Fabric.with( context, new Crashlytics() );

                Crashlytics.setString("git-rev", BuildConfig.GIT_REV );

                // Now crash as a test
                if ( false ) {
                    new Thread( new Runnable() {
                            @Override
                            public void run() {
                                try {
                                    Thread.sleep(5000);
                                } catch (InterruptedException ex) {}
                                String nullStr = null;
                                if ( nullStr.equals("") ) {
                                    Log.d( TAG, "something's very wrong" );
                                }
                            }
                        } ).start();
                }
            } catch ( Exception ex ) {
                Log.d( TAG, "problem initing crashlytics" );
            }
        }
    }

    public static void logAndSend( String msg )
    {
        Crashlytics.log( msg );
        new Thread( new Runnable() {
                @Override
                public void run() {
                    String foo = null;
                    try {
                        Thread.sleep( 5000 );
                        throw new RuntimeException( "crash generator" );
                    } catch ( InterruptedException ex ) {}
                }
            } ).start();
    }
}
