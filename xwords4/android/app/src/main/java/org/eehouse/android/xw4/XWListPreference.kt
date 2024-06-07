/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */ /*
 * Copyright 2010 - 2011 by Eric House (xwords@eehouse.org).  All
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
import androidx.preference.ListPreference

import org.eehouse.android.xw4.loc.LocUtils

open class XWListPreference(private val mContext: Context,
                            attrs: AttributeSet?) :
    ListPreference(mContext, attrs)
{
    override fun onAttached() {
        super.onAttached()
        summary = getPersistedString("")
    }

    override fun persistString(value: String): Boolean {
        summary = value
        return super.persistString(value)
    }

    override fun setSummary(summary: CharSequence?) {
        var summary = summary
        val entries = entries
        if (null != entries) {
            val indx = findIndexOfValue(summary.toString())
            if (0 <= indx && indx < entries.size) {
                summary = entries[indx]
            }
        }
        val xlated = LocUtils.xlateString(mContext, summary.toString())
        if (null != xlated) {
            summary = xlated
        }
        super.setSummary(summary)
    }
}
