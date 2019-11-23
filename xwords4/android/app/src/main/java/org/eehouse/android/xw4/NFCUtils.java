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
import android.nfc.NdefMessage;
import android.nfc.NdefRecord;
import android.nfc.NfcAdapter;
import android.nfc.NfcEvent;
import android.nfc.NfcManager;
import android.os.Build;
import android.os.Parcelable;

import org.json.JSONArray;
import org.json.JSONObject;
import org.json.JSONException;

import org.eehouse.android.xw4.loc.LocUtils;
import org.eehouse.android.xw4.jni.CommsAddrRec;

public class NFCUtils {
    private static final String TAG = NFCUtils.class.getSimpleName();

    private static final String NFC_TO_SELF_ACTION = "org.eehouse.nfc_to_self";
    private static final String NFC_TO_SELF_DATA = "nfc_data";

    private static final String MSGS = "MSGS";
    private static final String GAMEID = "GAMEID";

    public interface NFCActor {
        String makeNFCMessage();
    }

    private static boolean s_inSDK;
    private static boolean[] s_nfcAvail;
    private static SafeNFC s_safeNFC;
    static {
        s_inSDK = 14 <= Build.VERSION.SDK_INT
            && Build.VERSION.SDK_INT <= Build.VERSION_CODES.P;
        if ( s_inSDK ) {
            s_safeNFC = new SafeNFCImpl();
        }
    }

    private static interface SafeNFC {
        public void register( Activity activity, NFCActor actor );
    }

    private static class SafeNFCImpl implements SafeNFC {
        public void register( final Activity activity, final NFCActor actor )
        {
            NfcManager manager =
                (NfcManager)activity.getSystemService( Context.NFC_SERVICE );
            if ( null != manager ) {
                NfcAdapter adapter = manager.getDefaultAdapter();
                if ( null != adapter ) {
                    NfcAdapter.CreateNdefMessageCallback cb =
                        new NfcAdapter.CreateNdefMessageCallback() {
                            public NdefMessage createNdefMessage( NfcEvent evt )
                            {
                                NdefMessage msg = null;
                                String data = actor.makeNFCMessage();
                                if ( null != data ) {
                                    msg = makeMessage( activity, data );
                                }
                                return msg;
                            }
                        };
                    adapter.setNdefPushMessageCallback( cb, activity );
                }
            }
        }
    }

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
        return s_nfcAvail;
    }

    public static String getFromIntent( Intent intent )
    {
        String result = null;

        String action = intent.getAction();
        if ( NfcAdapter.ACTION_NDEF_DISCOVERED.equals( action ) ) {
            Parcelable[] rawMsgs =
                intent.getParcelableArrayExtra( NfcAdapter.EXTRA_NDEF_MESSAGES );
            // only one message sent during the beam
            NdefMessage msg = (NdefMessage)rawMsgs[0];
            // record 0 contains the MIME type, record 1 is the AAR, if present
            result = new String( msg.getRecords()[0].getPayload() );
        } else if ( NFC_TO_SELF_ACTION.equals( action ) ) {
            result = intent.getStringExtra( NFC_TO_SELF_DATA );
        }

        return result;
    }

    public static void populateIntent( Intent intent, String data )
    {
        intent.setAction( NFC_TO_SELF_ACTION )
            .putExtra( NFC_TO_SELF_DATA, data );
    }

    public static void register( Activity activity, NFCActor actor )
    {
        if ( null != s_safeNFC ) {
            s_safeNFC.register( activity, actor );
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

    private static NdefMessage makeMessage( Activity activity, String data )
    {
        String mimeType = LocUtils.getString( activity, R.string.xwords_nfc_mime );
        NdefMessage msg = new NdefMessage( new NdefRecord[] {
                new NdefRecord(NdefRecord.TNF_MIME_MEDIA,
                               mimeType.getBytes(), new byte[0],
                               data.getBytes())
                ,NdefRecord.
                createApplicationRecord( activity.getPackageName() )
            });
        return msg;
    }

    private static NfcAdapter getNFCAdapter( Context context )
    {
        NfcManager manager =
            (NfcManager)context.getSystemService( Context.NFC_SERVICE );
        return manager.getDefaultAdapter();
    }

    static String makeMsgsJSON( int gameID, byte[][] msgs )
    {
        String result = null;

        JSONArray arr = new JSONArray();
        for ( byte[] msg : msgs ) {
            arr.put( Utils.base64Encode( msg ) );
        }

        try {
            JSONObject obj = new JSONObject();
            obj.put( MSGS, arr );
            obj.put( GAMEID, gameID );

            result = obj.toString();
        } catch ( JSONException ex ) {
            Assert.assertFalse( BuildConfig.DEBUG );
        }
        return result;
    }

    static boolean receiveMsgs( Context context, String data )
    {
        Log.d( TAG, "receiveMsgs()" );
        int gameID[] = {0};
        byte[][] msgs = msgsFrom( data, gameID );
        boolean success = null != msgs && 0 < msgs.length;
        if ( success ) {
            NFCServiceHelper helper = new NFCServiceHelper( context );
            long[] rowids = DBUtils.getRowIDsFor( context, gameID[0] );
            for ( long rowid : rowids ) {
                NFCMsgSink sink = new NFCMsgSink( context, rowid );
                for ( byte[] msg : msgs ) {
                    helper.receiveMessage( rowid, sink, msg );
                }
            }
        }
        return success;
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
            Log.d( TAG, "receiveMessage()" );
            receiveMessage( rowid, sink, msg, mAddr );
        }
    }

    private static class NFCMsgSink extends MultiMsgSink {
        NFCMsgSink( Context context, long rowid )
        {
            super( context, rowid );
        }
    }

    private static byte[][] msgsFrom( String json, /*out*/ int[] gameID )
    {
        byte[][] result = null;
        try {
            JSONObject obj = new JSONObject( json );
            gameID[0] = obj.getInt( GAMEID );
            JSONArray arr = obj.getJSONArray( MSGS );
            if ( null != arr ) {
                result = new byte[arr.length()][];
                for ( int ii = 0; ii < arr.length(); ++ii ) {
                    String str = arr.getString( ii );
                    result[ii] = Utils.base64Decode( str );
                }
            }
        } catch ( JSONException ex ) {
            Assert.assertFalse( BuildConfig.DEBUG );
            result = null;
        }
        Log.d( TAG, "msgsFrom() => %s", (Object)result );
        return result;
    }

}
