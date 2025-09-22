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
import android.graphics.Color
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.OnLifecycleEvent
import androidx.lifecycle.ProcessLifecycleOwner
import org.eehouse.android.xw4.jni.Device

import java.util.UUID

class XWApp : Application(), LifecycleObserver {
    override fun onCreate() {
        sContext = this
        Assert.assertTrue(sContext === sContext!!.getApplicationContext())
        super.onCreate()
        Log.init(this)
        ProcessLifecycleOwner.get().lifecycle.addObserver(this)
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
        BTUtils.init(this, appName)
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_ANY)
    fun onAny(source: LifecycleOwner?, event: Lifecycle.Event?) {
        Log.d(TAG, "onAny(%s)", event)
        when (event) {
            Lifecycle.Event.ON_CREATE -> Log.d(TAG, "git_rev=${BuildConfig.GIT_REV}")
            Lifecycle.Event.ON_RESUME -> {
                MQTTUtils.onResume(this)
                BTUtils.onResume(this)
                GameUtils.resendAllIf(this, null)
            }

            Lifecycle.Event.ON_STOP -> BTUtils.onStop(this)
            Lifecycle.Event.ON_DESTROY -> MQTTUtils.onDestroy(this)
            else -> {}
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
        @JvmField
        val SEL_COLOR = Color.argb(0xFF, 0x09, 0x70, 0x93)
        const val GREEN = -0xff5100
        const val RED = -0x510000
        private var sContext: Context? = null
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
