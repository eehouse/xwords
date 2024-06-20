/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2017 - 2024 by Eric House (xwords@eehouse.org).  All rights
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
import android.content.Context

// On older Android we can't load the class WiDirService. So do it with
// try/catch here
object WiDirWrapper {
    private var s_working = false

    fun init(context: Context) {
        try {
            WiDirService.init(context)
            s_working = true
        } catch (err: VerifyError) {
        }
    }

    fun enabled(): Boolean {
        return s_working && WiDirService.enabled()
    }

    fun activityResumed(activity: Activity) {
        if (s_working) {
            WiDirService.activityResumed(activity)
        }
    }

    fun activityPaused(activity: Activity) {
        if (s_working) {
            WiDirService.activityPaused(activity)
        }
    }
}
