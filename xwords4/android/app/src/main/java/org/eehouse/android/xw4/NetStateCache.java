/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2022 by Eric House (xwords@eehouse.org).  All rights
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

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Build;
import android.os.Handler;

import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;

import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;

public class NetStateCache {
    private static final String TAG = NetStateCache.class.getSimpleName();
    private static final long WAIT_STABLE_MILLIS = 2 * 1000;

    // I'm leaving this stuff commented out because MQTT might want to use it
    // to try resending when the net comes back.
    public interface StateChangedIf {
        public void onNetAvail( Context context, boolean nowAvailable );
    }
    private static Set<StateChangedIf> s_ifs = new HashSet<>();

    private static AtomicBoolean s_haveReceiver = new AtomicBoolean( false );
    private static boolean s_netAvail = false;
    private static boolean s_isWifi;
    private static PvtBroadcastReceiver s_receiver;
    private static final boolean s_onSDKSim = Build.PRODUCT.contains("sdk"); // not genymotion

    public static void register( Context context, StateChangedIf proc )
    {
        DbgUtils.assertOnUIThread();
        if ( Utils.isOnUIThread() ) {
            initIfNot( context );
            synchronized( s_ifs ) {
                s_ifs.add( proc );
            }
        }
    }

    public static void unregister( Context context, StateChangedIf proc )
    {
        DbgUtils.assertOnUIThread();
        if ( Utils.isOnUIThread() ) {
            synchronized( s_ifs ) {
                s_ifs.remove( proc );
            }
        }
    }

    static long s_lastNetCheck = 0;
    public static boolean netAvail( Context context )
    {
        initIfNot( context );

        // Cache is returning false negatives. Don't trust it.
        if ( !s_netAvail ) {
            long now = System.currentTimeMillis();
            if ( now < s_lastNetCheck ) { // time moving backwards?
                s_lastNetCheck = 0;       // reset
            }
            if ( now - s_lastNetCheck > (1000 * 20) ) { // 20 seconds
                s_lastNetCheck = now;

                boolean netAvail = getIsConnected( context );
                if ( netAvail ) {
                    Log.i( TAG, "netAvail(): second-guessing successful!!!" );
                    s_netAvail = true;
                    if ( null != s_receiver ) {
                        s_receiver.notifyStateChanged( context );
                    }
                }
            }
        }

        boolean result = s_netAvail || s_onSDKSim;
        // Log.d( TAG, "netAvail() => %b", result );
        return result;
    }

    public static boolean onWifi()
    {
        return s_isWifi;
    }

    public static void reset( Context context )
    {
        synchronized( s_haveReceiver ) {
            s_haveReceiver.set( false );

            if ( null != s_receiver ) {
                context.getApplicationContext().unregisterReceiver( s_receiver );
                s_receiver = null;
            }
        }
    }

    private static boolean getIsConnected( Context context )
    {
        boolean result = false;
        NetworkInfo ni = ((ConnectivityManager)
                          context.getSystemService( Context.CONNECTIVITY_SERVICE ))
            .getActiveNetworkInfo();
        if ( null != ni && ni.isConnectedOrConnecting() ) {
            result = true;
        }
        Log.i( TAG, "NetStateCache.getConnected() => %b", result );
        return result;
    }

    private static void initIfNot( Context context )
    {
        synchronized( s_haveReceiver ) {
            if ( !s_haveReceiver.get() ) {
                // First figure out the current net state.  Note that
                // this doesn't seem to work on the emulator.

                ConnectivityManager connMgr = (ConnectivityManager)
                    context.getSystemService( Context.CONNECTIVITY_SERVICE );
                NetworkInfo ni = connMgr.getActiveNetworkInfo();

                s_netAvail = ni != null && ni.isAvailable() && ni.isConnected();
                // Log.d( TAG, "set s_netAvail = %b", s_netAvail );

                s_receiver = new PvtBroadcastReceiver();
                IntentFilter filter = new IntentFilter();
                filter.addAction( ConnectivityManager.CONNECTIVITY_ACTION );

                context.getApplicationContext()
                    .registerReceiver( s_receiver, filter );

                // s_ifs = new HashSet<>();
                s_haveReceiver.set( true );
            }
        }
    }

    private static void checkSame( Context context, boolean connectedCached )
    {
        if ( BuildConfig.DEBUG ) {
            ConnectivityManager cm =
                (ConnectivityManager)context.getSystemService(Context.CONNECTIVITY_SERVICE);
            NetworkInfo activeNetwork = cm.getActiveNetworkInfo();
            boolean connectedReal = activeNetwork != null &&
                activeNetwork.isConnectedOrConnecting();
            if ( connectedReal != connectedCached ) {
                Log.w( TAG, "connected: cached: %b; actual: %b",
                       connectedCached, connectedReal );
            }
        }
    }

    private static class PvtBroadcastReceiver extends BroadcastReceiver {
        private Runnable mNotifyLater;
        private Handler mHandler;
        private boolean mLastStateSent;

        public PvtBroadcastReceiver()
        {
            mLastStateSent = s_netAvail;
        }

        @Override
        public void onReceive( Context context, Intent intent )
        {
            DbgUtils.assertOnUIThread();

            if ( null == mHandler ) {
                DbgUtils.assertOnUIThread();
                mHandler = new Handler();
            }

            if ( intent.getAction().
                 equals( ConnectivityManager.CONNECTIVITY_ACTION)) {

                NetworkInfo ni = (NetworkInfo)intent.
                    getParcelableExtra(ConnectivityManager.EXTRA_NETWORK_INFO);
                NetworkInfo.State state = ni.getState();
                Log.d( TAG, "onReceive(state=%s)", state.toString() );

                boolean netAvail;
                switch ( state ) {
                case CONNECTED:
                    netAvail = true;
                    s_isWifi = ConnectivityManager.TYPE_WIFI == ni.getType();
                    break;
                case DISCONNECTED:
                    netAvail = false;
                    break;
                default:
                    // ignore everything else
                    netAvail = s_netAvail;
                    break;
                }

                if ( s_netAvail != netAvail ) {
                    s_netAvail = netAvail; // keep current in case we're asked
                    notifyStateChanged( context );
                } else {
                    Log.d( TAG, "onReceive: no change; doing nothing;"
                           + " s_netAvail=%b", s_netAvail );
                }
            }
        }

        private void notifyStateChanged( final Context context )
        {
            // We want to wait for WAIT_STABLE_MILLIS of inactivity
            // before informing listeners.  So each time there's a
            // change, kill any existing timer then set another, which
            // will only fire if we go that long without coming
            // through here again.

            if ( null == mHandler ) {
                Log.e( TAG, "notifyStateChanged(): handler null so dropping" );
            } else {
                if ( null != mNotifyLater ) {
                    mHandler.removeCallbacks( mNotifyLater );
                    mNotifyLater = null;
                }
                if ( mLastStateSent != s_netAvail ) {
                    mNotifyLater = new Runnable() {
                            @Override
                            public void run() {
                                if ( mLastStateSent != s_netAvail ) {
                                    mLastStateSent = s_netAvail;

                                    Log.i( TAG, "notifyStateChanged(%b)", s_netAvail );

                                    synchronized( s_ifs ) {
                                        Iterator<StateChangedIf> iter = s_ifs.iterator();
                                        while ( iter.hasNext() ) {
                                            iter.next().onNetAvail( context, s_netAvail );
                                        }
                                    }

                                    if ( s_netAvail ) {
                                        CommsConnType typ = CommsConnType
                                            .COMMS_CONN_RELAY;
                                        GameUtils.resendAllIf( context, typ );
                                    }
                                }
                            }
                        };
                    mHandler.postDelayed( mNotifyLater, WAIT_STABLE_MILLIS );
                }
            }
        }
    } // class PvtBroadcastReceiver
}
