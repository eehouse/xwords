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
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;
import android.telephony.SmsManager;
import android.telephony.SmsMessage;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.OutputStream;
import java.util.Arrays;
import java.util.HashMap;
import java.lang.System;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.CommsAddrRec;

public class NBSService extends Service {

    private static final int MAX_LEN_BIN = 128;
    private static final int HANDLE = 1;
    private static final int INVITE = 2;
    private static final int SEND = 3;

    private static final String CMD_STR = "CMD";
    private static final String BUFFER = "BUFFER";
    private static final String PHONE = "PHONE";
    private static final String GAMEID = "GAMEID";
    private static final String GAMENAME = "GAMENAME";
    private static final String LANG = "LANG";
    private static final String NPLAYERST = "NPLAYERST";
    private static final String NPLAYERSH = "NPLAYERSH";

    private enum NBS_CMD { NONE, INVITE, DATA, };

    private int m_nReceived = 0;
    private static int s_nSent = 0;
    private static HashMap<String, HashMap <Integer, MsgStore>> s_partialMsgs
        = new HashMap<String, HashMap <Integer, MsgStore>>();

    public static void handleFrom( Context context, byte[] buffer, String phone )
    {
        Intent intent = getIntentTo( context, HANDLE );
        intent.putExtra( BUFFER, buffer );
        intent.putExtra( PHONE, phone );
        context.startService( intent );
    }

    public static void inviteRemote( Context context, String phone,
                                     int gameID, String gameName, 
                                     int lang, int nPlayersT, 
                                     int nPlayersH )
    {
        DbgUtils.logf( "inviteRemote(%s)", phone );
        Intent intent = getIntentTo( context, INVITE );
        intent.putExtra( PHONE, phone );
        intent.putExtra( GAMEID, gameID );
        intent.putExtra( GAMENAME, gameName );
        intent.putExtra( LANG, lang );
        intent.putExtra( NPLAYERST, nPlayersT );
        intent.putExtra( NPLAYERSH, nPlayersH );
        context.startService( intent );
    }

    public static int sendPacket( Context context, String phone, 
                                   int gameID, byte[] buffer )
    {
        DbgUtils.logf( "NBSService.sendPacket()" );
        Intent intent = getIntentTo( context, SEND );
        intent.putExtra( PHONE, phone );
        intent.putExtra( GAMEID, gameID );
        intent.putExtra( BUFFER, buffer );
        context.startService( intent );
        return buffer.length;
    }

    private static Intent getIntentTo( Context context, int cmd )
    {
        Intent intent = new Intent( context, NBSService.class );
        intent.putExtra( CMD_STR, cmd );
        return intent;
    }

    @Override
    public void onCreate()
    {
        if ( XWApp.NBSSUPPORTED ) {
        } else {
            stopSelf();
        }
    }

    @Override
    public int onStartCommand( Intent intent, int flags, int startId )
    {
        int result;
        if ( XWApp.NBSSUPPORTED && null != intent ) {
            int cmd = intent.getIntExtra( CMD_STR, -1 );
            switch( cmd ) {
            case HANDLE:
                DbgUtils.showf( this, "got %dth nbs", ++m_nReceived );
                byte[] buffer = intent.getByteArrayExtra( BUFFER );
                String phone = intent.getStringExtra( PHONE );
                receiveBuffer( buffer, phone );
                break;
            case INVITE:
                phone = intent.getStringExtra( PHONE );
                DbgUtils.logf( "INVITE(%s)", phone );
                int gameID = intent.getIntExtra( GAMEID, -1 );
                String gameName = intent.getStringExtra( GAMENAME );
                int lang = intent.getIntExtra( LANG, -1 );
                int nPlayersT = intent.getIntExtra( NPLAYERST, -1 );
                int nPlayersH = intent.getIntExtra( NPLAYERSH, -1 );
                inviteRemote( phone, gameID, gameName, lang, nPlayersT, 
                              nPlayersH);
                break;
            case SEND:
                phone = intent.getStringExtra( PHONE );
                buffer = intent.getByteArrayExtra( BUFFER );
                gameID = intent.getIntExtra( GAMEID, -1 );
                sendPacket( phone, gameID, buffer );
                break;
            }

            result = Service.START_STICKY;
        } else {
            result = Service.START_STICKY_COMPATIBILITY;
        }
        return result;
    }

    @Override
    public IBinder onBind( Intent intent )
    {
        return null;
    }

    private void inviteRemote( String phone, int gameID, String gameName, 
                               int lang, int nPlayersT, int nPlayersH )
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

            send( NBS_CMD.INVITE, bas.toByteArray(), phone );
        } catch ( java.io.IOException ioe ) {
            DbgUtils.logf( "ioe: %s", ioe.toString() );
        }
    }

    public int sendPacket( String phone, int gameID, byte[] buf )
    {
        DbgUtils.logf( "non-static NBSService.sendPacket()" );
        int nSent = -1;
        ByteArrayOutputStream bas = new ByteArrayOutputStream( 128 );
        DataOutputStream das = new DataOutputStream( bas );
        try {
            das.writeInt( gameID );
            das.write( buf, 0, buf.length );
            das.flush();
            if ( send( NBS_CMD.DATA, bas.toByteArray(), phone ) ) {
                nSent = buf.length;
            }
        } catch ( java.io.IOException ioe ) {
            DbgUtils.logf( "ioe: %s", ioe.toString() );
        }
        return nSent;
    }

    private boolean send( NBS_CMD cmd, byte[] data, String phone )
        throws java.io.IOException
    {
        DbgUtils.logf( "non-static NBSService.sendPacket()" );
        int hash = Arrays.hashCode( data );
        DbgUtils.logf( "NBSService: outgoing hash on %d bytes: %X", 
                       data.length, hash );
        ByteArrayOutputStream bas = new ByteArrayOutputStream( 128 );
        DataOutputStream das = new DataOutputStream( bas );
        das.writeByte( 0 );     // protocol
        das.writeByte( cmd.ordinal() );
        das.writeInt( hash );
        das.write( data, 0, data.length );
        das.flush();

        byte[][] msgs = breakAndEncode( bas.toByteArray() );
        return sendBuffers( msgs, phone );
    }

    private byte[][] breakAndEncode( byte[] msg )
        throws java.io.IOException 
    {
        // TODO: as optimization, truncate header when only one packet
        // required

        int count = (msg.length + (MAX_LEN_BIN-1)) / MAX_LEN_BIN;
        byte[][] result = new byte[count][];
        int msgID = ++s_nSent;
        DbgUtils.logf( "preparing %d packets for msgid %x of length %d", 
                       count, msgID, msg.length );

        int start = 0;
        for ( int ii = 0; ii < count; ++ii ) {
            int len = msg.length - start;
            if ( len > MAX_LEN_BIN ) {
                len = MAX_LEN_BIN;
            }

            ByteArrayOutputStream bas = 
                new ByteArrayOutputStream( MAX_LEN_BIN + 16 ); // ~header size
            DataOutputStream das = new DataOutputStream( bas );
            das.writeByte( 0 );
            das.writeShort( msgID );
            das.writeByte( ii );
            das.writeByte( count );
            DbgUtils.logf( "copying %d bytes of msg from %d", 
                           len, start );
            das.write( msg, start, len );
            das.flush();
            result[ii] = bas.toByteArray();
            start += len;
        }
        return result;
    }

    private void receive( NBS_CMD cmd, byte[] data, String phone )
    {
        CommsAddrRec addr = new CommsAddrRec( phone );
        DataInputStream dis = 
            new DataInputStream( new ByteArrayInputStream(data) );
        try {
            switch( cmd ) {
            case INVITE:
                int gameID = dis.readInt();
                String gameName = dis.readUTF();
                int lang = dis.readInt();
                int nPlayersT = dis.readByte();
                int nPlayersH = dis.readByte();

                long rowid = GameUtils.makeNewNBSGame( this, gameID, addr,
                                                       lang, nPlayersT, nPlayersH );

                if ( null != gameName && 0 < gameName.length() ) {
                    DBUtils.setName( this, rowid, gameName );
                }
                String body = Utils.format( this, R.string.new_nbs_bodyf, phone );
                postNotification( gameID, R.string.new_nbs_title, body );
                break;
            case DATA:
                gameID = dis.readInt();
                byte[] rest = new byte[dis.available()];
                dis.read( rest );
                feedMessage( gameID, rest, addr );
                break;
            default:
                Assert.fail();
                break;
            }
        } catch ( java.io.IOException ioe ) {
            DbgUtils.logf( "ioe: %s", ioe.toString() );
        }
    }

    private void receiveBuffer( byte[] msg, String senderPhone )
    {
        DataInputStream dis = 
            new DataInputStream( new ByteArrayInputStream(msg) );
        try {
            byte proto = dis.readByte();
            if ( 0 == proto ) {
                int id = dis.readShort();
                int index = dis.readByte();
                int count = dis.readByte();
                byte[] rest = new byte[dis.available()];
                dis.read( rest );
                tryAssemble( senderPhone, id, index, count, rest );
            } else {
                DbgUtils.logf( "receiveBuffer: bad proto", proto );
            }
        } catch ( java.io.IOException ioe ) {
            DbgUtils.logf( "ioe: %s", ioe.toString() );
        }
    }

    private void tryAssemble( String senderPhone, int id, int index, 
                              int count, byte[] msg )
    {
        if ( index == 0 && count == 1 ) {
            disAssemble( senderPhone, msg );
        } else {
            synchronized( s_partialMsgs ) {
                HashMap <Integer, MsgStore> perPhone = 
                    s_partialMsgs.get( senderPhone );
                if ( null == perPhone ) {
                    perPhone = new HashMap <Integer, MsgStore>();
                    s_partialMsgs.put( senderPhone, perPhone );
                }
                MsgStore store = perPhone.get( id );
                if ( null == store ) {
                    store = new MsgStore( count );
                    perPhone.put( id, store );
                }
                store.add( index, msg );

                if ( store.isComplete() ) {
                    byte[] fullMsg = store.message();
                    perPhone.remove( id );
                    disAssemble( senderPhone, fullMsg );
                }
            }
        }
    }

    private void disAssemble( String senderPhone, byte[] data )
    {
        DbgUtils.logf( "disAssemble()" );
        DataInputStream dis = 
            new DataInputStream( new ByteArrayInputStream(data) );
        try {
            byte proto = dis.readByte();
            if ( 0 != proto ) {
                DbgUtils.logf( "NBSService.disAssemble: bad proto %d; dropping", 
                               proto );
            } else {
                NBS_CMD cmd = NBS_CMD.values()[dis.readByte()];
                int hashRead = dis.readInt();
                DbgUtils.logf( "NBSService: incoming hash: %X", hashRead );
                byte[] rest = new byte[dis.available()];
                dis.read( rest );
                int hashComputed = Arrays.hashCode( rest );
                if ( hashComputed == hashRead ) {
                    DbgUtils.logf( "NBSService: incoming hashes on %d " + 
                                   "bytes match: %X", rest.length, hashRead );
                    receive( cmd, rest, senderPhone );
                } else {
                    DbgUtils.logf( "NBSService: incoming hashes on %d bytes "
                                   + "DON'T match: read: %X; figured: %X", 
                                   rest.length, hashRead, hashComputed );
                }
            }
        } catch ( java.io.IOException ioe ) {
            DbgUtils.logf( "ioe: %s", ioe.toString() );
        }
    }

    private boolean sendBuffers( byte[][] fragments, String phone )
    {
        DbgUtils.logf( "NBSService.sendBuffers()" );
        boolean success = false;
        if ( XWApp.onEmulator() ) {
            DbgUtils.logf( "sendBuffer(phone=%s): FAKING IT", phone );
        } else {
            try {
                PendingIntent sent = 
                    PendingIntent.getBroadcast( this, 0, new Intent(), 0 );
                PendingIntent dlvrd = 
                    PendingIntent.getBroadcast( this, 0, new Intent(), 0 );

                SmsManager mgr = SmsManager.getDefault();
                short port = XWApp.getNBSPort();
                for ( byte[] fragment : fragments ) {
                    String tmp = new String(fragment);
                    DbgUtils.logf( "sending len %d packet: %s", tmp.length(), tmp );
                    mgr.sendDataMessage( phone, null, port, fragment, sent, dlvrd );
                    DbgUtils.logf( "sendDataMessage of %d bytes to %s on %d "
                                   + "finished", fragment.length, phone, port );
                }
                DbgUtils.showf( this, "send %dth msg", ++s_nSent );
                success = true;
            } catch ( IllegalArgumentException iae ) {
                DbgUtils.logf( "%s", iae.toString() );
            } catch ( Exception ee ) {
                DbgUtils.logf( "sendDataMessage message failed: %s", 
                               ee.toString() );
            }
        }
        return success;
    }

    private void feedMessage( int gameID, byte[] msg, CommsAddrRec addr )
    {
        if ( BoardActivity.feedMessage( gameID, msg, addr ) ) {
            // do nothing
        } else {
            long rowid = DBUtils.getRowIDFor( this, gameID );
            if ( DBUtils.ROWID_NOTFOUND != rowid ) {
                NBSMsgSink sink = new NBSMsgSink( this );
                if ( GameUtils.feedMessage( this, rowid, msg, addr, sink ) ) {
                    postNotification( gameID, R.string.new_nbsmove_title, 
                                      getString(R.string.new_move_body)
                                      );
                }
            }
        }
    }

    private void postNotification( int gameID, int title, String body )
    {
        Intent intent = new Intent( this, DispatchNotify.class );
        intent.putExtra( DispatchNotify.GAMEID_EXTRA, gameID );
        Utils.postNotification( this, intent, R.string.new_nbsmove_title, 
                                body );
    }

    private class NBSMsgSink extends MultiMsgSink {
        private Context m_context;
        public NBSMsgSink( Context context ) {
            super();
            m_context = context;
        }

        /***** TransportProcs interface *****/
        public int transportSend( byte[] buf, final CommsAddrRec addr, int gameID )
        {
            DbgUtils.logf( "NBSMsgSink.transportSend()" );
            int sent = -1;
            if ( null != addr ) {
                sent = sendPacket( addr.sms_phone, gameID, buf );
            } else {
                DbgUtils.logf( "NBSMsgSink.transportSend: "
                               + "addr null so not sending" );
            }
            return sent;
        }

        public boolean relayNoConnProc( byte[] buf, String relayID )
        {
            Assert.fail();
            return false;
        }
    }

    private class MsgStore {
        byte[][] m_msgs;
        int m_haveCount;
        int m_fullLength;

        public MsgStore( int count )
        {
            m_msgs = new byte[count][];
            m_fullLength = 0;
        }

        public void add( int index, byte[] msg )
        {
            if ( null == m_msgs[index] ) {
                ++m_haveCount;
                m_fullLength += msg.length;
            }
            m_msgs[index] = msg;
        }
        
        public boolean isComplete()
        {
            boolean complete = m_msgs.length == m_haveCount;
            return complete;
        }

        public byte[] message() 
        {
            byte[] result = new byte[m_fullLength];
            int offset = 0;
            for ( int ii = 0; ii < m_msgs.length; ++ii ) {
                byte[] src = m_msgs[ii];
                int len = src.length;
                System.arraycopy( src, 0, result, offset, len );
                offset += len;
            }
            return result;
        }
    } // class MsgStore
}
