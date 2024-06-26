/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2024 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
package org.eehouse.android.xw4

import android.content.Context
import android.util.AttributeSet

import org.eehouse.android.xw4.loc.LocUtils

class XWSumListPreference(private val mContext: Context, attrs: AttributeSet?) :
    XWListPreference(mContext, attrs) {

    private val TAG = XWSumListPreference::class.java.getSimpleName()

    override fun onAttached() {
        super.onAttached()

        val rows = getFieldIDs(mContext)
            .map{LocUtils.getString(mContext, it)}
            .toTypedArray()

        entries = rows
        entryValues = rows
    }

    companion object {
        private val _s_game_summary_values = intArrayOf(
            R.string.game_summary_field_empty,
            R.string.game_summary_field_language,
            R.string.game_summary_field_opponents,
            R.string.game_summary_field_state,
        )

        private val _s_game_summary_values_dbg = intArrayOf(
            R.string.game_summary_field_npackets,
            R.string.game_summary_field_rowid,
            R.string.game_summary_field_gameid,
            R.string.title_addrs_pref,
            R.string.game_summary_field_created,
        )

        private var s_game_summary_values: IntArray? = null
        fun getFieldIDs(context: Context): IntArray {
            if (null == s_game_summary_values) {
                var len = _s_game_summary_values.size
                val addDbg = (BuildConfig.NON_RELEASE
                        || XWPrefs.getDebugEnabled(context!!))
                if (addDbg) {
                    len += _s_game_summary_values_dbg.size
                }
                s_game_summary_values = IntArray(len)
                var ii = 0
                for (id in _s_game_summary_values) {
                    s_game_summary_values!![ii++] = id
                }
                if (addDbg) {
                    for (id in _s_game_summary_values_dbg) {
                        s_game_summary_values!![ii++] = id
                    }
                }
            }
            return s_game_summary_values!!
        }
    }
}
