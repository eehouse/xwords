/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2024 by Eric House (xwords@eehouse.org).  All
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

class LogPruneHoursPreference(context: Context, attrs: AttributeSet?) :
    XWEditTextPreference(context, attrs)
{
    override fun persistString(value: String): Boolean {
        // Make sure assumptions like default make sense. If there are other
        // instances of this class I'll need a map or something.
        val keyID = R.string.key_log_prune_hours
        Assert.assertTrueNR(LocUtils.getString(context, keyID).equals(getKey()))

        var value = value
        val parsedInt = value.toIntOrNull()
        if ( null == parsedInt || !parsedInt.toString().equals(value) ) {
            value = LocUtils.getString(context, R.string.dflt_log_prune_hours)
        }

        setSummary(value)

        return super.persistString(value)
    }

    companion object {
        private val TAG = LogPruneHoursPreference::class.java.getSimpleName()
        fun getHours(context: Context): Int {
            val strValue = XWPrefs.getPrefsString(context, R.string.key_log_prune_hours)
            var result = strValue.toIntOrNull()
            if ( null == result ) {
                result = LocUtils.getString(context, R.string.dflt_log_prune_hours).toInt()
            }
            return result
        }
    }
}
