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

import android.content.Context
import android.text.TextUtils
import org.eehouse.android.xw4.Assert.assertTrue
import org.eehouse.android.xw4.BuildConfig
import org.eehouse.android.xw4.jni.LocalPlayer
import java.io.Serializable

class LocalPlayer : Serializable {
    @JvmField
    var name: String
    var password: String
    @JvmField
    var dictName: String? = null
    var secondsUsed: Int = 0
    @JvmField
    var robotIQ: Int
    @JvmField
    var isLocal: Boolean

    constructor(context: Context, num: Int) {
        isLocal = true
        robotIQ = 0 // human
        name = CommonPrefs.getDefaultPlayerName(context, num, true)
        password = ""

        // Utils.testSerialization( this )
    }

    constructor(src: LocalPlayer) {
        isLocal = src.isLocal
        robotIQ = src.robotIQ
        name = src.name
        password = src.password
        dictName = src.dictName
        secondsUsed = src.secondsUsed

        // Utils.testSerialization( this );
    }

    override fun equals(obj: Any?): Boolean {
        var result: Boolean
        if (BuildConfig.DEBUG) {
            var other: LocalPlayer? = null
            result = null != obj && obj is LocalPlayer
            if (result) {
                other = obj as LocalPlayer?
                result =
                    (secondsUsed == other!!.secondsUsed && robotIQ == other.robotIQ && isLocal == other.isLocal && TextUtils.equals(
                        name,
                        other.name
                    )
                            && TextUtils.equals(password, other.password)
                            && TextUtils.equals(dictName, other.dictName))
            }
        } else {
            result = super.equals(obj)
        }
        return result
    }

    fun isRobot(): Boolean
    {
        return robotIQ > 0
    }

    fun setIsRobot( isRobot: Boolean )
    {
        robotIQ = if (isRobot) 1 else 0
    }

    fun setRobotSmartness(iq: Int) {
        assertTrue(iq > 0)
        robotIQ = iq
    }

    override fun toString(): String {
        val result = if (BuildConfig.DEBUG
        ) String.format(
            "{name: %s, isLocal: %b, robotIQ: %d}",
            name, isLocal, robotIQ
        ) else super.toString()
        return result
    }

    companion object {
        private val TAG: String = LocalPlayer::class.java.simpleName
    }
}
