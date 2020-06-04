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

import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

public class MQTTUtils extends Thread implements IMqttActionListener, MqttCallbackExtended {
    private static final String TAG = MQTTUtils.class.getSimpleName();
    private static final String KEY_NEXT_REG = TAG + "/next_reg";

    private static AtomicReference<MQTTUtils> sInstance = new AtomicReference<>();
    private static long sNextReg = 0;

    private MqttAsyncClient mClient;
    private long mPauseTime = 0L;
    private String mDevID;
    private String mTopic;
    private Context mContext;
    private MsgThread mMsgThread;
    private LinkedBlockingQueue<MessagePair> mOutboundQueue = new LinkedBlockingQueue<>();
    private boolean mShouldExit = false;

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
        // If we have an instance now, it's not working, so kill it. And start
        // another
        MQTTUtils instance = sInstance.get();
        if ( null != instance ) {
            clearInstance( instance );
        }
        getOrStart( context );
        Log.d( TAG, "onFCMReceived() DONE" );
    }

    static void onConfigChanged( Context context )
    {
        MQTTUtils instance = sInstance.get();
        if ( null != instance ) {
            clearInstance( instance );
        }
    }

    private static MQTTUtils getOrStart( Context context )
    {
        MQTTUtils result = null;
        if ( BuildConfig.OFFER_MQTT ) {
            result = sInstance.get();
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
        setup();
        while ( !mShouldExit ) {
            try {
                // this thread can be fed before the connection is
                // established. Wait for that before removing packets from the
                // queue.
                if ( !mClient.isConnected() ) {
                    Log.d( TAG, "not connected; sleeping..." );
                    Thread.sleep(500);
                    continue;
                }
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
    }

    private void enqueue( String topic, byte[] packet )
    {
        mOutboundQueue.add( new MessagePair( topic, packet ) );
    }

    private static void setInstance( MQTTUtils instance )
    {
        MQTTUtils oldInstance = sInstance.getAndSet(instance);
        if ( null != oldInstance ) {
            oldInstance.disconnect();
        }
    }

    private static void clearInstance( MQTTUtils curInstance )
    {
        MQTTUtils oldInstance = sInstance.getAndSet(null);
        if ( curInstance == oldInstance ) {
            oldInstance.disconnect();
        } else {
            Log.e( TAG, "unreachable instance still running???" );
        }
        // if ( sResumed ) {
        //     Log.d( TAG, "clearInstance(); looks like I could start another!!" );
        // }
    }

    public static void onPause()
    {
        // Log.d( TAG, "onPause()" );
        // MQTTUtils instance = sInstance.get();
        // if ( null != instance ) {
        //     instance.setPaused(true);
        // }
        // DbgUtils.assertOnUIThread();
        // // sResumed = false;
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

    private void setup()
    {
        Log.d( TAG, "setup()" );
        MqttConnectOptions mqttConnectOptions = new MqttConnectOptions();
        mqttConnectOptions.setAutomaticReconnect(true);
        mqttConnectOptions.setCleanSession(false);
        final int qos = XWPrefs.getPrefsInt( mContext, R.string.key_mqtt_qos, 2 );

        try {
            mClient.connect( mqttConnectOptions, null, new IMqttActionListener() {
                    @Override
                    public void onSuccess( IMqttToken asyncActionToken ) {
                        Log.d( TAG, "onSuccess()" );
                        try {
                            mClient.subscribe( mTopic, qos, null, MQTTUtils.this );
                            Log.d( TAG, "subscribed to %s", mTopic );
                            mMsgThread.start();
                        } catch ( MqttException ex ) {
                            ex.printStackTrace();
                        } catch ( Exception ex ) {
                            ex.printStackTrace();
                            clearInstance();
                        }
                    }
                    @Override
                    public void onFailure(IMqttToken asyncActionToken, Throwable exception) {
                        Log.d( TAG, "onFailure(%s, %s)", asyncActionToken, exception );
                        ConnStatusHandler.updateStatus( mContext, null,
                                                        CommsConnType.COMMS_CONN_MQTT,
                                                        false );
                        clearInstance();
                    }
                } );
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
        }
        long now = Utils.getCurSeconds();
        Log.d( TAG, "registerOnce(): now: %d; nextReg: %d", now, sNextReg );
        if ( now > sNextReg ) {
            try {
                JSONObject params = new JSONObject();
                params.put( "devid", mDevID );
                params.put( "gitrev", BuildConfig.GIT_REV );
                params.put( "os", Build.MODEL );
                params.put( "vers", Build.VERSION.RELEASE );
                params.put( "vrntCode", BuildConfig.VARIANT_CODE );
                params.put( "vrntName", BuildConfig.VARIANT_NAME );
                params.put( "myNow", now );

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

    // private void setPaused( boolean paused )
    // {
    //     if ( paused ) {
    //         if ( 0 == mPauseTime ) {
    //             mPauseTime = System.currentTimeMillis();
    //             Log.d( TAG, "setPaused() called for first time" );
    //         }
    //     } else {
    //         long diff = System.currentTimeMillis() - mPauseTime;
    //         Log.d( TAG, "unpausing after %d seconds", diff/1000);
    //         mPauseTime = 0;
    //     }
    // }

    private void disconnect()
    {
        if ( 0 == mPauseTime ) {
            Log.d( TAG, "disconnect()" );
        } else {
            long diff = System.currentTimeMillis() - mPauseTime;
            Log.d( TAG, "disconnect() called %d seconds after app paused", diff/1000 );
        }
        try {
            mShouldExit = true;
            mClient.unsubscribe( mDevID );
            mClient.disconnect();
            Log.d( TAG, "disconnect() succeeded" );
            mMsgThread.interrupt();
        } catch (MqttException ex){
            ex.printStackTrace();
        } catch (Exception ex){
            ex.printStackTrace();
            clearInstance();
        }
    }

    private void clearInstance() { clearInstance( this ); }

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

    public static void gameDied( String devID, int gameID )
    {
        Log.e( TAG, "gameDied() not handled" );
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
        Log.d( TAG, "messageArrived(topic=%s, message=%s)", topic, message );
        Assert.assertTrueNR( topic.equals(mTopic) );
        mMsgThread.add( message.getPayload() );
        ConnStatusHandler
            .updateStatusIn( mContext, CommsConnType.COMMS_CONN_MQTT, true );
    }

    @Override
    public void deliveryComplete(IMqttDeliveryToken token)
    {
        Log.d( TAG, "deliveryComplete(token=%s)", token );
        ConnStatusHandler
            .updateStatusOut( mContext, CommsConnType.COMMS_CONN_MQTT, true );
    }

    @Override
    public void onSuccess( IMqttToken asyncActionToken )
    {
        Log.d( TAG, "onSuccess(%s)", asyncActionToken );
    }

    @Override
    public void onFailure(IMqttToken asyncActionToken, Throwable exception)
    {
        Log.d( TAG, "onFailure(%s, %s)", asyncActionToken, exception );
    }

    private class MsgThread extends Thread {
        private LinkedBlockingQueue<byte[]> mQueue = new LinkedBlockingQueue<>();
        private long mStartTime = Utils.getCurSeconds();

        void add( byte[] msg ) {
            mQueue.add( msg );
        }

        @Override
        public void run()
        {
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
            Log.d( TAG, "%H.run() exiting after %d seconds", this, now - mStartTime );
        }
    }

    public static void handleMessage( Context context, CommsAddrRec from,
                                      int gameID, byte[] data )
    {
        Log.d( TAG, "handleMessage(gameID=%d): got message", gameID );
        MQTTServiceHelper helper = new MQTTServiceHelper( context, from );
        long[] rowids = DBUtils.getRowIDsFor( context, gameID );
        Log.d( TAG, "got %d rows for gameID %d", rowids == null ? 0 : rowids.length, gameID );
        if ( 0 == rowids.length ) {
            notifyNotHere( context, from.mqtt_devID, gameID );
        } else {
            for ( long rowid : rowids ) {
                MQTTMsgSink sink = new MQTTMsgSink( context, rowid );
                helper.receiveMessage( rowid, sink, data );
            }
        }
    }

    public static void handleGameGone( Context context, CommsAddrRec from, int gameID )
    {
        new MQTTServiceHelper( context, from )
            .postEvent( MultiService.MultiEvent.MESSAGE_NOGAME, gameID );
    }

    public static void fcmConfirmed( Context context, boolean working )
    {
        if ( working ) {
            DBUtils.setLongFor( context, KEY_NEXT_REG, 0 );
        }
    }

    private static class MQTTServiceHelper extends XWServiceHelper {
        private CommsAddrRec mReturnAddr;
        private Context mContext;

        MQTTServiceHelper( Context context, CommsAddrRec from )
        {
            super( context );
            mContext = context;
            mReturnAddr = from;
        }

        @Override
        protected MultiMsgSink getSink( long rowid )
        {
            Context context = getContext();
            return new MQTTMsgSink( context, rowid );
        }

        @Override
        void postNotification( String device, int gameID, long rowid )
        {
            Assert.failDbg();
            // Context context = getContext();
            // String body = LocUtils.getString( mContext, R.string.new_relay_body );
            // GameUtils.postInvitedNotification( mContext, gameID, body, rowid );
        }

        private void receiveMessage( long rowid, MQTTMsgSink sink, byte[] msg )
        {
            Log.d( TAG, "receiveMessage(rowid=%d, len=%d)", rowid, msg.length );
            receiveMessage( rowid, sink, msg, mReturnAddr );
        }
    }

    private static class MQTTMsgSink extends MultiMsgSink {
        MQTTMsgSink( Context context, long rowid )
        {
            super( context, rowid );
        }
    }

}
