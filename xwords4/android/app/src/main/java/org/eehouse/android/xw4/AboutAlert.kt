/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */ /*
 * Copyright 2017 - 2020 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.Dialog
import android.content.Context
import android.os.Bundle
import android.view.View
import android.widget.TextView

import java.text.DateFormat
import java.util.Date

import org.eehouse.android.xw4.jni.XwJNI
import org.eehouse.android.xw4.loc.LocUtils

public class AboutAlert : XWDialogFragment() {
    override fun onCreateDialog(sis: Bundle?): Dialog {
        val context: Context = activity!!
        val view = LocUtils.inflate(context, R.layout.about_dlg)
        val df = DateFormat.getDateTimeInstance(
            DateFormat.DEFAULT,
            DateFormat.DEFAULT
        )
        val dateString = df.format(Date(BuildConfig.BUILD_STAMP * 1000))
        var sb = StringBuilder(
            getString(
                R.string.about_vers_fmt2,
                BuildConfig.VARIANT_NAME,
                BuildConfig.VERSION_NAME,
                BuildConfig.VERSION_CODE,
                BuildConfig.GITREV_SHORT,
                dateString
            )
        )
        if (BuildConfig.NON_RELEASE) {
            sb.append("\n\t").append(BuildConfig.GIT_REV)
        }
        (view.findViewById<View>(R.id.version_string) as TextView).text = sb.toString()
        val xlator = view.findViewById<View>(R.id.about_xlator) as TextView
        val str = getString(R.string.xlator)
        if (str.length > 0 && str != "[empty]") {
            xlator.text = str
        } else {
            xlator.visibility = View.GONE
        }
        sb = StringBuilder(
            getString(
                R.string.about_devid_fmt,
                XwJNI.dvc_getMQTTDevID()
            )
        )
        if (BuildConfig.NON_RELEASE) {
            val pair = BTUtils.getBTNameAndAddress(context)
            if (null != pair && 2 >= pair.size && null != pair[1]) {
                sb.append("\n\n").append(getString(R.string.about_btaddr_fmt, pair[1]))
            }
            val am = context!!.assets
            try {
                val `is` = am.open(BuildConfig.LAST_COMMIT_FILE)
                val tmp = ByteArray(2 * 1024)
                val nRead = `is`.read(tmp, 0, tmp.size)
                sb.append("\n\n").append(String(tmp, 0, nRead))
            } catch (ex: Exception) {
                Log.ex(TAG, ex)
            }
        }
        (view.findViewById<View>(R.id.about_build) as TextView).text = sb.toString()
        val builder = LocUtils.makeAlertBuilder(context)
            .setIcon(R.drawable.icon48x48)
            .setTitle(R.string.app_name)
            .setView(view)
            .setPositiveButton(android.R.string.ok, null)
        if (context is XWActivity) {
            builder.setNegativeButton(R.string.changes_button) { dlg, which ->
                context.show(
                    FirstRunDialog.newInstance()
                )
            }
        }
        return builder.create()
    }

    override fun getFragTag(): String {
        return TAG
    }

    companion object {
        private val TAG = AboutAlert::class.java.getSimpleName()

        @JvmStatic
        fun newInstance(): AboutAlert {
            return AboutAlert()
        }
    }
}
