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
import android.nfc.cardemulation.HostApduService;
import android.os.Bundle;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;

import org.eehouse.android.xw4.NFCUtils.HEX_STR;
import org.eehouse.android.xw4.NFCUtils.MsgToken;

public class NFCCardService extends HostApduService {
    private static final String TAG = NFCCardService.class.getSimpleName();
    private static final int LEN_OFFSET = 4;

    private int mMyDevID;

    // Remove this once we don't need logging to confirm stuff's loading
    @Override
    public void onCreate()
    {
        super.onCreate();
        mMyDevID = NFCUtils.getNFCDevID( this );
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
                byte[] all = NFCUtils.reassemble( this, apdu, HEX_STR.CMD_MSG_PART );
                if ( null != all ) {
                    NFCUtils.addToMsgThread( this, all );
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
                                if ( minVersion == NFCUtils.VERSION_1 ) {
                                    int devID = NFCUtils.numFrom( bais );
                                    Log.d( TAG, "processCommandApdu(): read "
                                           + "remote devID: %d", devID );
                                    mGameID = NFCUtils.numFrom( bais );
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
                        Assert.failDbg();
                    }
                }
            }
        }

        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        try {
            baos.write( resStr.asBA() );
            if ( HEX_STR.STATUS_SUCCESS == resStr ) {
                if ( isAidCase ) {
                    baos.write( NFCUtils.VERSION_1 ); // min
                    baos.write( NFCUtils.numTo( mMyDevID ) );
                } else {
                    MsgToken token = NFCUtils.getMsgsFor( mGameID );
                    byte[][] tmp = NFCUtils.wrapMsg( token, Short.MAX_VALUE );
                    Assert.assertTrue( 1 == tmp.length || !BuildConfig.DEBUG );
                    baos.write( tmp[0] );
                }
            }
        } catch ( IOException ioe ) {
            Assert.failDbg();
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


}
