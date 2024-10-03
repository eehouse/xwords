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
import androidx.preference.CheckBoxPreference

abstract class ConfirmingCheckBoxPreference(context: Context, attrs: AttributeSet?) :
    CheckBoxPreference(context, attrs)
{
    private var mAttached = false

    override fun onAttached() {
        super.onAttached()
        mAttached = true
    }

    abstract fun checkIfConfirmed()

    override fun setChecked(checked: Boolean) {
        if (checked && mAttached && context is PrefsActivity) {
            checkIfConfirmed()
        } else {
            super.setChecked(checked)
        }
    }

    // Because s_this.super.setChecked() isn't allowed...
    fun super_setChecked(checked: Boolean) {
        super.setChecked(checked)
    }
}
