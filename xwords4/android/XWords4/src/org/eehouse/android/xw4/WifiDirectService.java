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

import android.app.Service;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.NetworkInfo;
import android.net.wifi.WpsInfo;
import android.net.wifi.p2p.WifiP2pConfig;
import android.net.wifi.p2p.WifiP2pDevice;
import android.net.wifi.p2p.WifiP2pDeviceList;
import android.net.wifi.p2p.WifiP2pInfo;
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
import java.io.IOException;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import org.json.JSONException;
import org.json.JSONObject;

import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.XwJNI;

import junit.framework.Assert;

public class WifiDirectService extends XWService {
    private static final String MAC_ADDR_KEY = "p2p_mac_addr";
    private static final String SERVICE_NAME = "_xwords";
    private static final String SERVICE_REG_TYPE = "_presence._tcp";
    private static boolean WIFI_DIRECT_ENABLED = true;
    private static final int USE_PORT = 5432;

    private enum P2PAction { _NONE,
                             GOT_MSG,
    }

    private static final String KEY_CMD = "cmd";
    private static final String KEY_GAMEID = "gmid";
    private static final String KEY_DATA = "data";
    private static final String KEY_MAC = "myMac";
    private static final String KEY_RETADDR = "raddr";

    private static final String CMD_PING = "ping";
    private static final String CMD_PONG = "pong";
    private static final String CMD_MSG = "msg";

    private static Channel sChannel;
    private static ChannelListener sListener;
    private static IntentFilter sIntentFilter;
    private static WFDBroadcastReceiver sReceiver;
    private static boolean sDiscoveryStarted;
    private static boolean sEnabled;
    private static boolean sAmServer;
    private static Thread sAcceptThread;
    private static ServerSocket sServerSock;
    private static BiDiSockWrap.Iface sIface;
    private static Map<String, BiDiSockWrap> sSocketWrapMap
        = new HashMap<String, BiDiSockWrap>();
    private static Map<String, Long> sPendingDevs = new HashMap<String, Long>();
    private static String sMacAddress;

    private P2pMsgSink m_sink;

    @Override
    public void onCreate()
    {
        m_sink = new P2pMsgSink();
    }

    @Override
    public int onStartCommand( Intent intent, int flags, int startId )
    {
        int result;

        if ( WIFI_DIRECT_ENABLED ) {
            result = Service.START_STICKY;

            int ordinal = intent.getIntExtra( KEY_CMD, -1 );
            if ( -1 != ordinal ) {
                P2PAction cmd = P2PAction.values()[ordinal];
                switch ( cmd ) {
                case GOT_MSG:
                    handleGotMessage( intent );
                    break;
                }
            }
        } else {
            result = Service.START_NOT_STICKY;
            stopSelf( startId );
        }

        return result;
    }

    public static boolean supported()
    {
        return WIFI_DIRECT_ENABLED;
    }

    public static String getMyMacAddress( Context context )
    {
        if ( WIFI_DIRECT_ENABLED ) {
            if ( null == sMacAddress && null != context ) {
                sMacAddress = DBUtils.getStringFor( context, MAC_ADDR_KEY, null );
            }
        }
        DbgUtils.logd( WifiDirectService.class, "getMyMacAddress() => %s",
                       sMacAddress );
        // Assert.assertNotNull(sMacAddress);
        return sMacAddress;
    }

    private static String getMyMacAddress() { return getMyMacAddress(null); }

    public static int sendPacket( Context context, String macAddr, int gameID,
                                  byte[] buf )
    {
        int nSent = -1;
        BiDiSockWrap wrap = sSocketWrapMap.get( macAddr );
        if ( null != wrap ) {
            try {
                JSONObject packet = new JSONObject()
                    .put( KEY_CMD, CMD_MSG )
                    .put( KEY_DATA, XwJNI.base64Encode( buf ) )
                    .put( KEY_GAMEID, gameID )
                    ;
                wrap.send( packet );
                nSent = buf.length;
            } catch ( JSONException jse ) {
                DbgUtils.logex( jse );
            }
        } else {
            DbgUtils.logd( WifiDirectService.class, "no socket for packet for %s",
                           macAddr );
        }
        return nSent;
    }
    
    public static void activityResumed( Activity activity )
    {
        if ( WIFI_DIRECT_ENABLED ) {
            if ( initListeners( activity ) ) {
                activity.registerReceiver( sReceiver, sIntentFilter );
                DbgUtils.logd( WifiDirectService.class, "activityResumed() done" );
                startDiscovery( activity );
            }
        }
    }

    public static void activityPaused( Activity activity )
    {
        if ( WIFI_DIRECT_ENABLED ) {
            Assert.assertNotNull( sReceiver );
            // No idea why I'm seeing this exception...
            try {
                activity.unregisterReceiver( sReceiver );
            } catch ( IllegalArgumentException iae ) {
                DbgUtils.logex( iae );
            }
            DbgUtils.logd( WifiDirectService.class, "activityPaused() done" );

            sDiscoveryStarted = false;
        }
    }

    private static boolean initListeners( final Context context )
    {
        boolean succeeded = false;
        if ( WIFI_DIRECT_ENABLED ) {
            if ( null == sListener ) {
                try {
                    sListener = new ChannelListener() {
                            @Override
                            public void onChannelDisconnected() {
                                DbgUtils.logd( WifiDirectService.class, "onChannelDisconnected()");
                            }
                        };

                    WifiP2pManager mgr = (WifiP2pManager)context
                        .getSystemService(Context.WIFI_P2P_SERVICE);
                    sChannel = mgr.initialize( context, Looper.getMainLooper(),
                                               sListener );
                    Assert.assertNotNull( sChannel );

                    sIface = new BiDiSockWrap.Iface() {
                            public void gotPacket( BiDiSockWrap socket, byte[] bytes )
                            {
                                DbgUtils.logd( WifiDirectService.class,
                                               "wrapper got packet!!!" );
                                processPacket( socket, bytes );
                            }

                            public void connectStateChanged( BiDiSockWrap wrap, boolean nowConnected )
                            {
                                if ( nowConnected ) {
                                    try {
                                        wrap.send( new JSONObject()
                                                   .put( KEY_CMD, CMD_PING )
                                                   .put( KEY_MAC, getMyMacAddress( context ) ) );
                                    } catch ( JSONException jse ) {
                                        DbgUtils.logex( jse );
                                    }
                                }
                            }
                        };

                    sIntentFilter = new IntentFilter();
                    sIntentFilter.addAction(WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION);
                    sIntentFilter.addAction(WifiP2pManager.WIFI_P2P_PEERS_CHANGED_ACTION);
                    sIntentFilter.addAction(WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION);
                    sIntentFilter.addAction(WifiP2pManager.WIFI_P2P_THIS_DEVICE_CHANGED_ACTION);

                    sReceiver = new WFDBroadcastReceiver( mgr, sChannel );
                    succeeded = true;
                } catch ( SecurityException se ) {
                    DbgUtils.logd( WifiDirectService.class, "disabling wifi; no permissions" );
                    WIFI_DIRECT_ENABLED = false;
                }
            } else {
                succeeded = true;
            }
        }
        return succeeded;
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
            DbgUtils.logd( WifiDirectService.class, "called mgr.discoverServices" );
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
            DbgUtils.logd( WifiDirectService.class, "setDiscoveryListeners done" );
        }
    }

    private static boolean connectPending( String macAddress )
    {
        boolean result = false;
        if ( sPendingDevs.containsKey( macAddress ) ) {
            long when = sPendingDevs.get( macAddress );
            long now = Utils.getCurSeconds();
            result = 5 >= now - when;
        }
        DbgUtils.logd( WifiDirectService.class, "connectPending(%s)=>%b",
                       macAddress, result );
        return result;
    }

    private static void notePending( String macAddress )
    {
        sPendingDevs.put( macAddress, Utils.getCurSeconds() );
    }

    private static void tryConnect( WifiP2pDevice device )
    {
        final String macAddress = device.deviceAddress;
        if ( sSocketWrapMap.containsKey(macAddress)
             && sSocketWrapMap.get(macAddress).isConnected() ) {
            DbgUtils.logd( WifiDirectService.class, "tryConnect(%s): already connected",
                           macAddress );
        } else if ( connectPending( macAddress ) ) {
            // Do nothing
        } else {
            DbgUtils.logd( WifiDirectService.class, "trying to connect to %s",
                           device.toString() );
            WifiP2pConfig config = new WifiP2pConfig();
            config.deviceAddress = device.deviceAddress;
            config.wps.setup = WpsInfo.PBC;

            WifiP2pManager mgr = (WifiP2pManager)XWApp.getContext()
                .getSystemService(Context.WIFI_P2P_SERVICE);

            mgr.connect( sChannel, config, new ActionListener() {
                            @Override
                            public void onSuccess() {
                                DbgUtils.logd( getClass(), "onSuccess(): %s", "connect_xx" );
                                notePending( macAddress );
                            }
                    @Override
                    public void onFailure(int reason) {
                        DbgUtils.logd( getClass(), "onFailure(%d): %s", reason, "connect_xx");
                    }
                } );
        }
    }

    private static void connectToOwner( InetAddress addr )
    {
        BiDiSockWrap wrap = new BiDiSockWrap( addr, USE_PORT, sIface );
        DbgUtils.logd( WifiDirectService.class, "connectToOwner(%s)", addr.toString() );
        wrap.connect();
    }

    private static void storeByAddress( BiDiSockWrap wrap, JSONObject packet )
    {
        String macAddress = packet.optString( KEY_MAC, null );
        // Assert.assertNotNull( macAddress );
        if ( null != macAddress ) {
            Assert.assertNull( sSocketWrapMap.get( macAddress ) );
            wrap.setMacAddress( macAddress );
            sSocketWrapMap.put( macAddress, wrap );
            DbgUtils.logd( WifiDirectService.class,
                           "storeByAddress(); storing wrap for %s",
                           macAddress );
        }
    }

    private void handleGotMessage( Intent intent )
    {
        DbgUtils.logd( getClass(), "processPacket(%s)", intent.toString() );
        int gameID = intent.getIntExtra( KEY_GAMEID, 0 );
        byte[] data = XwJNI.base64Decode( intent.getStringExtra( KEY_DATA ) );
        String macAddress = intent.getStringExtra( KEY_RETADDR );

        CommsAddrRec addr = new CommsAddrRec( CommsConnType.COMMS_CONN_P2P )
            .setP2PParams( macAddress );

        ReceiveResult rslt = receiveMessage( this, gameID, m_sink, data, addr );
    }

    private static void processPacket( BiDiSockWrap wrap, byte[] bytes )
    {
        String asStr = new String(bytes);
        DbgUtils.logd( WifiDirectService.class, "got string: %s", asStr );
        try {
            JSONObject asObj = new JSONObject( asStr );
            DbgUtils.logd( WifiDirectService.class, "got json: %s", asObj.toString() );
            final String cmd = asObj.optString( KEY_CMD, "" );
            if ( cmd.equals( CMD_PING ) ) {
                storeByAddress( wrap, asObj );
                try {
                    wrap.send( new JSONObject()
                               .put( KEY_CMD, CMD_PONG )
                               .put( KEY_MAC, getMyMacAddress() ) );
                } catch ( JSONException jse ) {
                    DbgUtils.logex( jse );
                }
            } else if ( cmd.equals( CMD_PONG ) ) {
                storeByAddress( wrap, asObj );
            } else if ( cmd.equals( CMD_MSG ) ) {
                // byte[] data = XwJNI.base64Decode( asObj.optString( KEY_DATA, null ) );
                int gameID = asObj.optInt( KEY_GAMEID, 0 );
                if ( 0 != gameID ) {
                    Intent intent = getIntentTo( P2PAction.GOT_MSG );
                    intent.putExtra( KEY_GAMEID, gameID );
                    intent.putExtra( KEY_DATA, asObj.optString( KEY_DATA, null ) );
                    intent.putExtra( KEY_RETADDR, wrap.getMacAddress() );
                    XWApp.getContext().startService( intent );
                } else {
                    Assert.fail(); // don't ship with this!!!
                }
            }
        } catch ( JSONException jse ) {
            DbgUtils.logex( jse );
        }
    }
    
    private static void startAcceptThread()
    {
        sAmServer = true;
        sAcceptThread = new Thread( new Runnable() {
                public void run() {
                    try {
                        sServerSock = new ServerSocket( USE_PORT );
                        while ( sAmServer ) {
                            DbgUtils.logd( WifiDirectService.class, "calling accept()" );
                            Socket socket = sServerSock.accept();
                            DbgUtils.logd( WifiDirectService.class, "accept() returned!!" );
                            new BiDiSockWrap( socket, sIface );
                        }
                    } catch ( IOException ioe ) {
                        sAmServer = false;
                        DbgUtils.logex( ioe );
                    }
                }
            } );
        sAcceptThread.start();
    }

    private static void stopAcceptThread()
    {
        if ( null != sAcceptThread ) {
            if ( null != sServerSock ) {
                try {
                    sServerSock.close();
                } catch ( IOException ioe ) {
                    DbgUtils.logex( ioe );
                }
                sServerSock = null;
            }
            sAcceptThread = null;
        }
    }

    private static Intent getIntentTo( P2PAction cmd )
    {
        Context context = XWApp.getContext();
        Intent intent = null;
        if ( null != context ) {
            intent = new Intent( context, WifiDirectService.class );
            intent.putExtra( KEY_CMD, cmd.ordinal() );
        } else {
            // This basically means we can't receive P2P messages when in the
            // background, which is silly. They're coming in on sockets. Do I
            // need a socket to have a context associated with it?
            DbgUtils.logd( WifiDirectService.class,
                           "getIntentTo(): contenxt null" );
        }
        return intent;
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
                    WifiP2pDevice device = (WifiP2pDevice) intent
                        .getParcelableExtra(WifiP2pManager.EXTRA_WIFI_P2P_DEVICE);
                    sMacAddress = device.deviceAddress;
                    String deviceName = device.deviceName;

                    DbgUtils.logd( getClass(), "Got my MAC Address: %s and name: %s",
                                   sMacAddress, deviceName );
                    String stored = DBUtils.getStringFor( context, MAC_ADDR_KEY, null );
                    Assert.assertTrue( null == stored || stored.equals(sMacAddress) );
                    if ( null == stored ) {
                        DBUtils.setStringFor( context, MAC_ADDR_KEY, sMacAddress );
                    }
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
            if (info.groupFormed ) {
                if ( info.isGroupOwner ) {
                    DbgUtils.logd( getClass(), "am group owner" );
                    startAcceptThread();
                } else {
                    DbgUtils.logd( getClass(), "am NOT group owner" );
                    connectToOwner( info.groupOwnerAddress );
                    // The other device acts as the client. In this case,
                    // you'll want to create a client thread that connects to the group
                    // owner.
                }
            } else {
                Assert.fail();
            }
        }
    }

    private static class PLL implements WifiP2pManager.PeerListListener {
        @Override
        public void onPeersAvailable( WifiP2pDeviceList peerList ) {
            DbgUtils.logd( getClass(), "got list of %d peers",
                           peerList.getDeviceList().size() );

            for ( WifiP2pDevice device : peerList.getDeviceList() ) {
                // DbgUtils.logd( getClass(), "not connecting to: %s", device.toString() );
                tryConnect( device );
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

    private class P2pMsgSink extends MultiMsgSink {

        public P2pMsgSink() { super( WifiDirectService.this ); }

        // @Override
        // public int sendViaP2P( byte[] buf, int gameID, CommsAddrRec addr )
        // {
        //     return WifiDirectService
        //         .sendPacket( m_context, addr.p2p_addr, gameID, buf );
        // }
    }
}
