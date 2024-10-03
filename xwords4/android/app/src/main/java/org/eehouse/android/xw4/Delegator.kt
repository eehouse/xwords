/*
 * Copyright 2014 - 2024 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.Activity
import android.os.Bundle
import android.widget.ListAdapter
import android.widget.ListView

interface Delegator {
    fun getActivity(): Activity?
    fun getArguments(): Bundle?
    fun finish()
    fun addFragment(fragment: XWFragment, extras: Bundle?)
    fun addFragmentForResult(
        fragment: XWFragment, extras: Bundle,
        requestCode: RequestCode
    )

    // For activities with lists
    fun setListAdapter(adapter: ListAdapter)
    fun getListAdapter(): ListAdapter?
    fun getListView(): ListView?
}
