/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2016 - 2018 by Eric House (xwords@eehouse.org).  All rights
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
import android.text.TextUtils;

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
import org.eehouse.android.xw4.MultiService.MultiEvent;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;


public class WiDirService extends XWService {
    private static final String TAG = WiDirService.class.getSimpleName();
    private static final Class CLAZZ = WiDirService.class;
    private static final String MAC_ADDR_KEY = "p2p_mac_addr";
    private static final String SERVICE_NAME = "srvc_" + BuildConfig.FLAVOR;
    private static final String SERVICE_REG_TYPE = "_presence._tcp";
    private static final int OWNER_PORT = 5432;
    private static final String PEERS_LIST_KEY = TAG + ".peers_key";

    private enum P2PAction { _NONE,
                             GOT_MSG,
                             GOT_INVITE,
                             GAME_GONE,
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

    private static boolean s_enabled = false;
    private static Channel sChannel;
    private static ServiceDiscoverer s_discoverer;
    private static IntentFilter sIntentFilter;
    private static GroupInfoListener sGroupListener;
    private static WFDBroadcastReceiver sReceiver;
    // private static boolean sDiscoveryStarted;
    private static boolean sDiscoveryRunning; // set via broadcast; unused
    private static boolean sEnabled;
    private static boolean sHavePermission;
    // These two kinda overlap...
    private static boolean sAmServer;
    private static boolean sAmGroupOwner;
    private static Thread sAcceptThread;
    private static ServerSocket sServerSock;
    private static BiDiSockWrap.Iface sIface;
    private static Map<String, BiDiSockWrap> sSocketWrapMap = new HashMap<>();
    private static Map<String, String> sUserMap = new HashMap<>();
    private static Map<String, Long> sPendingDevs = new HashMap<>();
    private static String sMacAddress;
    private static String sDeviceName;
    private static Set<DevSetListener> s_devListeners = new HashSet<>();
    private static Set<String> s_peersSet;

    private P2pMsgSink m_sink;
    private WiDirServiceHelper mHelper;

    public interface DevSetListener {
        void setChanged( Map<String, String> macToName );
    }

    @Override
    public void onCreate()
    {
        m_sink = new P2pMsgSink();
        mHelper = new WiDirServiceHelper(this);
    }

    @Override
    public int onStartCommand( Intent intent, int flags, int startId )
    {
        int result;

        if ( enabled() && null != intent ) {
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
                case GAME_GONE:
                    handleGameGone( intent );
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
            .updateStatusOut( XWApp.getContext(),
                              CommsConnType.COMMS_CONN_P2P, success );
    }

    private static void updateStatusIn( boolean success )
    {
        ConnStatusHandler
            .updateStatusIn( XWApp.getContext(), CommsConnType.COMMS_CONN_P2P,
                             success );
    }

    public static void init( Context context )
    {
        Log.d( TAG, "init()" );
        s_enabled = XWPrefs.getPrefsBoolean( context, R.string.key_enable_p2p,
                                             false );

        Assert.assertNull( s_peersSet );
        s_peersSet = new HashSet<>();
        String peers = DBUtils.getStringFor( context, PEERS_LIST_KEY );
        if ( null != peers ) {
            String[] macs = TextUtils.split( peers, "," );
            for ( String mac : macs ) {
                s_peersSet.add( mac );
            }
        }
        Log.d( TAG, "loaded saved peers: %s", s_peersSet.toString() );

        try {
            ChannelListener listener = new ChannelListener() {
                    @Override
                    public void onChannelDisconnected() {
                        Log.d( TAG, "onChannelDisconnected()");
                    }
                };
            sChannel = getMgr().initialize( context, Looper.getMainLooper(),
                                            listener );
            s_discoverer = new ServiceDiscoverer();
            sHavePermission = true;
        } catch ( NoClassDefFoundError ndf ) { // old os version
            sHavePermission = false;
        } catch ( SecurityException se ) {               // perm not in manifest
            sHavePermission = false;
        } catch ( NullPointerException npe ) { // Seeing this on Oreo emulator
            sHavePermission = false;
        }
    }

    public static void reset( Context context )
    {
        // Put experimental stuff here that might help get a connection

        if ( null != s_discoverer ) {
            s_discoverer.restart();
        }
    }

    public static boolean enabled()
    {
        boolean result = BuildConfig.WIDIR_ENABLED && s_enabled;
        return result;
    }

    public static boolean connecting()
    {
        return enabled()
            && 0 < sSocketWrapMap.size()
            && sSocketWrapMap.values().iterator().next().isConnected();
    }

    public static String getMyMacAddress( Context context )
    {
        if ( enabled() ) {
            if ( null == sMacAddress && null != context ) {
                sMacAddress = DBUtils.getStringFor( context, MAC_ADDR_KEY );
            }
        }
        Log.d( TAG, "getMyMacAddress() => %s", sMacAddress );
        // Assert.assertNotNull(sMacAddress);
        return sMacAddress;
    }

    public static String formatNetStateInfo()
    {
        String connectState = "";
        if ( null != s_discoverer ) {
            connectState = s_discoverer.stateToString();
        }

        String map = mapToString( copyUserMap() );
        connectState += String.format( "; role: %s; map: %s nThreads: %d",
                                       sAmGroupOwner ? "owner" : "guest", map,
                                       Thread.activeCount() );
        return connectState;
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
        Log.d( TAG, "inviteRemote(%s)", macAddr );
        Assert.assertNotNull( macAddr );
        String nliString = nli.toString();
        Log.d( TAG, "inviteRemote(%s)", nliString );

        boolean[] forwarding = { false };
        BiDiSockWrap wrap = getForSend( macAddr, forwarding );

        if ( null == wrap ) {
            Log.e( TAG, "inviteRemote: no socket for %s", macAddr );
        } else {
            XWPacket packet = new XWPacket( XWPacket.CMD.INVITE )
                .put( KEY_SRC, getMyMacAddress() )
                .put( KEY_NLI, nliString )
                ;
            if ( forwarding[0] ) {
                packet.put( KEY_DEST, macAddr );
            }
            wrap.send( packet );
        }
    }

    public static int sendPacket( Context context, String macAddr, int gameID,
                                  byte[] buf )
    {
        Log.d( TAG, "sendPacket(len=%d,addr=%s)", buf.length, macAddr );
        int nSent = -1;

        boolean[] forwarding = { false };
        BiDiSockWrap wrap = getForSend( macAddr, forwarding );

        if ( null != wrap ) {
            XWPacket packet = new XWPacket( XWPacket.CMD.MSG )
                .put( KEY_SRC, getMyMacAddress() )
                .put( KEY_DATA, Utils.base64Encode( buf ) )
                .put( KEY_GAMEID, gameID )
                ;
            if ( forwarding[0] ) {
                packet.put( KEY_DEST, macAddr );
            }
            wrap.send( packet );
            nSent = buf.length;
        } else {
            Log.d( TAG, "sendPacket: no socket for %s", macAddr );
        }
        return nSent;
    }

    public static void activityResumed( Activity activity )
    {
        if ( enabled() && sHavePermission ) {
            if ( initListeners( activity ) ) {
                activity.registerReceiver( sReceiver, sIntentFilter );
                Log.d( TAG, "activityResumed() done" );
                startDiscovery();
            }
        }
    }

    public static void activityPaused( Activity activity )
    {
        if ( enabled() && sHavePermission ) {
            Assert.assertNotNull( sReceiver );
            // No idea why I'm seeing this exception...
            try {
                activity.unregisterReceiver( sReceiver );
            } catch ( IllegalArgumentException iae ) {
                Log.ex( TAG, iae );
            }
            Log.d( TAG, "activityPaused() done" );

            // Examples seem to kick discovery off once and that's it
            // sDiscoveryStarted = false;
        }
    }

    public static void gameDied( String macAddr, int gameID )
    {
        // FIX ME when can test; probably just this:
        // sendNoGame( null, macAddr, gameID );

        Iterator<BiDiSockWrap> iter = sSocketWrapMap.values().iterator();
        while ( iter.hasNext() ) {
            sendNoGame( iter.next(), null, gameID );
        }
    }

    private static boolean initListeners( final Context context )
    {
        boolean succeeded = false;
        if ( enabled() ) {
            if ( null == sIface ) {
                try {
                    WifiP2pManager mgr = getMgr();
                    Assert.assertNotNull( sChannel );

                    sIface = new BiDiSockWrap.Iface() {
                            public void gotPacket( BiDiSockWrap socket, byte[] bytes )
                            {
                                Log.d( TAG, "wrapper got packet!!!" );
                                updateStatusIn( true );
                                processPacket( socket, bytes );
                            }

                            public void connectStateChanged( BiDiSockWrap wrap,
                                                             boolean nowConnected )
                            {
                                Log.d( TAG, "connectStateChanged(connected=%b)",
                                       nowConnected );
                                if ( nowConnected ) {
                                        wrap.send( new XWPacket( XWPacket.CMD.PING )
                                                   .put( KEY_NAME, sDeviceName )
                                                   .put( KEY_MAC, getMyMacAddress( context ) ) );
                                } else {
                                    int sizeBefore = sSocketWrapMap.size();
                                    sSocketWrapMap.values().remove( wrap );
                                    Log.d( TAG, "removed wrap; had %d, now have %d",
                                           sizeBefore, sSocketWrapMap.size() );
                                    if ( 0 == sSocketWrapMap.size() ) {
                                        updateStatusIn( false );
                                        updateStatusOut( false );
                                    }
                                }
                            }

                            public void onWriteSuccess( BiDiSockWrap wrap ) {
                                Log.d( TAG, "onWriteSuccess()" );
                                updateStatusOut( true );
                            }
                        };

                    sGroupListener = new GroupInfoListener() {
                            public void onGroupInfoAvailable( WifiP2pGroup group ) {
                                if ( null == group ) {
                                    Log.d( TAG, "onGroupInfoAvailable(null)!" );
                                } else {
                                    Log.d( TAG, "onGroupInfoAvailable(owner: %b)!",
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
                                                    Log.d( TAG,
                                                           "groupListener: no socket for %s",
                                                           macAddr );
                                                } else {
                                                    Log.d( TAG, "socket for %s connected: %b",
                                                           macAddr, wrap.isConnected() );
                                                }
                                            }
                                        }
                                    }
                                }
                                Log.d( TAG, "thread count: %d", Thread.activeCount() );
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
                    sIntentFilter.addAction(WifiP2pManager.WIFI_P2P_DISCOVERY_CHANGED_ACTION);

                    sReceiver = new WFDBroadcastReceiver( mgr, sChannel );
                    succeeded = true;
                } catch ( SecurityException se ) {
                    Log.d( TAG, "disabling wifi; no permissions" );
                    sEnabled = false;
                }
            } else {
                succeeded = true;
            }
        }
        return succeeded;
    }

    // See: http://stackoverflow.com/questions/26300889/wifi-p2p-service-discovery-works-intermittently
    private static class ServiceDiscoverer implements Runnable, ActionListener {
        private State m_curState = State.START;
        private State m_lastGoodState = State.START;
        private State m_lastBadState = State.START;
        private Channel m_channel;
        private Handler m_handler;
        private WifiP2pManager m_mgr;
        private int[] m_failures;
        private boolean m_lastSucceeded;

        enum State {
            START,
            CLEAR_LOCAL_SERVICES,
            ADD_LOCAL_SERVICES,
            CLEAR_SERVICES_REQUESTS,
            ADD_SERVICE_REQUEST,
            DISCOVER_PEERS,
            PEER_DISCOVERY_STARTED,
            DONE,
        }

        public ServiceDiscoverer()
        {
            m_mgr = getMgr();
            m_channel = sChannel;
            m_handler = new Handler();
        }

        public String stateToString()
        {
            return String.format("last good: %s(%d); last bad: %s(%d); success last: %b",
                                 m_lastGoodState.toString(), m_lastGoodState.ordinal(),
                                 m_lastBadState.toString(), m_lastBadState.ordinal(),
                                 m_lastSucceeded );
        }

        public void restart()
        {
            m_curState = State.START;
            schedule( 0 );
        }

        private void schedule( int waitSeconds )
        {
            Log.d( TAG, "scheduling %s in %d seconds",
                   m_curState.toString(), waitSeconds );
            m_handler.removeCallbacks( this ); // remove any others
            m_handler.postDelayed( this, waitSeconds * 1000 );
        }

        // ActionListener interface
        @Override
        public void onSuccess() {
            m_lastSucceeded = true;
            m_lastGoodState = m_curState;
            Log.d( TAG, "onSuccess(): state %s done", m_curState.toString() );
            m_curState = State.values()[m_curState.ordinal() + 1];
            schedule( 0 );
        }

        @Override
        public void onFailure( int code ) {
            m_lastSucceeded = false;
            m_lastBadState = m_curState;

            int count = ++m_failures[m_curState.ordinal()];
            String codeStr = null;
            switch ( code ) {
            case WifiP2pManager.ERROR:
                m_curState = State.START;
                schedule( 10 );
                codeStr = "ERROR";
                break;
            case WifiP2pManager.P2P_UNSUPPORTED:
                // Assert.fail();  // fires on emulator
                codeStr = "UNSUPPORTED";
                break;
            case WifiP2pManager.BUSY:
                if ( ! sEnabled ) {
                    Log.d( TAG, "onFailure(): no wifi, so stopping machine" );
                    break;
                } else if ( 8 < count ) {
                    Log.d( TAG, "too many errors; restarting machine" );
                    m_curState = State.START;
                }
                schedule( 10 );
                codeStr = "BUSY";
                break;
            }
            Log.d( TAG, "onFailure(%s): state %s failed (count=%d)",
                   codeStr, m_curState.toString(), count );
        }

        @Override
        public void run() {
            switch( m_curState ) {
            case START:
                m_failures = new int[State.values().length];
                onSuccess();    // move to next state
                break;

            case CLEAR_LOCAL_SERVICES:
                m_mgr.clearLocalServices( m_channel, this );
                break;

            case ADD_LOCAL_SERVICES:
                Map<String, String> record = new HashMap<>();
                record.put( "AVAILABLE", "visible");
                record.put( "PORT", "" + OWNER_PORT );
                record.put( "NAME", sDeviceName );
                WifiP2pDnsSdServiceInfo service = WifiP2pDnsSdServiceInfo
                    .newInstance( SERVICE_NAME, SERVICE_REG_TYPE, record );

                final WifiP2pManager mgr = getMgr();
                m_mgr.addLocalService( m_channel, service, this );
                break;

            case CLEAR_SERVICES_REQUESTS:
                setDiscoveryListeners( m_mgr );
                m_mgr.clearServiceRequests( m_channel, this );
                break;

            case ADD_SERVICE_REQUEST:
                m_mgr.addServiceRequest( m_channel,
                                         WifiP2pDnsSdServiceRequest.newInstance(),
                                         this );
                break;

            case DISCOVER_PEERS:
                m_mgr.discoverPeers( m_channel, this );
                break;

            case PEER_DISCOVERY_STARTED:
                m_mgr.discoverServices( m_channel, this );
                break;

            case DONE:
                Log.d( TAG, "machine done; should I try connecting to: %s?",
                       s_peersSet.toString() );
                // m_curState = State.START;
                // schedule( /*5 * */ 60 );
                break;

            default:
                Assert.failDbg();
            }
        }
    }

    // See: http://stackoverflow.com/questions/26300889/wifi-p2p-service-discovery-works-intermittently
    private static void startDiscovery()
    {
        if ( null != s_discoverer ) {
            s_discoverer.restart();
        }
    }

    private static WifiP2pManager getMgr()
    {
        Context context = XWApp.getContext();
        return (WifiP2pManager)context.getSystemService(Context.WIFI_P2P_SERVICE);
    }

    private static void setDiscoveryListeners( WifiP2pManager mgr )
    {
        if ( enabled() ) {
            DnsSdServiceResponseListener srl = new DnsSdServiceResponseListener() {
                    @Override
                    public void onDnsSdServiceAvailable(String instanceName,
                                                        String registrationType,
                                                        WifiP2pDevice srcDevice) {
                        // Service has been discovered. My app?
                        if ( instanceName.equalsIgnoreCase( SERVICE_NAME ) ) {
                            Log.d( TAG, "onDnsSdServiceAvailable: %s with name %s",
                                   instanceName, srcDevice.deviceName );
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
                        Log.d( TAG,
                               "onDnsSdTxtRecordAvailable(avail: %s, port: %s; name: %s)",
                               map.get("AVAILABLE"), map.get("PORT"), map.get("NAME"));
                    }
                };
            mgr.setDnsSdResponseListeners( sChannel, srl, trl );
            Log.d( TAG, "setDiscoveryListeners done" );
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
        Log.d( TAG, "connectPending(%s)=>%b", macAddress, result );
        return result;
    }

    private static void notePending( String macAddress )
    {
        sPendingDevs.put( macAddress, Utils.getCurSeconds() );
    }

    private static void tryConnect( WifiP2pDevice device )
    {
        final String macAddress = device.deviceAddress;
        if ( sAmGroupOwner ) {
            Log.d( TAG, "tryConnect(%s): dropping because group owner",
                   macAddress );
        } else if ( sSocketWrapMap.containsKey(macAddress)
             && sSocketWrapMap.get(macAddress).isConnected() ) {
            Log.d( TAG, "tryConnect(%s): already connected", macAddress );
        } else if ( connectPending( macAddress ) ) {
            // Do nothing
        } else {
            Log.d( TAG, "trying to connect to %s", macAddress );
            WifiP2pConfig config = new WifiP2pConfig();
            config.deviceAddress = macAddress;
            config.wps.setup = WpsInfo.PBC;

            getMgr().connect( sChannel, config, new ActionListener() {
                    @Override
                    public void onSuccess() {
                        Log.d( TAG, "onSuccess(): %s", "connect_xx" );
                        notePending( macAddress );
                    }
                    @Override
                    public void onFailure(int reason) {
                        Log.d( TAG, "onFailure(%d): %s", reason, "connect_xx");
                    }
                } );
        }
    }

    private static void connectToOwner( InetAddress addr )
    {
        BiDiSockWrap wrap = new BiDiSockWrap( addr, OWNER_PORT, sIface );
        Log.d( TAG, "connectToOwner(%s)", addr.toString() );
        wrap.connect();
    }

    private static void storeByAddress( BiDiSockWrap wrap, XWPacket packet )
    {
        String macAddress = packet.getString( KEY_MAC );
        // Assert.assertNotNull( macAddress );
        if ( null != macAddress ) {
            // this has fired. Sockets close and re-open?
            sSocketWrapMap.put( macAddress, wrap );
            Log.d( TAG, "storeByAddress(); storing wrap for %s",
                   macAddress );

            GameUtils.resendAllIf( XWApp.getContext(),
                                   CommsConnType.COMMS_CONN_P2P );
        }
    }

    private void handleGotMessage( Intent intent )
    {
        Log.d( TAG, "handleGotMessage(%s)", intent.toString() );
        int gameID = intent.getIntExtra( KEY_GAMEID, 0 );
        byte[] data = Utils.base64Decode( intent.getStringExtra( KEY_DATA ) );
        String macAddress = intent.getStringExtra( KEY_RETADDR );

        CommsAddrRec addr = new CommsAddrRec( CommsConnType.COMMS_CONN_P2P )
            .setP2PParams( macAddress );

        XWServiceHelper.ReceiveResult rslt = mHelper
            .receiveMessage( gameID, m_sink, data, addr );
        if ( XWServiceHelper.ReceiveResult.GAME_GONE == rslt ) {
            sendNoGame( null, macAddress, gameID );
        }
    }

    private void handleGotInvite( Intent intent )
    {
        Log.d( TAG, "handleGotInvite()" );
        String nliData = intent.getStringExtra( KEY_NLI );
        NetLaunchInfo nli = NetLaunchInfo.makeFrom( this, nliData );
        String returnMac = intent.getStringExtra( KEY_SRC );

        if ( !mHelper.handleInvitation( nli, returnMac, DictFetchOwner.OWNER_P2P ) ) {
            Log.d( TAG, "handleInvitation() failed" );
        }
    }

    private void handleGameGone( Intent intent )
    {
        int gameID = intent.getIntExtra( KEY_GAMEID, 0 );
        mHelper.postEvent( MultiEvent.MESSAGE_NOGAME, gameID );
    }

    private void makeGame( NetLaunchInfo nli, String senderMac )
    {
        long[] rowids = DBUtils.getRowIDsFor( this, nli.gameID() );
        if ( 0 == rowids.length ) {
            CommsAddrRec addr = nli.makeAddrRec( this );
            long rowid = GameUtils.makeNewMultiGame( this, nli,
                                                     m_sink,
                                                     mHelper.getUtilCtxt() );
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
        Context context = XWApp.getContext();
        Intent intent = null;
        String asStr = new String(bytes);
        Log.d( TAG, "got string: %s", asStr );
        XWPacket packet = new XWPacket( asStr );
        // JSONObject asObj = new JSONObject( asStr );
        Log.d( TAG, "got packet: %s", packet.toString() );
        final XWPacket.CMD cmd = packet.getCommand();
        switch ( cmd ) {
        case PING:
            storeByAddress( wrap, packet );
            XWPacket reply = new XWPacket( XWPacket.CMD.PONG )
                .put( KEY_MAC, getMyMacAddress() );
            addMappings( reply );
            wrap.send( reply );
            break;
        case PONG:
            storeByAddress( wrap, packet );
            readMappings( packet );
            break;
        case INVITE:
            if ( ! forwardedPacket( packet, bytes ) ) {
                intent = getIntentTo( P2PAction.GOT_INVITE );
                intent.putExtra( KEY_NLI, packet.getString( KEY_NLI ) );
                intent.putExtra( KEY_SRC, packet.getString( KEY_SRC ) );
            }
            break;
        case MSG:
            if ( ! forwardedPacket( packet, bytes ) ) {
                int gameID = packet.getInt( KEY_GAMEID, 0 );
                if ( 0 != gameID ) {
                    if ( DBUtils.haveGame( context, gameID ) ) {
                        intent = getIntentTo( P2PAction.GOT_MSG );
                        intent.putExtra( KEY_GAMEID, gameID );
                        intent.putExtra( KEY_DATA, packet.getString( KEY_DATA ) );
                        intent.putExtra( KEY_RETADDR, packet.getString( KEY_SRC ) );
                    } else {
                        sendNoGame( wrap, null, gameID );
                    }
                }
            }
            break;
        case NOGAME:
            if ( ! forwardedPacket( packet, bytes ) ) {
                int gameID = packet.getInt( KEY_GAMEID, 0 );
                intent = getIntentTo( P2PAction.GAME_GONE );
                intent.putExtra( KEY_GAMEID, gameID );
            }
            break;
        }

        if ( null != intent ) {
            context.startService( intent );
        }
    }

    private static void sendNoGame( BiDiSockWrap wrap, String macAddress,
                                    int gameID )
    {
        boolean[] forwarding = { false };
        if ( null == wrap ) {
            wrap = getForSend( macAddress, forwarding );
        }

        if ( null != wrap ) {
            XWPacket packet = new XWPacket( XWPacket.CMD.NOGAME )
                .put( KEY_GAMEID, gameID );
            if ( forwarding[0] ) {
                packet.put( KEY_DEST, macAddress );
            }
            wrap.send( packet );
        }
    }

    private static void addMappings( XWPacket packet )
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
                Log.ex( TAG, ex );
            }
        }
    }

    private static void readMappings( XWPacket packet )
    {
        synchronized( sUserMap ) {
            try {
                JSONArray array = packet.getJSONArray( KEY_MAP );
                for ( int ii = 0; ii < array.length(); ++ii ) {
                    JSONObject map = array.getJSONObject( ii );
                    String name = map.getString( KEY_NAME );
                    String mac = map.getString( KEY_MAC );
                    sUserMap.put( mac, name );
                }
            } catch ( JSONException ex ) {
                Log.ex( TAG, ex );
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
            macToName = new HashMap<>(sUserMap);
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

    private static boolean forwardedPacket( XWPacket packet, byte[] bytes )
    {
        boolean forwarded = false;
        String destAddr = packet.getString( KEY_DEST );
        if ( null != destAddr && 0 < destAddr.length() ) {
            forwarded = destAddr.equals( sMacAddress );
            if ( forwarded ) {
                forwardPacket( bytes, destAddr );
            } else {
                Log.d( TAG, "addr mismatch: %s vs %s", destAddr, sMacAddress );
            }
        }
        return forwarded;
    }

    private static void forwardPacket( byte[] bytes, String destAddr )
    {
        Log.d( TAG, "forwardPacket(mac=%s)", destAddr );
        if ( sAmGroupOwner ) {
            BiDiSockWrap wrap = sSocketWrapMap.get( destAddr );
            if ( null != wrap && wrap.isConnected() ) {
                wrap.send( bytes );
            } else {
                Log.e( TAG, "no working socket for %s", destAddr );
            }
        } else {
            Log.e( TAG, "can't forward; not group owner (any more?)" );
        }
    }

    private static void startAcceptThread()
    {
        sAmServer = true;
        sAcceptThread = new Thread( new Runnable() {
                public void run() {
                    Log.d( TAG, "accept thread starting" );
                    boolean done = false;
                    try {
                        sServerSock = new ServerSocket( OWNER_PORT );
                        while ( !done ) {
                            Log.d( TAG, "calling accept()" );
                            Socket socket = sServerSock.accept();
                            Log.d( TAG, "accept() returned!!" );
                            new BiDiSockWrap( socket, sIface );
                        }
                    } catch ( IOException ioe ) {
                        Log.e( TAG, ioe.toString() );
                        sAmServer = false;
                        done = true;
                    }
                    Log.d( TAG, "accept thread exiting" );
                }
            } );
        sAcceptThread.start();
    }

    private static void stopAcceptThread()
    {
        Log.d( TAG, "stopAcceptThread()" );
        if ( null != sAcceptThread ) {
            if ( null != sServerSock ) {
                try {
                    sServerSock.close();
                } catch ( IOException ioe ) {
                    Log.ex( TAG, ioe );
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
            wrap = sSocketWrapMap.values().iterator().next();
            Log.d( TAG, "forwarding to %s through group owner", macAddr );
            forwarding[0] = true;
        }

        if ( null == wrap ) {
            updateStatusOut( false );
        }
        return wrap;
    }

    private static void updatePeersList( WifiP2pDeviceList peerList )
    {
        Set<String> newSet = new HashSet<>();
        for ( WifiP2pDevice device : peerList.getDeviceList() ) {
            String macAddress = device.deviceAddress;
            newSet.add( macAddress );
        }

        Log.d( TAG, "updatePeersList(): old set: %s; new set: %s",
               s_peersSet.toString(), newSet.toString() );
        s_peersSet = newSet;

        DBUtils.setStringFor( XWApp.getContext(), PEERS_LIST_KEY,
                              TextUtils.join( ",", s_peersSet ) );
    }

    private static class WFDBroadcastReceiver extends BroadcastReceiver
        implements WifiP2pManager.ConnectionInfoListener,
                   WifiP2pManager.PeerListListener {
        private WifiP2pManager mManager;
        private Channel mChannel;

        public WFDBroadcastReceiver( WifiP2pManager manager, Channel channel )
        {
            super();
            mManager = manager;
            mChannel = channel;
        }

        @Override
        public void onReceive( Context context, Intent intent )
        {
            if ( enabled() ) {
                String action = intent.getAction();
                Log.d( TAG, "got intent: " + intent.toString() );

                if (WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION.equals(action)) {
                    int state = intent.getIntExtra(WifiP2pManager.EXTRA_WIFI_STATE, -1);
                    sEnabled = state == WifiP2pManager.WIFI_P2P_STATE_ENABLED;
                    Log.d( TAG, "WifiP2PEnabled: %b", sEnabled );
                    if ( sEnabled ) {
                        startDiscovery();
                    }
                } else if (WifiP2pManager.WIFI_P2P_PEERS_CHANGED_ACTION.equals(action)) {
                    mManager.requestPeers( mChannel, this );
                } else if (WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION.equals(action)) {
                    // Assert.fail();
                    NetworkInfo networkInfo = (NetworkInfo)intent
                        .getParcelableExtra(WifiP2pManager.EXTRA_NETWORK_INFO);
                    if ( networkInfo.isConnected() ) {
                        Log.d( TAG, "network %s connected",
                                       networkInfo.toString() );
                        mManager.requestConnectionInfo(mChannel, this );
                    } else {
                        // here
                        Log.d( TAG, "network %s NOT connected",
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
                    Log.d( TAG, "Got my MAC Address: %s and name: %s", sMacAddress,
                           sDeviceName );

                    String stored = DBUtils.getStringFor( context, MAC_ADDR_KEY );
                    Assert.assertTrue( null == stored || stored.equals(sMacAddress) );
                    if ( null == stored ) {
                        DBUtils.setStringFor( context, MAC_ADDR_KEY, sMacAddress );
                    }
                } else if (WifiP2pManager.WIFI_P2P_DISCOVERY_CHANGED_ACTION.equals(action)) {
                    int running = intent
                        .getIntExtra( WifiP2pManager.EXTRA_DISCOVERY_STATE, -1 );
                    Assert.assertTrue( running == 2 || running == 1 ); // remove soon
                    sDiscoveryRunning = 2 == running;
                    Log.d( TAG, "discovery changed: running: %b", sDiscoveryRunning );
                }
            }
        }

        // WifiP2pManager.ConnectionInfoListener interface
        @Override
        public void onConnectionInfoAvailable( final WifiP2pInfo info )
        {
            // InetAddress from WifiP2pInfo struct.
            InetAddress groupOwnerAddress = info.groupOwnerAddress;
            String hostAddress = groupOwnerAddress.getHostAddress();
            Log.d( TAG, "onConnectionInfoAvailable(%s); addr: %s",
                   info.toString(), hostAddress );

            // After the group negotiation, we can determine the group owner.
            if (info.groupFormed ) {
                sAmGroupOwner = info.isGroupOwner;
                Log.d( TAG, "am %sgroup owner", sAmGroupOwner ? "" : "NOT " );
                DbgUtils.showf( "Joining WiFi P2p group as %s",
                                sAmGroupOwner ? "owner" : "guest" );
                if ( info.isGroupOwner ) {
                    startAcceptThread();
                } else {
                    stopAcceptThread();
                    connectToOwner( info.groupOwnerAddress );
                    // The other device acts as the client. In this case,
                    // you'll want to create a client thread that connects to the group
                    // owner.
                }
                getMgr().requestGroupInfo( sChannel, sGroupListener );
            } else {
                Assert.failDbg();
            }
        }

        @Override
        public void onPeersAvailable( WifiP2pDeviceList peerList )
        {
            Log.d( TAG, "got list of %d peers",
                   peerList.getDeviceList().size() );

            updatePeersList( peerList );

            for ( WifiP2pDevice device : peerList.getDeviceList() ) {
                // Log.d( TAG, "not connecting to: %s", device.toString() );
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
    }

    private class WiDirServiceHelper extends XWServiceHelper {

        WiDirServiceHelper( Service service ) {
            super( service );
        }

        @Override
        MultiMsgSink getSink( long rowid )
        {
            return m_sink;
        }

        @Override
        void postNotification( String device, int gameID, long rowid )
        {
            Log.e( TAG, "postNotification() doing nothing" );
        }
    }
}
