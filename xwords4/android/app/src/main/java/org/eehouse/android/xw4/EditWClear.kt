/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2019 - 2024 by Eric House (xwords@eehouse.org).  All rights
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
import android.widget.SearchView

private val TAG: String = EditWClear::class.java.simpleName

class EditWClear(context: Context, aset: AttributeSet?) :
    SearchView(context, aset),
    SearchView.OnQueryTextListener
{
    private var mWatchers: MutableSet<TextWatcher>? = null

    interface TextWatcher {
        fun onTextChanged(newText: String)
    }

    @Synchronized
    fun addTextChangedListener(proc: TextWatcher) {
        if (null == mWatchers) {
            mWatchers = HashSet()
            setOnQueryTextListener(this)
        }
        mWatchers!!.add(proc)
    }

    fun setText(txt: String?) {
        super.setQuery(txt, false)
    }

    val text: CharSequence
        get() = super.getQuery()

    // from SearchView.OnQueryTextListener
    @Synchronized
    override fun onQueryTextChange(newText: String): Boolean {
        mWatchers!!.map{it.onTextChanged(newText)}
        return true
    }

    // from SearchView.OnQueryTextListener
    override fun onQueryTextSubmit(query: String): Boolean {
        Assert.assertFalse(BuildConfig.DEBUG) // WTF?
        return true
    }
}
