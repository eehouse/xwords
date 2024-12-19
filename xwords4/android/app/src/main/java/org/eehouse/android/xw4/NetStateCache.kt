/*
 * Copyright 2010 - 2022 by Eric House (xwords@eehouse.org).  All rights
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
package org.eehouse.android.xw4

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.ConnectivityManager
import android.net.NetworkInfo
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.Parcelable

import java.util.concurrent.atomic.AtomicBoolean

import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType

object NetStateCache {
    private val TAG: String = NetStateCache::class.java.simpleName
    private const val WAIT_STABLE_MILLIS = (2 * 1000).toLong()

    private val s_ifs: MutableSet<StateChangedIf> = HashSet()

    private val s_haveReceiver = AtomicBoolean(false)
    private var s_netAvail = false
    private var s_isWifi = false
    private var s_receiver: PvtBroadcastReceiver? = null
    private val s_onSDKSim = Build.PRODUCT.contains("sdk") // not genymotion

    fun register(context: Context, proc: StateChangedIf) {
        DbgUtils.assertOnUIThread()
        if (Utils.isOnUIThread()) {
            initIfNot(context)
            synchronized(s_ifs) {
                s_ifs.add(proc)
            }
        }
    }

    fun unregister(proc: StateChangedIf) {
        DbgUtils.assertOnUIThread()
        if (Utils.isOnUIThread()) {
            synchronized(s_ifs) {
                s_ifs.remove(proc)
            }
        }
    }

    var s_lastNetCheck: Long = 0
    fun netAvail(context: Context): Boolean {
        initIfNot(context)

        // Cache is returning false negatives. Don't trust it.
        if (!s_netAvail) {
            val now = System.currentTimeMillis()
            if (now < s_lastNetCheck) { // time moving backwards?
                s_lastNetCheck = 0 // reset
            }
            if (now - s_lastNetCheck > (1000 * 20)) { // 20 seconds
                s_lastNetCheck = now

                val netAvail = getIsConnected(context)
                if (netAvail) {
                    Log.i(TAG, "netAvail(): second-guessing successful!!!")
                    s_netAvail = true
                    if (null != s_receiver) {
                        s_receiver!!.notifyStateChanged(context)
                    }
                }
            }
        }

        val result = s_netAvail || s_onSDKSim
        // Log.d( TAG, "netAvail() => %b", result );
        return result
    }

    fun onWifi(): Boolean {
        return s_isWifi
    }

    fun reset(context: Context) {
        synchronized(s_haveReceiver) {
            s_haveReceiver.set(false)
            if (null != s_receiver) {
                context.applicationContext.unregisterReceiver(s_receiver)
                s_receiver = null
            }
        }
    }

    private fun getIsConnected(context: Context): Boolean {
        var result = false
        val ni = (context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager)
            .activeNetworkInfo
        if (null != ni && ni.isConnectedOrConnecting) {
            result = true
        }
        Log.i(TAG, "NetStateCache.getConnected() => %b", result)
        return result
    }

    private fun initIfNot(context: Context) {
        synchronized(s_haveReceiver) {
            if (!s_haveReceiver.get()) {
                // First figure out the current net state.  Note that
                // this doesn't seem to work on the emulator.

                val connMgr =
                    context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
                val ni = connMgr.activeNetworkInfo

                s_netAvail = ni != null && ni.isAvailable && ni.isConnected

                // Log.d( TAG, "set s_netAvail = %b", s_netAvail );
                s_receiver = PvtBroadcastReceiver()
                val filter = IntentFilter()
                filter.addAction(ConnectivityManager.CONNECTIVITY_ACTION)

                context.applicationContext
                    .registerReceiver(s_receiver, filter)

                // s_ifs = new HashSet<>();
                s_haveReceiver.set(true)
            }
        }
    }

    private fun checkSame(context: Context, connectedCached: Boolean) {
        if (BuildConfig.DEBUG) {
            val cm =
                context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
            val activeNetwork = cm.activeNetworkInfo
            val connectedReal = activeNetwork != null &&
                    activeNetwork.isConnectedOrConnecting
            if (connectedReal != connectedCached) {
                Log.w(
                    TAG, "connected: cached: %b; actual: %b",
                    connectedCached, connectedReal
                )
            }
        }
    }

    // I'm leaving this stuff commented out because MQTT might want to use it
    // to try resending when the net comes back.
    interface StateChangedIf {
        fun onNetAvail(context: Context, nowAvailable: Boolean)
    }

    private class PvtBroadcastReceiver : BroadcastReceiver() {
        private var mNotifyLater: Runnable? = null
        private var mHandler: Handler? = null
        private var mLastStateSent: Boolean

        init {
            mLastStateSent = s_netAvail
        }

        override fun onReceive(context: Context, intent: Intent) {
            DbgUtils.assertOnUIThread()

            if (null == mHandler) {
                mHandler = Handler(Looper.getMainLooper())
            }

            if (intent.action == ConnectivityManager.CONNECTIVITY_ACTION) {
                val ni: NetworkInfo = intent
                    .getParcelableExtra<Parcelable>(ConnectivityManager.EXTRA_NETWORK_INFO)
                        as NetworkInfo
                val state = ni.getState()
                Log.d(TAG, "onReceive(state=%s)", state.toString())

                val netAvail: Boolean
                when (state) {
                    NetworkInfo.State.CONNECTED -> {
                        netAvail = true
                        s_isWifi = ConnectivityManager.TYPE_WIFI == ni.type
                    }

                    NetworkInfo.State.DISCONNECTED -> netAvail = false
                    else ->                     // ignore everything else
                        netAvail = s_netAvail
                }
                if (s_netAvail != netAvail) {
                    s_netAvail = netAvail // keep current in case we're asked
                    notifyStateChanged(context)
                } else {
                    Log.d(
                        TAG, "onReceive: no change; doing nothing;"
                                + " s_netAvail=%b", s_netAvail
                    )
                }
            }
        }

        fun notifyStateChanged(context: Context) {
            // We want to wait for WAIT_STABLE_MILLIS of inactivity
            // before informing listeners.  So each time there's a
            // change, kill any existing timer then set another, which
            // will only fire if we go that long without coming
            // through here again.

            if (null == mHandler) {
                Log.e(TAG, "notifyStateChanged(): handler null so dropping")
            } else {
                if (null != mNotifyLater) {
                    mHandler!!.removeCallbacks(mNotifyLater!!)
                    mNotifyLater = null
                }
                if (mLastStateSent != s_netAvail) {
                    mNotifyLater = Runnable {
                        if (mLastStateSent != s_netAvail) {
                            mLastStateSent = s_netAvail

                            Log.i(TAG, "notifyStateChanged(%b)", s_netAvail)

                            synchronized(s_ifs) {
                                s_ifs.map{it.onNetAvail(context, s_netAvail)}
                            }

                            if (s_netAvail) {
                                val typ = CommsConnType.COMMS_CONN_RELAY
                                GameUtils.resendAllIf(context, typ)
                            }
                        }
                    }
                    mHandler!!.postDelayed(mNotifyLater!!, WAIT_STABLE_MILLIS)
                }
            }
        }
    } // class PvtBroadcastReceiver
}
