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

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.telephony.PhoneNumberUtils;
import android.telephony.SmsManager;
import android.telephony.TelephonyManager;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

import org.eehouse.android.nbsplib.NBSProxy;

import org.eehouse.android.xw4.MultiService.DictFetchOwner;
import org.eehouse.android.xw4.MultiService.MultiEvent;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.XwJNI.SMSProtoMsg;
import org.eehouse.android.xw4.jni.XwJNI.SMS_CMD;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

public class NBSProto {
    private static final String TAG = NBSProto.class.getSimpleName();

    private static final String MSG_SENT = "MSG_SENT";
    private static final String MSG_DELIVERED = "MSG_DELIVERED";

    private static Set<Integer> s_sentDied = new HashSet<Integer>();

    public static void handleFrom( Context context, byte[] buffer,
                                   String phone, short port )
    {
        getCurReceiver( phone, port ).addPacket( context, buffer );
    }

    public static void onGameDictDownload( Context context, Intent intentOld )
    {
    }

    public static void inviteRemote( Context context, String phone,
                                     NetLaunchInfo nli )
    {
        getCurSender( phone ).addInvite( context, nli );
    }

    public static int sendPacket( Context context, String phone,
                                  int gameID, byte[] binmsg )
    {
        getCurSender( phone ).addPacket( context, gameID, binmsg );
        return binmsg.length;
    }

    public static void gameDied( Context context, int gameID, String phone )
    {
        getCurSender( phone ).addDeathNotice( context, gameID );
    }

    public static void stopService( Context context )
    {
        Log.d( TAG, "stopService() does nothing" );
    }

    private static boolean s_showToasts;
    public static void smsToastEnable( boolean newVal )
    {
        s_showToasts = newVal;
    }

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


    private static class SendThread extends Thread {
        private String mPhone;
        private short mPort;
        private LinkedBlockingQueue<SendElem> mQueue = new LinkedBlockingQueue<>();

        SendThread( String phone ) {
            mPhone = phone;
            mPort = getNBSPort();
        }

        String getPhone() { return mPhone; }

        void addPacket( Context context, int gameID, byte[] binmsg )
        {
            mQueue.add( new SendElem( context, SMS_CMD.DATA, gameID, binmsg ) );
        }

        void addInvite( Context context, NetLaunchInfo nli )
        {
            mQueue.add( new SendElem( context, SMS_CMD.INVITE, nli ) );
        }

        void addDeathNotice( Context context, int gameID )
        {
            mQueue.add( new SendElem( context, SMS_CMD.DEATH, gameID, null ) );
        }

        void addAck( Context context, int gameID )
        {
            mQueue.add( new SendElem( context, SMS_CMD.ACK_INVITE, gameID, null ) );
        }

        @Override
        public void run() {
            Log.d( TAG, "%s.run() starting for %s", this, mPhone );
            int[] waitSecs = { 5 };

            while ( !isInterrupted() ) {
                SendElem elem;

                try {
                    elem = mQueue.poll( waitSecs[0], TimeUnit.SECONDS );

                    byte[][] msgs;
                    if ( null == elem ) { // timed out?
                        // removeSelf( this );
                        msgs = XwJNI.smsproto_prepOutbound( SMS_CMD.NONE, 0, null, mPhone,
                                                            mPort, true, waitSecs );
                    } else {
                        boolean newSMSEnabled = XWPrefs.getSMSProtoEnabled( elem.context );
                        boolean forceNow = !newSMSEnabled; // || cmd == SMS_CMD.INVITE;
                        msgs = XwJNI.smsproto_prepOutbound( elem.cmd, elem.gameID, elem.data, mPhone,
                                                            mPort, forceNow, waitSecs );
                    }

                    if ( null != msgs ) {
                        sendBuffers( msgs, mPhone, mPort );
                    }
                    if ( waitSecs[0] <= 0 ) {
                        waitSecs[0] = 5;
                    }
                } catch ( InterruptedException iex ) {
                    Log.d( TAG, "poll() threw: %s", iex.getMessage() );
                    break;
                }
            }

            // To be safe, call again
            removeSelf( this );

            // Better not be leaving anything behind!
            Assert.assertTrue( 0 == mQueue.size() || !BuildConfig.DEBUG );
            Log.d( TAG, "%s.run() DONE", this );
        }

        private void sendBuffers( byte[][] fragments, String phone, short nbsPort )
        {
            Context context = XWApp.getContext();
            boolean success = false;
            if ( XWPrefs.getNBSEnabled( context ) ) {

                // Try send-to-self
                if ( XWPrefs.getSMSToSelfEnabled( context ) ) {
                    String myPhone = getPhoneInfo( context ).number;
                    if ( null != myPhone
                         && PhoneNumberUtils.compare( phone, myPhone ) ) {
                        for ( byte[] fragment : fragments ) {
                            handleFrom( context, fragment, phone, nbsPort );
                        }
                        success = true;
                    }
                }

                if ( !success ) {
                    try {
                        SmsManager mgr = SmsManager.getDefault();
                        boolean useProxy = Perms23.Perm.SEND_SMS.isBanned( context )
                            && NBSProxy.isInstalled( context );
                        PendingIntent sent = useProxy ? null
                            : makeStatusIntent( context, MSG_SENT );
                        PendingIntent delivery = useProxy ? null
                            : makeStatusIntent( context, MSG_DELIVERED );
                        for ( byte[] fragment : fragments ) {
                            if ( useProxy ) {
                                NBSProxy.send( context, phone, nbsPort, fragment );
                            } else {
                                mgr.sendDataMessage( phone, null, nbsPort, fragment,
                                                     sent, delivery );
                            }
                        }
                        success = true;
                    } catch ( IllegalArgumentException iae ) {
                        Log.w( TAG, "sendBuffers(%s): %s", phone, iae.toString() );
                    } catch ( NullPointerException npe ) {
                        Assert.assertFalse( BuildConfig.DEBUG ); // shouldn't be trying to do this!!!
                    } catch ( java.lang.SecurityException se ) {
                        // getHelper(context).postEvent( MultiEvent.SMS_SEND_FAILED_NOPERMISSION );
                        Assert.assertFalse( BuildConfig.DEBUG );
                    } catch ( Exception ee ) {
                        Log.ex( TAG, ee );
                    }
                }
            } else {
                Log.i( TAG, "dropping because SMS disabled" );
            }

            // if ( showToasts( context ) && success && (0 == (++s_nSent % 5)) ) {
            //     DbgUtils.showf( context, "Sent msg %d", s_nSent );
            // }

            ConnStatusHandler.updateStatusOut( context, null,
                                               CommsConnType.COMMS_CONN_SMS,
                                               success );
        }

        private PendingIntent makeStatusIntent( Context context, String msg )
        {
            Intent intent = new Intent( msg );
            return PendingIntent.getBroadcast( context, 0, intent, 0 );
        }

        private static class SendElem {
            Context context;
            SMS_CMD cmd;
            int gameID;
            byte[] data;
            SendElem( Context context, SMS_CMD cmd, int gameID, byte[] data ) {
                this.context = context;
                this.cmd = cmd;
                this.gameID = gameID;
                this.data = data;
            }
            SendElem( Context context, SMS_CMD cmd, NetLaunchInfo nli ) {
                this( context, cmd, 0, nli.asByteArray() );
            }
        }
    }

    private static Map<String, SendThread> sSendersMap = new HashMap<>();
    
    private static SendThread getCurSender( String toPhone )
    {
        SendThread result = null;
        synchronized ( sSendersMap ) {
            result = sSendersMap.get( toPhone );
            if ( result == null ) {
                result = new SendThread( toPhone );
                result.start();
                sSendersMap.put( toPhone, result );
            }
        }
        return result;
    }

    private static void removeSelf( SendThread self )
    {
        synchronized ( sSendersMap ) {
            String phone = self.getPhone();
            if ( sSendersMap.get(phone) == self ) {
                sSendersMap.remove( phone );
            }
        }
    }

    private static class ReceiveThread extends Thread {
        private String mPhone;
        private short mPort;

        private LinkedBlockingQueue<ReceiveElem> mQueue = new LinkedBlockingQueue<>();

        ReceiveThread( String fromPhone, short port ) {
            mPhone = fromPhone;
            mPort = port;
        }

        short getPort() { return mPort; }

        void addPacket( Context context, byte[] data )
        {
            mQueue.add( new ReceiveElem( context, data ) );
        }
        
        @Override
        public void run() {
            Log.d( TAG, "%s.run() starting", this );
            while ( ! isInterrupted() ) {
                ReceiveElem elem;
                try {
                    elem = mQueue.poll( 10, TimeUnit.SECONDS );

                    if ( null != elem ) {
                        SMSProtoMsg[] msgs = XwJNI.smsproto_prepInbound( elem.data, mPhone, mPort );
                        if ( null != msgs ) {
                            Log.d( TAG, "got %d msgs combined!", msgs.length );
                            for ( int ii = 0; ii < msgs.length; ++ii ) {
                                Log.d( TAG, "%d: type: %s; len: %d", ii, msgs[ii].cmd, msgs[ii].data.length );
                            }
                            for ( SMSProtoMsg msg : msgs ) {
                                receive( elem.context, msg );
                            }
                            getHelper(elem.context).postEvent( MultiEvent.SMS_RECEIVE_OK );
                        } else {
                            Log.d( TAG, "receiveBuffer(): bogus or incomplete message from %s",
                                   mPhone );
                        }
                    }
                } catch (InterruptedException iex) {
                    Log.d( TAG, "%s.poll() threw: %s", this, iex.getMessage() );
                    break;
                }
            }
            Log.d( TAG, "%s.run() DONE", this );
        }

        private void receive( Context context, SMSProtoMsg msg )
        {
            Log.i( TAG, "receive(cmd=%s)", msg.cmd );
            switch( msg.cmd ) {
            case INVITE:
                makeForInvite( context, NetLaunchInfo.makeFrom( context, msg.data ) );
                break;
            case DATA:
                if ( feedMessage( context, msg.gameID, msg.data, new CommsAddrRec( mPhone ) ) ) {
                    SMSResendReceiver.resetTimer( context );
                }
                break;
            case DEATH:
                getHelper(context).postEvent( MultiEvent.MESSAGE_NOGAME, msg.gameID );
                break;
            case ACK_INVITE:
                getHelper(context).postEvent( MultiEvent.NEWGAME_SUCCESS, msg.gameID );
                break;
            default:
                Log.w( TAG, "unexpected cmd %s", msg.cmd );
                Assert.assertFalse( BuildConfig.DEBUG );
                break;
            }
        }

        private boolean feedMessage( Context context, int gameID, byte[] msg,
                                     CommsAddrRec addr )
        {
            XWServiceHelper.ReceiveResult rslt = getHelper(context)
                .receiveMessage( gameID, null, msg, addr );
            if ( XWServiceHelper.ReceiveResult.GAME_GONE == rslt ) {
                sendDiedPacket( context, addr.sms_phone, gameID );
            }
            return rslt == XWServiceHelper.ReceiveResult.OK;
        }

        private void sendDiedPacket( Context context, String phone, int gameID )
        {
            if ( !s_sentDied.contains( gameID ) ) {
                getCurSender( phone ).addDeathNotice( context, gameID );
                // resendFor( phone, SMS_CMD.DEATH, gameID, null );
                s_sentDied.add( gameID );
            }
        }

        private void makeForInvite( Context context, NetLaunchInfo nli )
        {
            if ( nli != null ) {
                getHelper(context).handleInvitation( nli, mPhone, DictFetchOwner.OWNER_SMS );
                getCurSender( mPhone ).addAck( context, nli.gameID() );
            }
        }

        private SMSServiceHelper mHelper = null;
        private SMSServiceHelper getHelper( Context context )
        {
            if ( null == mHelper ) {
                mHelper = new SMSServiceHelper( context );
            }
            return mHelper;
        }

        private static class ReceiveElem {
            Context context;
            byte[] data;
            ReceiveElem( Context context, byte[] data )
            {
                this.context = context;
                this.data = data;
            }
        }
    }

    private static Map<String, ReceiveThread> sReceiversMap = new HashMap<>();
    private static ReceiveThread getCurReceiver( String fromPhone, short port )
    {
        ReceiveThread result = null;
        synchronized ( sReceiversMap ) {
            result = sReceiversMap.get( fromPhone );
            if ( result == null ) {
                result = new ReceiveThread( fromPhone, port );
                result.start();
                sReceiversMap.put( fromPhone, result );
            } else {
                Assert.assertTrue( port == result.getPort() || !BuildConfig.DEBUG );
            }
        }
        return result;
    }

    private static class SMSMsgSink extends MultiMsgSink {
        private Context mContext;
        public SMSMsgSink( Context context ) {
            super( context );
            mContext = context;
        }

        @Override
        public int sendViaSMS( byte[] buf, int gameID, CommsAddrRec addr )
        {
            return sendPacket( mContext, addr.sms_phone, gameID, buf );
        }
    }

    private static class SMSServiceHelper extends XWServiceHelper {
        private Context mContext;

        SMSServiceHelper( Context context ) {
            super( context );
            mContext = context;
        }

        @Override
        protected MultiMsgSink getSink( long rowid )
        {
            return new SMSMsgSink( mContext );
        }

        @Override
        protected void postNotification( String phone, int gameID, long rowid )
        {
            String owner = Utils.phoneToContact( mContext, phone, true );
            String body = LocUtils.getString( mContext, R.string.new_name_body_fmt,
                                              owner );
            GameUtils.postInvitedNotification( mContext, gameID, body, rowid );
        }
    }

    private static Short s_nbsPort = null;
    private static short getNBSPort()
    {
        if ( null == s_nbsPort ) {
            String asStr = XWApp.getContext().getString( R.string.nbs_port );
            s_nbsPort = new Short((short)Integer.parseInt( asStr ) );
        }
        return s_nbsPort;
    }
}
