/*
 * Copyright 2009 - 2024 by Eric House (xwords@eehouse.org).  All
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

import android.content.Context
import android.util.AttributeSet
import java.lang.ref.WeakReference

import org.eehouse.android.xw4.DlgDelegate.Action
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.loc.LocUtils

class BTCheckBoxPreference(context: Context, attrs: AttributeSet?) :
    ConfirmingCheckBoxPreference(context, attrs)
{
    init {
        s_this = WeakReference(this)
    }

    override fun checkIfConfirmed() {
        val activity = context as PrefsActivity
        var msg = LocUtils.getString(
            activity,
            R.string.warn_bt_havegames
        )

        val count = DBUtils
            .getGameCountUsing(activity, CommsConnType.COMMS_CONN_BT)
        if (0 < count) {
            msg += LocUtils.getQuantityString(
                activity, R.plurals.warn_bt_games_fmt,
                count, count
            )
        }
        activity.makeConfirmThenBuilder(Action.DISABLE_BT_DO, msg)
            .setPosButton(R.string.button_disable_bt)
            .show()
    }

    companion object {
        private val TAG: String = BTCheckBoxPreference::class.java.simpleName
        private var s_this: WeakReference<BTCheckBoxPreference>? = null

        fun setChecked() {
            if (null != s_this) {
                val self = s_this!!.get()
                self?.super_setChecked(true)
            }
        }
    }
}
