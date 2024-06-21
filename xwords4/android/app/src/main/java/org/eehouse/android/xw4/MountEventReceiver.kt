/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent

private val TAG: String = MountEventReceiver::class.java.simpleName

class MountEventReceiver : BroadcastReceiver() {
    interface SDCardNotifiee {
        fun cardMounted(nowMounted: Boolean)
    }

    override fun onReceive(context: Context, intent: Intent) {
        Log.i(TAG, "onReceive(%s)", intent.action)
        synchronized(s_procs) {
            do {
                if (s_procs.isEmpty()) {
                    break
                }

                var mounted: Boolean
                val action = intent.action
                mounted = if (action == Intent.ACTION_MEDIA_MOUNTED) {
                    true
                } else if (action == Intent.ACTION_MEDIA_EJECT) {
                    false
                } else {
                    break
                }
                val iter: Iterator<SDCardNotifiee> = s_procs.iterator()
                while (iter.hasNext()) {
                    iter.next().cardMounted(mounted)
                }
            } while (false)
        }
    }

    companion object {

        private val s_procs = HashSet<SDCardNotifiee>()

        fun register(proc: SDCardNotifiee) {
            synchronized(s_procs) {
                s_procs.add(proc)
            }
        }

        fun unregister(proc: SDCardNotifiee) {
            synchronized(s_procs) {
                s_procs.remove(proc)
            }
        }
    }
}
