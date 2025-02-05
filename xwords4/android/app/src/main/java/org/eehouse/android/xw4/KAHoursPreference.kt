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
    XWListPreference(context, attrs)
{
    override fun getEntries(): Array<CharSequence> {
        return getEntriesImpl(context)
    }
    override fun getEntryValues(): Array<CharSequence> {
        return getEntriesImpl(context)
    }

    companion object {
        private var sEntries: Array<CharSequence>? = null
        private val DEFAULT_HOURS = 2
        private val sHours = arrayOf(1, DEFAULT_HOURS, 4, 8, 12, 24, 72)

        fun hoursFromEntry(context: Context, entry: String): Int {
            val result = sHours
                .firstNotNullOf{if (format(context, it).equals(entry)) it else null}
                ?: DEFAULT_HOURS
            // Log.d(TAG, "hoursFromEntry($entry) => $result")
            return result
        }

        private fun getEntriesImpl(context: Context):Array<CharSequence> {
            if (null == sEntries) {
                sEntries = sHours.map{format(context, it)}.toTypedArray()
            }
            return sEntries!!
        }

        private fun format(context: Context, hour: Int): String {
            val result = LocUtils
                .getQuantityString(context, R.plurals.hours_fmt, hour, hour)
            return result
        }
    }
}
