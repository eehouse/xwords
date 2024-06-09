/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2012 - 2024 by Eric House (xwords@eehouse.org).  All rights
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
import android.util.AttributeSet
import android.view.View
import android.widget.LinearLayout
import android.widget.TextView

private val TAG = TwoStrsItem::class.java.getSimpleName()

class TwoStrsItem(cx: Context, aset: AttributeSet?) : LinearLayout(cx, aset) {
    fun setStrings(str1: String, str2: String?) {
        Log.d(TAG, "setStrings($str1, $str2)")
        var tv = findViewById<View>(R.id.text1) as TextView
        tv.text = str1

        tv = findViewById<View>(R.id.text2) as TextView
        if (null == str2) {
            tv.visibility = GONE
        } else {
            tv.text = str2
        }
    }

    val str1: String
        get() {
            val tv = findViewById<View>(R.id.text1) as TextView
            return tv.text.toString()
        }
}
