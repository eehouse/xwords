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

import android.app.Activity
import android.app.Dialog
import android.app.PendingIntent
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.os.PowerManager
import android.provider.Settings

import org.eehouse.android.xw4.loc.LocUtils

class KAConfigAlert: XWDialogFragment(), DialogInterface.OnClickListener{
    private var mIsRunning: Boolean = false

    override fun onCreateDialog(sis: Bundle?): Dialog {
        val context = requireContext()
        mIsRunning = KAService.isRunning(context)

        val hours = XWPrefs.getKAServiceHours(context)
        val disableTxt = LocUtils.getString(context, R.string.ksconfig_disable)
        var msg = LocUtils.getString(context, R.string.ksconfig_body_fmt,
                                     hours, disableTxt)
        val buttonTxt =
            if ( mIsRunning ) R.string.ksconfig_button_stop
            else R.string.ksconfig_button_start

        msg += "\n\n" + LocUtils.getString(
            context,
            if (mIsRunning) R.string.ksconfig_running
            else R.string.ksconfig_notrunning
        )

        val builder = LocUtils.makeAlertBuilder(context)
            .setMessage(msg)
            .setNeutralButton(disableTxt) { dlg, item ->
                XWPrefs.setPrefsBoolean(context, R.string.key_enable_kaservice, false)
                KAService.stop(context)
            }
            .setNegativeButton(buttonTxt, this)
            .setPositiveButton(android.R.string.ok, null)
        return builder.create()
    }

    override fun onClick(dialog: DialogInterface?, which: Int) {
        val context = requireContext()
        if ( mIsRunning ) {
            KAService.stop(context, true)
        } else {
            KAService.startIf(context, true)
        }
    }


    override fun getFragTag(): String {
        return TAG
    }

    companion object {
        private val TAG = KAConfigAlert::class.java.getSimpleName()
        private val FOR_KACONFIG = BuildConfig.APPLICATION_ID + ".FOR_KACONFIG"

        fun newInstance(intent: Intent? = null): KAConfigAlert {
            val result = KAConfigAlert()
            return result
        }

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
