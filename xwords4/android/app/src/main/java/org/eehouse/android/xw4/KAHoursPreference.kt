/*
 * Copyright 2025  by Eric House (xwords@eehouse.org).  All
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
import org.eehouse.android.xw4.loc.LocUtils

private val TAG = KAHoursPreference::class.java.getSimpleName()

class KAHoursPreference(context: Context, attrs: AttributeSet?) :
    XWEditTextPreference(context, attrs)
{
    override fun persistString(value: String): Boolean {
        // Make sure assumptions like default make sense. If there are other
        // instances of this class I'll need a map or something.
        val keyID = R.string.key_ka_hours
        Assert.assertTrueNR(LocUtils.getString(context, keyID).equals(getKey()))

        val dfltVal = LocUtils.getString(context, R.string.dflt_ka_hours).toInt()
        val value = (value.toIntOrNull()?.let {
                         if (it >= 1 && it <= 72) it
                         else dfltVal
                     } ?: dfltVal).toString()

        setSummary(value)

        return super.persistString(value)
    }
}
