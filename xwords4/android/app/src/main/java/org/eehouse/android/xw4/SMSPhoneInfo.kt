/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2024 by Eric House (xwords@eehouse.org).  All rights
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

import android.content.Context
import android.telephony.TelephonyManager

class SMSPhoneInfo(var isPhone: Boolean,
                   @JvmField var number: String?,
                   @JvmField var isGSM: Boolean)
{
    companion object {
        private val TAG: String = SMSPhoneInfo::class.java.simpleName

        private var s_phoneInfo: SMSPhoneInfo? = null
        fun get(context: Context): SMSPhoneInfo? {
            if (null == s_phoneInfo) {
                try {
                    var number: String? = null
                    var isGSM = false
                    var isPhone = false
                    val mgr =
                        context.getSystemService(Context.TELEPHONY_SERVICE) as TelephonyManager
                    if (null != mgr) {
                        number = mgr.line1Number // needs permission
                        val type = mgr.phoneType
                        isGSM = TelephonyManager.PHONE_TYPE_GSM == type
                        isPhone = true
                    }

                    val radio =
                        XWPrefs.getPrefsString(context, R.string.key_force_radio)
                    val ids = intArrayOf(
                        R.string.radio_name_real,
                        R.string.radio_name_tablet,
                        R.string.radio_name_gsm,
                        R.string.radio_name_cdma,
                    )

                    // default so don't crash before set
                    var id = R.string.radio_name_real
                    for (ii in ids.indices) {
                        if (radio == context.getString(ids[ii])) {
                            id = ids[ii]
                            break
                        }
                    }

                    when (id) {
                        R.string.radio_name_real -> {}
                        R.string.radio_name_tablet -> {
                            number = null
                            isPhone = false
                        }

                        R.string.radio_name_gsm,
                        R.string.radio_name_cdma -> {
                            isGSM = id == R.string.radio_name_gsm
                            if (null == number) {
                                number = "000-000-0000"
                            }
                            isPhone = true
                        }
                    }
                    s_phoneInfo = SMSPhoneInfo(isPhone, number, isGSM)
                } catch (se: SecurityException) {
                    Log.e(TAG, "got SecurityException: %s", se)
                }
            }
            return s_phoneInfo
        }

        fun reset() {
            s_phoneInfo = null
        }
    }
}
