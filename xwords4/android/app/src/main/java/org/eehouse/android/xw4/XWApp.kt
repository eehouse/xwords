/*
 * Copyright 2010 - 2011 by Eric House (xwords@eehouse.org).  All
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
package org.eehouse.android.xw4

import android.app.Application
import android.content.Context
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ProcessLifecycleOwner

import org.eehouse.android.xw4.jni.Device
import org.eehouse.android.xw4.jni.GameMgr

import java.util.UUID

class XWApp : Application() {
    override fun onCreate() {
        sContext = this
        Assert.assertTrue(sContext === sContext!!.getApplicationContext())
        super.onCreate()
        Log.init(this)
        android.util.Log.i(TAG, "onCreate(); git_rev=${BuildConfig.GIT_REV}")
        Log.enable(this)
        OnBootReceiver.startTimers(this)
        Variants.checkUpdate(this)
        var mustCheck = Utils.firstBootThisVersion(this)
        PrefsDelegate.resetPrefs(this, mustCheck)
        if (mustCheck) {
            XWPrefs.setHaveCheckedUpgrades(this, false)
        } else {
            mustCheck = !XWPrefs.getHaveCheckedUpgrades(this)
        }
        if (mustCheck) {
            UpdateCheckReceiver.checkVersions(this, false)
        }
        UpdateCheckReceiver.restartTimer(this)
        WiDirWrapper.init(this)
        DupeModeTimer.init()
        MQTTUtils.init(this)
        // Eventually only one of these will remain
        BTUtils.init(this, appName)
        BleNetwork.init(this)

        ProcessLifecycleOwner.get().lifecycle
            .addObserver(MyLifecycleListener(this))
    }

    private class MyLifecycleListener(private val context: Context)
        : DefaultLifecycleObserver {
        override fun onCreate(owner: LifecycleOwner) {
            Log.d(TAG, "git_rev=${BuildConfig.GIT_REV}")
        }

        override fun onResume(owner: LifecycleOwner) {
            MQTTUtils.onResume(context)
            BTUtils.onResume(context)
            GameMgr.resendAll()
        }

        override fun onStart(owner: LifecycleOwner) {
            sInForeground = true
            Log.d(TAG, "onStart(): sInForeground now $sInForeground")
        }

        override fun onStop(owner: LifecycleOwner) {
            sInForeground = false
            Log.d(TAG, "onStop(): sInForeground now $sInForeground")
            BTUtils.onStop(context)
            // MQTTUtils.onStop(context) <- not yet
        }

        override fun onDestroy(owner: LifecycleOwner) {
            MQTTUtils.onDestroy(context)
        }
    }

    // This is called on emulator only, but good for ensuring no memory leaks
    // by forcing JNI cleanup
    override fun onTerminate() {
        Log.d(TAG, "onTerminate() called")
        Device.cleanGlobalsEmu()
        super.onTerminate()
    }

    companion object {
        private val TAG = XWApp::class.java.getSimpleName()
        const val DEBUG_EXP_TIMERS = false
        const val CONTEXT_MENUS_ENABLED = true
        const val OFFER_DUALPANE = false
        const val SMS_PUBLIC_HEADER = "-XW4"
        const val MIN_TRAY_TILES = 7 // comtypes.h
        const val GREEN = -0xff5100
        const val RED = -0x510000
        private var sContext: Context? = null
        var sInForeground: Boolean = false
            private set

        val appName: String
            get() {
                return getContext().getString(R.string.app_name)
            }

        fun getContext(): Context
		{
            return sContext!!
        }
    } // companion object
}
