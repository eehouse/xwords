/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2020 by Eric House (xwords@eehouse.org).  All
 * rights reserved.
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
import android.os.Build;

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

public class MQTTUtils extends Thread implements IMqttActionListener, MqttCallbackExtended {
    private static final String TAG = MQTTUtils.class.getSimpleName();
    private static final String KEY_NEXT_REG = TAG + "/next_reg";
    private static final String KEY_LAST_WRITE = TAG + "/last_write";
    private static final String KEY_TMP_KEY = TAG + "/tmp_key";
    private static enum State { NONE, CONNECTING, CONNECTED, SUBSCRIBING, SUBSCRIBED,
                                CLOSING };

    private static MQTTUtils[] sInstance = {null};
    private static long sNextReg = 0;
    private static String sLastRev = null;

    private MqttAsyncClient mClient;
    private String mDevID;
    private String mTopic;
    private Context mContext;
    private MsgThread mMsgThread;
    private LinkedBlockingQueue<MessagePair> mOutboundQueue = new LinkedBlockingQueue<>();
    private boolean mShouldExit = false;
    private State mState = State.NONE;

    public static void init( Context context )
    {
        Log.d( TAG, "init(OFFER_MQTT:%b)", BuildConfig.OFFER_MQTT );
        getOrStart( context );
    }

    public static void onResume( Context context )
    {
        Log.d( TAG, "onResume()" );
        getOrStart( context );
    }

    public static void onFCMReceived( Context context )
    {
        Log.d( TAG, "onFCMReceived()" );
        onConfigChanged( context );
        getOrStart( context );
    }

    public static void timerFired( Context context )
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
        if ( BuildConfig.OFFER_MQTT ) {
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
                ie.printStackTrace();
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
        String[] topic = {null};
        mDevID = XwJNI.dvc_getMQTTDevID( topic );
        Assert.assertTrueNR( 16 == mDevID.length() );
        mTopic = topic[0];
        mMsgThread = new MsgThread();

        String host = XWPrefs.getPrefsString( context, R.string.key_mqtt_host );
        Log.d( TAG, "host: %s", host );
        int port = XWPrefs.getPrefsInt( context, R.string.key_mqtt_port, 1883 );
        String url = String.format( java.util.Locale.US, "tcp://%s:%d", host, port );
        Log.d( TAG, "using url: %s", url );
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

    private void setup()
    {
        Log.d( TAG, "setup()" );
        MqttConnectOptions mqttConnectOptions = new MqttConnectOptions();
        mqttConnectOptions.setAutomaticReconnect(true);
        mqttConnectOptions.setCleanSession(false);
        mqttConnectOptions.setUserName("xwuser");
        mqttConnectOptions.setPassword("xw4r0cks".toCharArray());

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
                params.put( "vers", Build.VERSION.RELEASE );
                params.put( "vrntCode", BuildConfig.VARIANT_CODE );
                params.put( "vrntName", BuildConfig.VARIANT_NAME );
                params.put( "dbg", BuildConfig.DEBUG );
                params.put( "myNow", now );
                params.put( "loc", LocUtils.getCurLocale( mContext ) );
                params.put( "tmpKey", getTmpKey(mContext) );
                params.put( "frstV", Utils.getFirstVersion( mContext ) );

                String fcmid = FBMService.getFCMDevID( mContext );
                if ( null != fcmid ) {
                    params.put( "fcmid", fcmid );
                }

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

        // Hack. Problem is that e.g. unsubscribe will throw an exception if
        // you're not subscribed. That can't prevent us from continuing to
        // disconnect() and close. Rather than wrap each in its own try/catch,
        // run 'em in a loop in a single try/catch.
        if ( null == mClient ) {
            Log.e( TAG, "disconnect(): null client" );
        } else {
            outer:
            for ( int ii = 0; ; ++ii ) {
                String action = null;
                try {
                    switch ( ii ) {
                    case 0:
                        action = "unsubscribe";
                        mClient.unsubscribe( mDevID );
                        break;      // not continue, which skips the Log() below
                    case 1:
                        action = "disconnect";
                        mClient.disconnect();
                        break;
                    case 2:
                        action = "close";
                        mClient.close();
                        break;
                    default:
                        break outer;
                    }
                    Log.d( TAG, "%H.disconnect(): %s() succeeded", this, action );
                } catch ( MqttException mex ) {
                    Log.e( TAG, "%H.disconnect(): %s(): got mex %s",
                           this, action, mex );
                } catch ( Exception ex ) {
                    Log.e( TAG, "%H.disconnect(): %s(): got ex %s",
                           this, action, ex );
                }
            }
            mClient = null;
        }

        // Make sure we don't need to call clearInstance(this)
        synchronized ( sInstance ) {
            Assert.assertTrueNR( sInstance[0] != this );
        }
        Log.d( TAG, "%H.disconnect() DONE", this );
    }

    private void clearInstance()
    {
        Log.d( TAG, "%H.clearInstance()", this );
        clearInstance( this );
    }

    public static void inviteRemote( Context context, String invitee, NetLaunchInfo nli )
    {
        String[] topic = {invitee};
        byte[] packet = XwJNI.dvc_makeMQTTInvite( nli, topic );
        addToSendQueue( context, topic[0], packet );
    }

    private static void notifyNotHere( Context context, String addressee, int gameID )
    {
        String[] topic = {addressee};
        byte[] packet = XwJNI.dvc_makeMQTTNoSuchGame( gameID, topic );
        addToSendQueue( context, topic[0], packet );
    }

    public static int send( Context context, String addressee, int gameID, byte[] buf )
    {
        Log.d( TAG, "send(to:%s, len: %d)", addressee, buf.length );
        Assert.assertTrueNR( 16 == addressee.length() );
        String[] topic = {addressee};
        byte[] packet = XwJNI.dvc_makeMQTTMessage( gameID, buf, topic );
        addToSendQueue( context, topic[0], packet );
        return buf.length;
    }

    private static void addToSendQueue( Context context, String topic, byte[] packet )
    {
        MQTTUtils instance = getOrStart( context );
        if ( null != instance ) {
            instance.enqueue( topic, packet );
        }
    }

    public static void gameDied( Context context, String devID, int gameID )
    {
        if ( BuildConfig.DO_MQTT_GAME_GONE ) {
            String[] topic = { devID };
            byte[] packet = XwJNI.dvc_makeMQTTNoSuchGame( gameID, topic );
            addToSendQueue( context, topic[0], packet );
        } else {
            Log.e( TAG, "gameDied() not handled" ); // fix me
        }
    }

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
    public void messageArrived( String topic, MqttMessage message) throws Exception
    {
        Log.d( TAG, "%H.messageArrived(topic=%s)", this, topic );
        Assert.assertTrueNR( topic.equals(mTopic) );
        mMsgThread.add( message.getPayload() );
        ConnStatusHandler
            .updateStatusIn( mContext, CommsConnType.COMMS_CONN_MQTT, true );

        TimerReceiver.restartBackoff( mContext );
    }

    @Override
    public void deliveryComplete(IMqttDeliveryToken token)
    {
        Log.d( TAG, "%H.deliveryComplete(token=%s)", this, token );
        ConnStatusHandler
            .updateStatusOut( mContext, CommsConnType.COMMS_CONN_MQTT, true );
        TimerReceiver.restartBackoff( mContext );
    }

    private void subscribe()
    {
        final int qos = XWPrefs.getPrefsInt( mContext, R.string.key_mqtt_qos, 2 );
        setState( State.SUBSCRIBING );
        try {
            mClient.subscribe( mTopic, qos, null, this );
            // Log.d( TAG, "subscribed to %s", mTopic );
        } catch ( MqttException ex ) {
            ex.printStackTrace();
        } catch ( Exception ex ) {
            ex.printStackTrace();
            clearInstance();
        }
    }

    @Override
    public void onSuccess( IMqttToken asyncActionToken )
    {
        Log.d( TAG, "%H.onSuccess(%s); cur state: %s", asyncActionToken, this, mState );
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
        Log.d( TAG, "%H.onFailure(%s, %s); cur state: %s", this, asyncActionToken,
               exception, mState );
        ConnStatusHandler
            .updateStatus( mContext, null, CommsConnType.COMMS_CONN_MQTT, false );
    }

    private class MsgThread extends Thread {
        private LinkedBlockingQueue<byte[]> mQueue = new LinkedBlockingQueue<>();

        void add( byte[] msg ) {
            mQueue.add( msg );
        }

        @Override
        public void run()
        {
            long startTime = Utils.getCurSeconds();
            Log.d( TAG, "%H.MsgThread.run() starting", MQTTUtils.this );
            for ( ; ; ) {
                try {
                    byte[] packet = mQueue.take();
                    XwJNI.dvc_parseMQTTPacket( packet );
                } catch ( InterruptedException ie ) {
                    // Assert.failDbg();
                    break;
                }
            }
            long now = Utils.getCurSeconds();
            Log.d( TAG, "%H.MsgThread.run() exiting after %d seconds", MQTTUtils.this,
                   now - startTime );
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
        if ( BuildConfig.DO_MQTT_GAME_GONE ) {
            String player = XwJNI.kplr_nameForMqttDev( from.mqtt_devID );
            ConnExpl expl = null == player ? null
                : new ConnExpl( CommsConnType.COMMS_CONN_MQTT, player );
            new MQTTServiceHelper( context, from )
                .postEvent( MultiService.MultiEvent.MESSAGE_NOGAME, gameID,
                            expl );
        } else {
            Log.d( TAG, "not posting game-gone for now (gameID: %d)" , gameID );
        }
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
