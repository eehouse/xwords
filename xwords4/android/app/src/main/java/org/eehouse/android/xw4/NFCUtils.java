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
import android.os.Build;
import android.os.Parcelable;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eehouse.android.xw4.MultiService.MultiEvent;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.loc.LocUtils;

public class NFCUtils {
    private static final String TAG = NFCUtils.class.getSimpleName();

    private static final String NFC_TO_SELF_ACTION = "org.eehouse.nfc_to_self";
    private static final String NFC_TO_SELF_DATA = "nfc_data";

    private static final byte MESSAGE = 0x01;
    private static final byte INVITE = 0x02;
    private static final byte REPLY = 0x03;

    private static final byte REPLY_NOGAME = 0x00;

    private static boolean s_inSDK = 14 <= Build.VERSION.SDK_INT;
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

    public static byte[] getFromIntent( Intent intent )
    {
        byte[] result = null;

        String action = intent.getAction();
        if ( NFC_TO_SELF_ACTION.equals( action ) ) {
            result = intent.getByteArrayExtra( NFC_TO_SELF_DATA );
        }

        Log.d( TAG, "getFromIntent() => %s", result );
        return result;
    }

    public static void populateIntent( Context context, Intent intent,
                                       byte[] data )
    {
        NetLaunchInfo nli = NetLaunchInfo.makeFrom( context, data );
        if ( null != nli ) {
            intent.setAction( NFC_TO_SELF_ACTION )
                .putExtra( NFC_TO_SELF_DATA, data );
        } else {
            Assert.assertFalse( BuildConfig.DEBUG );
        }
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
                Assert.assertFalse( BuildConfig.DEBUG );
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
            Log.d( TAG, "getMsgsFor() => %d msgs", result == null ? 0 : result.length );
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
                    if ( null == rowids || 0 == rowids.length ) {
                        addReplyFor( new byte[]{REPLY_NOGAME}, gameID[0] );
                    } else {
                        for ( long rowid : rowids ) {
                            NFCMsgSink sink = new NFCMsgSink( context, rowid );
                            helper.receiveMessage( rowid, sink, body );
                        }
                    }
                    break;
                case INVITE:
                    GamesListDelegate.postNFCInvite( context, body );
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
                        Assert.assertFalse( BuildConfig.DEBUG );
                        break;
                    }
                    break;
                default:
                    Assert.assertFalse( BuildConfig.DEBUG );
                    break;
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
        protected MultiMsgSink getSink( long rowid )
        {
            Context context = getContext();
            return new NFCMsgSink( context, rowid );
        }

        @Override
        void postNotification( String device, int gameID, long rowid )
        {
            Context context = getContext();
            String body = LocUtils.getString( context, R.string.new_relay_body );
            GameUtils.postInvitedNotification( context, gameID, body, rowid );
        }

        private void receiveMessage( long rowid, NFCMsgSink sink, byte[] msg )
        {
            Log.d( TAG, "receiveMessage(rowid=%d, len=%d)", rowid, msg.length );
            receiveMessage( rowid, sink, msg, mAddr );
        }
    }

    private static class NFCMsgSink extends MultiMsgSink {
        NFCMsgSink( Context context, long rowid )
        {
            super( context, rowid );
        }
    }
}
