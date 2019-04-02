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

    static abstract class NBSProtoThread extends Thread {
        private String mPhone;
        private short mPort;
        private LinkedBlockingQueue<QueueElem> mQueue = new LinkedBlockingQueue<>();

        NBSProtoThread( String name, String phone, short port )
        {
            super( name );
            mPhone = phone;
            mPort = port;
        }

        String getPhone() { return mPhone; }
        short getPort() { return mPort; }

        abstract void removeSelf();
        abstract void process( QueueElem elem, boolean exiting );

        void add( QueueElem elem ) { mQueue.add( elem ); }

        @Override
        public void run() {
            Log.d( TAG, "%s.run() starting for %s", this, mPhone );

            while ( !isInterrupted() ) {
                try {
                    QueueElem elem = mQueue.poll( 5, TimeUnit.MINUTES );
                    process( elem, false );
                    if ( null == elem ) {
                        break;
                    }
                } catch ( InterruptedException iex ) {
                    Log.d( TAG, "poll() threw: %s", iex.getMessage() );
                    break;
                }
            }

            removeSelf();

            // Now just empty out the queue, in case anything was added
            // late. Note that if we're abandoning a half-assembled
            // multi-message buffer that data's lost until a higher level
            // resends. So don't let that happen.
            for ( ; ; ) {
                QueueElem elem = mQueue.poll();
                process( elem, true );
                if ( null == elem ) {
                    break;
                }
            }

            // Better not be leaving anything behind!
            Assert.assertTrue( 0 == mQueue.size() || !BuildConfig.DEBUG );
            Log.d( TAG, "%s.run() DONE", this );
        }

        private SMSServiceHelper mHelper = null;
        protected SMSServiceHelper getHelper( Context context )
        {
            if ( null == mHelper ) {
                mHelper = new SMSServiceHelper( context );
            }
            return mHelper;
        }

        static class QueueElem {
            Context context;
            QueueElem( Context context ) { this.context = context; }
        }
    }

    private static class SendThread extends NBSProtoThread {
        private int[] mWaitSecs = { 0 };
        private boolean mForceNow;

        SendThread( String phone ) {
            super( "SendThread", phone, getNBSPort() );

            boolean newSMSEnabled = XWPrefs.getSMSProtoEnabled( XWApp.getContext() );
            mForceNow = !newSMSEnabled;
        }

        void addPacket( Context context, int gameID, byte[] binmsg )
        {
            add( new SendElem( context, SMS_CMD.DATA, gameID, binmsg ) );
        }

        void addInvite( Context context, NetLaunchInfo nli )
        {
            add( new SendElem( context, SMS_CMD.INVITE, nli ) );
        }

        void addDeathNotice( Context context, int gameID )
        {
            add( new SendElem( context, SMS_CMD.DEATH, gameID, null ) );
        }

        void addAck( Context context, int gameID )
        {
            add( new SendElem( context, SMS_CMD.ACK_INVITE, gameID, null ) );
        }

        @Override
        protected void removeSelf() { NBSProto.removeSelf( this ); }

        @Override
        protected void process( QueueElem qelm, boolean exiting )
        {
            SendElem elem = (SendElem)qelm;
            byte[][] msgs;
            boolean forceNow = mForceNow || exiting;
            if ( null != elem ) {
                msgs = XwJNI.smsproto_prepOutbound( elem.cmd, elem.gameID, elem.data,
                                                    getPhone(), getPort(), forceNow, mWaitSecs );
            } else { // timed out
                msgs = XwJNI.smsproto_prepOutbound( SMS_CMD.NONE, 0, null, getPhone(),
                                                    getPort(), forceNow, mWaitSecs );
            }

            if ( null != msgs ) {
                sendBuffers( msgs );
            }
            // if ( mWaitSecs[0] <= 0 ) {
            //     mWaitSecs[0] = 5;
            // }
        }

        private void sendBuffers( byte[][] fragments )
        {
            Context context = XWApp.getContext();
            boolean success = false;
            if ( XWPrefs.getNBSEnabled( context ) ) {

                String phone = getPhone();
                short port = getPort();

                // Try send-to-self
                if ( XWPrefs.getSMSToSelfEnabled( context ) ) {
                    String myPhone = SMSPhoneInfo.get( context ).number;
                    if ( null != myPhone
                         && PhoneNumberUtils.compare( phone, myPhone ) ) {
                        for ( byte[] fragment : fragments ) {
                            handleFrom( context, fragment, phone, port );
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
                                NBSProxy.send( context, phone, port, fragment );
                            } else {
                                mgr.sendDataMessage( phone, null, port, fragment,
                                                     sent, delivery );
                            }
                        }
                        success = true;
                    } catch ( IllegalArgumentException iae ) {
                        Log.w( TAG, "sendBuffers(%s): %s", phone, iae.toString() );
                    } catch ( NullPointerException npe ) {
                        Assert.assertFalse( BuildConfig.DEBUG ); // shouldn't be trying to do this!!!
                    } catch ( java.lang.SecurityException se ) {
                        getHelper(context).postEvent( MultiEvent.SMS_SEND_FAILED_NOPERMISSION );
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

        private static class SendElem extends QueueElem {
            SMS_CMD cmd;
            int gameID;
            byte[] data;
            SendElem( Context context, SMS_CMD cmd, int gameID, byte[] data ) {
                super( context );
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

    private static class ReceiveThread extends NBSProtoThread {
        private short mPort;

        ReceiveThread( String fromPhone, short port ) {
            super( "ReceiveThread", fromPhone, port );
        }

        void addPacket( Context context, byte[] data )
        {
            add( new ReceiveElem( context, data ) );
        }

        @Override
        protected void removeSelf() { NBSProto.removeSelf( this ); }

        @Override
        protected void process( QueueElem qelem, boolean exiting )
        {
            ReceiveElem elem = (ReceiveElem)qelem;
            SMSProtoMsg[] msgs = XwJNI.smsproto_prepInbound( elem.data, getPhone(), getPort() );
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
                       getPhone() );
            }
        }

        private void receive( Context context, SMSProtoMsg msg )
        {
            Log.i( TAG, "receive(cmd=%s)", msg.cmd );
            switch( msg.cmd ) {
            case INVITE:
                makeForInvite( context, NetLaunchInfo.makeFrom( context, msg.data ) );
                break;
            case DATA:
                if ( feedMessage( context, msg.gameID, msg.data, new CommsAddrRec( getPhone() ) ) ) {
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
                String phone = getPhone();
                getHelper(context).handleInvitation( nli, phone, DictFetchOwner.OWNER_SMS );
                getCurSender( phone ).addAck( context, nli.gameID() );
            }
        }

        private static class ReceiveElem extends QueueElem {
            byte[] data;
            ReceiveElem( Context context, byte[] data )
            {
                super( context );
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

    private static void removeSelf( ReceiveThread self )
    {
        synchronized ( sReceiversMap ) {
            String phone = self.getPhone();
            if ( sReceiversMap.get(phone) == self ) {
                sReceiversMap.remove( phone );
            }
        }
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
