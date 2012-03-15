/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2010 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.telephony.SmsManager;
import android.telephony.SmsMessage;
import android.util.Base64;
import java.io.ByteArrayOutputStream;
import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.OutputStream;
import java.io.InputStream;
import java.util.Arrays;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.CommsAddrRec;

public class NBSReceiver extends BroadcastReceiver {

    // All messages are base64-encoded byte arrays.  The first byte is
    // always one of these.  What follows depends.
    private enum NBS_CMS { NBS_CMD_NONE,
            NBS_CMD_INVITE,
            NBS_CMD_DATA,
            };

    @Override
    public void onReceive( Context context, Intent intent )
    {
        DbgUtils.logf( "NBSReceiver::onReceive(intent=%s)!!!!",
                    intent.toString() );

        Bundle bundle = intent.getExtras();
        if ( null != bundle ) {
            Object[] pdus = (Object[])bundle.get( "pdus" );
            SmsMessage[] nbses = new SmsMessage[pdus.length];

            for ( int ii = 0; ii < nbses.length; ++ii ) {
                nbses[ii] = SmsMessage.createFromPdu((byte[])pdus[ii]);
                receiveBuffer( context, nbses[ii].getUserData(),
                               nbses[ii].getOriginatingAddress() );
            }
        }
    }

    static void tryNBSMessage( Context context, String phoneNo )
    {
        byte[] data = { 'a', 'b', 'c' };

        SmsManager mgr = SmsManager.getDefault();

        try {
            /* online comment says providing PendingIntents prevents 
               random crashes */
            PendingIntent sent = PendingIntent.getBroadcast( context,
                                                             0, new Intent(), 0 );
            PendingIntent dlvrd = PendingIntent.getBroadcast( context, 0, 
                                                              new Intent(), 0 );

            mgr.sendDataMessage( phoneNo, null, (short)50009, 
                                 data, sent, dlvrd );
            // PendingIntent sentIntent, 
            // PendingIntent deliveryIntent );
            DbgUtils.logf( "sendDataMessage finished" );
        } catch ( IllegalArgumentException iae ) {
            DbgUtils.logf( "%s", iae.toString() );
        }
    }

    public static void inviteRemote( Context context, String phone,
                                     int gameID, String gameName, 
                                     int lang, int nPlayersT, 
                                     int nPlayersH )
    {
        ByteArrayOutputStream bas = new ByteArrayOutputStream( 128 );
        DataOutputStream das = new DataOutputStream( bas );
        try {
            das.writeInt( gameID );
            das.writeUTF( gameName );
            das.writeInt( lang );
            das.writeByte( nPlayersT );
            das.writeByte( nPlayersH );
            das.flush();

            send( context, NBS_CMS.NBS_CMD_INVITE, bas.toByteArray(), phone );
        } catch ( java.io.IOException ioe ) {
            DbgUtils.logf( "ioe: %s", ioe.toString() );
        }
    }

    private static void send( Context context, NBS_CMS cmd, 
                              byte[] data, String phone )
    {
        int hash = Arrays.hashCode( data );
        DbgUtils.logf( "NBSReceiver: outgoing hash on %d bytes: %X", 
                       data.length, hash );
        ByteArrayOutputStream bas = new ByteArrayOutputStream( 128 );
        DataOutputStream das = new DataOutputStream( bas );
        try {
            das.writeByte( cmd.ordinal() );
            das.writeInt( hash );
            das.write( data, 0, data.length );
            das.flush();

            byte[] as64 = Base64.encode( bas.toByteArray(), Base64.NO_WRAP );
            sendBuffer( context, as64, phone );
        } catch ( java.io.IOException ioe ) {
            DbgUtils.logf( "ioe: %s", ioe.toString() );
        }
    }

    private void receive( Context context, NBS_CMS cmd, 
                          byte[] data, String phone )
    {
        switch( cmd ) {
        case NBS_CMD_INVITE:
            DataInputStream dis = 
                new DataInputStream( new ByteArrayInputStream(data) );
            try {
                int gameID = dis.readInt();
                String gameName = dis.readUTF();
                int lang = dis.readInt();
                int nPlayersT = dis.readByte();
                int nPlayersH = dis.readByte();

                CommsAddrRec addr = new CommsAddrRec( phone );
                long rowid = GameUtils.makeNewNBSGame( context, gameID, addr,
                                                       lang, nPlayersT, nPlayersH );

                if ( null != gameName && 0 < gameName.length() ) {
                    DBUtils.setName( context, rowid, gameName );
                }
                String body = Utils.format( context, R.string.new_nbs_bodyf, phone );
                postNotification( context, gameID, R.string.new_nbs_title, body );
            } catch ( java.io.IOException ioe ) {
                DbgUtils.logf( "ioe: %s", ioe.toString() );
            }
            break;
        default:
            Assert.fail();
            break;
        }
    }

    private void receiveBuffer( Context context, byte[] as64, String senderPhone )
    {
        byte[] data = Base64.decode( as64, Base64.NO_WRAP );
        DataInputStream dis = 
            new DataInputStream( new ByteArrayInputStream(data) );
        try {
            NBS_CMS cmd = NBS_CMS.values()[dis.readByte()];
            int hashRead = dis.readInt();
            DbgUtils.logf( "NBSReceiver: incoming hash: %X", hashRead );
            byte[] rest = new byte[dis.available()];
            dis.read( rest );
            int hashComputed = Arrays.hashCode( rest );
            if ( hashComputed == hashRead ) {
                DbgUtils.logf( "NBSReceiver: incoming hashes on %d bytes match: %X",
                               rest.length, hashRead );
                receive( context, cmd, rest, senderPhone );
            } else {
                DbgUtils.logf( "NBSReceiver: incoming hashes on %d bytes "
                               + "DON'T match: read: %X; figured: %X", 
                               rest.length, hashRead, hashComputed );
            }
        } catch ( java.io.IOException ioe ) {
            DbgUtils.logf( "ioe: %s", ioe.toString() );
        }
    }

    private static void sendBuffer( Context context, byte[] data, String phone )
    {
        if ( XWApp.onEmulator() ) {
            DbgUtils.logf( "sendBuffer(phone=%s): FAKING IT", phone );
        } else {
            try {
                PendingIntent sent = 
                    PendingIntent.getBroadcast( context, 0, new Intent(), 0 );
                PendingIntent dlvrd = 
                    PendingIntent.getBroadcast( context, 0, new Intent(), 0 );

                SmsManager mgr = SmsManager.getDefault();
                mgr.sendDataMessage( phone, null, XWApp.getNBSPort(), 
                                     data, sent, dlvrd );
                // PendingIntent sentIntent, 
                // PendingIntent deliveryIntent );
                DbgUtils.logf( "send to %s finished", phone );
            } catch ( IllegalArgumentException iae ) {
                DbgUtils.logf( "%s", iae.toString() );
            }
        }
    }

    private void postNotification( Context context, int gameID, int title, 
                                   String body )
    {
        Intent intent = new Intent( context, DispatchNotify.class );
        intent.putExtra( DispatchNotify.GAMEID_EXTRA, gameID );
        Utils.postNotification( context, intent, R.string.new_nbsmove_title, 
                                body );
    }

}
