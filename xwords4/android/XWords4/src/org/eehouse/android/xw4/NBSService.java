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
import android.util.Base64;
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

    private static final String PUBLIC_HEADER = "_XW4";

    private static final int MAX_LEN_TEXT = 100;
    private static final int HANDLE = 1;
    private static final int INVITE = 2;
    private static final int SEND = 3;

    private static final String CMD_STR = "CMD";
    private static final String BUFFER = "BUFFER";
    private static final String BINBUFFER = "BINBUFFER";
    private static final String PHONE = "PHONE";
    private static final String GAMEID = "GAMEID";
    private static final String GAMENAME = "GAMENAME";
    private static final String LANG = "LANG";
    private static final String NPLAYERST = "NPLAYERST";
    private static final String NPLAYERSH = "NPLAYERSH";

    // All messages are base64-encoded byte arrays.  The first byte is
    // always one of these.  What follows depends.
    private enum NBS_CMD { NONE, INVITE, DATA, };

    private int m_nReceived = 0;
    private static int s_nSent = 0;
    private static HashMap<String, HashMap <Integer, MsgStore>> s_partialMsgs
        = new HashMap<String, HashMap <Integer, MsgStore>>();

    public static void handleFrom( Context context, String buffer, String phone )
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
                                  int gameID, byte[] binmsg )
    {
        DbgUtils.logf( "NBSService.sendPacket()" );
        Intent intent = getIntentTo( context, SEND );
        intent.putExtra( PHONE, phone );
        intent.putExtra( GAMEID, gameID );
        intent.putExtra( BINBUFFER, binmsg );
        context.startService( intent );
        return binmsg.length;
    }

    public static String toPublicFmt( String msg )
    {
        return PUBLIC_HEADER + msg;
    }

    public static String fromPublicFmt( String msg )
    {
        String result = null;
        if ( msg.startsWith( PUBLIC_HEADER ) ) {
            result = msg.substring( PUBLIC_HEADER.length() );
        }
        return result;
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
                String buffer = intent.getStringExtra( BUFFER );
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
                byte[] bytes = intent.getByteArrayExtra( BINBUFFER );
                gameID = intent.getIntExtra( GAMEID, -1 );
                sendPacket( phone, gameID, bytes );
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

    public int sendPacket( String phone, int gameID, byte[] bytes )
    {
        DbgUtils.logf( "non-static NBSService.sendPacket()" );
        int nSent = -1;
        ByteArrayOutputStream bas = new ByteArrayOutputStream( 128 );
        DataOutputStream das = new DataOutputStream( bas );
        try {
            das.writeInt( gameID );
            das.write( bytes, 0, bytes.length );
            das.flush();
            if ( send( NBS_CMD.DATA, bas.toByteArray(), phone ) ) {
                nSent = bytes.length;
            }
        } catch ( java.io.IOException ioe ) {
            DbgUtils.logf( "ioe: %s", ioe.toString() );
        }
        return nSent;
    }

    private boolean send( NBS_CMD cmd, byte[] bytes, String phone )
        throws java.io.IOException
    {
        DbgUtils.logf( "non-static NBSService.sendPacket()" );
        int hash = Arrays.hashCode( bytes );
        DbgUtils.logf( "NBSService: outgoing hash on %d bytes: %X", 
                       bytes.length, hash );
        ByteArrayOutputStream bas = new ByteArrayOutputStream( 128 );
        DataOutputStream das = new DataOutputStream( bas );
        das.writeByte( 0 );     // protocol
        das.writeByte( cmd.ordinal() );
        das.writeInt( hash );
        das.write( bytes, 0, bytes.length );
        das.flush();

        String as64 = Base64.encodeToString( bas.toByteArray(), Base64.NO_WRAP );
        String[] msgs = breakAndEncode( as64 );
        return sendBuffers( msgs, phone );
    }

    private String[] breakAndEncode( String msg ) 
        throws java.io.IOException 
    {
        // TODO: as optimization, truncate header when only one packet
        // required
        Assert.assertFalse( msg.contains(":") );
        int count = (msg.length() + (MAX_LEN_TEXT-1)) / MAX_LEN_TEXT;
        String[] result = new String[count];
        int msgID = ++s_nSent % 0x000000FF;
        DbgUtils.logf( "preparing %d packets for msgid %x", count, msgID );

        int start = 0;
        int end = 0;
        for ( int ii = 0; ii < count; ++ii ) {
            int len = msg.length() - end;
            if ( len > MAX_LEN_TEXT ) {
                len = MAX_LEN_TEXT;
            }
            end += len;
            result[ii] = String.format( "0:%X:%X:%X:%s", msgID, ii, count, 
                                        msg.substring( start, end ) );
            DbgUtils.logf( "fragment[%d]: %s", ii, result[ii] );
            start = end;
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

    private void receiveBuffer( String as64, String senderPhone )
    {
        DbgUtils.logf( "receiveBuffer(%s)", as64 );
        String[] parts = as64.split( ":" );
        DbgUtils.logf( "receiveBuffer: got %d parts", parts.length );
        for ( String part : parts ) {
            DbgUtils.logf( "part: %s", part );
        }
        Assert.assertTrue( 5 == parts.length );
        if ( 5 == parts.length ) {
            byte proto = Byte.valueOf( parts[0], 10 );
            int id = Integer.valueOf( parts[1], 16 );
            int index = Integer.valueOf( parts[2], 16 );
            int count = Integer.valueOf( parts[3], 16 );
            tryAssemble( senderPhone, id, index, count, parts[4] );
        }
    }

    private void tryAssemble( String senderPhone, int id, int index, 
                              int count, String msg )
    {
        if ( index == 0 && count == 1 ) {
            disAssemble( senderPhone, msg.getBytes() );
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
                    store = new MsgStore( id, count );
                    perPhone.put( id, store );
                }
                store.add( index, msg );

                if ( store.isComplete() ) {
                    s_partialMsgs.remove( id );
                    byte[] fullMsg = store.message();
                    perPhone.remove( id );
                    disAssemble( senderPhone, fullMsg );
                }
            }
        }
    }

    private void disAssemble( String senderPhone, byte[] fullMsg )
    {
        DbgUtils.logf( "disAssemble()" );
        byte[] data = Base64.decode( fullMsg, Base64.NO_WRAP );
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

    private boolean sendBuffers( String[] fragments, String phone )
    {
        DbgUtils.logf( "NBSService.sendBuffers()" );
        boolean success = false;
        if ( XWApp.onEmulator() ) {
            DbgUtils.logf( "sendBuffer(phone=%s): FAKING IT", phone );
        } else {
            try {
                SmsManager mgr = SmsManager.getDefault();
                for ( String fragment : fragments ) {
                    DbgUtils.logf( "sending len %d packet: %s", 
                                   fragment.length(), fragment );
                    String asPublic = toPublicFmt( fragment );
                    mgr.sendTextMessage( phone, null, asPublic, null, null );
                    DbgUtils.logf( "Message \"%s\" of %d bytes sent to %s.", 
                                   asPublic, asPublic.length(), phone );
                }
                DbgUtils.showf( this, "sent %dth msg", ++s_nSent );
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
            return sendPacket( addr.sms_phone, gameID, buf );
        }

        public boolean relayNoConnProc( byte[] buf, String relayID )
        {
            Assert.fail();
            return false;
        }
    }

    private class MsgStore {
        String[] m_msgs;
        int m_msgID;
        int m_haveCount;
        int m_fullLength;

        public MsgStore( int id, int count )
        {
            m_msgID = id;
            m_msgs = new String[count];
            m_fullLength = 0;
        }

        public void add( int index, String msg )
        {
            if ( null == m_msgs[index] ) {
                ++m_haveCount;
                m_fullLength += msg.length();
            }
            m_msgs[index] = msg;
        }
        
        public boolean isComplete()
        {
            boolean complete = m_msgs.length == m_haveCount;
            DbgUtils.logf( "isComplete(msg %d)=>%b", m_msgID, complete );
            return complete;
        }

        public byte[] message() 
        {
            byte[] result = new byte[m_fullLength];
            int offset = 0;
            for ( int ii = 0; ii < m_msgs.length; ++ii ) {
                byte[] src = m_msgs[ii].getBytes();
                System.arraycopy( src, 0, result, offset, src.length );
                offset += src.length;
            }
            return result;
        }
    }
}
