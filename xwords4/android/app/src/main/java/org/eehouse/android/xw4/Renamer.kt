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

import android.content.Context
import android.util.AttributeSet
import android.view.View
import android.widget.LinearLayout
import android.widget.TextView

import org.eehouse.android.xw4.loc.LocUtils

class Renamer(context: Context, aset: AttributeSet?) :
    LinearLayout(context, aset)
{
    fun setLabel(label: String): Renamer {
        val view = findViewById<View>(R.id.name_label) as TextView
        view.text = label
        return this
    }

    fun setLabel(id: Int): Renamer {
        setLabel(LocUtils.getString(context, id))
        return this
    }

    fun setName(text: String): Renamer {
        edit.setText(text)
        return this
    }

    val name: String
        get() = edit.text.toString()

    private val edit: EditWClear
        get() {
            val view = findViewById<View>(R.id.name_edit) as EditWClear
            return view
        }
}
