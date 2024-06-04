/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009-2024 by Eric House (xwords@eehouse.org).  All
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
package org.eehouse.android.xw4.jni

import android.app.Activity
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet

interface BoardHandler {
    interface NewRecentsProc {
        fun sawNew()
    }

    fun startHandling(
        parent: Activity, thread: JNIThread,
        connTypes: CommsConnTypeSet?,
        proc: NewRecentsProc?
    )

    fun stopHandling()
}