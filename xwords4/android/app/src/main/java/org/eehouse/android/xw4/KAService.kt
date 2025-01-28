/*
 * Copyright 2024 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.Notification
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

import java.util.Date

import org.eehouse.android.xw4.loc.LocUtils

// From: https://robertohuertas.com/2019/06/29/android_foreground_services/

private val TAG = KAService::class.java.simpleName

class KAService: Service() {
    private var mWakeLock: PowerManager.WakeLock? = null
    private var mServiceStarted = false
    private var mSelfKilled = false

    override fun onBind(intent: Intent): IBinder? {
        return null
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        intent?.let {
            val action = intent.action
            Log.d(TAG, "%H.onStartCommand(): action: $action", this)
            when (action) {
                START_CMD -> startService()
                STOP_CMD -> stopService()
                else -> Assert.failDbg()
            }
        }
        return START_STICKY
    }

    override fun onCreate() {
        super.onCreate()
        val versionCode = Build.VERSION.SDK_INT
        Log.d(TAG, "%H.onCreate(code=$versionCode)", this)
        val notify = createNotification(this)
        val id = R.string.kaservice_title
        if (versionCode < Build.VERSION_CODES.Q) {
            startForeground(id, notify)
        } else {
            startForeground(id, notify, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC)
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        Log.d(TAG, "%H.onDestroy()", this)
        mServiceStarted = false
        sIsRunning = false

        if ( !mSelfKilled ) {
            postKilled()
        }
    }

    private fun postKilled()
    {
        val date = Date()
        val body = LocUtils.getString(this, R.string.ksconfig_killed_body_fmt, date)
        GamesListDelegate.makeSelfIntent(this).let {
            Utils.postNotification(this, it, R.string.ksconfig_killed_title,
                                   body, 1000)
        }
    }

    private fun startService() {
        if (!mServiceStarted && getEnabled(this)) {
            mServiceStarted = true
            sIsRunning = true

            // avoid hit from Doze Mode
            mWakeLock =
                (getSystemService(Context.POWER_SERVICE) as PowerManager).run {
                    newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "KAService::lock").apply {
                        acquire()
                    }
                }

            GlobalScope.launch(Dispatchers.IO) {
                val context = this@KAService
                val minsLeft = DBUtils.getKAMinutesLeft(context)
                Log.d(TAG, "will run $minsLeft minutes")
                delay(minsLeft * 60 * 1000)
                Log.d(TAG, "ran $minsLeft minutes; stopping now")
                stopService()
                startIf(context)
            }

            GlobalScope.launch(Dispatchers.IO) {
                Log.d(TAG, "%H.service loop starting", this@KAService)
                val startTime = System.currentTimeMillis()
                var counter = 0
                while (mServiceStarted) {
                    val curTime = System.currentTimeMillis()
                    Log.d(TAG, "%H: pass #$counter in loop; ${(curTime - startTime) / 1000} seconds now",
                          this@KAService)
                    delay(1 * 60 * 1000)
                    ++counter
                }
                Log.d(TAG, "%H.service loop exiting", this@KAService)
            }
        }
    }

    private fun stopService() {
        Log.d(TAG, "stopService()")
        try {
            mWakeLock?.let {
                if (it.isHeld) {
                    it.release()
                }
            }
            stopSelf()
        } catch (ex: Exception) {
            Log.ex(TAG, ex)
        }
        mServiceStarted = false
        mSelfKilled = true
    }

    private fun createNotification(context: Context): Notification {
        val channelID = Channels.ID.KEEP_ALIVE

        // depending on the Android API that we're dealing with we will have
        // to use a specific method to create the notification
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            channelID.makeChannel(context).also {
                (getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager)
                    .createNotificationChannel(it)
            }
        }

        val builder =
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                Notification.Builder(context, channelID.toString())
            } else {
                Notification.Builder(context)
            }
        return builder
            .setContentTitle(context.getString(R.string.kaservice_title))
            .setContentText(context.getString(R.string.kaservice_content))
            .setContentIntent(KAConfigView.makePendingIntent(this))
            .setSmallIcon(R.drawable.notify)
            .build()
    }

    companion object {
        private val START_CMD = "start"
        private val STOP_CMD = "stop"

        fun startIf(context: Context) {
            if (!sIsRunning
                    && getEnabled(context)
                    && 0L < DBUtils.getKAMinutesLeft(context)) {
                startWith(context, START_CMD)
            }
        }

        fun stop(context: Context)
            = startWith(context, STOP_CMD)

        fun syncUp(context:Context) {
            getEnabled(context).also { enabled ->
                if (enabled) startIf(context)
                else stop(context)
            }
        }

        private fun startWith(context: Context, cmd: String)
        {
            Intent(context, KAService::class.java).also {
                it.action = cmd
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    context.startForegroundService(it)
                } else {
                    context.startService(it)
                }
            }
        }

        private var sIsRunning = false
        fun isRunning(): Boolean
        {
            return sIsRunning
        }

        fun getEnabled(context: Context): Boolean
        {
            return XWPrefs.getPrefsBoolean(context, R.string.key_enable_kaservice, false)
        }

        fun setEnabled(context: Context, enabled: Boolean)
        {
            XWPrefs.setPrefsBoolean(context, R.string.key_enable_kaservice, enabled)
        }
    }
}
