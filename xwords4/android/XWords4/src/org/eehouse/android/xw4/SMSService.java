/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
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

import android.app.Activity;
import android.app.PendingIntent;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
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
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import junit.framework.Assert;

import org.eehouse.android.xw4.MultiService.MultiEvent;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.LastMoveInfo;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

public class SMSService extends XWService {

    private static final String INSTALL_URL = "http://eehouse.org/_/a.py/a ";
    private static final int MAX_SMS_LEN = 140; // ??? differs by network

    private static final String MSG_SENT = "MSG_SENT";
    private static final String MSG_DELIVERED = "MSG_DELIVERED";

    private static final int SMS_PROTO_VERSION = 0;
    private static final int MAX_LEN_TEXT = 100;
    private static final int MAX_LEN_BINARY = 100;
    private static final int HANDLE = 1;
    private static final int INVITE = 2;
    private static final int SEND = 3;
    private static final int REMOVE = 4;
    // private static final int MESG_GAMEGONE = 5;
    private static final int CHECK_MSGDB = 6;
    private static final int ADDED_MISSING = 7;
    private static final int STOP_SELF = 8;
    private static final int HANDLEDATA = 9;

    private static final String CMD_STR = "CMD";
    private static final String BUFFER = "BUFFER";
    private static final String BINBUFFER = "BINBUFFER";
    private static final String PHONE = "PHONE";
    private static final String GAMEDATA_STR = "GD";

    private static Boolean s_showToasts = null;
    private static Map<String,Boolean> s_phoneRecs 
        = new HashMap<String,Boolean>();
    
    // All messages are base64-encoded byte arrays.  The first byte is
    // always one of these.  What follows depends.
    private enum SMS_CMD { NONE, INVITE, DATA, DEATH, ACK, };

    private BroadcastReceiver m_sentReceiver;
    private BroadcastReceiver m_receiveReceiver;
    private OnSharedPreferenceChangeListener m_prefsListener;

    private int m_nReceived = 0;
    private static int s_nSent = 0;
    private static Map<String, HashMap <Integer, MsgStore>> s_partialMsgs
        = new HashMap<String, HashMap <Integer, MsgStore>>();
    private static Set<Integer> s_sentDied = new HashSet<Integer>();

    public static void smsToastEnable( boolean newVal ) 
    {
        s_showToasts = newVal;
    }

    public static void registerPhone( String phone, boolean isGSM )
    {
        Boolean val = s_phoneRecs.get( phone );
        if ( null == val || val != isGSM ) {
            DbgUtils.logf( "SMSService.registerPhone: making rec for %s", phone );
            val = new Boolean( isGSM );
            s_phoneRecs.put( phone, val );
            saveRecs();
        }
    }

    public static void registerPhone( NetLaunchInfo nli )
    {
        if ( nli.contains( CommsConnType.COMMS_CONN_SMS ) ) {
            registerPhone( nli.phone, nli.isGSM );
        }
    }

    public static void checkForInvites( Context context )
    {
        if ( XWApp.SMSSUPPORTED && Utils.deviceSupportsSMS( context ) ) {
            Intent intent = getIntentTo( context, CHECK_MSGDB );
            context.startService( intent );
        }
    }

    public static void stopService( Context context )
    {
        Intent intent = getIntentTo( context, STOP_SELF );
        context.startService( intent );
    }

    // Regular text message case
    public static void handleFrom( Context context, String buffer, 
                                   String phone )
    {
        Intent intent = getIntentTo( context, HANDLE );
        intent.putExtra( BUFFER, buffer );
        intent.putExtra( PHONE, phone );
        context.startService( intent );

        registerPhone( phone, false );
    }

    // NBS case
    public static void handleFrom( Context context, byte[] buffer, 
                                   String phone )
    {
        Intent intent = getIntentTo( context, HANDLEDATA );
        intent.putExtra( BUFFER, buffer );
        intent.putExtra( PHONE, phone );
        context.startService( intent );

        registerPhone( phone, true );
    }

    public static void inviteRemote( Context context, String phone,
                                     NetLaunchInfo nli )
    {
        Intent intent = getIntentTo( context, INVITE );
        intent.putExtra( PHONE, phone );
        String asString = nli.toString();
        DbgUtils.logf( "SMSService.inviteRemote(%s, '%s')", phone, asString );
        intent.putExtra( GAMEDATA_STR, asString );
        context.startService( intent );
    }

    public static int sendPacket( Context context, String phone, 
                                  int gameID, byte[] binmsg )
    {
        int nSent = -1;
        if ( XWPrefs.getSMSEnabled( context ) ) {
            Intent intent = getIntentTo( context, SEND );
            intent.putExtra( PHONE, phone );
            intent.putExtra( MultiService.GAMEID, gameID );
            intent.putExtra( BINBUFFER, binmsg );
            context.startService( intent );
            nSent = binmsg.length;
        } else {
            DbgUtils.logf( "sendPacket: dropping because SMS disabled" );
        }
        return nSent;
    }

    public static void gameDied( Context context, int gameID, String phone )
    {
        Intent intent = getIntentTo( context, REMOVE );
        intent.putExtra( PHONE, phone );
        intent.putExtra( MultiService.GAMEID, gameID );
        context.startService( intent );
    }

    public static void onGameDictDownload( Context context, Intent intentOld )
    {
        Intent intent = getIntentTo( context, ADDED_MISSING );
        intent.fillIn( intentOld, 0 );
        context.startService( intent );
    }

    public static String toPublicFmt( String msg )
    {
        int msglen = XWApp.SMS_PUBLIC_HEADER.length() + 4 + 1 + msg.length();
        int urllen = INSTALL_URL.length();
        String result = String.format( "%s%04X %s%s", XWApp.SMS_PUBLIC_HEADER, 
                                msg.hashCode() & 0xFFFF,
                                msglen + urllen < MAX_SMS_LEN? INSTALL_URL : "",
                                msg );
        return result;
    }

    public static String fromPublicFmt( String msg )
    {
        String result = null;
        if ( null != msg && msg.startsWith( XWApp.SMS_PUBLIC_HEADER ) ) {
            // Number format exception etc. can result from malicious
            // messages.  Be safe: use try;
            try {
                String tmp = msg.substring( 1 + msg.lastIndexOf( " " ) );

                int headerLen = XWApp.SMS_PUBLIC_HEADER.length();
                String hashString = 
                    msg.substring( headerLen, headerLen + 4 );
                int hashRead = Integer.parseInt( hashString, 16 );
                int hashCode = 0xFFFF & tmp.hashCode();
                if ( hashRead == hashCode ) {
                    result = tmp;
                } else {
                    DbgUtils.logf( "fromPublicFmt: hash code mismatch" );
                }
            } catch( Exception e ) {
            }
        }
        return result;
    }

    private static Intent getIntentTo( Context context, int cmd )
    {
        if ( null == s_showToasts ) {
            s_showToasts = 
                XWPrefs.getPrefsBoolean( context, R.string.key_show_sms, false );
        }

        Intent intent = new Intent( context, SMSService.class );
        intent.putExtra( CMD_STR, cmd );
        return intent;
    }

    @Override
    public void onCreate()
    {
        if ( XWApp.SMSSUPPORTED && Utils.deviceSupportsSMS( this ) ) {
            registerReceivers();
        } else {
            stopSelf();
        }
    }

    @Override
    public void onDestroy()
    {
        if ( null != m_sentReceiver ) {
            unregisterReceiver( m_sentReceiver );
            m_sentReceiver = null;
        }
        if ( null != m_receiveReceiver ) {
            unregisterReceiver( m_receiveReceiver );
            m_receiveReceiver = null;
        }
        if ( null != m_prefsListener ) {
            SharedPreferences sp
                = PreferenceManager.getDefaultSharedPreferences( this );
            sp.unregisterOnSharedPreferenceChangeListener( m_prefsListener );
            m_prefsListener = null;
        }

        super.onDestroy();
    }

    @Override
    public int onStartCommand( Intent intent, int flags, int startId )
    {
        int result = Service.START_NOT_STICKY;
        if ( XWApp.SMSSUPPORTED && null != intent ) {
            int cmd = intent.getIntExtra( CMD_STR, -1 );
            switch( cmd ) {
            case STOP_SELF:
                stopSelf();
                break;
            case CHECK_MSGDB:
                if ( ! XWPrefs.getHaveCheckedSMS( this ) ) {
                    XWPrefs.setHaveCheckedSMS( this, true );
                    new Thread( new Runnable() {
                            public void run() {
                                checkMsgDB();
                            } 
                        } ).start();
                }
                break;
            case HANDLE:
            case HANDLEDATA:
                ++m_nReceived;
                ConnStatusHandler.
                    updateStatusIn( this, null,
                                    CommsConnType.COMMS_CONN_SMS, true );
                if ( s_showToasts ) {
                    DbgUtils.showf( this, "got %dth msg", m_nReceived );
                }
                String phone = intent.getStringExtra( PHONE );
                if ( HANDLE == cmd ) {
                    String buffer = intent.getStringExtra( BUFFER );
                    receiveBuffer( buffer, phone );
                } else {
                    byte[] buffer = intent.getByteArrayExtra( BUFFER );
                    receiveBuffer( buffer, phone );
                }
                break;
            case INVITE:
                phone = intent.getStringExtra( PHONE );
                inviteRemote( phone, intent.getStringExtra( GAMEDATA_STR ) );
                break;
            case ADDED_MISSING:
                phone = intent.getStringExtra( PHONE );
                int gameID = intent.getIntExtra( MultiService.GAMEID, -1 );
                String gameName = intent.getStringExtra( MultiService.GAMENAME );
                int lang = intent.getIntExtra( MultiService.LANG, -1 );
                String dict = intent.getStringExtra( MultiService.DICT );
                int nPlayersT = intent.getIntExtra( MultiService.NPLAYERST, -1 );
                int nPlayersH = intent.getIntExtra( MultiService.NPLAYERSH, -1 );
                makeForInvite( phone, gameID, gameName, lang, dict, 
                               nPlayersT, nPlayersH, 1 );
                break;
            case SEND:
                phone = intent.getStringExtra( PHONE );
                byte[] bytes = intent.getByteArrayExtra( BINBUFFER );
                gameID = intent.getIntExtra( MultiService.GAMEID, -1 );
                sendPacket( phone, gameID, bytes );
                break;
            case REMOVE:
                gameID = intent.getIntExtra( MultiService.GAMEID, -1 );
                phone = intent.getStringExtra( PHONE );
                sendDiedPacket( phone, gameID );
                break;
            }

            result = Service.START_STICKY;
        }
        
        if ( Service.START_NOT_STICKY == result 
             || !XWPrefs.getSMSEnabled( this ) ) {
            stopSelf( startId );
        }

        return result;
    } // onStartCommand

    // Send as a regular (visible) SMS unless the phone is known
    private void inviteRemote( String phone, String nliData )
    {
        DbgUtils.logf( "inviteRemote()" );
        Boolean useData = getPhoneDoesData( phone );
        if ( null == useData ) { // unknown; send a text
            sendVisibleInvite( phone, nliData );
        } else {
            NetLaunchInfo nli = new NetLaunchInfo( nliData );
            ByteArrayOutputStream bas = new ByteArrayOutputStream( 128 );
            DataOutputStream das = new DataOutputStream( bas );
            try {
                das.writeInt( nli.gameID );
                das.writeUTF( nli.gameName );
                das.writeInt( nli.lang );
                das.writeUTF( nli.dict );
                das.writeByte( nli.nPlayersT );
                das.writeByte( nli.nPlayersH );
                das.flush();

                send( SMS_CMD.INVITE, bas.toByteArray(), phone );
            } catch ( java.io.IOException ioe ) {
                DbgUtils.loge( ioe );
            }
        }
    }

    private void ackInvite( String phone, int gameID )
    {
        ByteArrayOutputStream bas = new ByteArrayOutputStream( 128 );
        DataOutputStream das = new DataOutputStream( bas );
        try {
            das.writeInt( gameID );
            das.flush();

            send( SMS_CMD.ACK, bas.toByteArray(), phone );
        } catch ( java.io.IOException ioe ) {
            DbgUtils.loge( ioe );
        }
    }

    private void sendDiedPacket( String phone, int gameID )
    {
        if ( !s_sentDied.contains(gameID) ) {
            ByteArrayOutputStream bas = new ByteArrayOutputStream( 32 );
            DataOutputStream das = new DataOutputStream( bas );
            try {
                das.writeInt( gameID );
                das.flush();
                send( SMS_CMD.DEATH, bas.toByteArray(), phone );
                s_sentDied.add( gameID );
            } catch ( java.io.IOException ioe ) {
                DbgUtils.loge( ioe );
            }
        }
    }

    public int sendPacket( String phone, int gameID, byte[] bytes )
    {
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
            DbgUtils.loge( ioe );
        }
        return nSent;
    }

    private boolean send( SMS_CMD cmd, byte[] bytes, String phone )
        throws java.io.IOException
    {
        ByteArrayOutputStream bas = new ByteArrayOutputStream( 128 );
        DataOutputStream das = new DataOutputStream( bas );
        das.writeByte( SMS_PROTO_VERSION );
        das.writeByte( cmd.ordinal() );
        das.write( bytes, 0, bytes.length );
        das.flush();

        byte[] data = bas.toByteArray();
        boolean result = false;
        Boolean asData = getPhoneDoesData( phone );
        if ( null != asData ) {
            if ( asData ) {
                byte[][] msgs = breakAndEncode( data );
                result = sendBuffers( msgs, phone, data );
            } else {
                result = sendAsText( data, phone );
            }
        }
        return result;
    }

    // Eventually this should read from a DB we keep of known phone
    // capabilities
    private Boolean getPhoneDoesData( String phone )
    {
        Boolean result = null == s_phoneRecs ? null : s_phoneRecs.get( phone );
        if ( null == result ) {
            DbgUtils.logf( "getPhoneDoesData: no record for phone %s", phone );
        }
        DbgUtils.logf( "getPhoneDoesData(%s) => %b", phone, result );
        return result;
    }

    private String[] breakAndEncode( String msg ) throws java.io.IOException 
    {
        // TODO: as optimization, truncate header when only one packet
        // required
        Assert.assertFalse( msg.contains(":") );
        int count = (msg.length() + (MAX_LEN_TEXT-1)) / MAX_LEN_TEXT;
        String[] result = new String[count];
        int msgID = ++s_nSent % 0x000000FF;

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
            start = end;
        }
        return result;
    }

    private byte[][] breakAndEncode( byte msg[] ) throws java.io.IOException 
    {
        int count = (msg.length + (MAX_LEN_BINARY-1)) / MAX_LEN_BINARY;
        byte[][] result = new byte[count][];
        int msgID = ++s_nSent % 0x000000FF;

        int start = 0;
        int end = 0;
        for ( int ii = 0; ii < count; ++ii ) {
            int len = msg.length - end;
            if ( len > MAX_LEN_BINARY ) {
                len = MAX_LEN_BINARY;
            }
            end += len;
            byte[] part = new byte[4 + len]; 
            part[0] = (byte)0;  // proto
            part[1] = (byte)msgID;
            part[2] = (byte)ii;
            part[3] = (byte)count;
            System.arraycopy( msg, start, part, 4, len );

            result[ii] = part;
            start = end;
        }
        return result;
    }

    private void receive( SMS_CMD cmd, byte[] data, String phone )
    {
        DataInputStream dis = 
            new DataInputStream( new ByteArrayInputStream(data) );
        try {
            switch( cmd ) {
            case INVITE:
                int gameID = dis.readInt();
                String gameName = dis.readUTF();
                int lang = dis.readInt();
                String dict = dis.readUTF();
                int nPlayersT = dis.readByte();
                int nPlayersH = dis.readByte();
                
                if ( DictLangCache.haveDict( this, lang, dict ) ) {
                    makeForInvite( phone, gameID, gameName, lang, dict, 
                                   nPlayersT, nPlayersH, 1 );
                } else {
                    Intent intent = MultiService
                        .makeMissingDictIntent( this, gameName, lang, dict, 
                                                nPlayersT, nPlayersH );
                    intent.putExtra( PHONE, phone );
                    intent.putExtra( MultiService.OWNER, 
                                     MultiService.OWNER_SMS );
                    intent.putExtra( MultiService.INVITER, 
                                     Utils.phoneToContact( this, phone, true ) );
                    intent.putExtra( MultiService.GAMEID, gameID );
                    MultiService.postMissingDictNotification( this, intent, 
                                                              gameID );
                }
                break;
            case DATA:
                gameID = dis.readInt();
                byte[] rest = new byte[dis.available()];
                dis.read( rest );
                feedMessage( gameID, rest, new CommsAddrRec( phone ) );
                break;
            case DEATH:
                gameID = dis.readInt();
                sendResult( MultiEvent.MESSAGE_NOGAME, gameID );
                break;
            case ACK:
                gameID = dis.readInt();
                sendResult( MultiEvent.NEWGAME_SUCCESS, 
                                     gameID );
                break;
            default:
                DbgUtils.logf( "unexpected cmd %s", cmd.toString() );
                break;
            }
        } catch ( java.io.IOException ioe ) {
            DbgUtils.loge( ioe );
        }
    }

    private void receiveBuffer( byte[] buffer, String senderPhone )
    {
        byte proto = buffer[0];
        int id = buffer[1];
        int index = buffer[2];
        int count = buffer[3];
        byte[] rest = new byte[buffer.length - 4];
        System.arraycopy( buffer, 4, rest, 0, rest.length );
        tryAssemble( senderPhone, id, index, count, rest );
            
        sendResult( MultiEvent.SMS_RECEIVE_OK );
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
            
            sendResult( MultiEvent.SMS_RECEIVE_OK );
        }
    }

    private void tryAssemble( String senderPhone, int id, int index, 
                              int count, byte[] msg )
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
                    store = new MsgStore( id, count, false );
                    perPhone.put( id, store );
                }

                if ( store.add( index, msg ).isComplete() ) {
                    disAssemble( senderPhone, store.messageData() );
                    perPhone.remove( id );
                }
            }
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
                    store = new MsgStore( id, count, true );
                    perPhone.put( id, store );
                }

                if ( store.add( index, msg ).isComplete() ) {
                    disAssemble( senderPhone, store.messageText() );
                    perPhone.remove( id );
                }
            }
        }
    }

    private void disAssemble( String senderPhone, byte[] fullMsg )
    {
        DataInputStream dis = 
            new DataInputStream( new ByteArrayInputStream(fullMsg) );
        try {
            byte proto = dis.readByte();
            if ( SMS_PROTO_VERSION != proto ) {
                DbgUtils.logf( "SMSService.disAssemble: bad proto %d; dropping", 
                               proto );
            } else {
                SMS_CMD cmd = SMS_CMD.values()[dis.readByte()];
                byte[] rest = new byte[dis.available()];
                dis.read( rest );
                receive( cmd, rest, senderPhone );
            }
        } catch ( java.io.IOException ioe ) {
            DbgUtils.loge( ioe );
        } catch ( ArrayIndexOutOfBoundsException oob ) {
            // enum this older code doesn't know about; drop it
            DbgUtils.logf( "disAssemble: dropping message with too-new enum" );
        }
    }

    private void disAssemble( String senderPhone, String fullMsg )
    {
        byte[] data = XwJNI.base64Decode( fullMsg );
        DataInputStream dis = 
            new DataInputStream( new ByteArrayInputStream(data) );
        try {
            byte proto = dis.readByte();
            if ( SMS_PROTO_VERSION != proto ) {
                DbgUtils.logf( "SMSService.disAssemble: bad proto %d; dropping", 
                               proto );
            } else {
                SMS_CMD cmd = SMS_CMD.values()[dis.readByte()];
                byte[] rest = new byte[dis.available()];
                dis.read( rest );
                receive( cmd, rest, senderPhone );
            }
        } catch ( java.io.IOException ioe ) {
            DbgUtils.loge( ioe );
        } catch ( ArrayIndexOutOfBoundsException oob ) {
            // enum this older code doesn't know about; drop it
            DbgUtils.logf( "disAssemble: dropping message with too-new enum" );
        }
    }

    private void makeForInvite( String phone, int gameID, String gameName, 
                                int lang, String dict, int nPlayersT, 
                                int nPlayersH, int forceChannel )
    {
        long rowid = 
            GameUtils.makeNewGame( this, gameID, 
                                   new CommsAddrRec( phone ), lang, dict, 
                                   nPlayersT, nPlayersH, forceChannel );

        if ( null != gameName && 0 < gameName.length() ) {
            DBUtils.setName( this, rowid, gameName );
        }
        String owner = Utils.phoneToContact( this, phone, true );
        String body = LocUtils.getString( this, R.string.new_name_body_fmt, 
                                          owner );

        Intent intent = GamesListDelegate.makeGameIDIntent( this, gameID );
        Utils.postNotification( this, intent, R.string.new_sms_title, body, 
                                (int)rowid );

        ackInvite( phone, gameID );
    }

    private PendingIntent makeStatusIntent( String msg )
    {
        Intent intent = new Intent( msg );
        return PendingIntent.getBroadcast( this, 0, intent, 0 );
    }

    private void sendVisibleInvite( String phone, String nliData )
    {
        DbgUtils.logf("sendVisibleInvite(%s) to %s", nliData, phone );
        NetLaunchInfo nli = new NetLaunchInfo( nliData );
        Uri uri = nli.makeLaunchUri( this );
        String msg = "Click to launch: " + uri.toString();
        SmsManager mgr = SmsManager.getDefault();
        PendingIntent sent = makeStatusIntent( MSG_SENT );
        PendingIntent delivery = makeStatusIntent( MSG_DELIVERED );
        mgr.sendTextMessage( phone, null, msg, sent, delivery );
    }

    private boolean sendBuffers( String[] fragments, String phone )
    {
        boolean success = false;
        try {
            SmsManager mgr = SmsManager.getDefault();
            PendingIntent sent = makeStatusIntent( MSG_SENT );
            PendingIntent delivery = makeStatusIntent( MSG_DELIVERED );
            for ( String fragment : fragments ) {
                String asPublic = toPublicFmt( fragment );
                mgr.sendTextMessage( phone, null, asPublic, sent, delivery );
            }
            if ( s_showToasts ) {
                DbgUtils.showf( this, "sent %dth msg", s_nSent );
            }
            success = true;
        } catch ( IllegalArgumentException iae ) {
            DbgUtils.logf( "sendBuffers(%s): %s", phone, iae.toString() );
        } catch ( Exception ee ) {
            DbgUtils.loge( ee );
        }

        ConnStatusHandler.updateStatusOut( this, null, 
                                           CommsConnType.COMMS_CONN_SMS, 
                                           success );
        return success;
    }

    private boolean sendBuffers( byte[][] fragments, String phone, byte[] data )
    {
        boolean success = false;
        short nbsPort = (short)Integer.parseInt( getString( R.string.nbs_port ) );
        try {
            SmsManager mgr = SmsManager.getDefault();
            PendingIntent sent = makeStatusIntent( MSG_SENT );
            PendingIntent delivery = makeStatusIntent( MSG_DELIVERED );
            for ( byte[] fragment : fragments ) {
                mgr.sendDataMessage( phone, null, nbsPort, fragment, sent, 
                                     delivery );
            }
            if ( s_showToasts ) {
                DbgUtils.showf( this, "sent %dth msg", s_nSent );
            }
            success = true;
        } catch ( IllegalArgumentException iae ) {
            DbgUtils.logf( "sendBuffers(%s): %s", phone, iae.toString() );
        } catch ( NullPointerException npe ) {
            DbgUtils.showf( this, "Switching to regular SMS" );
            Assert.fail();
            // s_asData = new Boolean( false );
            // XWPrefs.setPrefsBoolean( this, R.string.key_send_data_sms,
            //                          false );
            // success = sendAsText( data, phone );
        } catch ( Exception ee ) {
            DbgUtils.loge( ee );
        }

        ConnStatusHandler.updateStatusOut( this, null, 
                                           CommsConnType.COMMS_CONN_SMS, 
                                           success );
        return success;
    }

    private static void saveRecs()
    {
        DbgUtils.logf( "need to write s_phoneRecs out somehow" );
    }

    private void feedMessage( int gameID, byte[] msg, CommsAddrRec addr )
    {
        long[] rowids = DBUtils.getRowIDsFor( this, gameID );
        if ( null == rowids || 0 == rowids.length ) {
            sendDiedPacket( addr.sms_phone, gameID );
        } else {
            for ( long rowid : rowids ) {
                if ( BoardDelegate.feedMessage( gameID, msg, addr ) ) {
                    // do nothing
                } else {
                    SMSMsgSink sink = new SMSMsgSink( this );
                    LastMoveInfo lmi = new LastMoveInfo();
                    if ( GameUtils.feedMessage( this, rowid, msg, addr, 
                                                sink, lmi ) ) {
                        GameUtils.postMoveNotification( this, rowid, lmi );
                    }
                }
            }
        }
    }

    // Runs in separate thread
    private void checkMsgDB()
    {
        Uri uri = Uri.parse( "content://sms/inbox" );
        ContentResolver resolver = getContentResolver();
        String[] columns = new String[] { "body","address" };
        Cursor cursor = resolver.query( uri, columns, null, null, null );
        if ( null != cursor ) {
            if ( 0 < cursor.getCount() ) {
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
                } catch ( Exception ee ) {
                    DbgUtils.loge( ee );
                }
            }
            cursor.close();
        }
    }

    private void registerReceivers()
    {
        m_sentReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context arg0, Intent arg1) 
                {
                    switch ( getResultCode() ) {
                    case Activity.RESULT_OK:
                        sendResult( MultiEvent.SMS_SEND_OK );
                        break;
                    case SmsManager.RESULT_ERROR_RADIO_OFF:
                        DbgUtils.showf( SMSService.this, "NO RADIO!!!" );
                        sendResult( MultiEvent.SMS_SEND_FAILED_NORADIO );
                        break;
                    case SmsManager.RESULT_ERROR_NO_SERVICE:
                        DbgUtils.showf( SMSService.this, "NO SERVICE!!!" );
                    default:
                        DbgUtils.logf( "FAILURE!!!" );
                        sendResult( MultiEvent.SMS_SEND_FAILED );
                        break;
                    }
                }
            };
        registerReceiver( m_sentReceiver, new IntentFilter(MSG_SENT) );

        m_receiveReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context arg0, Intent arg1) 
                {
                    DbgUtils.logf( "SMS delivery result: %s",
                                   Activity.RESULT_OK == getResultCode()
                                   ? "SUCCESS" : "FAILURE" );
                }
            };
        registerReceiver( m_receiveReceiver, new IntentFilter(MSG_DELIVERED) );
    }

    private boolean sendAsText( byte[] data, String phone ) 
    {
        boolean success = false;
        try {
            String[] msgs = breakAndEncode( XwJNI.base64Encode( data ) );
            success = sendBuffers( msgs, phone );
        } catch ( java.io.IOException ioe ) {
            DbgUtils.loge( ioe );
        }
        return success;
    }

    private class SMSMsgSink extends MultiMsgSink {
        public SMSMsgSink( Context context ) {
            super( context );
        }

        @Override
        public int sendViaSMS( byte[] buf, int gameID, CommsAddrRec addr )
        {
            return sendPacket( addr.sms_phone, gameID, buf );
        }
    }

    private class MsgStore {
        String[] m_msgsText;
        byte[][] m_msgsData;
        int m_msgID;
        int m_haveCount;
        int m_fullLength;

        public MsgStore( int id, int count, boolean usingStrings )
        {
            m_msgID = id;
            if ( usingStrings ) {
                m_msgsText = new String[count];
            } else {
                m_msgsData = new byte[count][];
            }
            m_fullLength = 0;
        }

        public MsgStore add( int index, String msg )
        {
            if ( null == m_msgsText[index] ) {
                ++m_haveCount;
                m_fullLength += msg.length();
            }
            m_msgsText[index] = msg;
            return this;
        }

        public MsgStore add( int index, byte[] msg )
        {
            if ( null == m_msgsData[index] ) {
                ++m_haveCount;
                m_fullLength += msg.length;
            }
            m_msgsData[index] = msg;
            return this;
        }
        
        public boolean isComplete()
        {
            int count = null != m_msgsText ? m_msgsText.length : m_msgsData.length;
            boolean complete = count == m_haveCount;
            return complete;
        }

        public String messageText() 
        {
            StringBuffer sb = new StringBuffer(m_fullLength);
            for ( String msg : m_msgsText ) {
                sb.append( msg );
            }
            return sb.toString();
        }

        public byte[] messageData() 
        {
            byte[] result = new byte[m_fullLength];
            int indx = 0;
            for ( byte[] msg : m_msgsData ) {
                System.arraycopy( msg, 0, result, indx, msg.length );
                indx += msg.length;
            }
            return result;
        }
    }
}
