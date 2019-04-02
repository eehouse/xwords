/* -*- compile-command: "find-and-gradle.sh inXw4Deb"; -*- */
/*
 * Copyright 2010 - 2019 by Eric House (xwords@eehouse.org).  All rights
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
    private static final int TOAST_FREQ = 5;

    private static Boolean s_showToasts;
    private static int s_nReceived = 0;
    private static int s_nSent = 0;

    private static Set<Integer> s_sentDied = new HashSet<Integer>();

    public static void handleFrom( Context context, byte[] buffer,
                                   String phone, short port )
    {
        getCurThread( phone, port ).addPacketFrom( context, buffer );
        if ( (0 == (++s_nReceived % TOAST_FREQ)) && showToasts( context ) ) {
            DbgUtils.showf( context, "Got msg %d", s_nReceived );
        }
    }

    public static void onGameDictDownload( Context context, Intent oldIntent )
    {
        NetLaunchInfo nli = MultiService.getMissingDictData( context, oldIntent );
        getCurThread( nli.phone ).addInviteFrom( context, nli );
    }

    public static void inviteRemote( Context context, String phone,
                                     NetLaunchInfo nli )
    {
        getCurThread( phone ).addInviteTo( context, nli );
    }

    public static int sendPacket( Context context, String phone,
                                  int gameID, byte[] binmsg )
    {
        getCurThread( phone ).addPacketTo( context, gameID, binmsg );
        return binmsg.length;
    }

    public static void gameDied( Context context, int gameID, String phone )
    {
        getCurThread( phone ).addGameDied( context, gameID );
    }

    public static void stopThreads()
    {
        stopCurThreads();
    }

    public static void smsToastEnable( boolean newVal )
    {
        s_showToasts = newVal;
    }

    static class NBSProtoThread extends Thread {
        private String mPhone;
        private short mPort;
        private LinkedBlockingQueue<QueueElem> mQueue = new LinkedBlockingQueue<>();
        private boolean mForceNow;
        private int[] mWaitSecs = { 0 };

        NBSProtoThread( String phone, short port )
        {
            super( "NBSProtoThread" );
            mPhone = phone;
            mPort = port;
            boolean newSMSEnabled = XWPrefs.getSMSProtoEnabled( XWApp.getContext() );
            mForceNow = !newSMSEnabled;
        }

        String getPhone() { return mPhone; }
        short getPort() { return mPort; }

        void addPacketFrom( Context context, byte[] data )
        {
            add( new ReceiveElem( context, data ) );
        }

        void addInviteFrom( Context context, NetLaunchInfo nli )
        {
            add( new ReceiveElem( context, nli ) );
        }

        void addPacketTo( Context context, int gameID, byte[] binmsg )
        {
            add( new SendElem( context, SMS_CMD.DATA, gameID, binmsg ) );
        }

        void addInviteTo( Context context, NetLaunchInfo nli )
        {
            add( new SendElem( context, SMS_CMD.INVITE, nli ) );
        }

        void addGameDied( Context context, int gameID )
        {
            add( new SendElem( context, SMS_CMD.DEATH, gameID, null ) );
        }

        void addAck( Context context, int gameID )
        {
            add( new SendElem( context, SMS_CMD.ACK_INVITE, gameID, null ) );
        }

        void removeSelf()
        {
            NBSProto.removeSelf( this );
        }

        @Override
        public void run() {
            Log.d( TAG, "%s.run() starting for %s", this, mPhone );

            while ( !isInterrupted() ) {
                try {
                    // We want to time out quickly IFF there's a potential
                    // message combination going on, i.e. if mWaitSecs[0] was
                    // set by smsproto_prepOutbound(). Otherwise sleep until
                    // there's something in the queue.
                    long waitSecs = mWaitSecs[0] <= 0 ? 10 * 60 : mWaitSecs[0];
                    QueueElem elem = mQueue.poll( waitSecs, TimeUnit.SECONDS );
                    boolean handled = process( elem, false );
                    if ( /*null == elem && */!handled ) {
                        break;
                    }
                } catch ( InterruptedException iex ) {
                    Log.d( TAG, "poll() threw: %s", iex.getMessage() );
                    break;
                }
            }

            removeSelf();       // should stop additions to the queue

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

        private void add( QueueElem elem ) {
            if ( XWPrefs.getNBSEnabled( elem.context ) ) {
                mQueue.add( elem );
            }
        }

        private boolean processReceive( ReceiveElem elem, boolean exiting )
        {
            if ( null != elem.data ) {
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
                           getPhone() );
                }
            }
            if ( null != elem.nli ) {
                makeForInvite( elem.context, elem.nli );
            }
            return true;
        }

        private boolean processSend( SendElem elem, boolean exiting )
        {
            byte[][] msgs;
            boolean forceNow = mForceNow || exiting;
            if ( null != elem ) {
                msgs = XwJNI.smsproto_prepOutbound( elem.cmd, elem.gameID, elem.data,
                                                    mPhone, mPort, forceNow, mWaitSecs );
            } else { // timed out
                msgs = XwJNI.smsproto_prepOutbound( SMS_CMD.NONE, 0, null, mPhone,
                                                    mPort, forceNow, mWaitSecs );
            }

            if ( null != msgs ) {
                sendBuffers( msgs );
            }
            return null != msgs || mWaitSecs[0] > 0;
        }

        private boolean process( QueueElem qelm, boolean exiting )
        {
            boolean handled;
            if ( null == qelm || qelm instanceof SendElem ) {
                handled = processSend( (SendElem)qelm, exiting );
            } else {
                handled = processReceive( (ReceiveElem)qelm, exiting );
            }
            Log.d( TAG, "%s.process(%s) => %b", this, qelm, handled );
            return handled;
        }

        private SMSServiceHelper mHelper = null;
        protected SMSServiceHelper getHelper( Context context )
        {
            if ( null == mHelper ) {
                mHelper = new SMSServiceHelper( context );
            }
            return mHelper;
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
                getCurThread( phone ).addGameDied( context, gameID );
                // resendFor( phone, SMS_CMD.DEATH, gameID, null );
                s_sentDied.add( gameID );
            }
        }

        private void makeForInvite( Context context, NetLaunchInfo nli )
        {
            if ( nli != null ) {
                getHelper(context).handleInvitation( nli, mPhone, DictFetchOwner.OWNER_SMS );
                getCurThread(mPhone).addAck( context, nli.gameID() );
            }
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
                            Log.d( TAG, "sendBuffers(): sent fragment of len %d",
                                   fragment.length );
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

            if ( success && (0 == (++s_nSent % TOAST_FREQ) && showToasts( context )) ) {
                DbgUtils.showf( context, "Sent msg %d", s_nSent );
            }

            ConnStatusHandler.updateStatusOut( context, null,
                                               CommsConnType.COMMS_CONN_SMS,
                                               success );
        }

        private PendingIntent makeStatusIntent( Context context, String msg )
        {
            Intent intent = new Intent( msg );
            return PendingIntent.getBroadcast( context, 0, intent, 0 );
        }


        static class QueueElem {
            Context context;
            QueueElem( Context context ) { this.context = context; }
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

            @Override
            public String toString()
            {
                return String.format( "SendElem: {cmd: %s, dataLen: %d}", cmd,
                                      data == null ? 0 : data.length );
            }
        }

        private static class ReceiveElem extends QueueElem {
            byte[] data;
            NetLaunchInfo nli;
            ReceiveElem( Context context, byte[] data )
            {
                super( context );
                this.data = data;
            }
            ReceiveElem( Context context, NetLaunchInfo nli )
            {
                super( context );
                this.nli = nli;
            }

            @Override
            public String toString()
            {
                return String.format( "ReceiveElem: {nli: %s, data: %s}", nli, data );
            }
        }
    }

    private static Map<String, NBSProtoThread> sThreadMap = new HashMap<>();

    private static NBSProtoThread getCurThread( String phone )
    {
        return getCurThread( phone, getNBSPort() );
    }

    private static NBSProtoThread getCurThread( String phone, short port )
    {
        NBSProtoThread result = null;
        synchronized ( sThreadMap ) {
            result = sThreadMap.get( phone );
            if ( result == null ) {
                result = new NBSProtoThread( phone, port );
                result.start();
                sThreadMap.put( phone, result );
            }
        }
        Assert.assertTrue( result.getPort() == port || !BuildConfig.DEBUG );
        return result;
    }

    private static void removeSelf( NBSProtoThread self )
    {
        synchronized ( sThreadMap ) {
            String phone = self.getPhone();
            if ( sThreadMap.get(phone) == self ) {
                sThreadMap.remove( phone );
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

    private static void stopCurThreads()
    {
        synchronized( sThreadMap ) {
            for ( String phone : sThreadMap.keySet() ) {
                // should cause them all to call removeSelf() soon
                sThreadMap.get( phone ).interrupt();
            }
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

    private static boolean showToasts( Context context )
    {
        if ( null == s_showToasts ) {
            s_showToasts =
                XWPrefs.getPrefsBoolean( context, R.string.key_show_sms, false );
        }
        boolean result = s_showToasts;
        return result;
    }
}
