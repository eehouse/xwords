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
package org.eehouse.android.xw4

import org.unifiedpush.android.connector.FailedReason
import org.unifiedpush.android.connector.PushService
import org.unifiedpush.android.connector.data.PushEndpoint
import org.unifiedpush.android.connector.data.PushMessage

private val TAG = Xw4PushService::class.java.simpleName

class Xw4PushService : PushService() {
    override fun onNewEndpoint(endpoint: PushEndpoint, instance: String) {
        val url = endpoint.url
        Log.d(TAG, "New UnifiedPush Endpoint: $url")
        
        // TODO: Save 'url' to SharedPreferences and send it to your server.
    }

    /**
     * Called when an actual push message arrives.
     * message.content is the raw byte array of the data.
     */
    override fun onMessage(message: PushMessage, instance: String) {
        val content = String(message.content)
        Log.d(TAG, "Push Message Received: $content")
        
        // TODO: Trigger your game board update logic here.
    }

    override fun onUnregistered(instance: String) {
        Log.d(TAG, "App was unregistered from ntfy.")
    }

    override fun onRegistrationFailed(reason: FailedReason, instance: String) {
        Log.e(TAG, "Registration failed: ${reason.name}")
    }
}
