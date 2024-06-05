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

import org.eehouse.android.xw4.BuildConfig
import org.eehouse.android.xw4.DictUtils.ON_SERVER
import org.eehouse.android.xw4.Utils.ISOCode

class DictInfo {
    // set in java code
    @JvmField
    var name: String? = null
    @JvmField
    var fullSum: String? = null // md5sum of the whole file
    @JvmField
    var onServer: ON_SERVER? = null // is it currently downloadable?

    // set in jni code
    @JvmField
    var wordCount: Int = 0
    var isoCodeStr: String? = null // public only for access from JNI; use isoCode() from java
    @JvmField
    var langName: String? = null
    @JvmField
    var md5Sum: String? = null // internal (skipping header?)

    fun isoCode(): ISOCode? {
        return ISOCode.newIf(isoCodeStr)
    }

    override fun toString(): String {
        return if (BuildConfig.NON_RELEASE) {
            StringBuilder("{")
                .append("name: ").append(name)
                .append(", isoCode: ").append(isoCodeStr)
                .append(", langName: ").append(langName)
                .append(", md5Sum: ").append(md5Sum)
                .append(", fullSum: ").append(fullSum)
                .append(", onServer: ").append(onServer)
                .append("}").toString()
        } else {
            super.toString()
        }
    }
}
