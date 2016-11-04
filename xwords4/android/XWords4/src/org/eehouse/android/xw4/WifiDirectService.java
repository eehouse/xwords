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

import java.net.InetAddress;
import android.app.Activity;
import android.net.NetworkInfo;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.wifi.WpsInfo;
import android.net.wifi.p2p.WifiP2pConfig;
import android.net.wifi.p2p.WifiP2pDevice;
import android.net.wifi.p2p.WifiP2pInfo;
import android.net.wifi.p2p.WifiP2pDeviceList;
import android.net.wifi.p2p.WifiP2pManager.ActionListener;
import android.net.wifi.p2p.WifiP2pManager.Channel;
import android.net.wifi.p2p.WifiP2pManager.ChannelListener;
import android.net.wifi.p2p.WifiP2pManager.DnsSdServiceResponseListener;
import android.net.wifi.p2p.WifiP2pManager.DnsSdTxtRecordListener;
import android.net.wifi.p2p.WifiP2pManager;
import android.net.wifi.p2p.nsd.WifiP2pDnsSdServiceInfo;
import android.net.wifi.p2p.nsd.WifiP2pDnsSdServiceRequest;
import android.net.wifi.p2p.nsd.WifiP2pUpnpServiceInfo;
import android.os.Looper;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

import junit.framework.Assert;

public class WifiDirectService {
    private static final String SERVICE_NAME = "_xwords";
    private static final String SERVICE_REG_TYPE = "_presence._tcp";
    private static final boolean WIFI_DIRECT_ENABLED = true;

    private static Context sContext;
    private static Channel sChannel;
    private static ChannelListener sListener;
    private static IntentFilter sIntentFilter;
    private static WFDBroadcastReceiver sReceiver;
    private static boolean sDiscoveryStarted;
    private static boolean sEnabled;
    
    public static void activityResumed( Activity activity )
    {
        if ( WIFI_DIRECT_ENABLED ) {
            initListeners( activity );
            activity.registerReceiver( sReceiver, sIntentFilter);
            DbgUtils.logd( WifiDirectService.class, "activityResumed() done" );
            startDiscovery( activity );
        }
    }

    public static void activityPaused( Activity activity )
    {
        if ( WIFI_DIRECT_ENABLED ) {
            activity.unregisterReceiver( sReceiver );
            DbgUtils.logd( WifiDirectService.class, "activityPaused() done" );
        }
    }

    private static void initListeners( Context context )
    {
        sContext = context;
        if ( WIFI_DIRECT_ENABLED ) {
            if ( null == sListener ) {
                sListener = new ChannelListener() {
                        @Override
                        public void onChannelDisconnected() {
                            DbgUtils.logd( WifiDirectService.class, "onChannelDisconnected()");
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

            mgr.addLocalService( sChannel, service, new WDAL("addLocalService") );

            setDiscoveryListeners( mgr );

            WifiP2pDnsSdServiceRequest serviceRequest = WifiP2pDnsSdServiceRequest.newInstance();
            mgr.addServiceRequest( sChannel, serviceRequest, new WDAL("addServiceRequest") );
            mgr.discoverServices( sChannel, new WDAL("discoverServices") );
            
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
                            tryConnect( srcDevice );
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

    private static void tryConnect( WifiP2pDevice device )
    {
        DbgUtils.logd( WifiDirectService.class, "trying to connect to %s",
                       device.toString() );
        WifiP2pConfig config = new WifiP2pConfig();
        config.deviceAddress = device.deviceAddress;
        config.wps.setup = WpsInfo.PBC;

        WifiP2pManager mgr = (WifiP2pManager)sContext
            .getSystemService(Context.WIFI_P2P_SERVICE);

        mgr.connect( sChannel, config, new WDAL("connect") );
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
                    mManager.requestPeers( mChannel, new PLL() );
                } else if (WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION.equals(action)) {
                    // Assert.fail();
                    NetworkInfo networkInfo = (NetworkInfo)intent
                        .getParcelableExtra(WifiP2pManager.EXTRA_NETWORK_INFO);
                    if ( networkInfo.isConnected() ) {
                        DbgUtils.logd( getClass(), "network %s connected",
                                       networkInfo.toString() );
                        mManager.requestConnectionInfo(mChannel, new CIL());
                    } else {
                        DbgUtils.logd( getClass(), "network %s NOT connected",
                                       networkInfo.toString() );
                    }
                } else if (WifiP2pManager.WIFI_P2P_THIS_DEVICE_CHANGED_ACTION.equals(action)) {
                    // Respond to this device's wifi state changing
                }
            }
        }
    }

    private static class WDAL implements ActionListener {
        private String mStr;
        public WDAL(String msg) { mStr = msg; }

        @Override
        public void onSuccess() {
            DbgUtils.logd( getClass(), "onSuccess(): %s", mStr );
        }
        @Override
        public void onFailure(int reason) {
            DbgUtils.logd( getClass(), "onFailure(%d): %s", reason, mStr);
        }
    }

    private static class CIL implements WifiP2pManager.ConnectionInfoListener {
        public void onConnectionInfoAvailable( final WifiP2pInfo info ) {

            // InetAddress from WifiP2pInfo struct.
            InetAddress groupOwnerAddress = info.groupOwnerAddress;
            String hostAddress = groupOwnerAddress.getHostAddress();
            DbgUtils.logd( getClass(), "onConnectionInfoAvailable(%s); addr: %s",
                           info.toString(), hostAddress );

            // After the group negotiation, we can determine the group owner.
            if (info.groupFormed && info.isGroupOwner) {
                DbgUtils.logd( getClass(), "am group owner" );
                // Do whatever tasks are specific to the group owner.
                // One common case is creating a server thread and accepting
                // incoming connections.
            } else if (info.groupFormed) {
                DbgUtils.logd( getClass(), "am NOT group owner" );
                // The other device acts as the client. In this case,
                // you'll want to create a client thread that connects to the group
                // owner.
            } else {
                Assert.fail();
            }
        }
    }

    private static class PLL implements WifiP2pManager.PeerListListener {
        @Override
        public void onPeersAvailable(WifiP2pDeviceList peerList) {
            DbgUtils.logd( getClass(), "got list of %d peers",
                           peerList.getDeviceList().size() );

            for ( WifiP2pDevice device : peerList.getDeviceList() ) {
                // tryConnect( device );
            }
            // Out with the old, in with the new.
            // peers.clear();
            // peers.addAll(peerList.getDeviceList());

            // // If an AdapterView is backed by this data, notify it
            // // of the change.  For instance, if you have a ListView of available
            // // peers, trigger an update.
            // ((WiFiPeerListAdapter) getListAdapter()).notifyDataSetChanged();
            // if (peers.size() == 0) {
            //     Log.d(WiFiDirectActivity.TAG, "No devices found");
            //     return;
            // }
        }
    }
}
