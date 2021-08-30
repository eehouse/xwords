/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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
import java.util.Iterator;
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

    private static Set<Integer> s_sentDied = new HashSet<>();

    public static void handleFrom( Context context, byte[] buffer,
                                   String phone, short port )
    {
        addPacketFrom( context, phone, port, buffer );
        Log.d( TAG, "got %d bytes from %s", buffer.length, phone );
        if ( (0 == (++s_nReceived % TOAST_FREQ)) && showToasts( context ) ) {
            DbgUtils.showf( context, "Got NBS msg %d", s_nReceived );
        }

        ConnStatusHandler.updateStatusIn( context, CommsConnType.COMMS_CONN_SMS,
                                          true );
    }

    public static void inviteRemote( Context context, String phone,
                                     NetLaunchInfo nli )
    {
        addInviteTo( context, phone, nli );
    }

    public static int sendPacket( Context context, String phone,
                                  int gameID, byte[] binmsg, String msgID )
    {
        Log.d( TAG, "sendPacket(phone=%s, gameID=%d, len=%d, msgID=%s)",
               phone, gameID, binmsg.length, msgID );
        addPacketTo( context, phone, gameID, binmsg );
        return binmsg.length;
    }

    public static void gameDied( Context context, int gameID, String phone )
    {
        addGameDied( context, phone, gameID );
    }

    public static void onGameDictDownload( Context context, Intent oldIntent )
    {
        NetLaunchInfo nli = MultiService.getMissingDictData( context, oldIntent );
        addInviteFrom( context, nli );
    }

    public static void stopThreads()
    {
        stopCurThreads();
    }

    public static void smsToastEnable( boolean newVal )
    {
        s_showToasts = newVal;
    }

    private static void addPacketFrom( Context context, String phone,
                                       short port, byte[] data )
    {
        add( new ReceiveElem( context, phone, port, data ) );
    }

    private static void addInviteFrom( Context context, NetLaunchInfo nli )
    {
        add( new ReceiveElem( context, nli ) );
    }

    private static void addPacketTo( Context context, String phone,
                                     int gameID, byte[] binmsg )
    {
        add( new SendElem( context, phone, SMS_CMD.DATA, gameID, binmsg ) );
    }

    private static void addInviteTo( Context context, String phone, NetLaunchInfo nli )
    {
        add( new SendElem( context, phone, SMS_CMD.INVITE, nli ) );
    }

    private static void addGameDied( Context context, String phone, int gameID )
    {
        add( new SendElem( context, phone, SMS_CMD.DEATH, gameID, null ) );
    }

    private static void addAck( Context context, String phone, int gameID )
    {
        add( new SendElem( context, phone, SMS_CMD.ACK_INVITE, gameID, null ) );
    }

    private static void add( QueueElem elem ) {
        if ( XWPrefs.getNBSEnabled( elem.context ) ) {
            sQueue.add( elem );
            startThreadOnce();
        }
    }

    private static LinkedBlockingQueue<QueueElem> sQueue = new LinkedBlockingQueue<>();

    static class NBSProtoThread extends Thread {
        private int[] mWaitSecs = { 0 };
        private Set<String> mCachedDests = new HashSet<>();

        NBSProtoThread()
        {
            super( "NBSProtoThread" );
        }

        @Override
        public void run() {
            Log.d( TAG, "%s.run() starting", this );

            while ( !isInterrupted() ) {
                try {
                    // We want to time out quickly IFF there's a potential
                    // message combination going on, i.e. if mWaitSecs[0] was
                    // set by smsproto_prepOutbound(). Otherwise sleep until
                    // there's something in the queue.
                    long waitSecs = mWaitSecs[0] <= 0 ? 10 * 60 : mWaitSecs[0];
                    QueueElem elem = sQueue.poll( waitSecs, TimeUnit.SECONDS );
                    if ( !process( elem ) ) {
                        break;
                    }
                } catch ( InterruptedException iex ) {
                    Log.d( TAG, "poll() threw: %s", iex.getMessage() );
                    break;
                }
            }

            removeSelf( this );

            Log.d( TAG, "%s.run() DONE", this );
        }

        private boolean processReceive( ReceiveElem elem )
        {
            if ( null != elem.data ) {
                SMSProtoMsg[] msgs = XwJNI.smsproto_prepInbound( elem.data, elem.phone, elem.port );
                if ( null != msgs ) {
                    Log.d( TAG, "got %d msgs combined!", msgs.length );
                    for ( int ii = 0; ii < msgs.length; ++ii ) {
                        Log.d( TAG, "%d: type: %s; len: %d", ii, msgs[ii].cmd, msgs[ii].data.length );
                    }
                    for ( SMSProtoMsg msg : msgs ) {
                        receive( elem.context, elem.phone, msg );
                    }
                    getHelper().postEvent( MultiEvent.SMS_RECEIVE_OK );
                } else {
                    Log.d( TAG, "processReceive(): bogus or incomplete message "
                           + "(%d bytes from %s)", elem.data.length, elem.phone );
                }
            }
            if ( null != elem.nli ) {
                makeForInvite( elem.context, elem.phone, elem.nli );
            }
            return true;
        }

        // Called when we have nothing to add, but might be sending what's
        // already waiting for possible combination with other messages.
        private boolean processRetry()
        {
            boolean handled = false;

            for ( Iterator<String> iter = mCachedDests.iterator();
                  iter.hasNext(); ) {
                String[] portAndPhone = iter.next().split( "\0", 2 );
                short port = Short.valueOf(portAndPhone[0]);
                byte[][] msgs = XwJNI
                    .smsproto_prepOutbound( portAndPhone[1], port, mWaitSecs );
                if ( null != msgs ) {
                    sendBuffers( msgs, portAndPhone[1], port );
                    handled = true;
                }
                boolean needsRetry = mWaitSecs[0] > 0;
                if ( !needsRetry ) {
                    iter.remove();
                }
                handled = handled || needsRetry;
            }
            return handled;
        }

        private boolean processSend( SendElem elem )
        {
            byte[][] msgs = XwJNI
                .smsproto_prepOutbound( elem.cmd, elem.gameID, elem.data,
                                        elem.phone, elem.port, mWaitSecs );
            if ( null != msgs ) {
                sendBuffers( msgs, elem.phone, elem.port );
            }

            boolean needsRetry = mWaitSecs[0] > 0;
            if ( needsRetry ) {
                cacheForRetry( elem );
            }

            return null != msgs || needsRetry;
        }

        private boolean process( QueueElem qelm )
        {
            boolean handled;
            if ( null == qelm ) {
                handled = processRetry();
            } else if ( qelm instanceof SendElem ) {
                handled = processSend( (SendElem)qelm );
            } else {
                handled = processReceive( (ReceiveElem)qelm );
            }
            Log.d( TAG, "%s.process(%s) => %b", this, qelm, handled );
            return handled;
        }

        private SMSServiceHelper mHelper = null;
        protected SMSServiceHelper getHelper()
        {
            if ( null == mHelper ) {
                mHelper = new SMSServiceHelper( XWApp.getContext() );
            }
            return mHelper;
        }

        private void receive( Context context, String phone, SMSProtoMsg msg )
        {
            Log.i( TAG, "receive(cmd=%s)", msg.cmd );
            switch( msg.cmd ) {
            case INVITE:
                makeForInvite( context, phone,
                               NetLaunchInfo.makeFrom( context, msg.data ) );
                break;
            case DATA:
                if ( feedMessage( context, msg.gameID, msg.data,
                                  new CommsAddrRec( phone ) ) ) {
                    SMSResendReceiver.resetTimer( context );
                }
                break;
            case DEATH:
                getHelper().postEvent( MultiEvent.MESSAGE_NOGAME, msg.gameID );
                break;
            case ACK_INVITE:
                getHelper().postEvent( MultiEvent.NEWGAME_SUCCESS, msg.gameID );
                break;
            default:
                Log.w( TAG, "unexpected cmd %s", msg.cmd );
                Assert.failDbg();
                break;
            }
        }

        private boolean feedMessage( Context context, int gameID, byte[] msg,
                                     CommsAddrRec addr )
        {
            XWServiceHelper.ReceiveResult rslt = getHelper()
                .receiveMessage( gameID, null, msg, addr );
            if ( XWServiceHelper.ReceiveResult.GAME_GONE == rslt ) {
                sendDiedPacket( context, addr.sms_phone, gameID );
            }
            Log.d( TAG, "feedMessage(): rslt: %s", rslt );
            return rslt == XWServiceHelper.ReceiveResult.OK;
        }

        private void sendDiedPacket( Context context, String phone, int gameID )
        {
            if ( !s_sentDied.contains( gameID ) ) {
                addGameDied( context, phone, gameID );
                s_sentDied.add( gameID );
            }
        }

        private void makeForInvite( Context context, String phone, NetLaunchInfo nli )
        {
            if ( nli != null ) {
                getHelper().handleInvitation( nli, phone, DictFetchOwner.OWNER_SMS );
                addAck( context, phone, nli.gameID() );
            }
        }

        private void sendBuffers( byte[][] fragments, String phone, short port )
        {
            Context context = XWApp.getContext();
            boolean success = false;
            if ( XWPrefs.getNBSEnabled( context ) ) {

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
                        Assert.failDbg(); // shouldn't be trying to do this!!!
                    } catch ( java.lang.SecurityException se ) {
                        getHelper().postEvent( MultiEvent.SMS_SEND_FAILED_NOPERMISSION );
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

            ConnStatusHandler.updateStatusOut( context, CommsConnType.COMMS_CONN_SMS,
                                               success );
        }

        private PendingIntent makeStatusIntent( Context context, String msg )
        {
            Intent intent = new Intent( msg );
            return PendingIntent.getBroadcast( context, 0, intent, 0 );
        }

        private void cacheForRetry( QueueElem elem )
        {
            String dest = elem.port + "\0" + elem.phone;
            mCachedDests.add( dest );
        }
    }

    private static class QueueElem {
        Context context;
        String phone;
        short port;
        QueueElem( Context context, String phone, short port )
        {
            this.context = context;
            this.phone = phone;
            this.port = port;
        }

        QueueElem( Context context, String phone )
        {
            this( context, phone, getNBSPort() );
        }
    }

    private static class SendElem extends QueueElem {
        SMS_CMD cmd;
        int gameID;
        byte[] data;
        SendElem( Context context, String phone, SMS_CMD cmd, int gameID,
                  byte[] data ) {
            super( context, phone );
            this.cmd = cmd;
            this.gameID = gameID;
            this.data = data;
        }
        SendElem( Context context, String phone, SMS_CMD cmd, NetLaunchInfo nli ) {
            this( context, phone, cmd, 0, nli.asByteArray() );
        }

        @Override
        public String toString()
        {
            return String.format( "SendElem: {cmd: %s, dataLen: %d}", cmd,
                                  data == null ? 0 : data.length );
        }
    }

    private static class ReceiveElem extends QueueElem {
        // One of these two will be set
        byte[] data;
        NetLaunchInfo nli;

        ReceiveElem( Context context, String phone, short port, byte[] data )
        {
            super( context, phone, port );
            this.data = data;
        }

        ReceiveElem( Context context, NetLaunchInfo nli )
        {
            super( context, nli.phone );
            this.nli = nli;
        }

        @Override
        public String toString()
        {
            return String.format( "ReceiveElem: {nli: %s, data: %s}", nli, data );
        }
    }

    private static NBSProtoThread[] sThreadHolder = { null };

    private static void startThreadOnce()
    {
        synchronized ( sThreadHolder ) {
            if ( sThreadHolder[0] == null ) {
                sThreadHolder[0] = new NBSProtoThread();
                sThreadHolder[0].start();
            }
        }
    }

    private static void removeSelf( NBSProtoThread self )
    {
        synchronized ( sThreadHolder ) {
            if ( sThreadHolder[0] == self ) {
                sThreadHolder[0] = null;
            }
        }
    }

    private static class NBSMsgSink extends MultiMsgSink {
        private Context mContext;
        public NBSMsgSink( Context context ) {
            super( context );
            mContext = context;
        }

        @Override
        public int sendViaSMS( byte[] buf, String msgID, int gameID, CommsAddrRec addr )
        {
            return sendPacket( mContext, addr.sms_phone, gameID, buf, msgID );
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
            return new NBSMsgSink( mContext );
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
        synchronized( sThreadHolder ) {
            NBSProtoThread self = sThreadHolder[0];
            if ( null != self ) {
                self.interrupt();
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
