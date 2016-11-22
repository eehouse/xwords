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

import android.app.Activity;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.NetworkInfo;
import android.net.wifi.WpsInfo;
import android.net.wifi.p2p.WifiP2pConfig;
import android.net.wifi.p2p.WifiP2pDevice;
import android.net.wifi.p2p.WifiP2pDeviceList;
import android.net.wifi.p2p.WifiP2pGroup;
import android.net.wifi.p2p.WifiP2pInfo;
import android.net.wifi.p2p.WifiP2pManager.ActionListener;
import android.net.wifi.p2p.WifiP2pManager.Channel;
import android.net.wifi.p2p.WifiP2pManager.ChannelListener;
import android.net.wifi.p2p.WifiP2pManager.DnsSdServiceResponseListener;
import android.net.wifi.p2p.WifiP2pManager.DnsSdTxtRecordListener;
import android.net.wifi.p2p.WifiP2pManager.GroupInfoListener;
import android.net.wifi.p2p.WifiP2pManager;
import android.net.wifi.p2p.nsd.WifiP2pDnsSdServiceInfo;
import android.net.wifi.p2p.nsd.WifiP2pDnsSdServiceRequest;
import android.net.wifi.p2p.nsd.WifiP2pUpnpServiceInfo;
import android.os.Handler;
import android.os.Looper;

import java.io.IOException;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.eehouse.android.xw4.MultiService.DictFetchOwner;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

import junit.framework.Assert;

public class WiDirService extends XWService {
    private static final Class CLAZZ = WiDirService.class;
    private static final String MAC_ADDR_KEY = "p2p_mac_addr";
    private static final String SERVICE_NAME = "srvc_" + BuildConstants.VARIANT;
    private static final String SERVICE_REG_TYPE = "_presence._tcp";
    private static boolean WIFI_DIRECT_ENABLED = true;
    private static final int OWNER_PORT = 5432;

    private enum P2PAction { _NONE,
                             GOT_MSG,
                             GOT_INVITE,
    }

    private static final String KEY_CMD = "cmd";
    private static final String KEY_SRC = "src";
    private static final String KEY_NLI = "nli";
    private static final String KEY_DEST = "dest";

    private static final String KEY_GAMEID = "gmid";
    private static final String KEY_DATA = "data";
    private static final String KEY_MAC = "mac";
    private static final String KEY_NAME = "name";
    private static final String KEY_MAP = "map";
    private static final String KEY_RETADDR = "raddr";

    private static final String CMD_PING = "ping";
    private static final String CMD_PONG = "pong";
    private static final String CMD_MSG = "msg";
    private static final String CMD_INVITE = "invite";

    private static Channel sChannel;
    private static ChannelListener sListener;
    private static IntentFilter sIntentFilter;
    private static GroupInfoListener sGroupListener;
    private static WFDBroadcastReceiver sReceiver;
    private static boolean sDiscoveryStarted;
    private static boolean sEnabled;
    // These two kinda overlap...
    private static boolean sAmServer;
    private static boolean sAmGroupOwner;
    private static Thread sAcceptThread;
    private static ServerSocket sServerSock;
    private static BiDiSockWrap.Iface sIface;
    private static Map<String, BiDiSockWrap> sSocketWrapMap
        = new HashMap<String, BiDiSockWrap>();
    private static Map<String, String> sUserMap = new HashMap<String, String>();
    private static Map<String, Long> sPendingDevs = new HashMap<String, Long>();
    private static String sMacAddress;
    private static String sDeviceName;
    private static Set<DevSetListener> s_devListeners
        = new HashSet<DevSetListener>();

    private P2pMsgSink m_sink;

    public interface DevSetListener {
        void setChanged( Map<String, String> macToName );
    }

    @Override
    public void onCreate()
    {
        m_sink = new P2pMsgSink();
    }

    @Override
    public int onStartCommand( Intent intent, int flags, int startId )
    {
        int result;

        if ( WIFI_DIRECT_ENABLED && null != intent ) {
            result = Service.START_STICKY;

            int ordinal = intent.getIntExtra( KEY_CMD, -1 );
            if ( -1 != ordinal ) {
                P2PAction cmd = P2PAction.values()[ordinal];
                switch ( cmd ) {
                case GOT_MSG:
                    handleGotMessage( intent );
                    updateStatusIn( true );
                    break;
                case GOT_INVITE:
                    handleGotInvite( intent );
                    break;
                }
            }
        } else {
            result = Service.START_NOT_STICKY;
            stopSelf( startId );
        }

        return result;
    }

    private static void updateStatusOut( boolean success )
    {
        ConnStatusHandler
            .updateStatusOut( XWApp.getContext(), null,
                              CommsConnType.COMMS_CONN_P2P, success );
    }

    private static void updateStatusIn( boolean success )
    {
        ConnStatusHandler
            .updateStatusIn( XWApp.getContext(), null,
                             CommsConnType.COMMS_CONN_P2P, success );
    }

    public static boolean supported()
    {
        return WIFI_DIRECT_ENABLED;
    }

    public static boolean connecting() {
        return supported()
            && 0 < sSocketWrapMap.size()
            && sSocketWrapMap.values().iterator().next().isConnected();
    }

    public static String getMyMacAddress( Context context )
    {
        if ( WIFI_DIRECT_ENABLED ) {
            if ( null == sMacAddress && null != context ) {
                sMacAddress = DBUtils.getStringFor( context, MAC_ADDR_KEY, null );
            }
        }
        DbgUtils.logd( CLAZZ, "getMyMacAddress() => %s",
                       sMacAddress );
        // Assert.assertNotNull(sMacAddress);
        return sMacAddress;
    }

    public static String formatNetStateInfo()
    {
        String map = mapToString( copyUserMap() );
        return String.format( "map: %s role: %s; nThreads: %d",
                              map, sAmServer ? "owner" : "guest",
                              Thread.activeCount() );
    }

    private static String getMyMacAddress() { return getMyMacAddress(null); }

    public static void registerDevSetListener( DevSetListener dsl )
    {
        synchronized( s_devListeners ) {
            s_devListeners.add( dsl );
        }
        updateListeners();
    }

    public static void unregisterDevSetListener( DevSetListener dsl )
    {
        synchronized( s_devListeners ) {
            s_devListeners.remove( dsl );
        }
    }

    public static void inviteRemote( Context context, String macAddr,
                                     NetLaunchInfo nli )
    {
        DbgUtils.logd( CLAZZ, "inviteRemote(%s)", macAddr );
        Assert.assertNotNull( macAddr );
        String nliString = nli.toString();
        DbgUtils.logd( CLAZZ, "inviteRemote(%s)", nliString );

        boolean[] forwarding = { false };
        BiDiSockWrap wrap = getForSend( macAddr, forwarding );

        if ( null == wrap ) {
            DbgUtils.loge( CLAZZ, "inviteRemote: no socket for %s", macAddr );
        } else {
            try {
                JSONObject packet = new JSONObject()
                    .put( KEY_CMD, CMD_INVITE )
                    .put( KEY_SRC, getMyMacAddress() )
                    .put( KEY_NLI, nliString )
                    ;
                if ( forwarding[0] ) {
                    packet.put( KEY_DEST, macAddr );
                }
                wrap.send( packet );
            } catch ( JSONException jse ) {
                DbgUtils.logex( jse );
            }
        }
    }

    public static int sendPacket( Context context, String macAddr, int gameID,
                                  byte[] buf )
    {
        DbgUtils.logd( CLAZZ, "sendPacket(len=%d,addr=%s)", buf.length, macAddr );
        int nSent = -1;

        boolean[] forwarding = { false };
        BiDiSockWrap wrap = getForSend( macAddr, forwarding );

        if ( null != wrap ) {
            try {
                JSONObject packet = new JSONObject()
                    .put( KEY_CMD, CMD_MSG )
                    .put( KEY_SRC, getMyMacAddress() )
                    .put( KEY_DATA, XwJNI.base64Encode( buf ) )
                    .put( KEY_GAMEID, gameID )
                    ;
                if ( forwarding[0] ) {
                    packet.put( KEY_DEST, macAddr );
                }
                wrap.send( packet );
                nSent = buf.length;
            } catch ( JSONException jse ) {
                DbgUtils.logex( jse );
            }
        } else {
            DbgUtils.logd( CLAZZ, "sendPacket: no socket for %s", macAddr );
        }
        return nSent;
    }
    
    public static void activityResumed( Activity activity )
    {
        if ( WIFI_DIRECT_ENABLED ) {
            if ( initListeners( activity ) ) {
                activity.registerReceiver( sReceiver, sIntentFilter );
                DbgUtils.logd( CLAZZ, "activityResumed() done" );
                startDiscovery();
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
            DbgUtils.logd( CLAZZ, "activityPaused() done" );

            // Examples seem to kick discovery off once and that's it
            // sDiscoveryStarted = false;
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
                                DbgUtils.logd( CLAZZ, "onChannelDisconnected()");
                            }
                        };

                    WifiP2pManager mgr = getMgr();
                    sChannel = mgr.initialize( context, Looper.getMainLooper(),
                                               sListener );
                    Assert.assertNotNull( sChannel );

                    sIface = new BiDiSockWrap.Iface() {
                            public void gotPacket( BiDiSockWrap socket, byte[] bytes )
                            {
                                DbgUtils.logd( CLAZZ, "wrapper got packet!!!" );
                                processPacket( socket, bytes );
                            }

                            public void connectStateChanged( BiDiSockWrap wrap, boolean nowConnected )
                            {
                                DbgUtils.logd( CLAZZ, "connectStateChanged(con=%b)", nowConnected );
                                if ( nowConnected ) {
                                    try {
                                        wrap.send( new JSONObject()
                                                   .put( KEY_CMD, CMD_PING )
                                                   .put( KEY_NAME, sDeviceName )
                                                   .put( KEY_MAC, getMyMacAddress( context ) ) );
                                    } catch ( JSONException jse ) {
                                        DbgUtils.logex( jse );
                                    }
                                } else {
                                    int sizeBefore = sSocketWrapMap.size();
                                    sSocketWrapMap.values().remove( wrap );
                                    DbgUtils.logd( CLAZZ, "removed wrap; had %d, now have %d",
                                                   sizeBefore, sSocketWrapMap.size() );
                                    if ( 0 == sSocketWrapMap.size() ) {
                                        updateStatusIn( false );
                                        updateStatusOut( false );
                                    }
                                }
                            }

                            public void onWriteSuccess( BiDiSockWrap wrap ) {
                                updateStatusOut( true );
                            }
                        };

                    sGroupListener = new GroupInfoListener() {
                            public void onGroupInfoAvailable( WifiP2pGroup group ) {
                                if ( null == group ) {
                                    DbgUtils.logd( CLAZZ, "onGroupInfoAvailable(null)!" );
                                } else {
                                    DbgUtils.logd( CLAZZ, "onGroupInfoAvailable(owner: %b)!",
                                                   group.isGroupOwner() );
                                    Assert.assertTrue( sAmGroupOwner == group.isGroupOwner() );
                                    if ( sAmGroupOwner ) {
                                        Collection<WifiP2pDevice> devs = group.getClientList();
                                        synchronized( sUserMap ) {
                                            for ( WifiP2pDevice dev : devs ) {
                                                String macAddr = dev.deviceAddress;
                                                sUserMap.put( macAddr, dev.deviceName );
                                                BiDiSockWrap wrap = sSocketWrapMap.get( macAddr );
                                                if ( null == wrap ) {
                                                    DbgUtils.logd( CLAZZ,
                                                                   "groupListener: no socket for %s",
                                                                   macAddr );
                                                } else {
                                                    DbgUtils.logd( CLAZZ, "socket for %s connected: %b",
                                                                   macAddr, wrap.isConnected() );
                                                }
                                            }
                                        }
                                    }
                                }
                                DbgUtils.logd( CLAZZ, "thread count: %d", Thread.activeCount() );
                                new Handler().postDelayed( new Runnable() {
                                        @Override
                                        public void run() {
                                            getMgr().requestGroupInfo( sChannel, sGroupListener );
                                        }
                                    }, 60 * 1000 );
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
                    DbgUtils.logd( CLAZZ, "disabling wifi; no permissions" );
                    WIFI_DIRECT_ENABLED = false;
                }
            } else {
                succeeded = true;
            }
        }
        return succeeded;
    }

    // See: http://stackoverflow.com/questions/26300889/wifi-p2p-service-discovery-works-intermittently
    private static void startDiscovery()
    {
        DbgUtils.logd( CLAZZ, "startDiscovery()" );
        if ( WIFI_DIRECT_ENABLED && ! sDiscoveryStarted ) {
            // Map<String, String> record = new HashMap<String, String>();
            // record.put( "AVAILABLE", "visible" );
            // record.put( "PORT", "" + OWNER_PORT );
            // WifiP2pDnsSdServiceInfo service = WifiP2pDnsSdServiceInfo
            //     .newInstance( SERVICE_NAME, SERVICE_REG_TYPE, record );
            getMgr().clearLocalServices( sChannel, new ActionListener() {
                    @Override
                    public void onSuccess() {
                        addLocalService();
                    }
                    @Override
                    public void onFailure(int code) {
                        // I've only seen this fail when wifi's off
                        tryAgain( "clearLocalServices", code );
                    }
                } );
        }
    }

    private static void addLocalService()
    {
        DbgUtils.logd( CLAZZ, "addLocalService()" );
        Map<String, String> record = new HashMap<String, String>();
        record.put( "AVAILABLE", "visible");
        record.put( "PORT", "" + OWNER_PORT );
        record.put( "NAME", sDeviceName );
        WifiP2pDnsSdServiceInfo service = WifiP2pDnsSdServiceInfo
            .newInstance( SERVICE_NAME, SERVICE_REG_TYPE, record );

        final WifiP2pManager mgr = getMgr();
        mgr.addLocalService( sChannel, service, new ActionListener() {
                @Override
                public void onSuccess() {
                    setDiscoveryListeners( mgr );
                    clearServiceRequests();
                }
                @Override
                public void onFailure(int code) { Assert.fail(); }
            } );
    }

    private static void clearServiceRequests()
    {
        DbgUtils.logd( CLAZZ, "clearServiceRequests()" );
        WifiP2pManager mgr = getMgr();
        setDiscoveryListeners( mgr );
        mgr.clearServiceRequests( sChannel, new ActionListener() {
                @Override
                public void onSuccess() {
                    addServiceRequest();
                }
                @Override
                public void onFailure(int code) { Assert.fail(); }
            } );
    }

    private static void addServiceRequest()
    {
        DbgUtils.logd( CLAZZ, "addServiceRequest()" );
        getMgr().addServiceRequest(sChannel, WifiP2pDnsSdServiceRequest.newInstance(),
                              new ActionListener() {
                                  @Override
                                  public void onSuccess() {
                                      discoverPeers();
                                  }
                                  @Override
                                  public void onFailure(int code) { Assert.fail(); }
                              } );
    }

    private static void discoverPeers()
    {
        DbgUtils.logd( CLAZZ, "discoverPeers()" );
        getMgr().discoverPeers( sChannel, new ActionListener() {
                @Override
                public void onSuccess() {
                    discoverServices();
                }
                @Override
                public void onFailure(int code) {
                    tryAgain( "discoverPeers", code );
                }
            } );
    }

    private static void discoverServices()
    {
        DbgUtils.logd( CLAZZ, "discoverServices()" );
        getMgr().discoverServices( sChannel, new ActionListener() {
                @Override
                public void onSuccess() {
                    DbgUtils.logd( CLAZZ, "discoverServices succeeded!!!");
                    sDiscoveryStarted = true;
                }
                @Override
                public void onFailure(int code) { Assert.fail(); }
            } );
    }

    private static void tryAgain( String proc, int code )
    {
        DbgUtils.logd( CLAZZ, "proc %s failed with code %d",
                       proc, code );
        new Handler().postDelayed( new Runnable() {
                @Override
                public void run() {
                    startDiscovery();
                }
            }, 10 * 1000 );     // PENDING: do a backoff here
    }

    private static WifiP2pManager getMgr()
    {
        Context context = XWApp.getContext();
        return (WifiP2pManager)context.getSystemService(Context.WIFI_P2P_SERVICE);
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
                            DbgUtils.logd( CLAZZ, "onBonjourServiceAvailable "
                                           + instanceName + " with name "
                                           + srcDevice.deviceName );
                            tryConnect( srcDevice );
                        }
                    }
                };

            DnsSdTxtRecordListener trl = new DnsSdTxtRecordListener() {
                    // This doesn't seen to be getting called
                    @Override
                    public void onDnsSdTxtRecordAvailable( String domainName,
                                                           Map<String, String> map,
                                                           WifiP2pDevice device ) {
                        DbgUtils.logd( CLAZZ,
                                       "onDnsSdTxtRecordAvailable(avail: %s, port: %s; name: %s)",
                                       map.get("AVAILABLE"), map.get("PORT"), map.get("NAME"));
                    }
                };
            mgr.setDnsSdResponseListeners( sChannel, srl, trl );
            DbgUtils.logd( CLAZZ, "setDiscoveryListeners done" );
        }
    }

    private static boolean connectPending( String macAddress )
    {
        boolean result = false;
        if ( sPendingDevs.containsKey( macAddress ) ) {
            long when = sPendingDevs.get( macAddress );
            long now = Utils.getCurSeconds();
            result = 3 >= now - when;
        }
        DbgUtils.logd( CLAZZ, "connectPending(%s)=>%b",
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
            DbgUtils.logd( CLAZZ, "tryConnect(%s): already connected",
                           macAddress );
        } else if ( connectPending( macAddress ) ) {
            // Do nothing
        } else {
            DbgUtils.logd( CLAZZ, "trying to connect to %s",
                           device.toString() );
            WifiP2pConfig config = new WifiP2pConfig();
            config.deviceAddress = device.deviceAddress;
            config.wps.setup = WpsInfo.PBC;

            getMgr().connect( sChannel, config, new ActionListener() {
                    @Override
                    public void onSuccess() {
                        DbgUtils.logd( CLAZZ, "onSuccess(): %s", "connect_xx" );
                        notePending( macAddress );
                    }
                    @Override
                    public void onFailure(int reason) {
                        DbgUtils.logd( CLAZZ, "onFailure(%d): %s", reason, "connect_xx");
                    }
                } );
        }
    }

    private static void connectToOwner( InetAddress addr )
    {
        BiDiSockWrap wrap = new BiDiSockWrap( addr, OWNER_PORT, sIface );
        DbgUtils.logd( CLAZZ, "connectToOwner(%s)", addr.toString() );
        wrap.connect();
    }

    private static void storeByAddress( BiDiSockWrap wrap, JSONObject packet )
    {
        String macAddress = packet.optString( KEY_MAC, null );
        // Assert.assertNotNull( macAddress );
        if ( null != macAddress ) {
            // this has fired. Sockets close and re-open?
            sSocketWrapMap.put( macAddress, wrap );
            DbgUtils.logd( CLAZZ,
                           "storeByAddress(); storing wrap for %s",
                           macAddress );
        }
    }

    private void handleGotMessage( Intent intent )
    {
        DbgUtils.logd( CLAZZ, "handleGotMessage(%s)", intent.toString() );
        int gameID = intent.getIntExtra( KEY_GAMEID, 0 );
        byte[] data = XwJNI.base64Decode( intent.getStringExtra( KEY_DATA ) );
        String macAddress = intent.getStringExtra( KEY_RETADDR );

        CommsAddrRec addr = new CommsAddrRec( CommsConnType.COMMS_CONN_P2P )
            .setP2PParams( macAddress );

        ReceiveResult rslt = receiveMessage( this, gameID, m_sink, data, addr );
    }

    private void handleGotInvite( Intent intent )
    {
        DbgUtils.logd( CLAZZ, "handleGotInvite()" );
        String nliData = intent.getStringExtra( KEY_NLI );
        NetLaunchInfo nli = new NetLaunchInfo( this, nliData );
        String returnMac = intent.getStringExtra( KEY_SRC );
        if ( checkNotDupe( nli ) ) {
            if ( DictLangCache.haveDict( this, nli.lang, nli.dict ) ) {
                makeGame( nli, returnMac );
            } else {
                Intent dictIntent = MultiService
                    .makeMissingDictIntent( this, nli,
                                            DictFetchOwner.OWNER_P2P );
                MultiService.postMissingDictNotification( this, dictIntent,
                                                          nli.gameID() );
            }
        }
    }

    private void makeGame( NetLaunchInfo nli, String senderMac )
    {
        long[] rowids = DBUtils.getRowIDsFor( this, nli.gameID() );
        if ( null == rowids || 0 == rowids.length ) {
            CommsAddrRec addr = nli.makeAddrRec( this );
            long rowid = GameUtils.makeNewMultiGame( this, nli,
                                                     m_sink,
                                                     getUtilCtxt() );
            if ( DBUtils.ROWID_NOTFOUND != rowid ) {
                if ( null != nli.gameName && 0 < nli.gameName.length() ) {
                    DBUtils.setName( this, rowid, nli.gameName );
                }
                String body = LocUtils.getString( this, R.string.new_bt_body_fmt,
                                                  senderMac );
                GameUtils.postInvitedNotification( this, nli.gameID(), body,
                                                   rowid );
            }
        }
    }

    private static void processPacket( BiDiSockWrap wrap, byte[] bytes )
    {
        String asStr = new String(bytes);
        DbgUtils.logd( CLAZZ, "got string: %s", asStr );
        try {
            JSONObject asObj = new JSONObject( asStr );
            DbgUtils.logd( CLAZZ, "got json: %s", asObj.toString() );
            final String cmd = asObj.optString( KEY_CMD, "" );
            if ( cmd.equals( CMD_PING ) ) {
                storeByAddress( wrap, asObj );
                try {
                    JSONObject packet = new JSONObject()
                        .put( KEY_CMD, CMD_PONG )
                        .put( KEY_MAC, getMyMacAddress() );
                    addMappings( packet );
                    wrap.send( packet );
                } catch ( JSONException jse ) {
                    DbgUtils.logex( jse );
                }
            } else if ( cmd.equals( CMD_PONG ) ) {
                storeByAddress( wrap, asObj );
                readMappings( asObj );
            } else if ( cmd.equals( CMD_INVITE ) ) {
                if ( ! forwardedPacket( asObj, bytes ) ) {
                    Intent intent = getIntentTo( P2PAction.GOT_INVITE );
                    intent.putExtra( KEY_NLI, asObj.getString( KEY_NLI ) );
                    intent.putExtra( KEY_SRC, asObj.getString( KEY_SRC ) );
                    XWApp.getContext().startService( intent );
                }
            } else if ( cmd.equals( CMD_MSG ) ) {
                if ( ! forwardedPacket( asObj, bytes ) ) {
                    int gameID = asObj.optInt( KEY_GAMEID, 0 );
                    if ( 0 != gameID ) {
                        Intent intent = getIntentTo( P2PAction.GOT_MSG );
                        intent.putExtra( KEY_GAMEID, gameID );
                        intent.putExtra( KEY_DATA, asObj.getString( KEY_DATA ) );
                        intent.putExtra( KEY_RETADDR, asObj.getString( KEY_SRC ) );
                        XWApp.getContext().startService( intent );
                    } else {
                        Assert.fail(); // don't ship with this!!!
                    }
                }
            }
        } catch ( JSONException jse ) {
            DbgUtils.logex( jse );
        }
    }

    private static void addMappings( JSONObject packet )
    {
        synchronized( sUserMap ) {
            try {
                JSONArray array = new JSONArray();
                for ( String mac : sUserMap.keySet() ) {
                    JSONObject map = new JSONObject()
                        .put( KEY_MAC, mac )
                        .put( KEY_NAME, sUserMap.get(mac) );
                    array.put( map );
                }
                packet.put( KEY_MAP, array );
            } catch ( JSONException ex ) {
                DbgUtils.logex( ex );
            }
        }
    }

    private static void readMappings( JSONObject asObj )
    {
        synchronized( sUserMap ) {
            try {
                JSONArray array = asObj.getJSONArray( KEY_MAP );
                for ( int ii = 0; ii < array.length(); ++ii ) {
                    JSONObject map = array.getJSONObject( ii );
                    String name = map.getString( KEY_NAME );
                    String mac = map.getString( KEY_MAC );
                    sUserMap.put( mac, name );
                }
            } catch ( JSONException ex ) {
                DbgUtils.logex( ex );
            }
        }
        updateListeners();
    }

    private static void updateListeners()
    {
        DevSetListener[] listeners = null;
        synchronized( s_devListeners ) {
            if ( 0 < s_devListeners.size() ) {
                listeners = new DevSetListener[s_devListeners.size()];
                Iterator<DevSetListener> iter = s_devListeners.iterator();
                for ( int ii = 0; ii < listeners.length; ++ii ) {
                    listeners[ii] = iter.next();
                }
            }
        }

        if ( null != listeners ) {
            Map<String, String> macToName = copyUserMap();
            macToName.remove( getMyMacAddress() );

            for ( DevSetListener listener : listeners ) {
                listener.setChanged( macToName );
            }
        }
    }

    private static Map<String, String> copyUserMap()
    {
        Map<String, String> macToName;
        synchronized ( sUserMap ) {
            macToName = new HashMap<String, String>(sUserMap);
        }
        return macToName;
    }

    private static String mapToString( Map<String, String> macToName )
    {
        String result = "";
        Iterator<String> iter = macToName.keySet().iterator();
        for ( int ii = 0; iter.hasNext(); ++ii ) {
            String mac = iter.next();
            result += String.format( "%d: %s=>%s; ", ii, mac,
                                     macToName.get( mac ) );
        }
        return result;
    }

    private static boolean forwardedPacket( JSONObject asObj, byte[] bytes )
    {
        boolean forwarded = false;
        String destAddr = asObj.optString( KEY_DEST );
        if ( 0 < destAddr.length() ) {
            Assert.assertFalse( destAddr.equals( sMacAddress ) );
            forwardPacket( bytes, destAddr );
            forwarded = true;
        }
        return forwarded;
    }

    private static void forwardPacket( byte[] bytes, String destAddr )
    {
        DbgUtils.logd( CLAZZ, "forwardPacket(mac=%s)", destAddr );
        if ( sAmGroupOwner ) {
            BiDiSockWrap wrap = sSocketWrapMap.get( destAddr );
            if ( null != wrap && wrap.isConnected() ) {
                wrap.send( bytes );
            } else {
                DbgUtils.loge( CLAZZ, "no working socket for %s", destAddr );
            }
        } else {
            DbgUtils.loge( CLAZZ, "can't forward; not group owner (any more?)" );
        }
    }
    
    private static void startAcceptThread()
    {
        sAmServer = true;
        sAcceptThread = new Thread( new Runnable() {
                public void run() {
                    DbgUtils.logd( CLAZZ, "accept thread starting" );
                    try {
                        sServerSock = new ServerSocket( OWNER_PORT );
                        while ( sAmServer ) {
                            DbgUtils.logd( CLAZZ, "calling accept()" );
                            Socket socket = sServerSock.accept();
                            DbgUtils.logd( CLAZZ, "accept() returned!!" );
                            new BiDiSockWrap( socket, sIface );
                        }
                    } catch ( IOException ioe ) {
                        sAmServer = false;
                        DbgUtils.logex( ioe );
                    }
                    DbgUtils.logd( CLAZZ, "accept thread exiting" );
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
        Intent intent = new Intent( context, WiDirService.class );
        intent.putExtra( KEY_CMD, cmd.ordinal() );
        return intent;
    }

    private static BiDiSockWrap getForSend( String macAddr, boolean[] forwarding )
    {
        BiDiSockWrap wrap = sSocketWrapMap.get( macAddr );

        // See if we need to forward through group owner instead
        if ( null == wrap && !sAmGroupOwner && 1 == sSocketWrapMap.size() ) {
            DbgUtils.logd( CLAZZ, "forwarding to %s through group owner", macAddr );
            wrap = sSocketWrapMap.values().iterator().next();
            forwarding[0] = true;
        }

        if ( null == wrap ) {
            updateStatusOut( false );
        }
        return wrap;
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
                DbgUtils.logd( CLAZZ, "got intent: " + intent.toString() );

                if (WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION.equals(action)) {
                    int state = intent.getIntExtra(WifiP2pManager.EXTRA_WIFI_STATE, -1);
                    sEnabled = state == WifiP2pManager.WIFI_P2P_STATE_ENABLED;
                    DbgUtils.logd( CLAZZ, "WifiP2PEnabled: %b", sEnabled );
                } else if (WifiP2pManager.WIFI_P2P_PEERS_CHANGED_ACTION.equals(action)) {
                    mManager.requestPeers( mChannel, new PLL() );
                } else if (WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION.equals(action)) {
                    // Assert.fail();
                    NetworkInfo networkInfo = (NetworkInfo)intent
                        .getParcelableExtra(WifiP2pManager.EXTRA_NETWORK_INFO);
                    if ( networkInfo.isConnected() ) {
                        DbgUtils.logd( CLAZZ, "network %s connected",
                                       networkInfo.toString() );
                        mManager.requestConnectionInfo(mChannel, new CIL());
                    } else {
                        // here
                        DbgUtils.logd( CLAZZ, "network %s NOT connected",
                                       networkInfo.toString() );
                    }
                } else if (WifiP2pManager.WIFI_P2P_THIS_DEVICE_CHANGED_ACTION.equals(action)) {
                    // Respond to this device's wifi state changing
                    WifiP2pDevice device = (WifiP2pDevice) intent
                        .getParcelableExtra(WifiP2pManager.EXTRA_WIFI_P2P_DEVICE);
                    sMacAddress = device.deviceAddress;
                    sDeviceName = device.deviceName;
                    synchronized( sUserMap ) {
                        sUserMap.put( sMacAddress, sDeviceName );
                    }
                    DbgUtils.logd( CLAZZ, "Got my MAC Address: %s and name: %s",
                                   sMacAddress, sDeviceName );

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
            DbgUtils.logd( CLAZZ, "onSuccess(): %s", mStr );
        }
        @Override
        public void onFailure(int reason) {
            DbgUtils.logd( CLAZZ, "onFailure(%d): %s", reason, mStr);
        }
    }

    private static class CIL implements WifiP2pManager.ConnectionInfoListener {
        public void onConnectionInfoAvailable( final WifiP2pInfo info ) {

            // InetAddress from WifiP2pInfo struct.
            InetAddress groupOwnerAddress = info.groupOwnerAddress;
            String hostAddress = groupOwnerAddress.getHostAddress();
            DbgUtils.logd( CLAZZ, "onConnectionInfoAvailable(%s); addr: %s",
                           info.toString(), hostAddress );

            // After the group negotiation, we can determine the group owner.
            if (info.groupFormed ) {
                sAmGroupOwner = info.isGroupOwner;
                if ( info.isGroupOwner ) {
                    DbgUtils.showf( "Joining %s WiFi P2p group as owner", SERVICE_NAME );
                    DbgUtils.logd( CLAZZ, "am group owner" );
                    startAcceptThread();
                } else {
                    DbgUtils.logd( CLAZZ, "am NOT group owner" );
                    DbgUtils.showf( "Joining %s WiFi P2p group as guest", SERVICE_NAME );
                    stopAcceptThread();
                    connectToOwner( info.groupOwnerAddress );
                    // The other device acts as the client. In this case,
                    // you'll want to create a client thread that connects to the group
                    // owner.
                }
                getMgr().requestGroupInfo( sChannel, sGroupListener );
            } else {
                Assert.fail();
            }
        }
    }

    private static class PLL implements WifiP2pManager.PeerListListener {
        @Override
        public void onPeersAvailable( WifiP2pDeviceList peerList ) {
            DbgUtils.logd( CLAZZ, "got list of %d peers",
                           peerList.getDeviceList().size() );

            for ( WifiP2pDevice device : peerList.getDeviceList() ) {
                // DbgUtils.logd( CLAZZ, "not connecting to: %s", device.toString() );
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

        public P2pMsgSink() { super( WiDirService.this ); }

        // @Override
        // public int sendViaP2P( byte[] buf, int gameID, CommsAddrRec addr )
        // {
        //     return WiDirService
        //         .sendPacket( m_context, addr.p2p_addr, gameID, buf );
        // }
    }
}
