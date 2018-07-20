/* -*- compile-command: "find-and-gradle.sh inXw4Deb"; -*- */
/*
 * Copyright 2010 - 2018 by Eric House (xwords@eehouse.org).  All rights
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
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.preference.PreferenceManager;
import android.telephony.PhoneNumberUtils;
import android.telephony.SmsManager;
import android.telephony.TelephonyManager;

import junit.framework.Assert;

import org.eehouse.android.xw4.MultiService.DictFetchOwner;
import org.eehouse.android.xw4.MultiService.MultiEvent;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.jni.XwJNI.SMSProtoMsg;
import org.eehouse.android.xw4.jni.XwJNI.SMS_CMD;
import org.eehouse.android.xw4.loc.LocUtils;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

public class SMSService extends XWService {
    private static final String TAG = SMSService.class.getSimpleName();

    private static final String MSG_SENT = "MSG_SENT";
    private static final String MSG_DELIVERED = "MSG_DELIVERED";

    private static final int SMS_PROTO_VERSION_WITHPORT = 1;
    private static final int SMS_PROTO_VERSION = SMS_PROTO_VERSION_WITHPORT;
    private enum SMSAction { _NONE,
                             INVITE,
                             SEND,
                             REMOVE,
                             ADDED_MISSING,
                             STOP_SELF,
                             HANDLEDATA,
                             RESEND,
    };

    private static final String CMD_STR = "CMD";
    private static final String BUFFER = "BUFFER";
    private static final String BINBUFFER = "BINBUFFER";
    private static final String PHONE = "PHONE";
    private static final String GAMEDATA_STR = "GD";

    private static final String PHONE_RECS_KEY =
        SMSService.class.getName() + "_PHONES";

    private static Boolean s_showToasts = null;

    private BroadcastReceiver m_sentReceiver;
    private BroadcastReceiver m_receiveReceiver;
    private OnSharedPreferenceChangeListener m_prefsListener;

    private int m_nReceived = 0;
    private static int s_nSent = 0;
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
            try {
                String number = null;
                boolean isGSM = false;
                boolean isPhone = false;
                TelephonyManager mgr = (TelephonyManager)
                    context.getSystemService(Context.TELEPHONY_SERVICE);
                if ( null != mgr ) {
                    number = mgr.getLine1Number(); // needs permission
                    int type = mgr.getPhoneType();
                    isGSM = TelephonyManager.PHONE_TYPE_GSM == type;
                    isPhone = true;
                }

                String radio =
                    XWPrefs.getPrefsString( context, R.string.key_force_radio );
                int[] ids = { R.string.radio_name_real,
                              R.string.radio_name_tablet,
                              R.string.radio_name_gsm,
                              R.string.radio_name_cdma,
                };

                // default so don't crash before set
                int id = R.string.radio_name_real;
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
            } catch ( SecurityException se ) {
                Log.e( TAG, "got SecurityException" );
            }
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
        Log.w( TAG, "inviteRemote(%s, '%s')", phone, asString );
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
            Log.i( TAG, "sendPacket: dropping because SMS disabled" );
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
                    Log.w( TAG, "fromPublicFmt: hash code mismatch" );
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
        if ( Utils.deviceSupportsSMS( this ) ) {
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
        if ( null != intent ) {
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
                case RESEND:
                    phone = intent.getStringExtra( PHONE );
                    resendFor( phone );
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
        try {
            byte[] asBytes = nliData.getBytes( "UTF-8" );
            resendFor( phone, SMS_CMD.INVITE, 0, asBytes, true );
        } catch ( java.io.UnsupportedEncodingException uee ) {
            Log.ex( TAG, uee );
        }
    }

    private void ackInvite( String phone, int gameID )
    {
        resendFor( phone, SMS_CMD.ACK_INVITE, gameID, null );
    }

    private void sendDiedPacket( String phone, int gameID )
    {
        if ( !s_sentDied.contains(gameID) ) {
            resendFor( phone, SMS_CMD.DEATH, gameID, null );
        }
    }

    public int sendPacket( String phone, int gameID, byte[] bytes )
    {
        resendFor( phone, SMS_CMD.DATA, gameID, bytes );
        return bytes.length;
    }

    private void sendOrRetry( byte[][] msgs, String toPhone, int waitSecs )
    {
        if ( null != msgs ) {
            sendBuffers( msgs, toPhone );
        }
        if ( waitSecs > 0 ) {
            postResend( toPhone, waitSecs );
        }
    }

    private void resendFor( String toPhone, SMS_CMD cmd, int gameID, byte[] data )
    {
        boolean newSMSEnabled = XWPrefs.getSMSProtoEnabled( this );
        boolean forceNow = !newSMSEnabled; // || cmd == SMS_CMD.INVITE;
        resendFor( toPhone, cmd, gameID, data, forceNow );
    }

    private void resendFor( String toPhone )
    {
        resendFor( toPhone, SMS_CMD.NONE, 0, null, false );
    }

    private void resendFor( String toPhone, SMS_CMD cmd, int gameID, byte[] data,
                            boolean forceNow )
    {
        int[] waitSecs = { 0 };
        byte[][] msgs = XwJNI.smsproto_prepOutbound( cmd, gameID, data, toPhone,
                                                     getNBSPort(), forceNow,
                                                     waitSecs );
        sendOrRetry( msgs, toPhone, waitSecs[0] );
    }

    private void postResend( final String phone, final int waitSecs )
    {
        Log.d( TAG, "postResend" );
        new Thread(new Runnable() {
                @Override
                public void run() {
                    try {
                        Thread.sleep( waitSecs * 1000 );

                        Log.d( TAG, "postResend.run()" );
                        Intent intent = getIntentTo( SMSService.this,
                                                     SMSAction.RESEND );
                        intent.putExtra( PHONE, phone );
                        startService( intent );
                    } catch ( InterruptedException ie ) {
                        Log.e( TAG, ie.getMessage() );
                    }
                }
            } ).start();
    }

    private void receive( SMSProtoMsg msg, String phone )
    {
        Log.i( TAG, "receive(cmd=%s)", msg.cmd );
        switch( msg.cmd ) {
        case INVITE:
            NetLaunchInfo nli = new NetLaunchInfo( this, new String(msg.data) );
            makeForInvite( phone, nli );
            break;
        case DATA:
            if ( feedMessage( msg.gameID, msg.data, new CommsAddrRec( phone ) ) ) {
                SMSResendReceiver.resetTimer( this );
            }
            break;
        case DEATH:
            postEvent( MultiEvent.MESSAGE_NOGAME, msg.gameID );
            break;
        case ACK_INVITE:
            postEvent( MultiEvent.NEWGAME_SUCCESS, msg.gameID );
            break;
        default:
            Log.w( TAG, "unexpected cmd %s", msg.cmd );
            Assert.assertFalse( BuildConfig.DEBUG );
            break;
        }
    }

    private void receiveBuffer( byte[] buffer, String senderPhone )
    {
        SMSProtoMsg[] msgs = XwJNI.smsproto_prepInbound( buffer, senderPhone );
        if ( null != msgs ) {
            for ( SMSProtoMsg msg : msgs ) {
                receive( msg, senderPhone );
            }
            postEvent( MultiEvent.SMS_RECEIVE_OK );
        } else {
            Log.w( TAG, "receiveBuffer(): bogus or incomplete message from phone %s",
                   senderPhone );
        }
    }

    @Override
    protected void postNotification( String phone, int gameID, long rowid )
    {
        String owner = Utils.phoneToContact( this, phone, true );
        String body = LocUtils.getString( this, R.string.new_name_body_fmt,
                                          owner );
        GameUtils.postInvitedNotification( this, gameID, body, rowid );
    }

    private void makeForInvite( String phone, NetLaunchInfo nli )
    {
        if ( handleInvitation( nli, phone, DictFetchOwner.OWNER_SMS ) ) {
            ackInvite( phone, nli.gameID() );
        }
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
                if ( null != myPhone
                     && PhoneNumberUtils.compare( phone, myPhone ) ) {
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
                        Log.i( TAG, "sendBuffers(): sent %d byte fragment to %s",
                               fragment.length, phone );
                    }
                    success = true;
                } catch ( IllegalArgumentException iae ) {
                    Log.w( TAG, "sendBuffers(%s): %s", phone, iae.toString() );
                } catch ( NullPointerException npe ) {
                    Assert.fail();      // shouldn't be trying to do this!!!
                } catch ( java.lang.SecurityException se ) {
                    postEvent( MultiEvent.SMS_SEND_FAILED_NOPERMISSION );
                } catch ( Exception ee ) {
                    Log.ex( TAG, ee );
                }
            }
        } else {
            Log.i( TAG, "dropping because SMS disabled" );
        }

        if ( showToasts( this ) && success && (0 == (++s_nSent % 5)) ) {
            DbgUtils.showf( this, "Sent msg %d", s_nSent );
        }

        ConnStatusHandler.updateStatusOut( this, null,
                                           CommsConnType.COMMS_CONN_SMS,
                                           success );
        return success;
    }

    private boolean feedMessage( int gameID, byte[] msg, CommsAddrRec addr )
    {
        ReceiveResult rslt = receiveMessage( this, gameID, null, msg, addr );
        if ( ReceiveResult.GAME_GONE == rslt ) {
            sendDiedPacket( addr.sms_phone, gameID );
        }
        return rslt == ReceiveResult.OK;
    }

    private void registerReceivers()
    {
        m_sentReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context arg0, Intent arg1)
                {
                    switch ( getResultCode() ) {
                    case Activity.RESULT_OK:
                        postEvent( MultiEvent.SMS_SEND_OK );
                        break;
                    case SmsManager.RESULT_ERROR_RADIO_OFF:
                        postEvent( MultiEvent.SMS_SEND_FAILED_NORADIO );
                        break;
                    case SmsManager.RESULT_ERROR_NO_SERVICE:
                    default:
                        Log.w( TAG, "FAILURE!!!" );
                        postEvent( MultiEvent.SMS_SEND_FAILED );
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
                        Log.w( TAG, "SMS delivery result: FAILURE" );
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
        Log.i( TAG, "matchKeyIf(%s) => %s", phone, result );
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
}
