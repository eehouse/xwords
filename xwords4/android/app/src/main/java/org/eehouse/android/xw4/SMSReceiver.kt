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

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.telephony.SmsMessage
import java.util.regex.Pattern

class SMSReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val action = intent.action
        // Log.d( TAG, "onReceive(): action=%s", action );
        if (action == "android.intent.action.DATA_SMS_RECEIVED" && checkPort(context, intent)) {
            val bundle = intent.extras
            if (null != bundle) {
                val pdus = bundle["pdus"] as Array<Any>?
                val smses = arrayOfNulls<SmsMessage>(
                    pdus!!.size
                )

                for (ii in pdus.indices) {
                    val sms = SmsMessage.createFromPdu(
                        pdus[ii] as ByteArray
                    )
                    if (null != sms) {
                        try {
                            val phone = sms.originatingAddress!!
                            val body = sms.userData
                            NBSProto.handleFrom(
                                context, body, phone,
                                getConfiguredPort(context)
                            )
                        } catch (npe: NullPointerException) {
                            Log.ex(TAG, npe)
                        }
                    }
                }
            }
        }
    }

    private fun checkPort(context: Context, intent: Intent): Boolean {
        var portsMatch = true
        val matcher = sPortPat.matcher(intent.dataString)
        if (matcher.find()) {
            val port = matcher.group(1).toShort()
            val myPort = getConfiguredPort(context)
            portsMatch = port == myPort
            if (!portsMatch) {
                Log.i(
                    TAG, "checkPort(): received msg on %d but expect %d",
                    port, myPort
                )
            }
        }
        return portsMatch
    }

    private fun getConfiguredPort(context: Context): Short {
        if (sPort == null) {
            sPort = context.getString(R.string.nbs_port).toShort()
        }
        return sPort!!
    }

    companion object {
        private val TAG: String = SMSReceiver::class.java.simpleName
        private val sPortPat: Pattern = Pattern.compile("^sms://localhost:(\\d+)$")

        private var sPort: Short? = null
    }
}
