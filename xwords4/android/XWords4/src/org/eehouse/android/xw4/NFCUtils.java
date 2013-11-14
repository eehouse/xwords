/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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
import android.content.Context;
import android.content.Intent;
import android.nfc.NdefMessage;
import android.nfc.NdefRecord;
import android.nfc.NfcAdapter;
import android.nfc.NfcEvent;
import android.nfc.NfcManager;
import android.os.Parcelable;

public class NFCUtils {
    private static boolean s_inSDK = 
        14 <= Integer.valueOf( android.os.Build.VERSION.SDK );
    private static String s_data = null;

    public static boolean nfcAvail( Context context )
    {
        boolean result = s_inSDK;
        if ( result ) {
            NfcManager manager = 
                (NfcManager)context.getSystemService( Context.NFC_SERVICE );
            NfcAdapter adapter = manager.getDefaultAdapter();
            result = null != adapter && adapter.isEnabled();
        }
        return result;
    }

    public static void register( final Activity activity )
    {
        NfcAdapter.CreateNdefMessageCallback cb = 
            new NfcAdapter.CreateNdefMessageCallback() {
                public NdefMessage createNdefMessage( NfcEvent event ) {
                    NdefMessage msg = null;
                    if ( null != s_data ) {
                        msg = makeMessage( activity, s_data );
                    }
                    return msg;
                }
            };

        NfcManager manager = 
            (NfcManager)activity.getSystemService( Context.NFC_SERVICE );
        NfcAdapter adapter = manager.getDefaultAdapter();
        adapter.setNdefPushMessageCallback( cb, activity );
    }

    public static void buildAndPush( Activity activity, String data )
    {
        s_data = data;
        // NfcAdapter nfcAdapter = NfcAdapter.getDefaultAdapter( activity );
        // NdefMessage msg = makeMessage( activity, data );
        // nfcAdapter.setNdefPushMessage( msg, activity );
    }

    public static String getFromIntent( Intent intent )
    {
        String result = null;

        if ( NfcAdapter.ACTION_NDEF_DISCOVERED.equals( intent.getAction() ) ) {
            Parcelable[] rawMsgs = 
                intent.getParcelableArrayExtra( NfcAdapter.EXTRA_NDEF_MESSAGES );
            // only one message sent during the beam
            NdefMessage msg = (NdefMessage)rawMsgs[0];
            // record 0 contains the MIME type, record 1 is the AAR, if present
            result = new String( msg.getRecords()[0].getPayload() );
        }

        return result;
    }

    private static NdefMessage makeMessage( Activity activity, String data )
    {
        String mimeType = activity.getString( R.string.xwords_nfc_mime );
        NdefMessage msg = new NdefMessage( new NdefRecord[] {
                new NdefRecord(NdefRecord.TNF_MIME_MEDIA, 
                               mimeType.getBytes(), new byte[0], 
                               data.getBytes())
                ,NdefRecord.
                createApplicationRecord( activity.getPackageName() )
            });
        return msg;
    }
}
