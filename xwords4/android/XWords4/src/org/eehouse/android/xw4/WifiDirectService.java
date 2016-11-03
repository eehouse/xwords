/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2016 by Eric House (xwords@eehouse.org).  All
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

import android.net.wifi.p2p.WifiP2pDevice;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.wifi.p2p.WifiP2pManager.ActionListener;
import android.net.wifi.p2p.WifiP2pManager.Channel;
import android.net.wifi.p2p.WifiP2pManager.ChannelListener;
import android.net.wifi.p2p.WifiP2pManager;
import android.net.wifi.p2p.nsd.WifiP2pUpnpServiceInfo;
import android.net.wifi.p2p.nsd.WifiP2pDnsSdServiceInfo;
import android.net.wifi.p2p.WifiP2pManager.DnsSdServiceResponseListener;
import android.net.wifi.p2p.WifiP2pManager.DnsSdTxtRecordListener;
import android.os.Looper;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import android.net.wifi.p2p.nsd.WifiP2pDnsSdServiceRequest;


import junit.framework.Assert;

public class WifiDirectService {
    private static final String SERVICE_NAME = "_xwords";
    private static final String SERVICE_REG_TYPE = "_presence._tcp";
    private static final boolean WIFI_DIRECT_ENABLED = false;
    
    private static Channel sChannel;
    private static ChannelListener sListener;
    private static ActionListener sActionListener;
    private static IntentFilter sIntentFilter;
    private static WFDBroadcastReceiver sReceiver;
    private static boolean sDiscoveryStarted;
    private static boolean sEnabled;
    
    public static void activityResumed( Activity activity )
    {
        if ( WIFI_DIRECT_ENABLED ) {
            initListeners( activity );
            activity.registerReceiver( sReceiver, sIntentFilter);
            DbgUtils.logd( WifiDirectService.class, "registerReceivers() done" );
            startDiscovery( activity );
        }
    }

    public static void activityPaused( Activity activity )
    {
        if ( WIFI_DIRECT_ENABLED ) {
            activity.unregisterReceiver( sReceiver );
            DbgUtils.logd( WifiDirectService.class, "unregisterReceivers() done" );
        }
    }

    private static void initListeners( Context context )
    {
        if ( WIFI_DIRECT_ENABLED ) {
            if ( null == sListener ) {
                sListener = new ChannelListener() {
                        @Override
                        public void onChannelDisconnected() {
                            DbgUtils.logd( WifiDirectService.class, "onChannelDisconnected()");
                        }
                    };
                
                sActionListener = new ActionListener() {
                        public void onFailure(int reason) {
                            DbgUtils.logd( WifiDirectService.class, "onFailure(%d)", reason);
                        }
                        public void onSuccess() {
                            // DbgUtils.logd( WifiDirectService.class, "onSuccess()" );
                        }
                    };

                sIntentFilter = new IntentFilter();
                sIntentFilter.addAction(WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION);
                sIntentFilter.addAction(WifiP2pManager.WIFI_P2P_PEERS_CHANGED_ACTION);
                sIntentFilter.addAction(WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION);
                sIntentFilter.addAction(WifiP2pManager.WIFI_P2P_THIS_DEVICE_CHANGED_ACTION);

                WifiP2pManager mgr = (WifiP2pManager)context
                    .getSystemService(Context.WIFI_P2P_SERVICE);
                sChannel = mgr.initialize( context, Looper.getMainLooper(),
                                           sListener );
                Assert.assertNotNull( sChannel );

                sReceiver = new WFDBroadcastReceiver( mgr, sChannel );
            }
        }
    }

    private static void startDiscovery( Context context )
    {
        if ( WIFI_DIRECT_ENABLED && ! sDiscoveryStarted ) {
            WifiP2pManager mgr = (WifiP2pManager)context
                .getSystemService(Context.WIFI_P2P_SERVICE);

            Map<String, String> record = new HashMap<String, String>();
            record.put("AVAILABLE", "visible");
            WifiP2pDnsSdServiceInfo service = WifiP2pDnsSdServiceInfo
                .newInstance( SERVICE_NAME, SERVICE_REG_TYPE, record );
            // WifiP2pUpnpServiceInfo info = WifiP2pUpnpServiceInfo
            //     .newInstance(XWApp.getAppUUID().toString(),
            //                  "device", new ArrayList<String>());

            mgr.addLocalService( sChannel, service, sActionListener );

            setDiscoveryListeners( mgr );

            WifiP2pDnsSdServiceRequest serviceRequest = WifiP2pDnsSdServiceRequest.newInstance();
            mgr.addServiceRequest( sChannel, serviceRequest, sActionListener );
            mgr.discoverServices( sChannel, sActionListener );
            
            // mgr.discoverPeers( sChannel, sActionListener );
            DbgUtils.logd( WifiDirectService.class, "called discoverServices" );
            sDiscoveryStarted = true;
        }
    }

    private static void setDiscoveryListeners( WifiP2pManager mgr )
    {
        if ( WIFI_DIRECT_ENABLED ) {
            DnsSdServiceResponseListener srl = new DnsSdServiceResponseListener() {
                    @Override
                    public void onDnsSdServiceAvailable(String instanceName,
                                                        String registrationType,
                                                        WifiP2pDevice srcDevice) {
                        // Service has been discovered. My app?
                        if ( instanceName.equalsIgnoreCase( SERVICE_NAME ) ) {
                            DbgUtils.logd( getClass(), "onBonjourServiceAvailable "
                                           + instanceName + " with name "
                                           + srcDevice.deviceName );
                        }
                    }
                };
            DnsSdTxtRecordListener trl = new DnsSdTxtRecordListener() {
                    @Override
                    public void onDnsSdTxtRecordAvailable( String domainName,
                                                           Map<String, String> map,
                                                           WifiP2pDevice device ) {
                        DbgUtils.logd( getClass(), device.deviceName + " is "
                                       + map.get("AVAILABLE"));
                    }
                };
            mgr.setDnsSdResponseListeners( sChannel, srl, trl );
        }
    }

    private static class WFDBroadcastReceiver extends BroadcastReceiver {
        private WifiP2pManager mManager;
        private Channel mChannel;

        public WFDBroadcastReceiver( WifiP2pManager manager, Channel channel) {
            super();
            mManager = manager;
            mChannel = channel;
        }

        @Override
        public void onReceive( Context context, Intent intent ) {
            if ( WIFI_DIRECT_ENABLED ) {
                String action = intent.getAction();
                DbgUtils.logd( getClass(), "got intent: " + intent.toString() );

                if (WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION.equals(action)) {
                    int state = intent.getIntExtra(WifiP2pManager.EXTRA_WIFI_STATE, -1);
                    sEnabled = state == WifiP2pManager.WIFI_P2P_STATE_ENABLED;
                    DbgUtils.logd( getClass(), "WifiP2PEnabled: %b", sEnabled );
                } else if (WifiP2pManager.WIFI_P2P_PEERS_CHANGED_ACTION.equals(action)) {
                    // Call WifiP2pManager.requestPeers() to get a list of current peers
                } else if (WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION.equals(action)) {
                    // Respond to new connection or disconnections
                } else if (WifiP2pManager.WIFI_P2P_THIS_DEVICE_CHANGED_ACTION.equals(action)) {
                    // Respond to this device's wifi state changing
                }
            }
        }
    }
}
