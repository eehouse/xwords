/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2015 by Eric House (xwords@eehouse.org).  All
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
import android.os.Bundle;
import android.support.v4.app.JobIntentService;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;

abstract class XWJIService extends JobIntentService {
    private static final String TAG = XWJIService.class.getSimpleName();
    private static final boolean LOG_INTENT_COUNTS = false;
    private static final boolean LOG_PACKETS = false;

    static final String CMD_KEY = "CMD";
    private static final String TIMESTAMP = "TIMESTAMP";

    public interface XWJICmds {
        public int ordinal();
    }

    abstract void onHandleWorkImpl( Intent intent, XWJICmds cmd, long timestamp );
    abstract XWJICmds[] getCmds();

    private static Map<Class, Integer> sJobIDs = new HashMap<>();
    private static Map<Class, CommsConnType> sTypes = new HashMap<>();
    static void register( Class clazz, int jobID, CommsConnType typ )
    {
        sJobIDs.put( clazz, jobID );
        sTypes.put( clazz, typ );
    }
    
    @Override
    public final void onHandleWork( Intent intent )
    {
        long timestamp = getTimestamp(intent);
        long ageMS = System.currentTimeMillis() - timestamp;
        forget( this, getClass(), intent, ageMS );

        XWJICmds cmd = cmdFrom( intent );
        if ( LOG_PACKETS ) {
            Log.d( getClass().getSimpleName(),
                   "onHandleWork(): cmd=%s; age=%dms; threadCount: %d)",
                   cmd, ageMS, Thread.activeCount() );
        }

        onHandleWorkImpl( intent, cmd, timestamp );
    }

    protected static void enqueueWork( Context context, Class clazz, Intent intent )
    {
        remember( context, clazz, intent );
        enqueueWork( context, clazz, sJobIDs.get(clazz), intent );
        checkForStall( context, clazz );
    }

    static XWJICmds cmdFrom( Intent intent, XWJICmds[] values )
    {
        int ord = intent.getIntExtra( CMD_KEY, -1 );
        return values[ord];
    }

    XWJICmds cmdFrom( Intent intent )
    {
        return cmdFrom( intent, getCmds() );
    }

    long getTimestamp( Intent intent )
    {
        long result = intent.getLongExtra( TIMESTAMP, 0 );
        return result;
    }

    static Intent getIntentTo( Context context, Class clazz, XWJICmds cmd )
    {
        Intent intent = new Intent( context, clazz )
            .putExtra( CMD_KEY, cmd.ordinal() )
            .putExtra( TIMESTAMP, System.currentTimeMillis() );
        return intent;
    }

    private static Map<String, List<Intent>> sPendingIntents = new HashMap<>();

    private static void remember( Context context, Class clazz, Intent intent )
    {
        if ( stallCheckEnabled( context ) ) {
            String name = clazz.getSimpleName();
            synchronized ( sPendingIntents ) {
                if ( !sPendingIntents.containsKey( name )) {
                    sPendingIntents.put( name, new ArrayList<Intent>() );
                }
                sPendingIntents.get(name).add( intent );
                if ( LOG_INTENT_COUNTS ) {
                    Log.d( TAG, "remember(): now have %d intents for class %s",
                           sPendingIntents.get(name).size(), name );
                }
            }
        }
    }

    private static final long AGE_THRESHOLD_MS = 1000 * 60; // one minute to start
    private static void checkForStall( Context context, Class clazz )
    {
        if ( stallCheckEnabled( context ) ) {
            long now = System.currentTimeMillis();
            long maxAge = 0;
            synchronized ( sPendingIntents ) {
                for ( String simpleName : sPendingIntents.keySet() ) {
                    List<Intent> intents = sPendingIntents.get( simpleName );
                    if ( 1 <= intents.size() ) {
                        Intent intent = intents.get(0);
                        long timestamp = intent.getLongExtra( TIMESTAMP, -1 );
                        long age = now - timestamp;
                        if ( age > maxAge ) {
                            maxAge = age;
                        }
                    }
                }
            }

            if ( maxAge > AGE_THRESHOLD_MS ) {
                // ConnStatusHandler.noteStall( sTypes.get( clazz ), maxAge );
                Utils.showStallNotification( context, maxAge );
            }
        }
    }

    // Called when an intent is successfully delivered
    private static void forget( Context context, Class clazz,
                                Intent intent, long ageMS )
    {
        if ( stallCheckEnabled( context ) ) {
            String name = clazz.getSimpleName();
            synchronized ( sPendingIntents ) {
                String found = null;
                if ( sPendingIntents.containsKey( name ) ) {
                    List<Intent> intents = sPendingIntents.get( name );
                    for (Iterator<Intent> iter = intents.iterator();
                         iter.hasNext(); ) {
                        Intent candidate = iter.next();
                        if ( areSame( candidate, intent ) ) {
                            found = name;
                            iter.remove();
                            break;
                        } else {
                            Log.d( TAG, "skipping intent: %s",
                                   DbgUtils.extrasToString( candidate ) );
                        }
                    }

                    if ( found != null ) {
                        if ( LOG_INTENT_COUNTS ) {
                            Log.d( TAG, "forget(): now have %d intents for class %s",
                                   sPendingIntents.get(found).size(), found );
                        }
                    }
                }
            }

            ConnStatusHandler.noteIntentHandled( context, sTypes.get( clazz ), ageMS );
            Utils.clearStallNotification( context, ageMS );
        }
    }

    private static boolean stallCheckEnabled( Context context )
    {
        return XWPrefs.getPrefsBoolean( context, R.string.key_enable_stallnotify,
                                        BuildConfig.DEBUG );
    }

    private static boolean areSame( Intent intent1, Intent intent2 )
    {
        boolean equal = intent1.filterEquals( intent2 );
        if ( equal ) {
            Bundle bundle1 = intent1.getExtras();
            equal = null != bundle1;
            if ( equal ) {
                Bundle bundle2 = intent2.getExtras();
                equal = null != bundle2 && bundle1.size() == bundle2.size();
                if ( equal ) {
                    for ( final String key : bundle1.keySet()) {
                        if ( ! bundle2.containsKey( key ) ) {
                            equal = false;
                            break;
                        }

                        Object obj1 = bundle1.get( key );
                        Object obj2 = bundle2.get( key );
                        if ( obj1 == obj2 ) { // catches case where both null
                            continue;
                        } else if ( obj1 == null || obj2 == null ) {
                            equal = false;
                            break;
                        }

                        if ( obj1.getClass() != obj2.getClass() ) { // NPE
                            equal = false;
                            break;
                        }

                        if ( obj1 instanceof byte[] ) {
                            equal = Arrays.equals( (byte[])obj1, (byte[])obj2 );
                        } else if ( obj1 instanceof String[] ) {
                            equal = Arrays.equals( (String[])obj1, (String[])obj2 );
                        } else {
                            if ( BuildConfig.DEBUG ) {
                                if ( obj1 instanceof Long
                                     || obj1 instanceof String
                                     || obj1 instanceof Boolean
                                     || obj1 instanceof Integer ) {
                                    // expected class; log nothing
                                } else {
                                    Log.d( TAG, "areSame: using default for class %s",
                                           obj1.getClass().getSimpleName() );
                                }
                            }
                            equal = obj1.equals( obj2 );
                        }
                        if ( ! equal ) {
                            break;
                        }
                    }
                }
            }
        }

        return equal;
    }
}
