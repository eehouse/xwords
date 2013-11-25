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
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface;
import android.content.Intent;
import android.nfc.NdefMessage;
import android.nfc.NdefRecord;
import android.nfc.NfcAdapter;
import android.nfc.NfcEvent;
import android.nfc.NfcManager;
import android.os.Parcelable;

import junit.framework.Assert;

public class NFCUtils {

    public interface NFCActor {
        String makeNFCMessage();
    }

    private static boolean s_inSDK;
    private static boolean[] s_nfcAvail;
    private static SafeNFC s_safeNFC;
    static {
        s_inSDK = 14 <= Integer.valueOf( android.os.Build.VERSION.SDK );
        if ( s_inSDK ) {
            s_safeNFC = new SafeNFCImpl();
        }
    }

    private static interface SafeNFC {
        public void register( Activity activity );
    }

    private static class SafeNFCImpl implements SafeNFC {
        public void register( final Activity activity )
        {
            Assert.assertTrue( activity instanceof NFCActor );
            final NFCActor actor = (NFCActor)activity;
            NfcAdapter.CreateNdefMessageCallback cb = 
                new NfcAdapter.CreateNdefMessageCallback() {
                    public NdefMessage createNdefMessage( NfcEvent event ) {
                        NdefMessage msg = null;
                        String data = actor.makeNFCMessage();
                        if ( null != data ) {
                            msg = makeMessage( activity, data );
                        }
                        return msg;
                    }
                };

            NfcManager manager = 
                (NfcManager)activity.getSystemService( Context.NFC_SERVICE );
            NfcAdapter adapter = manager.getDefaultAdapter();
            if ( null != adapter ) {
                adapter.setNdefPushMessageCallback( cb, activity );
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

    public static void register( Activity activity )
    {
        if ( null != s_safeNFC ) {
            s_safeNFC.register( activity );
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
        return new AlertDialog.Builder( activity )
            .setTitle( R.string.info_title )
            .setMessage( R.string.enable_nfc )
            .setPositiveButton( R.string.button_cancel, null )
            .setNegativeButton( R.string.button_go_settings, lstnr )
            .create();
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

    private static NfcAdapter getNFCAdapter( Context context )
    {
        NfcManager manager = 
            (NfcManager)context.getSystemService( Context.NFC_SERVICE );
        return manager.getDefaultAdapter();
    }
    
}
