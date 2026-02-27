/*
 * Copyright 2026 by Eric House (xwords@eehouse.org).  All rights
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

// This file was written with a lot of help from Gemini. Though I had to
// suggest that we look for a working open source example before it stopped
// feeding me garbage.

package org.eehouse.android.xw4

import android.content.Context

import org.unifiedpush.android.connector.FailedReason
import org.unifiedpush.android.connector.PushService
import org.unifiedpush.android.connector.UnifiedPush
import org.unifiedpush.android.connector.data.PushEndpoint
import org.unifiedpush.android.connector.data.PushMessage

private val TAG = Xw4PushService::class.java.simpleName
private val KEY_ENDPOINT = TAG + "_KEY_ENDPOINT"

class Xw4PushService : PushService() {
    override fun onNewEndpoint(endpoint: PushEndpoint, instance: String) {
        val url = endpoint.url
        Log.d(TAG, "New UnifiedPush Endpoint: $url")

        val oldVal = getPush(this)
        if (! url.equals(oldVal) ) {
            setPush(url)
        }
        DBUtils.setStringFor(this, KEY_ENDPOINT, url)
    }

    /**
     * Called when an actual push message arrives.
     * message.content is the raw byte array of the data.
     */
    override fun onMessage(message: PushMessage, instance: String) {
        val content = String(message.content)
        Log.d(TAG, "Push Message Received: $content")
        Utils.postWakeNotification(this)
    }

    override fun onUnregistered(instance: String) {
        Log.d(TAG, "App was unregistered from ntfy.")
    }

    override fun onRegistrationFailed(reason: FailedReason, instance: String) {
        Log.e(TAG, "Registration failed: ${reason.name}")
    }

    private fun setPush(url: String) {
        Log.d(TAG, "setPush($url)")
        DBUtils.setStringFor(this, KEY_ENDPOINT, url)
    }

    companion object {
        fun init(context: Context) {
            // I'm not using anything but ntfy for now. For whatever reason,
            // different distributors seem to require different code on the server
            // side to reach them, and I don't want to wind up not working with
            // some obscure thing a user's installed. If I can't find ntfy and
            // thing the user should be advised to install it, I'll do so.

            UnifiedPush.saveDistributor(context, "io.heckel.ntfy")
            UnifiedPush.register(context)

            // val distributors = UnifiedPush.getDistributors(this)
            // if (distributors.isNotEmpty()) {
            //     val distributor = distributors[0]
            //     Log.d(TAG, "using distributor $distributor")
            //     UnifiedPush.saveDistributor(this, distributor)
            //     UnifiedPush.register(this)
            // }
        }

        fun getPush(context: Context): String {
            val result = DBUtils.getStringFor(context, KEY_ENDPOINT, "")!!
            Log.d(TAG, "getPush() => $result")
            return result
        }
    }
}
