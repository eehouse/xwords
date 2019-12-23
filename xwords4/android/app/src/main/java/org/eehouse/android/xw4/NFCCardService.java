/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2019 by Eric House (xwords@eehouse.org).  All rights reserved.
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
import android.content.Context;
import android.nfc.NfcAdapter;
import android.nfc.Tag;
import android.nfc.cardemulation.HostApduService;
import android.nfc.tech.IsoDep;
import android.os.Bundle;
import android.text.TextUtils;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.math.BigInteger;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.Set;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.atomic.AtomicInteger;

import org.eehouse.android.xw4.NFCUtils.MsgToken;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;

public class NFCCardService extends HostApduService {
    private static final String TAG = NFCCardService.class.getSimpleName();
    private static final boolean USE_BIGINTEGER = true;
    private static final int LEN_OFFSET = 4;
    private static final byte VERSION_1 = (byte)0x01;

    private int mMyDevID;

    private static enum HEX_STR {
        DEFAULT_CLA( "00" )
        , SELECT_INS( "A4" )
        , STATUS_FAILED( "6F00" )
        , CLA_NOT_SUPPORTED( "6E00" )
        , INS_NOT_SUPPORTED( "6D00" )
        , STATUS_SUCCESS( "9000" )
        , CMD_MSG_PART( "70FC" )
        ;

        private byte[] mBytes;
        private HEX_STR( String hex ) { mBytes = Utils.hexStr2ba(hex); }
        private byte[] asBA() { return mBytes; }
        private boolean matchesFrom( byte[] src )
        {
            return matchesFrom( src, 0 );
        }
        private boolean matchesFrom( byte[] src, int offset )
        {
            boolean result = offset + mBytes.length <= src.length;
            for ( int ii = 0; result && ii < mBytes.length; ++ii ) {
                result = src[offset + ii] == mBytes[ii];
            }
            // Log.d( TAG, "%s.matchesFrom(%s) => %b", this, src, result );
            return result;
        }
        int length() { return asBA().length; }
    }

    private static int sNextMsgID = 0;
    private static synchronized int getNextMsgID()
    {
        return ++sNextMsgID;
    }

    private static byte[] numTo( int num )
    {
        byte[] result;
        if ( USE_BIGINTEGER ) {
            BigInteger bi = BigInteger.valueOf( num );
            byte[] bibytes = bi.toByteArray();
            result = new byte[1 + bibytes.length];
            result[0] = (byte)bibytes.length;
            System.arraycopy( bibytes, 0, result, 1, bibytes.length );
        } else {
            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            DataOutputStream dos = new DataOutputStream( baos );
            try {
                dos.writeInt( num );
                dos.flush();
            } catch ( IOException ioe ) {
                Assert.assertFalse( BuildConfig.DEBUG );
            }
            result = baos.toByteArray();
        }
        // Log.d( TAG, "numTo(%d) => %s", num, DbgUtils.hexDump(result) );
        return result;
    }

    private static int numFrom( ByteArrayInputStream bais ) throws IOException
    {
        int biLen = bais.read();
        // Log.d( TAG, "numFrom(): read biLen: %d", biLen );
        byte[] bytes = new byte[biLen];
        bais.read( bytes );
        BigInteger bi = new BigInteger( bytes );
        int result = bi.intValue();

        // Log.d( TAG, "numFrom() => %d", result );
        return result;
    }

    private static int numFrom( byte[] bytes, int start, int out[] )
    {
        int result;
        if ( USE_BIGINTEGER ) {
            byte biLen = bytes[start];
            byte[] rest = Arrays.copyOfRange( bytes, start + 1, start + 1 + biLen );
            BigInteger bi = new BigInteger(rest);
            out[0] = bi.intValue();
            result = biLen + 1;
        } else {
            ByteArrayInputStream bais = new ByteArrayInputStream( bytes, start,
                                                                  bytes.length - start );
            DataInputStream dis = new DataInputStream( bais );
            try {
                out[0] = dis.readInt();
            } catch ( IOException ioe ) {
                Log.e( TAG, "from readInt(): %s", ioe.getMessage() );
            }
            result = bais.available() - start;
        }
        return result;
    }

    private static void testNumThing()
    {
        Log.d( TAG, "testNumThing() starting" );

        int[] out = {0};
        for ( int ii = 1; ii > 0 && ii < Integer.MAX_VALUE; ii *= 2 ) {
            byte[] tmp = numTo( ii );
            numFrom( tmp, 0, out );
            if ( ii != out[0] ) {
                Log.d( TAG, "testNumThing(): %d failed; got %d", ii, out[0] );
                break;
            } else {
                Log.d( TAG, "testNumThing(): %d ok", ii );
            }
        }
        Log.d( TAG, "testNumThing() DONE" );
    }

    private static class QueueElem {
        Context context;
        byte[] msg;
        QueueElem( Context pContext, byte[] pMsg ) {
            context = pContext;
            msg = pMsg;
        }
    }

    private static LinkedBlockingQueue<QueueElem> sQueue = null;

    private synchronized static void addToMsgThread( Context context, byte[] msg )
    {
        if ( 0 < msg.length ) {
            QueueElem elem = new QueueElem( context, msg );
            if ( null == sQueue ) {
                sQueue = new LinkedBlockingQueue<>();
                new Thread( new Runnable() {
                        @Override
                        public void run() {
                            Log.d( TAG, "addToMsgThread(): run starting" );
                            for ( ; ; ) {
                                try {
                                    QueueElem elem = sQueue.take();
                                    NFCUtils.receiveMsgs( elem.context, elem.msg );
                                    updateStatus( elem.context, true );
                                } catch ( InterruptedException ie ) {
                                    break;
                                }
                            }
                            Log.d( TAG, "addToMsgThread(): run exiting" );
                        }
                    } ).start();
            }
            sQueue.add( elem );
        // } else {
        //     // This is very common right now
        //     Log.d( TAG, "addToMsgThread(): dropping 0-length msg" );
        }
    }

    private static void updateStatus( Context context, boolean in )
    {
        if ( in ) {
            ConnStatusHandler
                .updateStatusIn( context, CommsConnType.COMMS_CONN_NFC, true );
        } else {
            ConnStatusHandler
                .updateStatusOut( context, CommsConnType.COMMS_CONN_NFC, true );
        }
    }

    // Remove this once we don't need logging to confirm stuff's loading
    @Override
    public void onCreate()
    {
        super.onCreate();
        mMyDevID = DevID.getNFCDevID( this );
        Log.d( TAG, "onCreate() got mydevid %d", mMyDevID );
    }

    private int mGameID;

    @Override
    public byte[] processCommandApdu( byte[] apdu, Bundle extras )
    {
        // Log.d( TAG, "processCommandApdu(%s)", DbgUtils.hexDump(apdu ) );

        HEX_STR resStr = HEX_STR.STATUS_FAILED;
        boolean isAidCase = false;

        if ( null != apdu ) {
            if ( HEX_STR.CMD_MSG_PART.matchesFrom( apdu ) ) {
                resStr = HEX_STR.STATUS_SUCCESS;
                int[] msgID = {0};
                byte[] all = reassemble( this, apdu, msgID, HEX_STR.CMD_MSG_PART );
                if ( null != all ) {
                    addToMsgThread( this, all );
                    setLatestAck( msgID[0] );
                }
            } else {
                Log.d( TAG, "processCommandApdu(): aid case?" );
                if ( ! HEX_STR.DEFAULT_CLA.matchesFrom( apdu ) ) {
                    resStr = HEX_STR.CLA_NOT_SUPPORTED;
                } else if ( ! HEX_STR.SELECT_INS.matchesFrom( apdu, 1 ) ) {
                    resStr = HEX_STR.INS_NOT_SUPPORTED;
                } else if ( LEN_OFFSET >= apdu.length ) {
                    Log.d( TAG, "processCommandApdu(): apdu too short" );
                    // Not long enough for length byte
                } else {
                    try {
                        ByteArrayInputStream bais
                            = new ByteArrayInputStream( apdu, LEN_OFFSET,
                                                        apdu.length - LEN_OFFSET );
                        byte aidLen = (byte)bais.read();
                        Log.d( TAG, "aidLen=%d", aidLen );
                        if ( bais.available() >= aidLen + 1 ) {
                            byte[] aidPart = new byte[aidLen];
                            bais.read( aidPart );
                            String aidStr = Utils.ba2HexStr( aidPart );
                            if ( BuildConfig.NFC_AID.equals( aidStr ) ) {
                                byte minVersion = (byte)bais.read();
                                byte maxVersion = (byte)bais.read();
                                if ( minVersion == VERSION_1 ) {
                                    int devID = numFrom( bais );
                                    Log.d( TAG, "processCommandApdu(): read "
                                           + "remote devID: %d", devID );
                                    mGameID = numFrom( bais );
                                    Log.d( TAG, "read gameID: %d", mGameID );
                                    if ( 0 < bais.available() ) {
                                        Log.d( TAG, "processCommandApdu(): "
                                               + "leaving anything behind?" );
                                    }
                                    resStr = HEX_STR.STATUS_SUCCESS;
                                    isAidCase = true;
                                } else {
                                    Log.e( TAG, "unexpected version %d; I'm too old?",
                                           minVersion );
                                }
                            } else {
                                Log.e( TAG, "aid mismatch: got %s but wanted %s",
                                       aidStr, BuildConfig.NFC_AID );
                            }
                        }
                    } catch ( IOException ioe ) {
                        Assert.assertFalse( BuildConfig.DEBUG );
                    }
                }
            }
        }

        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        try {
            baos.write( resStr.asBA() );
            if ( HEX_STR.STATUS_SUCCESS == resStr ) {
                if ( isAidCase ) {
                    baos.write( VERSION_1 ); // min
                    baos.write( numTo( mMyDevID ) );
                } else {
                    MsgToken token = NFCUtils.getMsgsFor( mGameID );
                    byte[][] tmp = wrapMsg( token, Short.MAX_VALUE );
                    Assert.assertTrue( 1 == tmp.length || !BuildConfig.DEBUG );
                    baos.write( tmp[0] );
                }
            }
        } catch ( IOException ioe ) {
            Assert.assertFalse( BuildConfig.DEBUG );
        }
        byte[] result = baos.toByteArray();

        Log.d( TAG, "processCommandApdu(%s) => %s", DbgUtils.hexDump( apdu ),
               DbgUtils.hexDump( result ) );
        // this comes out of transceive() below!!!
        return result;
    } // processCommandApdu

    @Override
    public void onDeactivated( int reason )
    {
        String str = "<other>";
        switch ( reason ) {
        case HostApduService.DEACTIVATION_LINK_LOSS:
            str = "DEACTIVATION_LINK_LOSS";
            break;
        case HostApduService.DEACTIVATION_DESELECTED:
            str = "DEACTIVATION_DESELECTED";
            break;
        }

        Log.d( TAG, "onDeactivated(reason=%s)", str );
    }

    private static Map<Integer, MsgToken> sSentTokens = new HashMap<>();
    private static void removeSentMsgs( Context context, int ack )
    {
        MsgToken msgs = null;
        if ( 0 != ack ) {
            Log.d( TAG, "removeSentMsgs(msgID=%d)", ack );
            synchronized ( sSentTokens ) {
                msgs = sSentTokens.remove( ack );
                Log.d( TAG, "removeSentMsgs(): removed %s, now have %s", msgs, keysFor() );
            }
            updateStatus( context, false );
        }
        if ( null != msgs ) {
            msgs.removeSentMsgs();
        }
    }

    private static void remember( int msgID, MsgToken msgs )
    {
        if ( 0 != msgID ) {
            Log.d( TAG, "remember(msgID=%d)", msgID );
            synchronized ( sSentTokens ) {
                sSentTokens.put( msgID, msgs );
                Log.d( TAG, "remember(): now have %s", keysFor() );
            }
        }
    }

    private static String keysFor()
    {
        String result = "";
        if ( BuildConfig.DEBUG ) {
            result = TextUtils.join( ",", sSentTokens.keySet() );
        }
        return result;
    }

    private static byte[][] sParts = null;
    private static int sMsgID = 0;
    private synchronized static byte[] reassemble( Context context, byte[] part,
                                                   int[] msgIDOut, HEX_STR cmd )
    {
        return reassemble( context, part, msgIDOut, cmd.length() );
    }

    private synchronized static byte[] reassemble( Context context, byte[] part,
                                                   int[] msgIDOut, int offset )
    {
        part = Arrays.copyOfRange( part, offset, part.length );
        return reassemble( context, part, msgIDOut );
    }

    private synchronized static byte[] reassemble( Context context,
                                                   byte[] part, int[] msgIDOut )
    {
        byte[] result = null;
        try {
            ByteArrayInputStream bais = new ByteArrayInputStream( part );

            final int cur = bais.read();
            final int count = bais.read();
            if ( 0 == cur ) {
                sMsgID = numFrom( bais );
                int ack = numFrom( bais );
                removeSentMsgs( context, ack );
            }

            boolean inSequence = true;
            if ( sParts == null ) {
                if ( 0 == cur ) {
                    sParts = new byte[count][];
                } else {
                    Log.e( TAG, "reassemble(): out-of-order message 1" );
                    inSequence = false;
                }
            } else if ( cur >= count || count != sParts.length || null != sParts[cur] ) {
                // result = HEX_STR.STATUS_FAILED;
                inSequence = false;
                Log.e( TAG, "reassemble(): out-of-order message 2" );
            }

            if ( !inSequence ) {
                sParts = null;  // so we can try again later
            } else {
                // write rest into array
                byte[] rest = new byte[bais.available()];
                bais.read( rest, 0, rest.length );
                sParts[cur] = rest;
                // Log.d( TAG, "addOrProcess(): added elem %d: %s", cur, DbgUtils.hexDump( rest ) );

                // Done? Process!!
                if ( cur + 1 == count ) {
                    ByteArrayOutputStream baos = new ByteArrayOutputStream();
                    for ( int ii = 0; ii < sParts.length; ++ii ) {
                        baos.write( sParts[ii] );
                    }
                    sParts = null;

                    result = baos.toByteArray();
                    msgIDOut[0] = sMsgID;
                    if ( 0 != sMsgID ) {
                        Log.d( TAG, "reassemble(): done reassembling msgID=%d: %s",
                               msgIDOut[0], DbgUtils.hexDump(result) );
                    }
                }
            }
        } catch ( IOException ioe ) {
            Assert.assertFalse( BuildConfig.DEBUG );
        }
        return result;
    }

    private static AtomicInteger sLatestAck = new AtomicInteger(0);
    private static int getLatestAck()
    {
        int result = sLatestAck.getAndSet(0);
        if ( 0 != result ) {
            Log.d( TAG, "getLatestAck() => %d", result );
        }
        return result;
    }

    private static void setLatestAck( int ack )
    {
        if ( 0 != ack ) {
            Log.e( TAG, "setLatestAck(%d)", ack );
        }
        int oldVal = sLatestAck.getAndSet( ack );
        if ( 0 != oldVal ) {
            Log.e( TAG, "setLatestAck(%d): dropping ack msgID %d", ack, oldVal );
        }
    }
    
    private static final int HEADER_SIZE = 10;
    private static byte[][] wrapMsg( MsgToken token, int maxLen )
    {
        byte[] msg = token.getMsgs();
        final int length = null == msg ? 0 : msg.length;
        final int msgID = (0 == length) ? 0 : getNextMsgID();
        if ( 0 < msgID ) {
            Log.d( TAG, "wrapMsg(%s); msgID=%d", DbgUtils.hexDump( msg ), msgID );
        }
        final int count = 1 + (length / (maxLen - HEADER_SIZE));
        byte[][] result = new byte[count][];
        try {
            int offset = 0;
            for ( int ii = 0; ii < count; ++ii ) {
                ByteArrayOutputStream baos = new ByteArrayOutputStream();
                baos.write( HEX_STR.CMD_MSG_PART.asBA() );
                baos.write( (byte)ii );
                baos.write( (byte)count );
                if ( 0 == ii ) {
                    baos.write( numTo( msgID ) );
                    int latestAck = getLatestAck();
                    baos.write( numTo( latestAck ) );
                }
                Assert.assertTrue( HEADER_SIZE >= baos.toByteArray().length );

                int thisLen = Math.min( maxLen - HEADER_SIZE, length - offset );
                if ( 0 < thisLen ) {
                    // Log.d( TAG, "writing %d bytes starting from offset %d",
                    //        thisLen, offset );
                    baos.write( msg, offset, thisLen );
                    offset += thisLen;
                }
                byte[] tmp = baos.toByteArray();
                // Log.d( TAG, "wrapMsg(): adding res[%d]: %s", ii, DbgUtils.hexDump(tmp) );
                result[ii] = tmp;
            }
            remember( msgID, token );
        } catch ( IOException ioe ) {
            Assert.assertFalse( BuildConfig.DEBUG );
        }
        return result;
    }

    public static class Wrapper implements NfcAdapter.ReaderCallback,
                                           NFCUtils.HaveDataListener {
        private Activity mActivity;
        private boolean mHaveData;
        private Procs mProcs;
        private NfcAdapter mAdapter;
        private int mMinMS = 300;
        private int mMaxMS = 500;
        private boolean mConnected = false;
        private int mMyDevID;
        
        public interface Procs {
            void onReadingChange( boolean nowReading );
        }

        public static Wrapper init( Activity activity, Procs procs, int devID )
        {
            Wrapper instance = null;
            if ( null != NfcAdapter.getDefaultAdapter( activity ) ) {
                instance = new Wrapper( activity, procs, devID );
            }
            Log.d( TAG, "Wrapper.init(devID=%d) => %s", devID, instance );
            return instance;
        }

        static void setResumed( Wrapper instance, boolean resumed )
        {
            if ( null != instance ) {
                instance.setResumed( resumed );
            }
        }

        static void setGameID( Wrapper instance, int gameID )
        {
            if ( null != instance ) {
                instance.setGameID( gameID );
            }
        }

        private Wrapper( Activity activity, Procs procs, int devID )
        {
            mActivity = activity;
            mProcs = procs;
            mMyDevID = devID;
            mAdapter = NfcAdapter.getDefaultAdapter( activity );
        }

        private void setResumed( boolean resumed )
        {
            if ( resumed ) {
                startReadModeThread();
            } else {
                stopReadModeThread();
            }
        }

        @Override
        public void onHaveDataChanged( boolean haveData )
        {
            if ( mHaveData != haveData ) {
                mHaveData = haveData;
                Log.d( TAG, "onHaveDataChanged(): mHaveData now %b", mHaveData );
                interruptThread();
            }
        }

        private boolean haveData()
        {
            boolean result = mHaveData;
            // Log.d( TAG, "haveData() => %b", result );
            return result;
        }

        private int mGameID;
        private void setGameID( int gameID )
        {
            Log.d( TAG, "setGameID(%d)", gameID );
            mGameID = gameID;
            NFCUtils.setHaveDataListener( gameID, this );
            interruptThread();
        }

        private void interruptThread()
        {
            synchronized ( mThreadRef ) {
                if ( null != mThreadRef[0] ) {
                    mThreadRef[0].interrupt();
                }
            }
        }

        @Override
        public void onTagDiscovered( Tag tag )
        {
            mConnected = true;
            IsoDep isoDep = IsoDep.get( tag );
            try {
                isoDep.connect();
                int maxLen = isoDep.getMaxTransceiveLength();
                Log.d( TAG, "onTagDiscovered() connected; max len: %d", maxLen );
                byte[] aidBytes = Utils.hexStr2ba( BuildConfig.NFC_AID );
                ByteArrayOutputStream baos = new ByteArrayOutputStream();
                baos.write( Utils.hexStr2ba( "00A40400" ) );
                baos.write( (byte)aidBytes.length );
                baos.write( aidBytes );
                baos.write( VERSION_1 ); // min
                baos.write( VERSION_1 ); // max
                baos.write( numTo( mMyDevID ) );
                baos.write( numTo( mGameID ) );
                byte[] msg = baos.toByteArray();
                Assert.assertTrue( msg.length < maxLen || !BuildConfig.DEBUG );
                byte[] response = isoDep.transceive( msg );

                // The first reply from transceive() is special. If it starts
                // with STATUS_SUCCESS then it also includes the version we'll
                // be using to communicate, either what we sent over or
                // something lower (for older code on the other side), and the
                // remote's deviceID
                if ( HEX_STR.STATUS_SUCCESS.matchesFrom( response ) ) {
                    int offset = HEX_STR.STATUS_SUCCESS.length();
                    byte version = response[offset++];
                    if ( version == VERSION_1 ) {
                        int[] out = {0};
                        offset += numFrom( response, offset, out );
                        Log.d( TAG, "onTagDiscovered(): read remote devID: %d",
                               out[0] );
                        runMessageLoop( isoDep, maxLen );
                    } else {
                        Log.e( TAG, "onTagDiscovered(): remote sent version %d, "
                               + "not %d; exiting", version, VERSION_1 );
                    }
                }
                isoDep.close();
            } catch ( IOException ioe ) {
                Log.e( TAG, "got ioe: " + ioe.getMessage() );
            }

            mConnected = false;
            interruptThread();  // make sure we leave read mode!
            Log.d( TAG, "onTagDiscovered() DONE" );
        }

        private void runMessageLoop( IsoDep isoDep, int maxLen ) throws IOException
        {
            outer:
            for ( ; ; ) {
                MsgToken token = NFCUtils.getMsgsFor( mGameID );
                // PENDING: no need for this Math.min thing once well tested
                byte[][] toFit = wrapMsg( token, Math.min( 50, maxLen ) );
                for ( int ii = 0; ii < toFit.length; ++ii ) {
                    byte[] one = toFit[ii];
                    Assert.assertTrue( one.length < maxLen || !BuildConfig.DEBUG );
                    byte[] response = isoDep.transceive( one );
                    if ( ! receiveAny( response ) ) {
                        break outer;
                    }
                }
            }
        }

        private boolean receiveAny( byte[] response )
        {
            boolean statusOK = HEX_STR.STATUS_SUCCESS.matchesFrom( response );
            if ( statusOK ) {
                int offset = HEX_STR.STATUS_SUCCESS.length();
                if ( HEX_STR.CMD_MSG_PART.matchesFrom( response, offset ) ) {
                    int[] msgID = {0};
                    byte[] all = reassemble( mActivity, response, msgID,
                                             offset + HEX_STR.CMD_MSG_PART.length() );
                    if ( null != all ) {
                        addToMsgThread( mActivity, all );
                        setLatestAck( msgID[0] );
                    }
                }
            }
            Log.d( TAG, "receiveAny(%s) => %b", DbgUtils.hexDump( response ), statusOK );
            return statusOK;
        }

        private class ReadModeThread extends Thread {
            private boolean mShouldStop = false;
            private boolean mInReadMode = false;
            private final int mFlags = NfcAdapter.FLAG_READER_NFC_A
                | NfcAdapter.FLAG_READER_SKIP_NDEF_CHECK;

            @Override
            public void run()
            {
                Log.d( TAG, "ReadModeThread.run() starting" );
                Random random = new Random();

                while ( !mShouldStop ) {
                    boolean wantReadMode = mConnected || !mInReadMode && haveData();
                    if ( wantReadMode && !mInReadMode ) {
                        mAdapter.enableReaderMode( mActivity, Wrapper.this, mFlags, null );
                    } else if ( mInReadMode && !wantReadMode ) {
                        mAdapter.disableReaderMode( mActivity );
                    }
                    mInReadMode = wantReadMode;
                    Log.d( TAG, "run(): inReadMode now: %b", mInReadMode );

                    // Now sleep. If we aren't going to want to toggle read
                    // mode soon, sleep until interrupted by a state change,
                    // e.g. getting data or losing connection.
                    long intervalMS = Long.MAX_VALUE;
                    if ( (mInReadMode && !mConnected) || haveData() ) {
                        intervalMS = mMinMS + (Math.abs(random.nextInt())
                                               % (mMaxMS - mMinMS));
                    }
                    try {
                        Thread.sleep( intervalMS );
                    } catch ( InterruptedException ie ) {
                        Log.d( TAG, "run interrupted" );
                    }
                    // toggle();
                    // try {
                    //     // How long to sleep.
                    //     int intervalMS = mMinMS + (Math.abs(mRandom.nextInt())
                    //                                % (mMaxMS - mMinMS));
                    //     // Log.d( TAG, "sleeping for %d ms", intervalMS );
                    //     Thread.sleep( intervalMS );
                    // } catch ( InterruptedException ie ) {
                    //     Log.d( TAG, "run interrupted" );
                    // }
                }

                // Kill read mode on the way out
                if ( mInReadMode ) {
                    mAdapter.disableReaderMode( mActivity );
                    mInReadMode = false;
                }

                // Clear the reference only if it's me
                synchronized ( mThreadRef ) {
                    if ( mThreadRef[0] == this ) {
                        mThreadRef[0] = null;
                    }
                }
                Log.d( TAG, "ReadModeThread.run() exiting" );
            }

            public void doStop()
            {
                mShouldStop = true;
                interrupt();
            }
        }

        private ReadModeThread[] mThreadRef = {null};
        private void startReadModeThread()
        {
            synchronized ( mThreadRef ) {
                if ( null == mThreadRef[0] ) {
                    mThreadRef[0] = new ReadModeThread();
                    mThreadRef[0].start();
                }
            }
        }

        private void stopReadModeThread()
        {
            ReadModeThread thread;
            synchronized ( mThreadRef ) {
                thread = mThreadRef[0];
                mThreadRef[0] = null;
            }

            if ( null != thread ) {
                thread.doStop();
                try {
                    thread.join();
                } catch ( InterruptedException ex ) {
                    Log.d( TAG, "stopReadModeThread(): %s", ex );
                }
            }
        }
    }
}
