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

import android.telephony.TelephonyManager;
import android.app.Activity;
import android.app.PendingIntent;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.telephony.PhoneNumberUtils;
import android.telephony.SmsManager;
import android.telephony.SmsMessage;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.OutputStream;
import java.lang.System;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

import junit.framework.Assert;

import org.eehouse.android.xw4.XWService.ReceiveResult;
import org.eehouse.android.xw4.GameUtils.BackMoveResult;
import org.eehouse.android.xw4.MultiService.DictFetchOwner;
import org.eehouse.android.xw4.MultiService.MultiEvent;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.LastMoveInfo;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.jni.JNIThread;
import org.eehouse.android.xw4.loc.LocUtils;

public class SMSService extends XWService {

    private static final String INSTALL_URL = "http://eehouse.org/_/a.py/a ";
    private static final int MAX_SMS_LEN = 140; // ??? differs by network
    private static final int KITKAT = 19;

    private static final String MSG_SENT = "MSG_SENT";
    private static final String MSG_DELIVERED = "MSG_DELIVERED";

    private static final int SMS_PROTO_VERSION_ORIG = 0;
    private static final int SMS_PROTO_VERSION_WITHPORT = 1;
    private static final int SMS_PROTO_VERSION = SMS_PROTO_VERSION_WITHPORT;
    private static final int MAX_LEN_TEXT = 100;
    private static final int MAX_LEN_BINARY = 100;
    private static final int MAX_MSG_COUNT = 16; // 1.6K enough? Should be....
    private enum SMSAction { _NONE,
                             INVITE,
                             SEND,
                             REMOVE,
                             ADDED_MISSING,
                             STOP_SELF,
                             HANDLEDATA,
    };

    private static final String CMD_STR = "CMD";
    private static final String BUFFER = "BUFFER";
    private static final String BINBUFFER = "BINBUFFER";
    private static final String PHONE = "PHONE";
    private static final String GAMEDATA_STR = "GD";

    private static final String PHONE_RECS_KEY = 
        SMSService.class.getName() + "_PHONES";

    private static Boolean s_showToasts = null;
    
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

    public static class SMSPhoneInfo {
        public SMSPhoneInfo( boolean isAPhone, String num, boolean gsm ) {
            isPhone = isAPhone;
            number = num;
            isGSM = gsm;
        }
        public boolean isPhone;
        public String number;
        public boolean isGSM;
    }
    private static SMSPhoneInfo s_phoneInfo;

    public static SMSPhoneInfo getPhoneInfo( Context context )
    {
        if ( null == s_phoneInfo ) {
            String number = null;
            boolean isGSM = false;
            boolean isPhone = false;
            TelephonyManager mgr = (TelephonyManager)
                context.getSystemService(Context.TELEPHONY_SERVICE);
            if ( null != mgr ) {
                number = mgr.getLine1Number();
                int type = mgr.getPhoneType();
                isGSM = TelephonyManager.PHONE_TYPE_GSM == type;
                isPhone = true;
            }

            String radio = XWPrefs.getPrefsString( context, R.string.key_force_radio );
            int[] ids = { R.string.radio_name_real,
                          R.string.radio_name_tablet,
                          R.string.radio_name_gsm,
                          R.string.radio_name_cdma,
            };

            int id = R.string.radio_name_real; // default so don't crash before set
            for ( int ii = 0; ii < ids.length; ++ii ) {
                if ( radio.equals(context.getString(ids[ii])) ) {
                    id = ids[ii];
                    break;
                }
            }

            switch( id ) {
            case R.string.radio_name_real:
                break;          // go with above
            case R.string.radio_name_tablet:
                number = null;
                isPhone = false;
                break;
            case R.string.radio_name_gsm:
            case R.string.radio_name_cdma:
                isGSM = id == R.string.radio_name_gsm;
                if ( null == number ) {
                    number = "000-000-0000";
                }
                isPhone = true;
                break;
            }

            s_phoneInfo = new SMSPhoneInfo( isPhone, number, isGSM );
        }
        return s_phoneInfo;
    }

    public static void resetPhoneInfo()
    {
        s_phoneInfo = null;
    }

    public static void smsToastEnable( boolean newVal ) 
    {
        s_showToasts = newVal;
    }

    public static void stopService( Context context )
    {
        Intent intent = getIntentTo( context, SMSAction.STOP_SELF );
        context.startService( intent );
    }

    // NBS case
    public static void handleFrom( Context context, byte[] buffer, 
                                   String phone )
    {
        Intent intent = getIntentTo( context, SMSAction.HANDLEDATA );
        intent.putExtra( BUFFER, buffer );
        intent.putExtra( PHONE, phone );
        context.startService( intent );
    }

    public static void inviteRemote( Context context, String phone,
                                     NetLaunchInfo nli )
    {
        Intent intent = getIntentTo( context, SMSAction.INVITE );
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
            Intent intent = getIntentTo( context, SMSAction.SEND );
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
        Intent intent = getIntentTo( context, SMSAction.REMOVE );
        intent.putExtra( PHONE, phone );
        intent.putExtra( MultiService.GAMEID, gameID );
        context.startService( intent );
    }

    public static void onGameDictDownload( Context context, Intent intentOld )
    {
        Intent intent = getIntentTo( context, SMSAction.ADDED_MISSING );
        intent.fillIn( intentOld, 0 );
        context.startService( intent );
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

    private static Intent getIntentTo( Context context, SMSAction cmd )
    {
        Intent intent = new Intent( context, SMSService.class );
        intent.putExtra( CMD_STR, cmd.ordinal() );
        return intent;
    }

    private static boolean showToasts( Context context )
    {
        if ( null == s_showToasts ) {
            s_showToasts = 
                XWPrefs.getPrefsBoolean( context, R.string.key_show_sms, false );
        }
        return s_showToasts;
    }

    @Override
    protected MultiMsgSink getSink( long rowid )
    {
        return new SMSMsgSink( this );
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
            int ordinal = intent.getIntExtra( CMD_STR, -1 );
            if ( -1 == ordinal ) {
                // ???
            } else {
                SMSAction cmd = SMSAction.values()[ordinal];
                switch( cmd ) {
                case STOP_SELF:
                    stopSelf();
                    break;
                case HANDLEDATA:
                    ++m_nReceived;
                    ConnStatusHandler.
                        updateStatusIn( this, null,
                                        CommsConnType.COMMS_CONN_SMS, true );
                    if ( showToasts( this ) && (0 == (m_nReceived % 5)) ) {
                        DbgUtils.showf( this, "Got msg %d", m_nReceived );
                    }
                    String phone = intent.getStringExtra( PHONE );
                    byte[] buffer = intent.getByteArrayExtra( BUFFER );
                    receiveBuffer( buffer, phone );
                    break;
                case INVITE:
                    phone = intent.getStringExtra( PHONE );
                    inviteRemote( phone, intent.getStringExtra( GAMEDATA_STR ) );
                    break;
                case ADDED_MISSING:
                    NetLaunchInfo nli
                        = MultiService.getMissingDictData( this, intent );
                    phone = intent.getStringExtra( PHONE );
                    makeForInvite( phone, nli );
                    break;
                case SEND:
                    phone = intent.getStringExtra( PHONE );
                    byte[] bytes = intent.getByteArrayExtra( BINBUFFER );
                    int gameID = intent.getIntExtra( MultiService.GAMEID, -1 );
                    sendPacket( phone, gameID, bytes );
                    break;
                case REMOVE:
                    gameID = intent.getIntExtra( MultiService.GAMEID, -1 );
                    phone = intent.getStringExtra( PHONE );
                    sendDiedPacket( phone, gameID );
                    break;
                }
            }

            result = Service.START_STICKY;
        }
        
        if ( Service.START_NOT_STICKY == result 
             || !XWPrefs.getSMSEnabled( this ) ) {
            stopSelf( startId );
        }

        return result;
    } // onStartCommand

    private void inviteRemote( String phone, String nliData )
    {
        DbgUtils.logf( "SMSService.inviteRemote()" );
        ByteArrayOutputStream bas = new ByteArrayOutputStream( 128 );
        DataOutputStream dos = new DataOutputStream( bas );
        try {
            dos.writeUTF( nliData );
            dos.flush();

            send( SMS_CMD.INVITE, bas.toByteArray(), phone );
        } catch ( java.io.IOException ioe ) {
            DbgUtils.loge( ioe );
        }
    }

    private void ackInvite( String phone, int gameID )
    {
        ByteArrayOutputStream bas = new ByteArrayOutputStream( 128 );
        DataOutputStream dos = new DataOutputStream( bas );
        try {
            dos.writeInt( gameID );
            dos.flush();

            send( SMS_CMD.ACK, bas.toByteArray(), phone );
        } catch ( java.io.IOException ioe ) {
            DbgUtils.loge( ioe );
        }
    }

    private void sendDiedPacket( String phone, int gameID )
    {
        if ( !s_sentDied.contains(gameID) ) {
            ByteArrayOutputStream bas = new ByteArrayOutputStream( 32 );
            DataOutputStream dos = new DataOutputStream( bas );
            try {
                dos.writeInt( gameID );
                dos.flush();
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
        DataOutputStream dos = new DataOutputStream( bas );
        try {
            dos.writeInt( gameID );
            dos.write( bytes, 0, bytes.length );
            dos.flush();
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
        DataOutputStream dos = new DataOutputStream( bas );
        dos.writeByte( SMS_PROTO_VERSION );
        if ( SMS_PROTO_VERSION_WITHPORT <= SMS_PROTO_VERSION ) {
            dos.writeShort( getNBSPort() );
        }
        dos.writeByte( cmd.ordinal() );
        dos.write( bytes, 0, bytes.length );
        dos.flush();

        byte[] data = bas.toByteArray();
        byte[][] msgs = breakAndEncode( data );
        boolean result = null != msgs && sendBuffers( msgs, phone );
        return result;
    }

    private byte[][] breakAndEncode( byte msg[] ) throws java.io.IOException 
    {
        byte[][] result = null;
        int count = (msg.length + (MAX_LEN_BINARY-1)) / MAX_LEN_BINARY;
        if ( count < MAX_MSG_COUNT ) {
            result = new byte[count][];
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
                part[0] = (byte)SMS_PROTO_VERSION;
                part[1] = (byte)msgID;
                part[2] = (byte)ii;
                part[3] = (byte)count;
                System.arraycopy( msg, start, part, 4, len );

                result[ii] = part;
                start = end;
            }
        } else {
            DbgUtils.logf( "breakAndEncode(): msg count %d too large; dropping",
                           count );
        }
        return result;
    }

    private void receive( SMS_CMD cmd, byte[] data, String phone )
    {
        DbgUtils.logf( "SMSService.receive(cmd=%s)", cmd.toString() );
        DataInputStream dis = 
            new DataInputStream( new ByteArrayInputStream(data) );
        try {
            switch( cmd ) {
            case INVITE:
                String nliData = dis.readUTF();
                NetLaunchInfo nli = new NetLaunchInfo( this, nliData );
                if ( nli.isValid() && checkNotDupe( nli ) ) {
                    if ( DictLangCache.haveDict( this, nli.lang, nli.dict ) ) {
                        makeForInvite( phone, nli );
                    } else {
                        Intent intent = MultiService
                            .makeMissingDictIntent( this, nli, 
                                                    DictFetchOwner.OWNER_SMS );
                        MultiService.postMissingDictNotification( this, intent, 
                                                                  nli.gameID() );
                    }
                } else {
                    DbgUtils.logf( "invalid nli from: %s", nliData );
                }
                break;
            case DATA:
                int gameID = dis.readInt();
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
        if ( tryAssemble( senderPhone, id, index, count, rest ) ) {
            sendResult( MultiEvent.SMS_RECEIVE_OK );
        } else {
            DbgUtils.logf( "SMSService: receiveBuffer(): bogus message from"
                           + " phone %s", senderPhone );
        }
    }

    private boolean tryAssemble( String senderPhone, int id, int index, 
                                 int count, byte[] msg )
    {
        boolean success = true;
        if ( index == 0 && count == 1 ) { // most common case
            success = disAssemble( senderPhone, msg );
        } else if ( count > 0 && count < MAX_MSG_COUNT && index < count ) {
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
                    success = disAssemble( senderPhone, store.messageData() );
                    perPhone.remove( id );
                }
            }
        } else {
            success = false;
        }
        return success;
    }

    private boolean disAssemble( String senderPhone, byte[] fullMsg )
    {
        boolean success = false;
        DataInputStream dis = 
            new DataInputStream( new ByteArrayInputStream(fullMsg) );
        try {
            byte proto = dis.readByte();
            short myPort = getNBSPort();
            short gotPort;
            if ( SMS_PROTO_VERSION_WITHPORT > proto ) {
                gotPort = myPort;
            } else {
                gotPort = dis.readShort();
            }
            if ( SMS_PROTO_VERSION < proto ) {
                DbgUtils.logf( "SMSService.disAssemble: bad proto %d from %s;"
                               + " dropping", proto, senderPhone );
                sendResult( MultiEvent.BAD_PROTO_SMS, senderPhone );
            } else if ( gotPort != myPort ) {
                DbgUtils.logdf( "SMSService.disAssemble(): received on port %d"
                                + " but expected %d", gotPort, myPort );
            } else {
                SMS_CMD cmd = SMS_CMD.values()[dis.readByte()];
                byte[] rest = new byte[dis.available()];
                dis.read( rest );
                receive( cmd, rest, senderPhone );
                success = true;
            }
        } catch ( java.io.IOException ioe ) {
            DbgUtils.loge( ioe );
        } catch ( ArrayIndexOutOfBoundsException oob ) {
            // enum this older code doesn't know about; drop it
            DbgUtils.logf( "disAssemble: dropping message with too-new enum" );
        }
        return success;
    }

    private void postNotification( String phone, int gameID, long rowid )
    {
        String owner = Utils.phoneToContact( this, phone, true );
        String body = LocUtils.getString( this, R.string.new_name_body_fmt, 
                                          owner );
        GameUtils.postInvitedNotification( this, gameID, body, rowid );
    }

    private void makeForInvite( String phone, NetLaunchInfo nli )
    {
        long rowid = GameUtils.makeNewMultiGame( this, nli,
                                                 new SMSMsgSink( this ),
                                                 getUtilCtxt() );
        postNotification( phone, nli.gameID(), rowid );
        ackInvite( phone, nli.gameID() );
    }

    private PendingIntent makeStatusIntent( String msg )
    {
        Intent intent = new Intent( msg );
        return PendingIntent.getBroadcast( this, 0, intent, 0 );
    }

    private boolean sendBuffers( byte[][] fragments, String phone )
    {
        boolean success = false;
        if ( XWPrefs.getSMSEnabled( this ) ) {

            // Try send-to-self
            if ( XWPrefs.getSMSToSelfEnabled( this ) ) {
                String myPhone = getPhoneInfo( this ).number;
                if ( PhoneNumberUtils.compare( phone, myPhone ) ) {
                    for ( byte[] fragment : fragments ) {
                        handleFrom( this, fragment, phone );
                    }
                    success = true;
                }
            }

            if ( !success ) {
                short nbsPort = getNBSPort();
                try {
                    SmsManager mgr = SmsManager.getDefault();
                    PendingIntent sent = makeStatusIntent( MSG_SENT );
                    PendingIntent delivery = makeStatusIntent( MSG_DELIVERED );
                    for ( byte[] fragment : fragments ) {
                        mgr.sendDataMessage( phone, null, nbsPort, fragment, sent, 
                                             delivery );
                        DbgUtils.logf( "SMSService.sendBuffers(): sent %d byte fragment",
                                       fragment.length );
                    }
                    success = true;
                } catch ( IllegalArgumentException iae ) {
                    DbgUtils.logf( "sendBuffers(%s): %s", phone, iae.toString() );
                } catch ( NullPointerException npe ) {
                    Assert.fail();      // shouldn't be trying to do this!!!
                } catch ( Exception ee ) {
                    DbgUtils.loge( ee );
                }
            }
        } else {
            DbgUtils.logf( "sendBuffers(): dropping because SMS disabled" );
        }

        if ( showToasts( this ) && success && (0 == (s_nSent % 5)) ) {
            DbgUtils.showf( this, "Sent msg %d", s_nSent );
        }

        ConnStatusHandler.updateStatusOut( this, null, 
                                           CommsConnType.COMMS_CONN_SMS, 
                                           success );
        return success;
    }

    private void feedMessage( int gameID, byte[] msg, CommsAddrRec addr )
    {
        ReceiveResult rslt = receiveMessage( this, gameID, null, msg, addr );
        if ( ReceiveResult.GAME_GONE == rslt ) {
            sendDiedPacket( addr.sms_phone, gameID );
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
                public void onReceive( Context context, Intent intent )
                {
                    if ( Activity.RESULT_OK != getResultCode() ) {
                        DbgUtils.logf( "SMS delivery result: FAILURE" );
                    }
                }
            };
        registerReceiver( m_receiveReceiver, new IntentFilter(MSG_DELIVERED) );
    }

    private static String matchKeyIf( Map<String, ?> map, final String phone )
    {
        String result = phone;
        Set<String> keys = map.keySet();
        if ( ! keys.contains( result ) ) {
            for ( Iterator<String> iter = keys.iterator(); iter.hasNext(); ) {
                String key = iter.next();
                if ( PhoneNumberUtils.compare( key, phone ) ) {
                    result = key;
                    break;
                }
            }
        }
        DbgUtils.logf( "matchKeyIf(%s) => %s", phone, result );
        return result;
    }

    private static Short s_nbsPort = null;
    private short getNBSPort()
    {
        if ( null == s_nbsPort ) {
            String asStr = getString( R.string.nbs_port );
            s_nbsPort = new Short((short)Integer.parseInt( asStr ) );
        }
        return s_nbsPort;
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
