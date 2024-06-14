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

import android.content.Context
import android.util.AttributeSet
import android.view.View
import android.widget.CheckBox
import android.widget.LinearLayout
import android.widget.TextView
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType

class DisablesItem(context: Context, aset: AttributeSet?) :
    LinearLayout(context, aset)
{
    private var m_type: CommsConnType? = null
    private var m_state: BooleanArray? = null

    fun init(typ: CommsConnType, state: BooleanArray) {
        if (BuildConfig.DEBUG) {
            m_state = state
            m_type = typ
            (findViewById<View>(R.id.addr_type) as TextView).text = typ.shortName()

            setupCheckbox(R.id.send, true)
            setupCheckbox(R.id.receive, false)
        }
    }

    private fun setupCheckbox(id: Int, forSend: Boolean) {
        if (BuildConfig.DEBUG) {
            val cb = findViewById<View>(id) as CheckBox
            cb.isChecked = m_state!![if (forSend) 1 else 0]
            cb.setOnClickListener { m_state!![if (forSend) 1 else 0] = cb.isChecked }
        }
    }
}
