/*
 * Copyright 2009-2024 by Eric House (xwords@eehouse.org).  All
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

import android.widget.BaseAdapter
import android.widget.ListAdapter

/**
 * Let's see if we can implement a few of these methods just once.
 */
abstract class XWListAdapter @JvmOverloads constructor(private val m_count: Int = 0) :
    BaseAdapter(), ListAdapter {
    override fun areAllItemsEnabled(): Boolean {
        return true
    }

    override fun isEnabled(position: Int): Boolean {
        return true
    }

    override fun getCount(): Int {
        return m_count
    }

    override fun getItem(position: Int): Any? {
        return null
    }

    override fun getItemId(position: Int): Long {
        return position.toLong()
    }

    override fun getItemViewType(position: Int): Int {
        return IGNORE_ITEM_VIEW_TYPE
    }

    override fun getViewTypeCount(): Int {
        return 1
    }

    override fun hasStableIds(): Boolean {
        return true
    }

    override fun isEmpty(): Boolean {
        return count == 0
    } // public void registerDataSetObserver( DataSetObserver observer ) {}

    // public void unregisterDataSetObserver( DataSetObserver observer ) {}
    companion object {
        private val TAG: String = XWListAdapter::class.java.simpleName
    }
}
