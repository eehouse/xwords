/*
 * Copyright 2025 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.provider.Settings
import android.util.AttributeSet
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleCoroutineScope
import androidx.lifecycle.ViewTreeLifecycleOwner
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

import org.eehouse.android.xw4.gen.PrefsWrappers
import org.eehouse.android.xw4.loc.LocUtils

class KAConfigView(private val mContext: Context, aset: AttributeSet?):
    ScrollView(mContext, aset) {
    private var mIsRunning: Boolean = false
    private var mScope: LifecycleCoroutineScope? = null

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        mScope = ViewTreeLifecycleOwner.get(this)?.lifecycleScope

        mScope?.launch(Dispatchers.Main) {
            while (true) {
                update()
                delay(60 * 1000)    // every minute so minutes left can change
            }
        }
    }

    override fun onWindowFocusChanged(hasWindowFocus: Boolean)
    {
        super.onWindowFocusChanged(hasWindowFocus)
        if ( hasWindowFocus ) {
            update()   // in case we came back from changing them
        }
    }

    private fun update()
    {
        mIsRunning = KAService.isRunning()

        val hours = XWPrefs.getKAServiceHours(context)
        val settingsTxt = LocUtils.getString(context, R.string.button_settings)
        LocUtils.getString(context, R.string.ka_config_fmt, hours, settingsTxt )
            .also {
                (findViewById(R.id.config_expl) as TextView)
                    .setText(it)
            }

        (findViewById(R.id.settings) as Button)
            .setOnClickListener {
                PrefsDelegate
                    .launch(
                        context,
                        PrefsWrappers.prefs_net_kaservice::class.java
                    )
            }

        KAService.getEnabled(context).let { enabled ->
            val msg =
                if ( !enabled ) {
                    LocUtils.getString(context, R.string.ksconfig_disabled)
                } else if (mIsRunning) {
                    DBUtils.getKAMinutesLeft(context).let {
                        val minutes = it % 60
                        val hours = it / 60
                        LocUtils.getString(context, R.string.ksconfig_running_fmt,
                                           hours, minutes)
                    }
                } else {
                    LocUtils.getString(context, R.string.ksconfig_notrunning)
                }
            findViewById<TextView>(R.id.start_stop_expl).setText(msg)

            findViewById<View>(R.id.notify_group).setVisibility(
                if ( enabled ) View.VISIBLE
                else View.GONE
            )
            if ( enabled ) {
                findViewById<Button>(R.id.hide_notify_button).also {
                    it.setOnClickListener { gotoSettings() }
                }
            }
        }
    }

    // from: https://www.spiria.com/en/blog/mobile-development/hiding-foreground-services-notifications-in-android/
    private fun gotoSettings()
    {
        val channel = Channels.ID.KEEP_ALIVE.toString()
        val intent = Intent(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS)
            .putExtra(Settings.EXTRA_APP_PACKAGE, mContext.getPackageName())
            .putExtra(Settings.EXTRA_CHANNEL_ID, channel)
            .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        mContext.startActivity(intent)
    }

    private fun updateAfter()
    {
        mScope!!.launch(Dispatchers.Main) {
            delay(250)
            update()
        }
    }

    companion object {
        private val TAG = KAConfigView::class.java.getSimpleName()
        private val FOR_KACONFIG = BuildConfig.APPLICATION_ID + ".FOR_KACONFIG"

        fun makePendingIntent(context: Context): PendingIntent
        {
            val intent = makeIntent(context)

            val flags = PendingIntent.FLAG_IMMUTABLE
            val pi = PendingIntent.getActivity(context, Utils.nextRandomInt(),
                                               intent, flags)
            return pi
        }

        private fun makeIntent(context: Context): Intent {
            val intent = GamesListDelegate.makeSelfIntent(context).also {
                it.putExtra(FOR_KACONFIG, true)
                Assert.assertTrue(isMyIntent(it))
            }
            return intent
        }

        fun isMyIntent(intent: Intent): Boolean
        {
            val result = intent.getBooleanExtra(FOR_KACONFIG, false)
            return result
        }

        fun launch(context: Context)
        {
            makeIntent(context).also {
                it.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                context.startActivity(it)
            }
        }
    }
}
