/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2013 by Eric House (xwords@eehouse.org).  All rights
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
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.nfc.NfcAdapter;
import android.nfc.NfcEvent;
import android.nfc.NfcManager;
import android.nfc.Tag;
import android.nfc.tech.IsoDep;
import android.os.Build;
import android.os.Parcelable;
import android.text.TextUtils;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.lang.ref.WeakReference;
import java.math.BigInteger;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.atomic.AtomicInteger;

import org.eehouse.android.xw4.MultiService.MultiEvent;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.loc.LocUtils;

public class NFCUtils {
    private static final String TAG = NFCUtils.class.getSimpleName();
    private static final boolean USE_BIGINTEGER = true;

    static final byte VERSION_1 = (byte)0x01;

    private static final byte MESSAGE = 0x01;
    private static final byte INVITE = 0x02;
    private static final byte REPLY = 0x03;

    private static final byte REPLY_NOGAME = 0x00;

    private static boolean s_inSDK = 19 <= Build.VERSION.SDK_INT;
    private static boolean[] s_nfcAvail;

    // Return array of two booleans, the first indicating whether the
    // device supports NFC and the second whether it's on.  Only the
    // second can change.
    public static boolean[] nfcAvail( Context context )
    {
        if ( null == s_nfcAvail ) {
            s_nfcAvail = new boolean[] {
                s_inSDK && null != getNFCAdapter( context ),
                false
            };
        }
        if ( s_nfcAvail[0] ) {
            s_nfcAvail[1] = getNFCAdapter( context ).isEnabled();
        }
        // Log.d( TAG, "nfcAvail() => {%b,%b}", s_nfcAvail[0], s_nfcAvail[1] );
        return s_nfcAvail;
    }

    public static Dialog makeEnableNFCDialog( final Activity activity )
    {
        DialogInterface.OnClickListener lstnr
            = new DialogInterface.OnClickListener() {
                    public void onClick( DialogInterface dialog,
                                         int item ) {
                        activity.
                            startActivity( new Intent("android.settings"
                                                      + ".NFC_SETTINGS" ) );
                    }
                };
        return LocUtils.makeAlertBuilder( activity )
            .setTitle( R.string.info_title )
            .setMessage( R.string.enable_nfc )
            .setPositiveButton( android.R.string.cancel, null )
            .setNegativeButton( R.string.button_go_settings, lstnr )
            .create();
    }

    private static NfcAdapter getNFCAdapter( Context context )
    {
        NfcManager manager =
            (NfcManager)context.getSystemService( Context.NFC_SERVICE );
        return manager.getDefaultAdapter();
    }

    private static byte[] formatMsgs( int gameID, List<byte[]> msgs )
    {
        return formatMsgs( gameID, msgs.toArray( new byte[msgs.size()][] ) );
    }

    private static byte[] formatMsgs( int gameID, byte[][] msgs )
    {
        byte[] result = null;

        if ( null != msgs && 0 < msgs.length ) {
            try {
                ByteArrayOutputStream baos = new ByteArrayOutputStream();
                DataOutputStream dos = new DataOutputStream( baos );
                dos.writeInt( gameID );
                Log.d( TAG, "formatMsgs(): wrote gameID: %d", gameID );
                dos.flush();
                baos.write( msgs.length );
                for ( int ii = 0; ii < msgs.length; ++ii ) {
                    byte[] msg = msgs[ii];
                    short len = (short)msg.length;
                    baos.write( len & 0xFF );
                    baos.write( (len >> 8) & 0xFF );
                    baos.write( msg );
                }
                result = baos.toByteArray();
            } catch ( IOException ioe ) {
                Assert.failDbg();
            }
        }
        Log.d( TAG, "formatMsgs(gameID=%d) => %s", gameID, DbgUtils.hexDump( result ) );
        return result;
    }

    private static byte[][] unformatMsgs( byte[] data, int start, int[] gameID )
    {
        byte[][] result = null;
        try {
            ByteArrayInputStream bais
                = new ByteArrayInputStream( data, start, data.length );
            DataInputStream dis = new DataInputStream( bais );
            gameID[0] = dis.readInt();
            Log.d( TAG, "unformatMsgs(): read gameID: %d", gameID[0] );
            int count = bais.read();
            Log.d( TAG, "unformatMsgs(): read count: %d", count );
            result = new byte[count][];

            for ( int ii = 0; ii < count; ++ii ) {
                short len = (short)bais.read();
                len |= (int)(bais.read() << 8);
                Log.d( TAG, "unformatMsgs(): read len %d for msg %d", len, ii );
                byte[] msg = new byte[len];
                int nRead = bais.read( msg );
                Assert.assertTrue( nRead == msg.length );
                result[ii] = msg;
            }
        } catch ( IOException ex ) {
            Log.d( TAG, "ex: %s: %s", ex, ex.getMessage() );
            result = null;
            gameID[0] = 0;
        }
        Log.d( TAG, "unformatMsgs() => %s (len=%d)", result,
               null == result ? 0 : result.length );
        return result;
    }

    interface HaveDataListener {
        void onHaveDataChanged( boolean nowHaveData );
    }

    public static class MsgToken {
        private MsgsStore mStore;
        private byte[][] mMsgs;
        private int mGameID;

        private MsgToken( MsgsStore store, int gameID )
        {
            mStore = store;
            mGameID = gameID;
            mMsgs = mStore.getMsgsFor( gameID );
        }

        byte[] getMsgs()
        {
            return formatMsgs( mGameID, mMsgs );
        }

        void removeSentMsgs()
        {
            mStore.removeSentMsgs( mGameID, mMsgs );
        }
    }

    private static class MsgsStore {
        private Map<Integer, WeakReference<HaveDataListener>> mListeners
            = new HashMap<>();
        private static Map<Integer, List<byte[]>> mMsgMap = new HashMap<>();

        void setHaveDataListener( int gameID, HaveDataListener listener )
        {
            Assert.assertFalse( gameID == 0 );
            WeakReference<HaveDataListener> ref = new WeakReference<>(listener);
            synchronized ( mListeners ) {
                mListeners.put( gameID, ref );
            }

            byte[][] msgs = getMsgsFor( gameID );
            listener.onHaveDataChanged( null != msgs && 0 < msgs.length );
        }

        private int addMsgFor( int gameID, byte typ, byte[] msg )
        {
            Boolean nowHaveData = null;

            synchronized ( mMsgMap ) {
                if ( !mMsgMap.containsKey( gameID ) ) {
                    mMsgMap.put( gameID, new ArrayList<byte[]>() );
                }
                List<byte[]> msgs = mMsgMap.get( gameID );

                byte[] full = new byte[msg.length + 1];
                full[0] = typ;
                System.arraycopy( msg, 0, full, 1, msg.length );

                // Can't use msgs.contains() because it uses equals()
                boolean isDuplicate = false;
                for ( byte[] curMsg : msgs ) {
                    if ( Arrays.equals( curMsg, full ) ) {
                        isDuplicate = true;
                        break;
                    }
                }

                if ( !isDuplicate ) {
                    msgs.add( full );
                    nowHaveData = 0 < msgs.size();
                    Log.d( TAG, "addMsgFor(gameID=%d): added %s; now have %d msgs",
                           gameID, DbgUtils.hexDump(msg), msgs.size() );
                }
            }

            reportHaveData( gameID, nowHaveData );

            return msg.length;
        }

        private byte[][] getMsgsFor( int gameID )
        {
            Assert.assertFalse( gameID == 0 );
            byte[][] result = null;
            synchronized ( mMsgMap ) {
                if ( mMsgMap.containsKey( gameID ) ) {
                    List<byte[]> msgs = mMsgMap.get( gameID );
                    result = msgs.toArray( new byte[msgs.size()][] );
                }
            }
            Log.d( TAG, "getMsgsFor(gameID=%d) => %d msgs", gameID,
                   result == null ? 0 : result.length );
            return result;
        }

        private void removeSentMsgs( int gameID, byte[][] msgs )
        {
            Boolean nowHaveData = null;
            if ( null != msgs ) {
                synchronized ( mMsgMap ) {
                    if ( mMsgMap.containsKey( gameID ) ) {
                        List<byte[]> list = mMsgMap.get( gameID );
                        // Log.d( TAG, "removeSentMsgs(%d): size before: %d", gameID,
                        //        list.size() );
                        int origSize = list.size();
                        for ( byte[] msg : msgs ) {
                            list.remove( msg );
                        }
                        if ( 0 < origSize ) {
                            Log.d( TAG, "removeSentMsgs(%d): size was %d, now %d", gameID,
                                   origSize, list.size() );
                        }
                        nowHaveData = 0 < list.size();
                    }
                }
            }
            reportHaveData( gameID, nowHaveData );
        }

        private void reportHaveData( int gameID, Boolean nowHaveData )
        {
            Log.d( TAG, "reportHaveData(" + nowHaveData + ")" );
            if ( null != nowHaveData ) {
                HaveDataListener proc = null;
                synchronized ( mListeners ) {
                    WeakReference<HaveDataListener> ref = mListeners.get( gameID );
                    if ( null != ref ) {
                        proc = ref.get();
                        if ( null == proc ) {
                            mListeners.remove( gameID );
                        }
                    } else {
                        Log.d( TAG, "reportHaveData(): no listener for %d", gameID );
                    }
                }
                if ( null != proc ) {
                    proc.onHaveDataChanged( nowHaveData );
                }
            }
        }

        static byte[] split( byte[] msg, byte[] headerOut )
        {
            headerOut[0] = msg[0];
            byte[] result = Arrays.copyOfRange( msg, 1, msg.length );
            Log.d( TAG, "split(%s) => %d/%s", DbgUtils.hexDump( msg ),
                   headerOut[0], DbgUtils.hexDump( result ) );
            return result;
        }
    }
    private static MsgsStore sMsgsStore = new MsgsStore();

    static void setHaveDataListener( int gameID, HaveDataListener listener )
    {
        sMsgsStore.setHaveDataListener( gameID, listener );
    }

    static int addMsgFor( byte[] msg, int gameID )
    {
        return sMsgsStore.addMsgFor( gameID, MESSAGE, msg );
    }

    static int addInvitationFor( byte[] msg, int gameID )
    {
        return sMsgsStore.addMsgFor( gameID, INVITE, msg );
    }

    static int addReplyFor( byte[] msg, int gameID )
    {
        return sMsgsStore.addMsgFor( gameID, REPLY, msg );
    }

    static MsgToken getMsgsFor( int gameID )
    {
        MsgToken token = new MsgToken( sMsgsStore, gameID );
        return token;
    }

    static void receiveMsgs( Context context, byte[] data )
    {
        receiveMsgs( context, data, 0 );
    }

    static void receiveMsgs( Context context, byte[] data, int offset )
    {
        // Log.d( TAG, "receiveMsgs(gameID=%d, %s, offset=%d)", gameID,
        //        DbgUtils.hexDump(data), offset );
        DbgUtils.assertOnUIThread( false );
        int[] gameID = {0};
        byte[][] msgs = unformatMsgs( data, offset, gameID );
        if ( null != msgs ) {
            NFCServiceHelper helper = new NFCServiceHelper( context );
            for ( byte[] msg : msgs ) {
                byte[] typ = {0};
                byte[] body = MsgsStore.split( msg, typ );
                switch ( typ[0] ) {
                case MESSAGE:
                    long[] rowids = DBUtils.getRowIDsFor( context, gameID[0] );
                    if ( 0 == rowids.length ) {
                        addReplyFor( new byte[]{REPLY_NOGAME}, gameID[0] );
                    } else {
                        for ( long rowid : rowids ) {
                            MultiMsgSink sink = new MultiMsgSink( context, rowid );
                            helper.receiveMessage( rowid, sink, body );
                        }
                    }
                    break;
                case INVITE:
                    GamesListDelegate.postReceivedInvite( context, body );
                    break;
                case REPLY:
                    switch( body[0] ) {
                    case REPLY_NOGAME:
                        // PENDING Don't enable this until deviceID is being
                        // checked. Otherwise it'll happen every time I tap my
                        // device against another that doesn't have my game,
                        // which could be common.
                        // helper.postEvent( MultiEvent.MESSAGE_NOGAME, gameID );
                        Log.e( TAG, "receiveMsgs(): not calling helper.postEvent( "
                               + "MultiEvent.MESSAGE_NOGAME, gameID );" );
                        break;
                    default:
                        Log.e( TAG, "unexpected reply %d", body[0] );
                        Assert.failDbg();
                        break;
                    }
                    break;
                default:
                    Assert.failDbg();
                    break;
                }
            }
        }
    }

    static enum HEX_STR {
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
        byte[] asBA() { return mBytes; }
        boolean matchesFrom( byte[] src )
        {
            return matchesFrom( src, 0 );
        }
        boolean matchesFrom( byte[] src, int offset )
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

    static byte[] numTo( int num )
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
                Assert.failDbg();
            }
            result = baos.toByteArray();
        }
        // Log.d( TAG, "numTo(%d) => %s", num, DbgUtils.hexDump(result) );
        return result;
    }

    static int numFrom( ByteArrayInputStream bais ) throws IOException
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

    static int numFrom( byte[] bytes, int start, int out[] )
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

    // private static void testNumThing()
    // {
    //     Log.d( TAG, "testNumThing() starting" );

    //     int[] out = {0};
    //     for ( int ii = 1; ii > 0 && ii < Integer.MAX_VALUE; ii *= 2 ) {
    //         byte[] tmp = numTo( ii );
    //         numFrom( tmp, 0, out );
    //         if ( ii != out[0] ) {
    //             Log.d( TAG, "testNumThing(): %d failed; got %d", ii, out[0] );
    //             break;
    //         } else {
    //             Log.d( TAG, "testNumThing(): %d ok", ii );
    //         }
    //     }
    //     Log.d( TAG, "testNumThing() DONE" );
    // }

    private static AtomicInteger sLatestAck = new AtomicInteger(0);
    static int getLatestAck()
    {
        int result = sLatestAck.getAndSet(0);
        if ( 0 != result ) {
            Log.d( TAG, "getLatestAck() => %d", result );
        }
        return result;
    }

    static void setLatestAck( int ack )
    {
        if ( 0 != ack ) {
            Log.e( TAG, "setLatestAck(%d)", ack );
        }
        int oldVal = sLatestAck.getAndSet( ack );
        if ( 0 != oldVal ) {
            Log.e( TAG, "setLatestAck(%d): dropping ack msgID %d", ack, oldVal );
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
    synchronized static byte[] reassemble( Context context, byte[] part,
                                                   HEX_STR cmd )
    {
        return reassemble( context, part, cmd.length() );
    }

    synchronized static byte[] reassemble( Context context, byte[] part,
                                                   int offset )
    {
        part = Arrays.copyOfRange( part, offset, part.length );
        return reassemble( context, part );
    }

    synchronized static byte[] reassemble( Context context, byte[] part )
    {
        byte[] result = null;
        try {
            ByteArrayInputStream bais = new ByteArrayInputStream( part );

            final int cur = bais.read();
            final int count = bais.read();
            if ( 0 == cur ) {
                sMsgID = NFCUtils.numFrom( bais );
                int ack = NFCUtils.numFrom( bais );
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
                    setLatestAck( sMsgID );
                    if ( 0 != sMsgID ) {
                        Log.d( TAG, "reassemble(): done reassembling msgID=%d: %s",
                               sMsgID, DbgUtils.hexDump(result) );
                    }
                }
            }
        } catch ( IOException ioe ) {
            Assert.failDbg();
        }
        return result;
    }

    private static final int HEADER_SIZE = 10;
    static byte[][] wrapMsg( MsgToken token, int maxLen )
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
                Assert.assertTrue( HEADER_SIZE >= baos.toByteArray().length
                                   || !BuildConfig.DEBUG );

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
            Assert.failDbg();
        }
        return result;
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
    synchronized static void addToMsgThread( Context context, byte[] msg )
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

    public static class Wrapper {
        private Reader mReader;

        public interface Procs {
            void onReadingChange( boolean nowReading );
        }

        private Wrapper( Activity activity, Procs procs, int devID )
        {
            mReader = new Reader( activity, procs, devID );
        }

        public static Wrapper init( Activity activity, Procs procs, int devID )
        {
            Wrapper instance = null;

            if ( nfcAvail( activity )[1] ) {
                instance = new Wrapper( activity, procs, devID );
            }
            Log.d( TAG, "Wrapper.init(devID=%d) => %s", devID, instance );
            return instance;
        }

        static void setResumed( Wrapper instance, boolean resumed )
        {
            if ( null != instance ) {
                instance.mReader.setResumed( resumed );
            }
        }

        static void setGameID( Wrapper instance, int gameID )
        {
            if ( null != instance ) {
                instance.mReader.setGameID( gameID );
            }
        }
    }

    private static class Reader implements NfcAdapter.ReaderCallback,
                                           HaveDataListener {
        private Activity mActivity;
        private boolean mHaveData;
        private Wrapper.Procs mProcs;
        private NfcAdapter mAdapter;
        private int mMinMS = 300;
        private int mMaxMS = 500;
        private boolean mConnected = false;
        private int mMyDevID;

        private Reader( Activity activity, Wrapper.Procs procs, int devID )
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
                    byte[] all = reassemble( mActivity, response,
                                             offset + HEX_STR.CMD_MSG_PART.length() );
                    Log.d( TAG, "receiveAny(%s) => %b", DbgUtils.hexDump( response ), statusOK );
                    if ( null != all ) {
                        addToMsgThread( mActivity, all );
                    }
                }
            }
            if ( !statusOK ) {
                Log.d( TAG, "receiveAny(%s) => %b", DbgUtils.hexDump( response ), statusOK );
            }
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
                        mAdapter.enableReaderMode( mActivity, Reader.this, mFlags, null );
                    } else if ( mInReadMode && !wantReadMode ) {
                        mAdapter.disableReaderMode( mActivity );
                    }
                    mInReadMode = wantReadMode;
                    // Log.d( TAG, "run(): inReadMode now: %b", mInReadMode );

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

    private static class NFCServiceHelper extends XWServiceHelper {
        private CommsAddrRec mAddr
            = new CommsAddrRec( CommsAddrRec.CommsConnType.COMMS_CONN_NFC );

        NFCServiceHelper( Context context )
        {
            super( context );
        }

        @Override
        void postNotification( String device, int gameID, long rowid )
        {
            Context context = getContext();
            String body = LocUtils.getString( context, R.string.new_game_body );
            GameUtils.postInvitedNotification( context, gameID, body, rowid );
        }

        private void receiveMessage( long rowid, MultiMsgSink sink, byte[] msg )
        {
            Log.d( TAG, "receiveMessage(rowid=%d, len=%d)", rowid, msg.length );
            receiveMessage( rowid, sink, msg, mAddr );
        }
    }
}
