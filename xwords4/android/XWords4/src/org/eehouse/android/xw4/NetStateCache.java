/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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
import android.net.NetworkInfo;
import android.net.ConnectivityManager;
import java.util.HashSet;
import java.util.Iterator;
import android.os.Build;
import junit.framework.Assert;

public class NetStateCache {

    public interface StateChangedIf {
        public void netAvail( boolean nowAvailable );
    }

    private static Boolean s_haveReceiver = new Boolean( false );
    private static HashSet<StateChangedIf> s_ifs;
    private static boolean s_netAvail = false;
    private static CommsBroadcastReceiver s_receiver;
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

                s_receiver = new CommsBroadcastReceiver();
                IntentFilter filter = new IntentFilter();
                filter.addAction( ConnectivityManager.CONNECTIVITY_ACTION );

                Intent intent = context.getApplicationContext().
                    registerReceiver( s_receiver, filter );

                s_ifs = new HashSet<StateChangedIf>();
                s_haveReceiver = true;
            }
        }
    }

    private static class CommsBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive( Context context, Intent intent ) 
        {
            if ( intent.getAction().
                 equals( ConnectivityManager.CONNECTIVITY_ACTION)) {

                NetworkInfo ni = (NetworkInfo)intent.
                    getParcelableExtra(ConnectivityManager.EXTRA_NETWORK_INFO);
                DbgUtils.logf( "CommsBroadcastReceiver.onReceive: %s", 
                               ni.getState().toString() );

                boolean netAvail;
                switch ( ni.getState() ) {
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
                    Iterator<StateChangedIf> iter = s_ifs.iterator();
                    while ( iter.hasNext() ) {
                        StateChangedIf proc = iter.next();
                        proc.netAvail( netAvail );
                    }
                    s_netAvail = netAvail;
                }
            }
        }
    } // class CommsBroadcastReceiver

}

