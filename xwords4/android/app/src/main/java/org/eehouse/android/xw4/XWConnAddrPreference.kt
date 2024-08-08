/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2024 by Eric House (xwords@eehouse.org).  All
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

import android.app.Dialog
import android.content.Context
import android.content.DialogInterface
import android.os.Bundle
import android.util.AttributeSet
import android.view.View
import androidx.preference.DialogPreference

import org.eehouse.android.xw4.PrefsActivity.DialogProc
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.loc.LocUtils

private val TAG = XWConnAddrPreference::class.java.getSimpleName()

class XWConnAddrPreference(private val m_context: Context, attrs: AttributeSet?) :
    DialogPreference(m_context, attrs), DialogProc
{
    init {
        val curSet = XWPrefs.getAddrTypes(context)
        summary = curSet.toString(context, true)
    }

    override fun makeDialogFrag(): XWDialogFragment {
        return XWConnAddrDialogFrag(this)
    }

    class XWConnAddrDialogFrag(private val mSelf: XWConnAddrPreference) :
        XWDialogFragment()
    {
        override fun onCreateDialog(sis: Bundle?): Dialog {
            val activity = context as PrefsActivity?
            val view = LocUtils.inflate(activity!!, R.layout.conn_types_display)

            val cvl = view.findViewById<View>(R.id.conn_types) as ConnViaViewLayout
            val cew = object : ConnViaViewLayout.CheckEnabledWarner {
                override fun warnDisabled(typ: CommsConnType) {
                    var msg: String? = null
                    var msgID = 0
                    var action: DlgDelegate.Action? = null
                    var buttonID = 0
                    when (typ) {
                        CommsConnType.COMMS_CONN_SMS -> {
                            msgID = R.string.warn_sms_disabled
                            action = DlgDelegate.Action.ENABLE_NBS_ASK
                            buttonID = R.string.button_enable_sms
                        }

                        CommsConnType.COMMS_CONN_BT -> {
                            msgID = R.string.warn_bt_disabled
                            action = DlgDelegate.Action.ENABLE_BT_DO
                            buttonID = R.string.button_enable_bt
                        }

                        CommsConnType.COMMS_CONN_MQTT -> {
                            msg = LocUtils.getString(
                                activity, R.string.warn_mqtt_disabled
                            )
                            msg += "\n\n" + LocUtils.getString(
                                activity,
                                R.string.warn_mqtt_later
                            )
                            action = DlgDelegate.Action.ENABLE_MQTT_DO
                            buttonID = R.string.button_enable_mqtt
                        }

                        else -> Assert.failDbg()
                    }
                    if (0 != msgID) {
                        Assert.assertTrueNR(null == msg)
                        msg = LocUtils.getString(activity, msgID)
                    }
                    if (null != msg) {
                        activity.makeConfirmThenBuilder(action!!, msg)
                            .setPosButton(buttonID)
                            .setNegButton(R.string.button_later)
                            .show()
                    }
                }
            }
            val saw = object: ConnViaViewLayout.SetEmptyWarner {
                override fun typeSetEmpty() {
                    activity
                        .makeOkOnlyBuilder(R.string.warn_no_comms)
                        .show()
                }
            }
            cvl.configure(activity.getDelegate(), XWPrefs.getAddrTypes(activity),
                          cew, saw, activity)

            val onOk =
                DialogInterface.OnClickListener { di, which ->
                    val curSet = cvl.types!!
                    XWPrefs.setAddrTypes(activity, curSet)
                    mSelf.summary = curSet.toString(activity, true)
                }

            return LocUtils.makeAlertBuilder(activity)
                .setTitle(R.string.title_addrs_pref)
                .setView(view)
                .setPositiveButton(android.R.string.ok, onOk)
                .setNegativeButton(android.R.string.cancel, null)
                .create()
        }

        override fun getFragTag(): String {
            return javaClass.simpleName
        }
    }
}
