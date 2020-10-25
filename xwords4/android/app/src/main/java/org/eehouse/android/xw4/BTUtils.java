/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2019 by Eric House (xwords@eehouse.org).  All
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

import android.bluetooth.BluetoothServerSocket;
import android.bluetooth.BluetoothSocket;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothClass.Device.Major;
import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.content.Intent;
import android.provider.Settings;
import android.text.TextUtils;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.atomic.AtomicBoolean;

import org.eehouse.android.xw4.DbgUtils.DeadlockWatch;
import org.eehouse.android.xw4.MultiService.DictFetchOwner;
import org.eehouse.android.xw4.MultiService.MultiEvent;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

public class BTUtils {
    private static final String TAG = BTUtils.class.getSimpleName();
    private static final String BOGUS_MARSHMALLOW_ADDR = "02:00:00:00:00:00";
    private static final int MAX_PACKET_LEN = 4 * 1024;
    private static int CONNECT_SLEEP_MS = 2500;

    private enum BTCmd {
        BAD_PROTO,
        PING,
        PONG,
        SCAN,
        INVITE,
        INVITE_ACCPT,
        INVITE_DECL,            // unused
        INVITE_DUPID,
        INVITE_FAILED,      // generic error
        MESG_SEND,
        MESG_ACCPT,
        MESG_DECL,              // unused
        MESG_GAMEGONE,
        REMOVE_FOR,             // unused
        INVITE_DUP_INVITE,
    };

    interface ScanListener {
        void onDeviceScanned( BluetoothDevice dev );
        void onScanDone();
    };

    private static final int BT_PROTO_JSONS = 1; // using jsons instead of lots of fields
    private static final int BT_PROTO_BATCH = 2;
    private static final int BT_PROTO = BT_PROTO_JSONS; /* BT_PROTO_BATCH */
    private static boolean IS_BATCH_PROTO() { return BT_PROTO_BATCH == BT_PROTO; }

    private static AtomicBoolean sBackUser = new AtomicBoolean(false);
    private static String sAppName;
    private static UUID sUUID;

    public static boolean BTAvailable()
    {
        BluetoothAdapter adapter = getAdapterIf();
        return null != adapter;
    }

    public static boolean BTEnabled()
    {
        BluetoothAdapter adapter = getAdapterIf();
        return null != adapter && adapter.isEnabled();
    }

    public static void enable()
    {
        BluetoothAdapter adapter = getAdapterIf();
        if ( null != adapter ) {
            // Only do this after explicit action from user -- Android guidelines
            adapter.enable();
        }
    }

    static void init( Context context, String appName, UUID uuid )
    {
        Log.d( TAG, "init()" );
        sAppName = appName;
        sUUID = uuid;
        onResume( context );
    }

    static void timerFired( Context context )
    {
        onResume( context );
    }

    static void onResume( Context context )
    {
        Log.d( TAG, "onResume()" );
        // Should only run this in the background if we have BT games
        // going. In the foreground we want to
        ListenThread.getOrStart();
    }

    static void onStop( Context context )
    {
        Log.d( TAG, "onStop(): doing nothing for now" );
    }

    public static void openBTSettings( Activity activity )
    {
        Intent intent = new Intent();
        intent.setAction( Settings.ACTION_BLUETOOTH_SETTINGS );
        activity.startActivity( intent );
    }

    public static boolean isBogusAddr( String addr )
    {
        boolean result = BOGUS_MARSHMALLOW_ADDR.equals( addr );
        Log.d( TAG, "isBogusAddr(%s) => %b", addr, result );
        return result;
    }

    public static String[] getBTNameAndAddress()
    {
        String[] result = null;
        BluetoothAdapter adapter = getAdapterIf();
        if ( null != adapter ) {
            String addr = adapter.getAddress();
            if ( isBogusAddr( addr ) ) {
                addr = null;
            }
            result = new String[] { adapter.getName(), addr };
        }
        return result;
    }

    public static String nameForAddr( String btAddr )
    {
        BluetoothAdapter adapter = getAdapterIf();
        return nameForAddr( adapter, btAddr );
    }

    public static void setAmForeground()
    {
        sBackUser.set(false);
    }

    private static String nameForAddr( BluetoothAdapter adapter, String btAddr )
    {
        String result = null;
        if ( null != adapter ) {
            result = adapter.getRemoteDevice( btAddr ).getName();
        }
        return result;
    }

    public static int sendPacket( Context context, byte[] buf, String msgID,
                                  CommsAddrRec targetAddr, int gameID )
    {
        Log.d( TAG, "sendPacket(%s): name: %s; addr: %s", targetAddr,
               targetAddr.bt_hostName, targetAddr.bt_btAddr );
        int nSent = -1;
        String name = targetAddr.bt_hostName;
        if ( isActivePeer( name ) ) {
            String btAddr = getSafeAddr(targetAddr);
            getPA( btAddr ).addMsg( gameID, buf, msgID );
        } else {
            Log.d( TAG, "sendPacket(): addressee %s unknown so dropping", name );
        }
        return nSent;
    }

    public static void gameDied( Context context, String btAddr, int gameID )
    {
        getPA( btAddr ).addDied( gameID );
    }

    public static void pingHost( Context context, String btAddr, int gameID )
    {
        getPA( btAddr ).addPing( gameID );
    }

    public static void inviteRemote( Context context, String btAddr,
                                     NetLaunchInfo nli )
    {
        getPA( btAddr ).addInvite( nli );
    }

    private static Set<ScanListener> sListeners = new HashSet<>();
    public static void addScanListener( ScanListener listener )
    {
        synchronized ( sListeners ) {
            sListeners.add( listener );
        }
    }

    public static void removeScanListener( ScanListener listener )
    {
        synchronized ( sListeners ) {
            sListeners.remove( listener );
        }
    }

    private static void callListeners( BluetoothDevice dev )
    {
        Log.d( TAG, "callListeners(%s)", dev.getName() );
        synchronized ( sListeners ) {
            for ( ScanListener listener : sListeners ) {
                listener.onDeviceScanned( dev );
            }
        }
    }

    public static int scan( Context context, int timeoutMS )
    {
        Log.e( TAG, "scan(): not implemented" );
        Set<BluetoothDevice> devs = getCandidates();
        int count = devs.size();
        if ( 0 < count ) {
            ScanThread.startOnce( timeoutMS, devs );
        }
        return count;
    }

    static BluetoothAdapter getAdapterIf()
    {
        BluetoothAdapter result = null;
        // Later this will change to include at least a test whether we're
        // running as background user account, a situation in which BT crashes
        // a lot inside the OS.
        if ( !sBackUser.get() ) {
            result = BluetoothAdapter.getDefaultAdapter();
        }
        return result;
    }

    private static boolean isActivePeer( String devName )
    {
        boolean result = false;

        Set<BluetoothDevice> devs = getCandidates();
        for ( BluetoothDevice dev : devs ) {
            if ( dev.getName().equals( devName ) ) {
                result = true;
                break;
            }
        }
        Log.d( TAG, "isActivePeer(%s) => %b", devName, result );
        return result;
    }

    static Set<BluetoothDevice> getCandidates()
    {
        Set<BluetoothDevice> result = new HashSet<>();
        BluetoothAdapter adapter = getAdapterIf();
        if ( null != adapter ) {
            for ( BluetoothDevice dev : adapter.getBondedDevices() ) {
                int clazz = dev.getBluetoothClass().getMajorDeviceClass();
                switch ( clazz ) {
                case Major.AUDIO_VIDEO:
                case Major.HEALTH:
                case Major.IMAGING:
                case Major.TOY:
                case Major.PERIPHERAL:
                    break;
                default:
                    result.add( dev );
                    break;
                }
            }
        }
        return result;
    }

    private static void updateStatusIn( boolean success )
    {
        Context context = XWApp.getContext();
        ConnStatusHandler
            .updateStatusIn( context, CommsConnType.COMMS_CONN_BT, success );
    }

    private static void updateStatusOut( boolean success )
    {
        Context context = XWApp.getContext();
        ConnStatusHandler
            .updateStatusOut( context, CommsConnType.COMMS_CONN_BT, success );
    }

    private static PacketAccumulator getPA( String addr )
    {
        Assert.assertTrue( !TextUtils.isEmpty(addr) );
        PacketAccumulator pa = getSenderFor( addr ).wake();
        return pa; // .setService( this );
    }

    private static void removeSenderFor( PacketAccumulator pa )
    {
        try ( DeadlockWatch dw = new DeadlockWatch( sSenders ) ) {
            synchronized ( sSenders ) {
                if ( pa == sSenders.get( pa.getBTAddr() ) ) {
                    sSenders.remove( pa );
                } else {
                    Log.e( TAG, "race? There's a different PA for %s", pa.getBTAddr() );
                }
            }
        }
    }

    private static Map<String, PacketAccumulator> sSenders = new HashMap<>();
    private static PacketAccumulator getSenderFor( String addr )
    {
        return getSenderFor( addr, true );
    }

    private static PacketAccumulator getSenderFor( String addr, boolean create )
    {
        PacketAccumulator result;
        try ( DeadlockWatch dw = new DeadlockWatch( sSenders ) ) {
            synchronized ( sSenders ) {
                if ( create && !sSenders.containsKey( addr ) ) {
                    sSenders.put( addr, new PacketAccumulator( addr ) );
                }
                result = sSenders.get( addr );
            }
        }
        return result;
    }

    private static class ScanThread extends Thread {
        static ScanThread[] sInstance = {null};
        private int mTimeoutMS;
        private Set<BluetoothDevice> mDevs;

        static void startOnce( int timeoutMS, Set<BluetoothDevice> devs )
        {
            synchronized ( sInstance ) {
                if ( sInstance[0] == null ) {
                    ScanThread thread = new ScanThread( timeoutMS, devs );
                    sInstance[0] = thread;
                    thread.start();
                }
            }
        }

        private static void clearInstance( ScanThread instance )
        {
            synchronized (sInstance) {
                if ( sInstance[0] == null ) {
                    // nothing to do
                } else if ( sInstance[0] == instance ) {
                    sInstance[0] = null;
                } else {
                    Log.e( TAG, "ScanThread.clearInstance(): cur instance %s not == %s",
                           sInstance[0], instance );
                }
            }
        }

        private ScanThread( int timeoutMS, Set<BluetoothDevice> devs )
        {
            mTimeoutMS = timeoutMS;
            mDevs = devs;
        }

        @Override
        public void run()
        {
            Assert.assertTrueNR( this == sInstance[0] );
            Map<BluetoothDevice, PacketAccumulator> pas = new HashMap<>();

            for ( BluetoothDevice dev : mDevs ) {
                PacketAccumulator pa =
                    new PacketAccumulator( dev.getAddress(), mTimeoutMS )
                    .addPing( 0 )
                    .setExitWhenEmpty()
                    .setLifetimeMS( mTimeoutMS )
                    .wake()
                    ;
                pas.put( dev, pa );
            }

            for ( BluetoothDevice dev : pas.keySet() ) {
                PacketAccumulator pa = pas.get( dev );
                try {
                    pa.join();
                    if ( 0 < pa.getResponseCount() ) {
                        callListeners( dev );
                    }
                } catch ( InterruptedException ex ) {
                    Assert.failDbg();
                }
            }

            clearInstance( this );
        }
    }

    private static class PacketAccumulator extends Thread {

        private static class MsgElem {
            BTCmd mCmd;
            String mMsgID;
            int mGameID;
            long mStamp;
            byte[] mData;
            int mLocalID;

            MsgElem( BTCmd cmd, int gameID, String msgID, OutputPair op )
            {
                mCmd = cmd;
                mMsgID = msgID;
                mGameID = gameID;
                mStamp = System.currentTimeMillis();

                OutputPair tmpOp = new OutputPair();
                try {
                    tmpOp.dos.writeByte( cmd.ordinal() );
                    byte[] data = op.bos.toByteArray();
                    if ( IS_BATCH_PROTO() ) {
                        tmpOp.dos.writeShort( data.length );
                    }
                    tmpOp.dos.write( data, 0, data.length );
                    mData = tmpOp.bos.toByteArray();
                } catch (IOException ioe ) {
                    // With memory-backed IO this should be impossible
                    Log.e( TAG, "MsgElem.__init(): got ioe!: %s",
                           ioe.getMessage() );
                }
            }

            void setLocalID( int id ) { mLocalID = id; }

            boolean isSameAs( MsgElem other )
            {
                boolean result = mCmd == other.mCmd
                    && mGameID == other.mGameID
                    && Arrays.equals( mData, other.mData );
                if ( result ) {
                    if ( null != mMsgID && !mMsgID.equals( other.mMsgID ) ) {
                        Log.d( TAG, "hmmm: identical but msgIDs differ: new %s vs old %s",
                               mMsgID, other.mMsgID );
                        // new 0:0 vs old 2:0 is ok!! since 0: is replaced by
                        // 2 or more when device becomes a client
                        // Assert.assertFalse( BuildConfig.DEBUG ); // fired!!!
                    }
                }
                return result;
            }

            int size() { return mData.length; }
            @Override
            public String toString()
            {
                return String.format("{cmd: %s, msgID: %s}", mCmd, mMsgID );
            }
        }

        final private String mAddr;
        private String mName;
        private List<MsgElem> mElems;
        private long mLastFailTime;
        private int mFailCount;
        private int mLength;
        private int mCounter;
        private long mDieTimeMS = Long.MAX_VALUE;
        private int mResponseCount;
        private int mTimeoutMS;
        private volatile boolean mExitWhenEmpty = false;
        private BluetoothAdapter mAdapter;
        private BTHelper mHelper;

        PacketAccumulator( String addr ) { this(addr, 20000); }

        PacketAccumulator( String addr, int timeoutMS )
        {
            Assert.assertTrue( !TextUtils.isEmpty(addr) );
            mAddr = addr;
            mName = getName( addr );
            mElems = new ArrayList<>();
            mFailCount = 0;
            mLength = 0;
            mTimeoutMS = timeoutMS;
            mAdapter = getAdapterIf();
            Assert.assertTrueNR( null != mAdapter );
            mHelper = new BTHelper( mName, mAddr );
            start();
        }

        private String getName( String addr )
        {
            Assert.assertTrue( !TextUtils.isEmpty(addr) );
            Assert.assertFalse( BOGUS_MARSHMALLOW_ADDR.equals( addr ) );
            String result = "<unknown>";
            Set<BluetoothDevice> devs = getCandidates();
            for ( BluetoothDevice dev : devs ) {
                String devAddr = dev.getAddress();
                Assert.assertFalse( BOGUS_MARSHMALLOW_ADDR.equals( devAddr ) );
                if ( devAddr.equals( addr ) ) {
                    result = dev.getName();
                    break;
                }
            }
            Log.d( TAG, "getName('%s') => %s", addr, result );
            return result;
        }

        synchronized PacketAccumulator wake()
        {
            notifyAll();
            return this;
        }
        
        // synchronized PacketAccumulator setService( BTService service )
        // {
        //     Assert.assertNotNull( service );
        //     mService = service;

        //     notifyAll();

        //     return this;
        // }

        PacketAccumulator setExitWhenEmpty()
        {
            mExitWhenEmpty = true;
            return this;
        }

        PacketAccumulator setLifetimeMS( long msToLive )
        {
            mDieTimeMS = System.currentTimeMillis() + msToLive;
            return this;
        }

        int getResponseCount()
        {
            return mResponseCount;
        }

        void addInvite( NetLaunchInfo nli )
        {
            try {
                OutputPair op = new OutputPair();
                if ( IS_BATCH_PROTO() ) {
                    byte[] nliData = XwJNI.nliToStream( nli );
                    op.dos.writeShort( nliData.length );
                    op.dos.write( nliData, 0, nliData.length );
                } else {
                    op.dos.writeUTF( nli.toString() );
                }
                append( BTCmd.INVITE, op );
            } catch ( IOException ioe ) {
                Assert.failDbg();
            }
        }

        void addMsg( int gameID, byte[] buf, String msgID )
        {
            try {
                OutputPair op = new OutputPair();
                op.dos.writeInt( gameID );
                op.dos.writeShort( buf.length );
                op.dos.write( buf, 0, buf.length );
                append( BTCmd.MESG_SEND, gameID, msgID, op );
            } catch ( IOException ioe ) {
                Assert.failDbg();
            }
        }

        PacketAccumulator addPing( int gameID )
        {
            try {
                OutputPair op = new OutputPair();
                op.dos.writeInt( gameID );
                append( BTCmd.PING, gameID, op );
            } catch ( IOException ioe ) {
                Assert.failDbg();
            }
            return this;
        }

        void addDied( int gameID )
        {
            try {
                OutputPair op = new OutputPair();
                op.dos.writeInt( gameID );
                append( BTCmd.MESG_GAMEGONE, gameID, op );
            } catch ( IOException ioe ) {
                Assert.failDbg();
            }
        }

        private void append( BTCmd cmd, OutputPair op ) throws IOException
        {
            append( cmd, 0, null, op );
        }

        private void append( BTCmd cmd, int gameID, OutputPair op ) throws IOException
        {
            append( cmd, gameID, null, op );
        }

        private boolean append( BTCmd cmd, int gameID, String msgID,
                                OutputPair op ) throws IOException
        {
            boolean haveSpace;
            try ( DeadlockWatch dw = new DeadlockWatch( this ) ) {
                synchronized ( this ) {
                    MsgElem newElem = new MsgElem( cmd, gameID, msgID, op );
                    haveSpace = mLength + newElem.size() < MAX_PACKET_LEN;
                    if ( haveSpace ) {
                        // Let's check for duplicates....
                        boolean dupFound = false;
                        for ( MsgElem elem : mElems ) {
                            if ( elem.isSameAs( newElem ) ) {
                                dupFound = true;
                                break;
                            }
                        }

                        if ( dupFound ) {
                            Log.d( TAG, "append(): dropping dupe: %s", newElem );
                        } else {
                            newElem.setLocalID( mCounter++ );
                            mElems.add( newElem );
                            mLength += newElem.size();
                        }
                        // for now, we restart timer on new data, even if a dupe
                        mFailCount = 0;
                        notifyAll();
                    }
                }
            }
            // Log.d( TAG, "append(%s): now %s", cmd, this );
            return haveSpace;
        }

        private void unappend( int nToRemove )
        {
            Assert.assertTrue( nToRemove <= mElems.size() );
            try ( DeadlockWatch dw = new DeadlockWatch( this ) ) {
                synchronized ( this ) {
                    for ( int ii = 0; ii < nToRemove; ++ii ) {
                        MsgElem elem = mElems.remove(0);
                        mLength -= elem.size();
                    }
                    Log.d( TAG, "unappend(): after removing %d, have %d left for size %d",
                           nToRemove, mElems.size(), mLength );

                    resetBackoff(); // we were successful sending, so should retry immediately
                }
            }
        }

        void resetBackoff()
        {
            // Log.d( TAG, "resetBackoff() IN" );
            try ( DeadlockWatch dw = new DeadlockWatch( this ) ) {
                synchronized ( this ) {
                    mFailCount = 0;
                }
            }
            // Log.d( TAG, "resetBackoff() OUT" );
        }

        @Override
        public void run()
        {
            Log.d( TAG, "PacketAccumulator.run() starting for %s", this );
            // Run as long as I have something to send. Sleep for as long as
            // appropriate based on backoff logic, and be awakened when
            // something new comes in or there's reason to hope a send try
            // will succeed.
            while ( BTEnabled() ) {
                synchronized ( this ) {
                    if ( mExitWhenEmpty && 0 == mElems.size() ) {
                        break;
                    } else if ( System.currentTimeMillis() >= mDieTimeMS ) {
                        break;
                    }

                    long waitTimeMS = figureWait();
                    if ( waitTimeMS > 0 ) {
                        Log.d( TAG, "%s: waiting %dms", this, waitTimeMS );
                        try {
                            wait( waitTimeMS );
                            Log.d( TAG, "%s: done waiting", this );
                            continue; // restart in case state's changed
                        } catch ( InterruptedException ie ) {
                            Log.d( TAG, "ie inside wait: %s", ie.getMessage() );
                        }
                    }
                }
                mResponseCount += trySend();
            }
            Log.d( TAG, "PacketAccumulator.run finishing for %s" 
                   + " after sending %d packets", this, mResponseCount );

            // A hack: mExitWhenEmpty only set in the ping case
            if ( !mExitWhenEmpty ) {
                removeSenderFor( this );
            }
        }

        String getBTAddr() { return mAddr; }
        String getBTName() { return mName; }

        private long figureWait()
        {
            long result = Long.MAX_VALUE;

            try ( DeadlockWatch dw = new DeadlockWatch( this ) ) {
                synchronized ( this ) {
                    if ( 0 < mElems.size() ) { // something to send
                        if ( 0 == mFailCount ) {
                            result = 0;
                        } else {
                            // If we're failing, use a backoff.
                            long wait = 1000 * (long)Math.pow( mFailCount, 2 );
                            result = wait - (System.currentTimeMillis() - mLastFailTime);
                        }
                    }
                }
            }
            Log.d( TAG, "%s.figureWait() => %dms", this, result );
            return result;
        }

        private int trySend()
        {
            int nDone = 0;
            BluetoothSocket socket = null;
            try {
                Log.d( TAG, "trySend(): attempting to connect to %s", mName );
                socket = mAdapter.getRemoteDevice( getBTAddr() )
                    .createRfcommSocketToServiceRecord( sUUID );
                DataOutputStream dos = connect( socket, mTimeoutMS );
                if ( null == dos ) {
                    setNoHost();
                    updateStatusOut( false );
                } else {
                    Log.d( TAG, "PacketAccumulator.run(): connect(%s) => %s",
                           getBTName(), dos );
                    nDone += writeAndCheck( socket, dos );
                    updateStatusOut( true );
                }
            } catch ( IOException ioe ) {
                Log.e( TAG, "PacketAccumulator.run(): ioe: %s",
                       ioe.getMessage() );
            } finally {
                if ( null != socket ) {
                    try { socket.close(); }
                    catch (Exception ex) {}
                }
            }
            return nDone;
        }

        private int writeAndCheck( BluetoothSocket socket, DataOutputStream dos )
            throws IOException
        {
            Log.d( TAG, "%s.writeAndCheck() IN", this );
            Assert.assertNotNull( dos );

            List<MsgElem> localElems = new ArrayList<>();
            try ( DeadlockWatch dw = new DeadlockWatch( this ) ) {
                synchronized ( this ) {
                    if ( 0 < mLength ) {
                        try {
                            // Format is <proto><len-of-rest><msgCount><msg1>..<msgN> To
                            // insert len-of-rest at the beginning we have to create a
                            // tmp byte array then append it after writing its length.

                            OutputPair tmpOP = new OutputPair();
                            int msgCount = IS_BATCH_PROTO() ? mElems.size() : 1;
                            if ( IS_BATCH_PROTO() ) {
                                tmpOP.dos.writeByte( msgCount );
                            }

                            for ( int ii = 0; ii < msgCount; ++ii ) {
                                MsgElem elem = mElems.get(ii);
                                byte[] elemData = elem.mData;
                                tmpOP.dos.write( elemData, 0, elemData.length );
                                localElems.add( elem );
                            }
                            byte[] data = tmpOP.bos.toByteArray();

                            // now write to the socket. Note that connect()
                            // writes BT_PROTO as the first byte.
                            if ( IS_BATCH_PROTO() ) {
                                dos.writeShort( data.length );
                            }
                            dos.write( data, 0, data.length );
                            dos.flush();
                            Log.d( TAG, "writeAndCheck(): wrote %d msgs as"
                                   + " %d-byte payload with sum %s (for %s)",
                                   msgCount, data.length, Utils.getMD5SumFor( data ),
                                   this );
                        } catch ( IOException ioe ) {
                            Log.e( TAG, "writeAndCheck(): ioe: %s", ioe.getMessage() );
                            localElems = null;
                        }
                    }
                } // synchronized
            }

            int nDone = 0;
            if ( null != localElems ) {
                Log.d( TAG, "writeAndCheck(): reading %d replies", localElems.size() );
                try ( KillerIn ki = new KillerIn( socket, 30 ) ) {
                    DataInputStream inStream =
                        new DataInputStream( socket.getInputStream() );
                    for ( int ii = 0; ii < localElems.size(); ++ii ) {
                        MsgElem elem = localElems.get(ii);
                        BTCmd cmd = elem.mCmd;
                        int gameID = elem.mGameID;
                        byte cmdOrd = inStream.readByte();
                        if ( cmdOrd >= BTCmd.values().length ) {
                            break; // SNAFU!!!
                        }
                        BTCmd reply = BTCmd.values()[cmdOrd];
                        Log.d( TAG, "writeAndCheck() %s: got response %s to cmd[%d] %s",
                               this, reply, ii, cmd );

                        if ( reply == BTCmd.BAD_PROTO ) {
                            mHelper.postEvent( MultiEvent.BAD_PROTO_BT,
                                               socket.getRemoteDevice().getName() );
                        } else {
                            handleReply( inStream, cmd, gameID, reply );
                        }
                        ++nDone;
                    }
                } catch ( IOException ioe ) {
                    Log.d( TAG, "failed reading replies for %s: %s", this, ioe.getMessage() );
                }
            }
            unappend( nDone );
            Log.d( TAG, "writeAndCheck() => %d", nDone );
            if ( nDone > 0 ) {
                updateStatusOut( true );
            }
            return nDone;
        }

        private void handleReply( DataInputStream inStream, BTCmd cmd, int gameID,
                                  BTCmd reply ) throws IOException
        {
            MultiEvent evt = null;
            switch ( cmd ) {
            case MESG_SEND:
            case MESG_GAMEGONE:
                switch ( reply ) {
                case MESG_ACCPT:
                    evt = MultiEvent.MESSAGE_ACCEPTED;
                    break;
                case MESG_GAMEGONE:
                    evt = MultiEvent.MESSAGE_NOGAME;
                    break;
                }
                break;
                    
            case INVITE:
                switch ( reply ) {
                case INVITE_ACCPT:
                    mHelper.postEvent( MultiEvent.NEWGAME_SUCCESS, gameID );
                    break;
                case INVITE_DUPID:
                    mHelper.postEvent( MultiEvent.NEWGAME_DUP_REJECTED, mName );
                    break;
                default:
                    mHelper.postEvent( MultiEvent.NEWGAME_FAILURE, gameID );
                    break;
                }
                break;
            case PING:
                if ( BTCmd.PONG == reply && inStream.readBoolean() ) {
                    mHelper.postEvent( MultiEvent.MESSAGE_NOGAME, gameID );
                }
                break;

            default:
                Log.e( TAG, "handleReply(cmd=%s) case not handled", cmd );
                Assert.failDbg(); // fired
            }

            if ( null != evt ) {
                mHelper.postEvent( evt, gameID, 0, mName );
            }
        }

        private DataOutputStream connect( BluetoothSocket socket, int timeout )
        {
            BluetoothDevice remote = socket.getRemoteDevice();
            String name = remote.getName();
            String addr = remote.getAddress();
            Log.w( TAG, "connect(%s/%s, timeout=%d) starting", name, addr, timeout );
            // DbgUtils.logf( "connecting to %s to send cmd %s", name, cmd.toString() );
            // Docs say always call cancelDiscovery before trying to connect
            mAdapter.cancelDiscovery();

            DataOutputStream dos = null;

            // Retry for some time. Some devices take a long time to generate and
            // broadcast ACL conn ACTION
            int nTries = 0;
            for ( long end = timeout + System.currentTimeMillis(); ; ) {
                try {
                    // Log.d( TAG, "trying connect(%s/%s) (check accept() logs)", name, addr );
                    ++nTries;
                    socket.connect();
                    Log.i( TAG, "connect(%s/%s) succeeded after %d tries",
                           name, addr, nTries );
                    dos = new DataOutputStream( socket.getOutputStream() );
                    dos.writeByte( BT_PROTO );
                    break;          // success!!!
                } catch (IOException|SecurityException ioe) {
                    // Log.d( TAG, "connect(): %s", ioe.getMessage() );
                    long msLeft = end - System.currentTimeMillis();
                    if ( msLeft <= 0 ) {
                        break;
                    }
                    try {
                        Thread.sleep( Math.min( CONNECT_SLEEP_MS, msLeft ) );
                    } catch ( InterruptedException ex ) {
                        break;
                    }
                }
            }
            Log.e( TAG, "connect(%s/%s) => %s", name, addr, dos );
            return dos;
        }

        // private long figureWait()
        // {
        //     long waitFromNow;
        //     try ( DeadlockWatch dw = new DeadlockWatch( this ) ) {
        //         synchronized ( this ) {
        //             if ( 0 == mElems.size() ) { // nothing to send
        //                 waitFromNow = Long.MAX_VALUE;
        //             } else if ( 0 == mFailCount ) {
        //                 waitFromNow = 0;
        //             } else {
        //                 // If we're failing, use a backoff.
        //                 long wait = 1000 * (long)Math.pow( mFailCount, 2 );
        //                 waitFromNow = wait - (System.currentTimeMillis() - mLastFailTime);
        //             }
        //         }
        //     }
        //     Log.d( TAG, "%s.figureWait() => %dms", this, waitFromNow );
        //     return waitFromNow;
        // }

        void setNoHost()
        {
            try ( DeadlockWatch dw = new DeadlockWatch( this ) ) {
                synchronized ( this ) {
                    mLastFailTime = System.currentTimeMillis();
                    ++mFailCount;
                }
            }
        }

        @Override
        public synchronized String toString()
        {
            StringBuilder sb = new StringBuilder("{")
                .append("name: ").append( mName )
                .append( ", addr: ").append( mAddr )
                .append( ", failCount: " ).append( mFailCount )
                .append( ", len: " ).append( mLength )
                ;

            if ( 0 < mElems.size() ) {
                long age = System.currentTimeMillis() - mElems.get(0).mStamp;
                int lowID = mElems.get(0).mLocalID;
                int highID = mElems.get(mElems.size() - 1).mLocalID;
                List<BTCmd> cmds = new ArrayList<>();
                for ( MsgElem elem : mElems ) {
                    cmds.add( elem.mCmd );
                }
                sb.append( ", age: " ).append( age )
                    .append( ", ids: ").append(lowID).append('-').append(highID)
                    .append( ", cmds: " ).append( TextUtils.join(",", cmds) )
                    ;
            }

            return sb.append('}').toString();
        }
    }

    private static class OutputPair {
        ByteArrayOutputStream bos;
        DataOutputStream dos;
        OutputPair() {
            bos = new ByteArrayOutputStream();
            dos = new DataOutputStream( bos );
        }

        int length() { return bos.toByteArray().length; }
    }

    private static Map<String, String> s_namesToAddrs;
    private static String getSafeAddr( CommsAddrRec addr )
    {
        String btAddr = addr.bt_btAddr;
        if ( TextUtils.isEmpty(btAddr) || BOGUS_MARSHMALLOW_ADDR.equals( btAddr ) ) {
            final String original = btAddr;
            String btName = addr.bt_hostName;
            if ( null == s_namesToAddrs ) {
                s_namesToAddrs = new HashMap<>();
            }

            if ( s_namesToAddrs.containsKey( btName ) ) {
                btAddr = s_namesToAddrs.get( btName );
            } else {
                btAddr = null;
            }
            if ( null == btAddr ) {
                Set<BluetoothDevice> devs = getCandidates();
                for ( BluetoothDevice dev : devs ) {
                    // Log.d( TAG, "%s => %s", dev.getName(), dev.getAddress() );
                    if ( btName.equals( dev.getName() ) ) {
                        btAddr = dev.getAddress();
                        s_namesToAddrs.put( btName, btAddr );
                        break;
                    }
                }
            }
            Log.d( TAG, "getSafeAddr(\"%s\") => %s", original, btAddr );
        }
        return btAddr;
    }

    private static class ListenThread extends Thread {
        private static ListenThread[] sInstance = {null};
        private BluetoothAdapter mAdapter;

        private ListenThread( BluetoothAdapter adapter )
        {
            mAdapter = adapter;
        }

        @Override
        public void run()
        {
            Log.d( TAG, "ListenThread: %s.run() starting", this );

            BluetoothServerSocket serverSocket;
            try {
                serverSocket = mAdapter
                    .listenUsingRfcommWithServiceRecord( sAppName, sUUID );
            } catch ( IOException ioe ) {
                Log.ex( TAG, ioe );
                serverSocket = null;
            } catch ( SecurityException ex ) {
                // Got this with a message saying not allowed to call
                // listenUsingRfcommWithServiceRecord() in background (on
                // Android 9)
                sBackUser.set( true ); // two-user system: disable BT
                Log.d( TAG, "set sBackUser; outta here (first case)" );
                serverSocket = null;
            }

            while ( null != serverSocket ) {
                Log.d( TAG, "%s.run(): calling accept()", this );
                try {
                    BluetoothSocket socket = serverSocket.accept(); // blocks
                    Log.d( TAG, "%s.run(): accept() returned", this );
                    ReadThread.read( socket );
                } catch ( IOException ioe ) {
                    Log.ex( TAG, ioe );
                    break;
                }
            }

            clearInstance( this );
            Log.d( TAG, "ListenThread: %s.run() exiting", this );
        }

        private static ListenThread getOrStart()
        {
            ListenThread result = null;
            BluetoothAdapter adapter = getAdapterIf();
            if ( null != adapter ) {
                synchronized (sInstance) {
                    result = sInstance[0];
                    if ( null == result ) {
                        sInstance[0] = result = new ListenThread( adapter );
                        result.start();
                    }
                }
            }
            return result;
        }

        private static void clearInstance( ListenThread instance )
        {
            synchronized (sInstance) {
                if ( sInstance[0] == null ) {
                    // nothing to do
                } else if ( sInstance[0] == instance ) {
                    sInstance[0] = null;
                } else {
                    Log.e( TAG, "clearInstance(): cur instance %s not == %s",
                           sInstance[0], instance );
                }
            }
        }
    }

    private static class ReadThread extends Thread {
        private static ReadThread[] sInstance = {null};
        private LinkedBlockingQueue<BluetoothSocket> mQueue;
        private BTMsgSink mBTMsgSink;

        private ReadThread()
        {
            mQueue = new LinkedBlockingQueue<>();
            mBTMsgSink = new BTMsgSink();
        }

        @Override
        public void run()
        {
            Log.d( TAG, "ReadThread: %s.run() starting", this );
            for ( ; ; ) {
                try {
                    BluetoothSocket socket = mQueue.take();
                    DataInputStream inStream =
                        new DataInputStream( socket.getInputStream() );
                    byte proto = inStream.readByte();
                    if ( proto == BT_PROTO_BATCH || proto == BT_PROTO_JSONS ) {
                        BTInviteDelegate.onHeardFromDev( socket.getRemoteDevice() );
                        parsePacket( proto, inStream, socket );
                        updateStatusIn( true );
                        TimerReceiver.restartBackoff( XWApp.getContext() );
                        // nBadCount = 0;
                    } else {
                        writeBack( socket, BTCmd.BAD_PROTO );
                    }
                    Log.d( TAG, "%s.run(): closing %s", this, socket );
                    socket.close();
                } catch ( InterruptedException ie ) {
                    break;
                } catch ( IOException ioe ) {
                    Log.ex( TAG, ioe );
                }
            }
            clearInstance( this );
            Log.d( TAG, "ReadThread: %s.run() exiting", this );
        }

        private void writeBack( BluetoothSocket socket, BTCmd cmd )
        {
            try {
                DataOutputStream os = new DataOutputStream( socket.getOutputStream() );
                os.writeByte( cmd.ordinal() );
                os.flush();
            } catch ( IOException ex ) {
                Log.ex( TAG, ex );
            }
            Log.d( TAG, "writeBack(%s) DONE", cmd );
        }

        private void parsePacket( byte proto, DataInputStream inStream,
                                  BluetoothSocket socket ) throws IOException
        {
            Log.d( TAG, "parsePacket(socket=%s, proto=%d)", socket, proto );
            boolean isOldProto = proto == BT_PROTO_JSONS;
            short inLen = isOldProto
                ? (short)inStream.available() : inStream.readShort();
            if ( inLen >= MAX_PACKET_LEN ) {
                Log.e( TAG, "packet too big; dropping!!!" );
                Assert.failDbg();
            } else if ( 0 < inLen ) {
                byte[] data = new byte[inLen];
                inStream.readFully( data );

                ByteArrayInputStream bis = new ByteArrayInputStream( data );
                DataInputStream dis = new DataInputStream( bis );
                int nMessages = isOldProto ? 1 : dis.readByte();

                Log.d( TAG, "dispatchAll(): read %d-byte payload with sum %s containing %d messages",
                       data.length, Utils.getMD5SumFor( data ), nMessages );

                for ( int ii = 0; ii < nMessages; ++ii ) {
                    byte cmdOrd = dis.readByte();
                    short oneLen = isOldProto ? 0 : dis.readShort(); // used only to skip
                    int availableBefore = dis.available();
                    if ( cmdOrd < BTCmd.values().length ) {
                        BTCmd cmd = BTCmd.values()[cmdOrd];
                        Log.d( TAG, "parsePacket(): reading msg %d: %s", ii, cmd );
                        switch ( cmd ) {
                        case PING:
                            int gameID = dis.readInt();
                            receivePing( gameID, socket );
                            break;
                        case INVITE:
                            NetLaunchInfo nli;
                            if ( isOldProto ) {
                                nli = NetLaunchInfo.makeFrom( XWApp.getContext(),
                                                              dis.readUTF() );
                            } else {
                                data = new byte[dis.readShort()];
                                dis.readFully( data );
                                nli = XwJNI.nliFromStream( data );
                            }
                            receiveInvitation( nli, socket );
                            break;
                        case MESG_SEND:
                            gameID = dis.readInt();
                            data = new byte[dis.readShort()];
                            dis.readFully( data );
                            receiveMessage( gameID, data, socket );
                            break;
                        case MESG_GAMEGONE:
                            gameID = dis.readInt();
                            receiveGameGone( gameID, socket );
                            break;
                        default:
                            Assert.failDbg();
                            break;
                        }
                    } else {
                        Log.e( TAG, "unexpected command (ord: %d);"
                               + " skipping %d bytes", cmdOrd, oneLen );
                        if ( oneLen <= dis.available() ) {
                            dis.readFully( new byte[oneLen] );
                            Assert.failDbg();
                        }
                    }

                    // sanity-check based on packet length
                    int availableAfter = dis.available();
                    Assert.assertTrue( 0 == oneLen
                                       || oneLen == availableBefore - availableAfter
                                       || !BuildConfig.DEBUG );
                }
            } else {
                Log.e( TAG, "parsePacket(): bad packet? len == 0" );
            }
        }

        private void receivePing( int gameID, BluetoothSocket socket )
            throws IOException
        {
            Log.d( TAG, "receivePing()" );
            boolean deleted = 0 != gameID
                && !DBUtils.haveGame( XWApp.getContext(), gameID );

            DataOutputStream os = new DataOutputStream( socket.getOutputStream() );
            os.writeByte( BTCmd.PONG.ordinal() );
            os.writeBoolean( deleted );
            os.flush();
        }

        private void receiveInvitation( NetLaunchInfo nli, BluetoothSocket socket )
        {
            BluetoothDevice host = socket.getRemoteDevice();
            BTCmd response = makeOrNotify( nli, host.getName(), host.getAddress() );
            Log.d( TAG, "receiveInvitation() => %s", response );
            writeBack( socket, response );
        }

        private BTCmd makeOrNotify( NetLaunchInfo nli, String btName, String btAddr )
        {
            BTCmd result;
            BTHelper helper = new BTHelper( btName, btAddr );
            if ( helper.handleInvitation( nli, btName, DictFetchOwner.OWNER_BT ) ) {
                result = BTCmd.INVITE_ACCPT;
            } else {
                result = BTCmd.INVITE_DUP_INVITE; // dupe of rematch
            }
            return result;
        }

        private void receiveMessage( int gameID, byte[] buf, BluetoothSocket socket )
        {
            BTHelper helper = new BTHelper( socket );
            XWServiceHelper.ReceiveResult rslt
                = helper.receiveMessage( gameID, mBTMsgSink, buf, helper.getAddr() );

            BTCmd response = rslt == XWServiceHelper.ReceiveResult.GAME_GONE ?
                BTCmd.MESG_GAMEGONE : BTCmd.MESG_ACCPT;

            writeBack( socket, response );
        }

        private void receiveGameGone( int gameID, BluetoothSocket socket )
        {
            BTHelper helper = new BTHelper( socket );
            helper.postEvent( MultiEvent.MESSAGE_NOGAME, gameID );
            writeBack( socket, BTCmd.MESG_ACCPT );
        }

        private void enqueue( BluetoothSocket socket )
        {
            mQueue.add( socket );
        }

        static void read( BluetoothSocket incoming )
        {
            Log.d( TAG, "read(%s)", incoming );
            ReadThread self = getOrStart();
            self.enqueue( incoming );
        }

        private static ReadThread getOrStart()
        {
            ReadThread result;
            synchronized (sInstance) {
                result = sInstance[0];
                if ( null == result ) {
                    sInstance[0] = result = new ReadThread();
                    result.start();
                }
            }
            return result;
        }

        private static void clearInstance( ReadThread instance )
        {
            synchronized (sInstance) {
                if ( sInstance[0] == null ) {
                    // nothing to do
                } else if ( sInstance[0] == instance ) {
                    sInstance[0] = null;
                } else {
                    Log.e( TAG, "clearInstance(): cur instance %s not == %s",
                           sInstance[0], instance );
                }
            }
        }
    }

    private static class BTMsgSink extends MultiMsgSink {
        public BTMsgSink() { super( XWApp.getContext() ); }

        @Override
        public int sendViaBluetooth( byte[] buf, String msgID, int gameID,
                                     CommsAddrRec addr )
        {
            int nSent = -1;
            String btAddr = getSafeAddr( addr );
            if ( null != btAddr && 0 < btAddr.length() ) {
                getPA( btAddr ).addMsg( gameID, buf, msgID );
                nSent = buf.length;
            } else {
                Log.i( TAG, "sendViaBluetooth(): no addr for dev %s",
                       addr.bt_hostName );
            }
            return nSent;
        }
    }

    private static class BTHelper extends XWServiceHelper {
        private CommsAddrRec mReturnAddr;
        private Context mContext;

        private BTHelper() { super( XWApp.getContext() ); }

        BTHelper( CommsAddrRec from )
        {
            this();
            init( from );
        }

        BTHelper( String fromName, String fromAddr )
        {
            this();
            init( new CommsAddrRec( fromName, fromAddr ) );
        }

        BTHelper( BluetoothSocket socket )
        {
            this();
            BluetoothDevice host = socket.getRemoteDevice();
            init( new CommsAddrRec( host.getName(), host.getAddress() ) );
        }

        CommsAddrRec getAddr() { return mReturnAddr; }

        private void init( CommsAddrRec addr )
        {
            mReturnAddr = addr;
        }

        @Override
        void postNotification( String device, int gameID, long rowid )
        {
            Context context = XWApp.getContext();
            String body = LocUtils.getString( context, R.string.new_game_body );
            GameUtils.postInvitedNotification( context, gameID, body, rowid );
        }

        private void receiveMessage( long rowid, MultiMsgSink sink, byte[] msg )
        {
            Log.d( TAG, "receiveMessage(rowid=%d, len=%d)", rowid, msg.length );
            receiveMessage( rowid, sink, msg, mReturnAddr );
        }
    }

    private static class KillerIn extends Thread implements AutoCloseable {
        private int mSeconds;
        private java.io.Closeable mSocket;

        KillerIn( final java.io.Closeable socket, final int seconds )
        {
            mSeconds = seconds;
            mSocket = socket;
            start();
        }

        @Override
        public void run()
        {
            try {
                Thread.sleep( 1000 * mSeconds );
                Log.d( TAG, "KillerIn(): time's up; closing socket" );
                mSocket.close();
            } catch ( InterruptedException ie ) {
                // Log.d( TAG, "KillerIn: killed by owner" );
            } catch( IOException ioe ) {
                Log.ex( TAG, ioe );
            }
        }

        @Override
        public void close() { interrupt(); }
    }
}
