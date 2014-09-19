/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2010 by Eric House (xwords@eehouse.org).  All rights
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
import java.util.HashSet;
import java.util.Iterator;

import junit.framework.Assert;

import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;

public class NetStateCache {
    private static final long WAIT_STABLE_MILLIS = 2 * 1000;

    public interface StateChangedIf {
        public void netAvail( boolean nowAvailable );
    }

    private static Boolean s_haveReceiver = new Boolean( false );
    private static HashSet<StateChangedIf> s_ifs;
    private static boolean s_netAvail = false;
    private static PvtBroadcastReceiver s_receiver;
    private static final boolean s_onSim = Build.PRODUCT.contains("sdk");

    public static void register( Context context, StateChangedIf proc )
    {
        initIfNot( context );
        synchronized( s_ifs ) {
            s_ifs.add( proc );
        }
    }

    public static void unregister( Context context, StateChangedIf proc )
    {
        initIfNot( context );
        synchronized( s_ifs ) {
            s_ifs.remove( proc );
        }
    }

    public static boolean netAvail( Context context )
    {
        initIfNot( context );
        return s_netAvail || s_onSim;
    }

    private static void initIfNot( Context context )
    {
        synchronized( s_haveReceiver ) {
            if ( !s_haveReceiver ) {
                // First figure out the current net state.  Note that
                // this doesn't seem to work on the emulator.

                ConnectivityManager connMgr = (ConnectivityManager)
                    context.getSystemService( Context.CONNECTIVITY_SERVICE );
                NetworkInfo ni = connMgr.getActiveNetworkInfo();

                s_netAvail = ni != null && ni.isAvailable() && ni.isConnected();

                s_receiver = new PvtBroadcastReceiver();
                IntentFilter filter = new IntentFilter();
                filter.addAction( ConnectivityManager.CONNECTIVITY_ACTION );

                context.getApplicationContext()
                    .registerReceiver( s_receiver, filter );

                s_ifs = new HashSet<StateChangedIf>();
                s_haveReceiver = true;
            }
        }
    }

    private static class PvtBroadcastReceiver extends BroadcastReceiver {
        private Runnable mNotifyLater;
        private Handler mHandler;
        private boolean mLastStateSent;

        public PvtBroadcastReceiver()
        {
            DbgUtils.assertOnUIThread();
            mHandler = new Handler();
            mLastStateSent = s_netAvail;
        }

        @Override
        public void onReceive( final Context context, Intent intent ) 
        {
            DbgUtils.assertOnUIThread();

            if ( intent.getAction().
                 equals( ConnectivityManager.CONNECTIVITY_ACTION)) {

                NetworkInfo ni = (NetworkInfo)intent.
                    getParcelableExtra(ConnectivityManager.EXTRA_NETWORK_INFO);
                NetworkInfo.State state = ni.getState();

                boolean netAvail;
                switch ( state ) {
                case CONNECTED:
                    netAvail = true;
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

                    // We want to wait for WAIT_STABLE_MILLIS of inactivity
                    // before informing listeners.  So each time there's a
                    // change, kill any existing timer then set another, which
                    // will only fire if we go that long without coming
                    // through here again.

                    if ( null != mNotifyLater ) {
                        mHandler.removeCallbacks( mNotifyLater );
                        mNotifyLater = null;
                    }
                    if ( mLastStateSent != s_netAvail ) {
                        mNotifyLater = new Runnable() {
                                @Override
                                public void run() {
                                    Assert.assertTrue( mLastStateSent != s_netAvail );
                                    mLastStateSent = s_netAvail;

                                    synchronized( s_ifs ) {
                                        Iterator<StateChangedIf> iter = s_ifs.iterator();
                                        while ( iter.hasNext() ) {
                                            iter.next().netAvail( s_netAvail );
                                        }
                                    }

                                    if ( s_netAvail ) {
                                        CommsConnType typ = CommsConnType
                                            .COMMS_CONN_RELAY;
                                        GameUtils.resendAllIf( context, typ, 
                                                               false );
                                    }
                                }
                            };
                        mHandler.postDelayed( mNotifyLater, WAIT_STABLE_MILLIS );
                    }
                }
            }
        }
    } // class PvtBroadcastReceiver

}
