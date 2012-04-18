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
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.IBinder;
import android.preference.PreferenceManager;
import android.telephony.SmsManager;
import android.telephony.SmsMessage;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.OutputStream;
import java.lang.System;
import java.util.Arrays;
import java.util.HashMap;
import junit.framework.Assert;

import org.eehouse.android.xw4.MultiService.MultiEvent;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.CommonPrefs;
import org.eehouse.android.xw4.jni.XwJNI;

public class SMSService extends Service {

    private static final String INSTALL_URL = "http://eehouse.org/_/aa.htm ";
    private static final int MAX_SMS_LEN = 140; // ??? differs by network

    private static final int MAX_LEN_TEXT = 100;
    private static final int HANDLE = 1;
    private static final int INVITE = 2;
    private static final int SEND = 3;
    private static final int REMOVE = 4;
    private static final int MESG_GAMEGONE = 5;
    private static final int CHECK_MSGDB = 6;

    private static final String CMD_STR = "CMD";
    private static final String BUFFER = "BUFFER";
    private static final String BINBUFFER = "BINBUFFER";
    private static final String PHONE = "PHONE";
    private static final String GAMEID = "GAMEID";
    private static final String GAMENAME = "GAMENAME";
    private static final String LANG = "LANG";
    private static final String NPLAYERST = "NPLAYERST";
    private static final String NPLAYERSH = "NPLAYERSH";

    private static Boolean s_showToasts = null;
    private static MultiService s_srcMgr = new MultiService();
    private static boolean s_dbCheckPending = true;

    // All messages are base64-encoded byte arrays.  The first byte is
    // always one of these.  What follows depends.
    private enum SMS_CMD { NONE, INVITE, DATA, DEATH, };

    private int m_nReceived = 0;
    private static int s_nSent = 0;
    private static HashMap<String, HashMap <Integer, MsgStore>> s_partialMsgs
        = new HashMap<String, HashMap <Integer, MsgStore>>();

    public static void smsToastEnable( boolean newVal ) 
    {
        s_showToasts = newVal;
    }

    public static void checkForInvites( Context context )
    {
        if ( XWApp.SMSSUPPORTED ) {
            Intent intent = getIntentTo( context, CHECK_MSGDB );
            context.startService( intent );
        }
    }

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
        Intent intent = getIntentTo( context, SEND );
        intent.putExtra( PHONE, phone );
        intent.putExtra( GAMEID, gameID );
        intent.putExtra( BINBUFFER, binmsg );
        context.startService( intent );
        return binmsg.length;
    }

    public static void gameDied( Context context, int gameID, String phone )
    {
        Intent intent = getIntentTo( context, REMOVE );
        intent.putExtra( PHONE, phone );
        intent.putExtra( GAMEID, gameID );
        context.startService( intent );
    }

    public static String toPublicFmt( String msg )
    {
        String result;
        int msglen = msg.length() + 1 + XWApp.SMS_PUBLIC_HEADER.length();
        int urllen = INSTALL_URL.length();
        result = String.format( "%s %s%s", XWApp.SMS_PUBLIC_HEADER, 
                                msglen + urllen < MAX_SMS_LEN ? INSTALL_URL : "",
                                msg );
        return result;
    }

    public static String fromPublicFmt( String msg )
    {
        String result = null;
        if ( null != msg && msg.startsWith( XWApp.SMS_PUBLIC_HEADER ) ) {
            int index = msg.lastIndexOf( " " );
            if ( 0 <= index ) {
                result = msg.substring( index + 1 );
            }
        }
        return result;
    }

    public static MultiService getMultiEventSrc()
    {
        return s_srcMgr;
    }

    private static Intent getIntentTo( Context context, int cmd )
    {
        if ( null == s_showToasts ) {
            SharedPreferences sp
                = PreferenceManager.getDefaultSharedPreferences( context );
            String key = context.getString( R.string.key_show_sms );
            s_showToasts = sp.getBoolean( key, false );
        }

        Intent intent = new Intent( context, SMSService.class );
        intent.putExtra( CMD_STR, cmd );
        return intent;
    }

    @Override
    public void onCreate()
    {
        if ( XWApp.SMSSUPPORTED ) {
        } else {
            stopSelf();
        }
    }

    @Override
    public int onStartCommand( Intent intent, int flags, int startId )
    {
        int result;
        if ( XWApp.SMSSUPPORTED && null != intent ) {
            int cmd = intent.getIntExtra( CMD_STR, -1 );
            switch( cmd ) {
            case CHECK_MSGDB:
                if ( s_dbCheckPending ) {
                    if ( Utils.firstBootEver( this ) ) {
                        new Thread( new Runnable() {
                            public void run() {
                                checkMsgDB();
                            } 
                        } ).start();
                    }
                    s_dbCheckPending = false;
                }
                break;
            case HANDLE:
                ++m_nReceived;
                if ( s_showToasts ) {
                    DbgUtils.showf( this, "got %dth msg", m_nReceived );
                }
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
            case REMOVE:
                gameID = intent.getIntExtra( GAMEID, -1 );
                phone = intent.getStringExtra( PHONE );
                sendDiedPacket( phone, gameID );
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

            send( SMS_CMD.INVITE, bas.toByteArray(), phone );
        } catch ( java.io.IOException ioe ) {
            DbgUtils.logf( "ioe: %s", ioe.toString() );
        }
    }

    private void sendDiedPacket( String phone, int gameID )
    {
        ByteArrayOutputStream bas = new ByteArrayOutputStream( 32 );
        DataOutputStream das = new DataOutputStream( bas );
        try {
            das.writeInt( gameID );
            das.flush();
            send( SMS_CMD.DEATH, bas.toByteArray(), phone );
        } catch ( java.io.IOException ioe ) {
            DbgUtils.logf( "ioe: %s", ioe.toString() );
        }
    }

    public int sendPacket( String phone, int gameID, byte[] bytes )
    {
        DbgUtils.logf( "non-static SMSService.sendPacket()" );
        int nSent = -1;
        ByteArrayOutputStream bas = new ByteArrayOutputStream( 128 );
        DataOutputStream das = new DataOutputStream( bas );
        try {
            das.writeInt( gameID );
            das.write( bytes, 0, bytes.length );
            das.flush();
            if ( send( SMS_CMD.DATA, bas.toByteArray(), phone ) ) {
                nSent = bytes.length;
            }
        } catch ( java.io.IOException ioe ) {
            DbgUtils.logf( "ioe: %s", ioe.toString() );
        }
        return nSent;
    }

    private boolean send( SMS_CMD cmd, byte[] bytes, String phone )
        throws java.io.IOException
    {
        DbgUtils.logf( "non-static SMSService.sendPacket()" );
        int hash = Arrays.hashCode( bytes );
        DbgUtils.logf( "SMSService: outgoing hash on %d bytes: %X", 
                       bytes.length, hash );
        ByteArrayOutputStream bas = new ByteArrayOutputStream( 128 );
        DataOutputStream das = new DataOutputStream( bas );
        das.writeByte( 0 );     // protocol
        das.writeByte( cmd.ordinal() );
        das.writeInt( hash );
        das.write( bytes, 0, bytes.length );
        das.flush();

        String as64 = XwJNI.base64Encode( bas.toByteArray() );
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

    private void receive( SMS_CMD cmd, byte[] data, String phone )
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

                long rowid = GameUtils.makeNewSMSGame( this, gameID, addr,
                                                       lang, nPlayersT, nPlayersH );

                if ( null != gameName && 0 < gameName.length() ) {
                    DBUtils.setName( this, rowid, gameName );
                }
                String body = Utils.format( this, R.string.new_sms_bodyf, phone );
                postNotification( gameID, R.string.new_sms_title, body );
                break;
            case DATA:
                gameID = dis.readInt();
                byte[] rest = new byte[dis.available()];
                dis.read( rest );
                feedMessage( gameID, rest, addr );
                break;
            case DEATH:
                gameID = dis.readInt();
                s_srcMgr.sendResult( MultiEvent.MESSAGE_NOGAME, gameID );
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
        String[] parts = as64.split( ":" );
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
            disAssemble( senderPhone, msg );
        } else {
            // required?  Should always be in main thread.
            synchronized( s_partialMsgs ) { 
                HashMap<Integer, MsgStore> perPhone = 
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

                if ( store.add( index, msg ).isComplete() ) {
                    disAssemble( senderPhone, store.message() );
                    perPhone.remove( id );
                }
            }
        }
    }

    private void disAssemble( String senderPhone, String fullMsg )
    {
        DbgUtils.logf( "disAssemble()" );
        byte[] data = XwJNI.base64Decode( fullMsg );
        DataInputStream dis = 
            new DataInputStream( new ByteArrayInputStream(data) );
        try {
            byte proto = dis.readByte();
            if ( 0 != proto ) {
                DbgUtils.logf( "SMSService.disAssemble: bad proto %d; dropping", 
                               proto );
            } else {
                SMS_CMD cmd = SMS_CMD.values()[dis.readByte()];
                int hashRead = dis.readInt();
                DbgUtils.logf( "SMSService: incoming hash: %X", hashRead );
                byte[] rest = new byte[dis.available()];
                dis.read( rest );
                int hashComputed = Arrays.hashCode( rest );
                if ( hashComputed == hashRead ) {
                    DbgUtils.logf( "SMSService: incoming hashes on %d " + 
                                   "bytes match: %X", rest.length, hashRead );
                    receive( cmd, rest, senderPhone );
                } else {
                    DbgUtils.logf( "SMSService: incoming hashes on %d bytes "
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
        DbgUtils.logf( "SMSService.sendBuffers()" );
        boolean success = false;
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
            if ( s_showToasts ) {
                DbgUtils.showf( this, "sent %dth msg", s_nSent );
            }
            success = true;
        } catch ( IllegalArgumentException iae ) {
            DbgUtils.logf( "%s", iae.toString() );
        } catch ( Exception ee ) {
            DbgUtils.logf( "sendDataMessage message failed: %s", 
                           ee.toString() );
        }
        return success;
    }

    private void feedMessage( int gameID, byte[] msg, CommsAddrRec addr )
    {
        long rowid = DBUtils.getRowIDFor( this, gameID );
        if ( DBUtils.ROWID_NOTFOUND == rowid ) {
            sendDiedPacket( addr.sms_phone, gameID );
        } else if ( BoardActivity.feedMessage( gameID, msg, addr ) ) {
            // do nothing
        } else {
            SMSMsgSink sink = new SMSMsgSink( this );
            if ( GameUtils.feedMessage( this, rowid, msg, addr, sink ) ) {
                postNotification( gameID, R.string.new_smsmove_title, 
                                  getString(R.string.new_move_body)
                                  );
            }
        }
    }

    private void postNotification( int gameID, int title, String body )
    {
        Intent intent = new Intent( this, DispatchNotify.class );
        intent.putExtra( DispatchNotify.GAMEID_EXTRA, gameID );
        Utils.postNotification( this, intent, title, body, gameID );
    }

    // Runs in separate thread
    private void checkMsgDB()
    {
        Uri uri = Uri.parse( "content://sms/inbox" );
        String[] columns = new String[] { "body","address" };
        Cursor cursor = getContentResolver().query( uri, columns,
                                                    null, null, null );
        try {
            int bodyIndex = cursor.getColumnIndexOrThrow( "body" );
            int phoneIndex = cursor.getColumnIndexOrThrow( "address" );
            while ( cursor.moveToNext() ) {
                String msg = fromPublicFmt( cursor.getString( bodyIndex ) );
                if ( null != msg ) {
                    String number = cursor.getString( phoneIndex );
                    if ( null != number ) {
                        handleFrom( this, msg, number );
                    }
                }
            }
            cursor.close();
        } catch ( Exception ee ) {
            DbgUtils.logf( "checkMsgDB: %s", ee.toString() );
        }
    }

    private class SMSMsgSink extends MultiMsgSink {
        private Context m_context;
        public SMSMsgSink( Context context ) {
            super();
            m_context = context;
        }

        /***** TransportProcs interface *****/
        public int transportSend( byte[] buf, final CommsAddrRec addr, int gameID )
        {
            int nSent = -1;
            DbgUtils.logf( "SMSMsgSink.transportSend()" );
            if ( null != addr ) {
                nSent = sendPacket( addr.sms_phone, gameID, buf );
            } else {
                DbgUtils.logf( "SMSMsgSink.transportSend: "
                               + "addr null so not sending" );
            }
            return nSent;
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

        public MsgStore add( int index, String msg )
        {
            if ( null == m_msgs[index] ) {
                ++m_haveCount;
                m_fullLength += msg.length();
            }
            m_msgs[index] = msg;
            return this;
        }
        
        public boolean isComplete()
        {
            boolean complete = m_msgs.length == m_haveCount;
            DbgUtils.logf( "isComplete(msg %d)=>%b", m_msgID, complete );
            return complete;
        }

        public String message() 
        {
            StringBuffer sb = new StringBuffer(m_fullLength);
            for ( String msg : m_msgs ) {
                sb.append( msg );
            }
            return sb.toString();
        }
    }
}
