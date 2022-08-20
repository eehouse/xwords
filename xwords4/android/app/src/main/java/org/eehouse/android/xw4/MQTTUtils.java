/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2022 by Eric House (xwords@eehouse.org).  All rights
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

import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.text.TextUtils;

import org.eclipse.paho.client.mqttv3.IMqttActionListener;
import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.eclipse.paho.client.mqttv3.IMqttToken;
import org.eclipse.paho.client.mqttv3.MqttAsyncClient;
import org.eclipse.paho.client.mqttv3.MqttCallbackExtended;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;

import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.atomic.AtomicReference;
import javax.net.ssl.HttpsURLConnection;

import org.json.JSONException;
import org.json.JSONObject;

import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.ConnExpl;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

public class MQTTUtils extends Thread
    implements IMqttActionListener, MqttCallbackExtended {
    private static final String TAG = MQTTUtils.class.getSimpleName();
    private static final String KEY_NEXT_REG = TAG + "/next_reg";
    private static final String KEY_LAST_WRITE = TAG + "/last_write";
    private static final String KEY_TMP_KEY = TAG + "/tmp_key";
    private static enum State { NONE, CONNECTING, CONNECTED, SUBSCRIBING, SUBSCRIBED,
                                CLOSING };

    private static final long MIN_BACKOFF = 1000 * 60 * 2; // 2 minutes
    private static final long MAX_BACKOFF = 1000 * 60 * 60 * 4; // 4 hours, to test

    private static MQTTUtils[] sInstance = {null};
    private static long sNextReg = 0;
    private static String sLastRev = null;

    private MqttAsyncClient mClient;
    private final String mDevID;
    private String[] mTopics = { null, null };
    private Context mContext;
    private MsgThread mMsgThread;
    private LinkedBlockingQueue<MessagePair> mOutboundQueue = new LinkedBlockingQueue<>();
    private boolean mShouldExit = false;
    private State mState = State.NONE;

    private static TimerReceiver.TimerCallback sTimerCallbacks
        = new TimerReceiver.TimerCallback() {
                @Override
                public void timerFired( Context context )
                {
                    Log.d( TAG, "timerFired()" );
                    MQTTUtils.timerFired( context );
                }

                @Override
                public long incrementBackoff( long backoff )
                {
                    if ( backoff < MIN_BACKOFF ) {
                        backoff = MIN_BACKOFF;
                    } else {
                        backoff = backoff * 150 / 100;
                    }
                    if ( MAX_BACKOFF <= backoff ) {
                        backoff = MAX_BACKOFF;
                    }
                    return backoff;
                }
            };

    private static NetStateCache.StateChangedIf sStateChangedIf =
        new NetStateCache.StateChangedIf() {
            @Override
            public void onNetAvail( Context context, boolean nowAvailable ) {
                Log.d( TAG, "onNetAvail(avail=%b)", nowAvailable );
                DbgUtils.assertOnUIThread();
                if ( nowAvailable ) {
                    GameUtils.resendAllIf( context, CommsConnType.COMMS_CONN_MQTT );
                }
            }
        };

    public static void init( Context context )
    {
        Log.d( TAG, "init()" );
        NetStateCache.register( context, sStateChangedIf );
        getOrStart( context );
    }

    public static void onResume( Context context )
    {
        Log.d( TAG, "onResume()" );
        getOrStart( context );
        NetStateCache.register( context, sStateChangedIf );
    }

    public static void onDestroy( Context context )
    {
        NetStateCache.unregister( context, sStateChangedIf );
    }

    public static void setEnabled( Context context, boolean enabled )
    {
        Log.d( TAG, "setEnabled( %b )", enabled );
        if ( enabled ) {
            getOrStart( context );
        } else {
            onConfigChanged( context );
        }
    }

    private static void timerFired( Context context )
    {
        MQTTUtils instance;
        synchronized ( sInstance ) {
            instance = sInstance[0];
        }

        if ( null != instance && !instance.isConnected() ) {
            clearInstance( instance );
        }
        getOrStart( context );  // no-op if have instance
    }

    static void onConfigChanged( Context context )
    {
        MQTTUtils instance;
        synchronized ( sInstance ) {
            instance = sInstance[0];
        }
        if ( null != instance ) {
            clearInstance( instance );
        }
    }

    private static MQTTUtils getOrStart( Context context )
    {
        MQTTUtils result = null;
        if ( XWPrefs.getMQTTEnabled( context ) ) {
            synchronized( sInstance ) {
                result = sInstance[0];
            }
            if ( null == result ) {
                try {
                    result = new MQTTUtils(context);
                    setInstance( result );
                    result.start();
                } catch ( MqttException me ) {
                    result = null;
                }
            }
        }
        return result;
    }

    private static class MessagePair {
        byte[] mPacket;
        String mTopic;
        MessagePair( String topic, byte[] packet ) {
            mPacket = packet;
            mTopic = topic;
        }
    }

    @Override
    public void run()
    {
        long startTime = Utils.getCurSeconds();
        Log.d( TAG, "%H.run() starting", this );

        setup();
        for ( long totalSlept = 0; !mShouldExit && totalSlept < 10000; ) {
            try {
                // this thread can be fed before the connection is
                // established. Wait for that before removing packets from the
                // queue.
                if ( !mClient.isConnected() ) {
                    Log.d( TAG, "%H.run(): not connected; sleeping...", MQTTUtils.this );
                    final long thisSleep = 1000;
                    Thread.sleep(thisSleep);
                    totalSlept += thisSleep;
                    continue;
                }
                totalSlept = 0;
                MessagePair pair = mOutboundQueue.take();
                MqttMessage message = new MqttMessage( pair.mPacket );
                mClient.publish( pair.mTopic, message );
            } catch ( MqttException me ) {
                me.printStackTrace();
                break;
            } catch ( InterruptedException ie ) {
                // ie.printStackTrace();
                break;
            }
        }
        clearInstance();

        long now = Utils.getCurSeconds();
        Log.d( TAG, "%H.run() exiting after %d seconds", this,
               now - startTime );
    }

    private boolean isConnected()
    {
        MqttAsyncClient client = mClient;
        boolean result = null != client
            && client.isConnected()
            && mState != State.CLOSING;
        Log.d( TAG, "isConnected() => %b", result );
        return result;
    }

    private void enqueue( String topic, byte[] packet )
    {
        mOutboundQueue.add( new MessagePair( topic, packet ) );
    }

    private static void setInstance( MQTTUtils newInstance )
    {
        MQTTUtils oldInstance;
        synchronized ( sInstance ) {
            oldInstance = sInstance[0];
            Log.d( TAG, "setInstance(): changing sInstance[0] from %H to %H", oldInstance, newInstance );
            sInstance[0] = newInstance;
        }
        if ( null != oldInstance ) {
            oldInstance.disconnect();
        }
    }

    private static void clearInstance( final MQTTUtils curInstance )
    {
        synchronized ( sInstance ) {
            if ( sInstance[0] == curInstance ) {
                sInstance[0] = null;
            } else {
                Log.e( TAG, "clearInstance(): was NOT disconnecting %H because "
                       + "not current", curInstance );
                // I don't know why I was NOT disconnecting if the instance didn't match.
                // If it was the right thing to do after all, add explanation here!!!!
                // curInstance = null; // protect from disconnect() call -- ????? WHY DO THIS ?????
            }
        }
        curInstance.disconnect();
    }

    private MQTTUtils( Context context ) throws MqttException
    {
        Log.d( TAG, "%H.<init>()", this );
        mContext = context;
        mDevID = XwJNI.dvc_getMQTTDevID( mTopics );
        Assert.assertTrueNR( 16 == mDevID.length() );
        mMsgThread = new MsgThread();

        String host = XWPrefs.getPrefsString( context, R.string.key_mqtt_host );
        int port = XWPrefs.getPrefsInt( context, R.string.key_mqtt_port, 1883 );
        String url = String.format( java.util.Locale.US, "tcp://%s:%d", host, port );
        Log.d( TAG, "Using url: %s", url );
        mClient = new MqttAsyncClient( url, mDevID, new MemoryPersistence() );
        mClient.setCallback( this );
    }

    private void setState( State newState )
    {
        Log.d( TAG, "%H.setState(): was %s, now %s", this, mState, newState );
        boolean stateOk;
        switch ( newState ) {
        case CONNECTED:
            stateOk = mState == State.CONNECTING;
            if ( stateOk ) {
                mState = newState;
                subscribe();
            }
            break;
        case SUBSCRIBED:
            stateOk = mState == State.SUBSCRIBING;
            if ( stateOk ) {
                mState = newState;
                mMsgThread.start();
            }
            break;
        default:
            stateOk = true;
            mState = newState;
            Log.d( TAG, "doing nothing on %s", mState );
            break;
        }

        if ( !stateOk ) {
            Log.e( TAG, "%H.setState(): bad state for %s: %s", this, newState, mState );
        }
    }

    // Last Will and Testimate is the way I can get the server notified of a
    // closed connection.
    private void addLWT( MqttConnectOptions mqttConnectOptions )
    {
        try {
            JSONObject payload = new JSONObject();
            payload.put( "devid", mDevID );
            payload.put( "ts", Utils.getCurSeconds() );
            mqttConnectOptions.setWill( "xw4/device/LWT", payload.toString().getBytes(), 2, false );

            // mqttConnectOptions.setKeepAliveInterval( 15 ); // seconds; for testing
        } catch ( JSONException je ) {
            Log.e( TAG, "addLWT() ex: %s", je );
        }
    }

    private void setup()
    {
        Log.d( TAG, "setup()" );
        MqttConnectOptions mqttConnectOptions = new MqttConnectOptions();
        mqttConnectOptions.setAutomaticReconnect(true);
        mqttConnectOptions.setCleanSession(false);
        mqttConnectOptions.setUserName("xwuser");
        mqttConnectOptions.setPassword("xw4r0cks".toCharArray());
        addLWT( mqttConnectOptions );

        try {
            setState( State.CONNECTING );
            mClient.connect( mqttConnectOptions, null, this );
        } catch ( MqttException ex ) {
            ex.printStackTrace();
        } catch ( java.lang.IllegalStateException ise ) {
            ise.printStackTrace();
        } catch ( Exception ise ) {
            ise.printStackTrace();
            clearInstance();
        }

        registerOnce();
    }

    private void registerOnce()
    {
        if ( 0 == sNextReg ) {
            sNextReg = DBUtils.getLongFor( mContext, KEY_NEXT_REG, 1 );
            sLastRev = DBUtils.getStringFor( mContext, KEY_LAST_WRITE, "" );
        }
        long now = Utils.getCurSeconds();
        Log.d( TAG, "registerOnce(): now: %d; nextReg: %d", now, sNextReg );
        String revString = BuildConfig.GIT_REV + ':' + BuildConfig.VARIANT_NAME;
        if ( now > sNextReg || ! revString.equals(sLastRev) ) {
            try {
                JSONObject params = new JSONObject();
                params.put( "devid", mDevID );
                params.put( "gitrev", BuildConfig.GIT_REV );
                params.put( "os", Build.MODEL );
                // PENDING remove me in favor of SDK_INT
                params.put( "vers", Build.VERSION.RELEASE );
                params.put( "versI", Build.VERSION.SDK_INT );
                params.put( "vrntCode", BuildConfig.VARIANT_CODE );
                params.put( "vrntName", BuildConfig.VARIANT_NAME );
                if ( BuildConfig.DEBUG ) {
                    params.put( "dbg", true );
                }
                params.put( "myNow", now );
                params.put( "loc", LocUtils.getCurLocale( mContext ) );
                params.put( "tmpKey", getTmpKey(mContext) );
                params.put( "frstV", Utils.getFirstVersion( mContext ) );
                params.put( "relayDID", DevID.getRelayDevID( mContext ) );

                Log.d( TAG, "registerOnce(): sending %s", params );
                HttpsURLConnection conn
                    = NetUtils.makeHttpsMQTTConn( mContext, "register" );
                String resStr = NetUtils.runConn( conn, params, true );
                if ( null != resStr ) {
                    JSONObject response = new JSONObject( resStr );
                    Log.d( TAG, "registerOnce(): got %s", response );

                    if ( response.optBoolean( "success", true ) ) {
                        long atNext = response.optLong( "atNext", 0 );
                        if ( 0 < atNext ) {
                            DBUtils.setLongFor( mContext, KEY_NEXT_REG, atNext );
                            sNextReg = atNext;
                            DBUtils.setStringFor( mContext, KEY_LAST_WRITE, revString );
                            sLastRev = revString;
                        }

                        String dupID = response.optString( "dupID", "" );
                        if ( dupID.equals( mDevID ) ) {
                            Log.e( TAG, "********** %s bad; need new devID!!! **********", dupID );
                            XwJNI.dvc_resetMQTTDevID();
                            // Force a reconnect asap
                            DBUtils.setLongFor( mContext, KEY_NEXT_REG, 0 );
                            sNextReg = 0;
                            clearInstance();
                        }
                    }
                } else {
                    Log.e( TAG, "registerOnce(): null back from runConn()" );
                }
            } catch ( JSONException je ) {
                Log.e( TAG, "registerOnce() ex: %s", je );
            }
        }
    }

    private static int sTmpKey;
    private static int getTmpKey( Context context )
    {
        while ( 0 == sTmpKey ) {
            sTmpKey = DBUtils.getIntFor( context, KEY_TMP_KEY, 0 );
            if ( 0 == sTmpKey ) {
                sTmpKey = Math.abs( Utils.nextRandomInt() );
                DBUtils.setIntFor( context, KEY_TMP_KEY, sTmpKey );
            }
        }
        return sTmpKey;
    }

    private void disconnect()
    {
        Log.d( TAG, "%H.disconnect()", this );

        interrupt();
        mMsgThread.interrupt();
        try {
            mMsgThread.join();
            Log.d( TAG, "%H.disconnect(); JOINED thread", this );
        } catch ( InterruptedException ie ) {
            Log.e( TAG, "%H.disconnect(); got ie from join: %s", this, ie );
        }

        mShouldExit = true;

        setState( State.CLOSING );

        MqttAsyncClient client;
        synchronized ( this ) {
            client = mClient;
            mClient = null;
        }

        // Hack. Problem is that e.g. unsubscribe will throw an exception if
        // you're not subscribed. That can't prevent us from continuing to
        // disconnect() and close. Rather than wrap each in its own try/catch,
        // run 'em in a loop in a single try/catch.
        if ( null == client ) {
            Log.e( TAG, "disconnect(): null client" );
        } else {
            startDisconThread( client );
        }

        // Make sure we don't need to call clearInstance(this)
        synchronized ( sInstance ) {
            Assert.assertTrueNR( sInstance[0] != this );
        }
        Log.d( TAG, "%H.disconnect() DONE", this );
    }

    private void startDisconThread( final MqttAsyncClient client )
    {
        new Thread( new Runnable() {
                @Override
                public void run() {
                    outer:
                    for ( int ii = 0; ; ++ii ) {
                        String action = null;
                        IMqttToken token = null;
                        try {
                            switch ( ii ) {
                            case 0:
                                action = "unsubscribe";
                                token = client.unsubscribe( mTopics );
                                break;      // not continue, which skips the Log() below
                            case 1:
                                action = "disconnect";
                                token = client.disconnect();
                                break;
                            case 2:
                                action = "close";
                                client.close();
                                break;
                            default:
                                break outer;
                            }
                            if ( null != token ) {
                                Log.d( TAG, "%H.disconnect(): %s() waiting", this, action );
                                token.waitForCompletion();
                            }
                            if ( null != action ) {
                                Log.d( TAG, "%H.run(): client.%s() succeeded", this, action );
                            }
                        } catch ( MqttException mex ) {
                            Log.e( TAG, "%H.run(): client.%s(): got mex: %s",
                                   this, action, mex );
                            // Assert.failDbg(); // fired, so remove for now
                        } catch ( Exception ex ) {
                            Log.e( TAG, "%H.run(): client.%s(): got ex %s",
                                   this, action, ex );
                            Assert.failDbg(); // is this happening?
                        }
                    }
                }
            } ).start();
    }

    private void clearInstance()
    {
        Log.d( TAG, "%H.clearInstance()", this );
        clearInstance( this );
    }

    public static void inviteRemote( Context context, String invitee,
                                     NetLaunchInfo nli )
    {
        String[] topic = {invitee};
        byte[] packet = XwJNI.dvc_makeMQTTInvite( nli, topic );
        addToSendQueue( context, topic[0], packet );
    }

    private static void notifyNotHere( Context context, String addressee,
                                       int gameID )
    {
        String[] topic = {addressee};
        byte[] packet = XwJNI.dvc_makeMQTTNoSuchGame( gameID, topic );
        addToSendQueue( context, topic[0], packet );
    }

    public static int send( Context context, String addressee, int gameID,
                            int timestamp, byte[] buf )
    {
        Log.d( TAG, "send(to:%s, len: %d)", addressee, buf.length );
        Assert.assertTrueNR( 16 == addressee.length() );
        String[] topic = {addressee};
        byte[] packet = XwJNI.dvc_makeMQTTMessage( gameID, timestamp, buf, topic );
        addToSendQueue( context, topic[0], packet );
        return buf.length;
    }

    private static void addToSendQueue( Context context, String topic,
                                        byte[] packet )
    {
        MQTTUtils instance = getOrStart( context );
        if ( null != instance ) {
            instance.enqueue( topic, packet );
        }
    }

    public static void gameDied( Context context, String devID, int gameID )
    {
        String[] topic = { devID };
        byte[] packet = XwJNI.dvc_makeMQTTNoSuchGame( gameID, topic );
        addToSendQueue( context, topic[0], packet );
    }

    public static void ackMessage( Context context, int gameID,
                                   String senderDevID, byte[] payload )
    {
        String sum = Utils.getMD5SumFor( payload );
        JSONObject params = new JSONObject();
        try {
            params.put( "sum", sum );
            params.put( "gid", gameID );
            // params.put( "from", senderDevID );
            // params.put( "to", XwJNI.dvc_getMQTTDevID( null ) );

            HttpsURLConnection conn
                = NetUtils.makeHttpsMQTTConn( context, "ack" );
            String resStr = NetUtils.runConn( conn, params, true );
            Log.d( TAG, "runConn(ack) => %s", resStr );
        } catch ( JSONException je ) {
            Log.e( TAG, "ackMessage() ex: %s", je );
        }
    }

    // MqttCallbackExtended
    @Override
    public void connectComplete(boolean reconnect, String serverURI)
    {
        Log.d( TAG, "%H.connectComplete(reconnect=%b, serverURI=%s)", this,
               reconnect, serverURI );
    }

    @Override
    public void connectionLost( Throwable cause )
    {
        Log.d( TAG, "%H.connectionLost(%s)", this, cause );
        clearInstance();
    }

    @Override
    public void messageArrived( String topic, MqttMessage message )
        throws Exception
    {
        Log.d( TAG, "%H.messageArrived(topic=%s)", this, topic );
        mMsgThread.add( topic, message.getPayload() );
        ConnStatusHandler
            .updateStatusIn( mContext, CommsConnType.COMMS_CONN_MQTT, true );

        TimerReceiver.setBackoff( mContext, sTimerCallbacks, MIN_BACKOFF );
    }

    @Override
    public void deliveryComplete(IMqttDeliveryToken token)
    {
        // Log.d( TAG, "%H.deliveryComplete(token=%s)", this, token );
        ConnStatusHandler
            .updateStatusOut( mContext, CommsConnType.COMMS_CONN_MQTT, true );
        TimerReceiver.setBackoff( mContext, sTimerCallbacks, MIN_BACKOFF );
    }

    private void subscribe()
    {
        Assert.assertTrueNR( null != mTopics && 2 == mTopics.length );
        final int qos = XWPrefs
            .getPrefsInt( mContext, R.string.key_mqtt_qos, 2 );
        int qoss[] = { qos, qos };

        setState( State.SUBSCRIBING );
        try {
            mClient.subscribe( mTopics, qoss, null, this );
            // Log.d( TAG, "subscribed to %s", TextUtils.join( ", ", mTopics ) );
        } catch ( MqttException ex ) {
            ex.printStackTrace();
        } catch ( Exception ex ) {
            ex.printStackTrace();
            clearInstance();
        }
    }

    // IMqttActionListener
    @Override
    public void onSuccess( IMqttToken asyncActionToken )
    {
        Log.d( TAG, "%H.onSuccess(%s); cur state: %s", asyncActionToken,
               this, mState );
        switch ( mState ) {
        case CONNECTING:
            setState( State.CONNECTED );
            break;
        case SUBSCRIBING:
            setState( State.SUBSCRIBED );
            break;
        default:
            Log.e( TAG, "%H.onSuccess(): unexpected state %s", mState );
        }
    }

    @Override
    public void onFailure(IMqttToken asyncActionToken, Throwable exception)
    {
        Log.d( TAG, "%H.onFailure(%s, %s); cur state: %s", this,
               asyncActionToken, exception, mState );
        ConnStatusHandler
            .updateStatus( mContext, null, CommsConnType.COMMS_CONN_MQTT,
                           false );
    }

    private class MsgThread extends Thread {
        private LinkedBlockingQueue<MessagePair> mQueue
            = new LinkedBlockingQueue<>();

        void add( String topic, byte[] msg ) {
            mQueue.add( new MessagePair( topic, msg ) );
        }

        @Override
        public void run()
        {
            long startTime = Utils.getCurSeconds();
            Log.d( TAG, "%H.MsgThread.run() starting", MQTTUtils.this );
            for ( ; ; ) {
                try {
                    MessagePair pair = mQueue.take();
                    String topic = pair.mTopic;
                    if ( topic.equals( mTopics[0] ) ) {
                        XwJNI.dvc_parseMQTTPacket( pair.mPacket );
                    } else if ( topic.equals( mTopics[1] ) ) {
                        postNotification( pair );
                    }
                } catch ( InterruptedException ie ) {
                    // Assert.failDbg();
                    break;
                } catch ( JSONException je ) {
                    Log.e( TAG, "run() ex: %s", je );
                }
            }
            long now = Utils.getCurSeconds();
            Log.d( TAG, "%H.MsgThread.run() exiting after %d seconds",
                   MQTTUtils.this, now - startTime );
        }

        private void postNotification( MessagePair pair ) throws JSONException
        {
            JSONObject obj = new JSONObject( new String(pair.mPacket) );
            String msg = obj.optString( "msg", null );
            if ( null != msg ) {
                String title = obj.optString( "title", null );
                if ( null == title ) {
                    title = LocUtils.getString( mContext, R.string.remote_msg_title );
                }
                Intent alertIntent = GamesListDelegate.makeAlertIntent( mContext, msg );
                int code = msg.hashCode() ^ title.hashCode();
                Utils.postNotification( mContext, alertIntent, title, msg, code );
            }
        }
    }

    public static void handleMessage( Context context, CommsAddrRec from,
                                      int gameID, byte[] data )
    {
        long[] rowids = DBUtils.getRowIDsFor( context, gameID );
        Log.d( TAG, "handleMessage(): got %d rows for gameID %d", rowids.length, gameID );
        if ( 0 == rowids.length ) {
            notifyNotHere( context, from.mqtt_devID, gameID );
        } else {
            MQTTServiceHelper helper = new MQTTServiceHelper( context, from );
            for ( long rowid : rowids ) {
                MultiMsgSink sink = new MultiMsgSink( context, rowid );
                helper.receiveMessage( rowid, sink, data );
            }
        }
    }

    public static void handleGameGone( Context context, CommsAddrRec from, int gameID )
    {
        String player = XwJNI.kplr_nameForMqttDev( from.mqtt_devID );
        ConnExpl expl = null == player ? null
            : new ConnExpl( CommsConnType.COMMS_CONN_MQTT, player );
        new MQTTServiceHelper( context, from )
            .postEvent( MultiService.MultiEvent.MESSAGE_NOGAME, gameID,
                        expl );
    }

    public static void fcmConfirmed( Context context, boolean working )
    {
        if ( working ) {
            DBUtils.setLongFor( context, KEY_NEXT_REG, 0 );
        }
    }

    public static void makeOrNotify( Context context, NetLaunchInfo nli )
    {
        new MQTTServiceHelper( context ).handleInvitation( nli );
    }

    private static class MQTTServiceHelper extends XWServiceHelper {
        private CommsAddrRec mReturnAddr;

        MQTTServiceHelper( Context context )
        {
            super( context );
        }

        MQTTServiceHelper( Context context, CommsAddrRec from )
        {
            this( context );
            mReturnAddr = from;
        }

        private void handleInvitation( NetLaunchInfo nli )
        {
            handleInvitation( nli, null, MultiService.DictFetchOwner.OWNER_MQTT );
        }

        private void receiveMessage( long rowid, MultiMsgSink sink, byte[] msg )
        {
            Log.d( TAG, "receiveMessage(rowid=%d, len=%d)", rowid, msg.length );
            receiveMessage( rowid, sink, msg, mReturnAddr );
        }
    }
}
