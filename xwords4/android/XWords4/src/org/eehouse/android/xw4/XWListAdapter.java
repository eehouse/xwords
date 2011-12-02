/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2009-2010 by Eric House (xwords@eehouse.org).  All
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

package org.eehouse.android.xw4;

import android.app.ListActivity;
import android.widget.ListAdapter;
import android.content.Context;
import android.database.DataSetObserver;

/**
 * Let's see if we can implement a few of these methods just once.
 */
public abstract class XWListAdapter implements ListAdapter {
    private int m_count;

    public XWListAdapter( ) {
        this( 0 );
    }
    public XWListAdapter( int count ) {
        m_count = count;
    }

    public boolean areAllItemsEnabled() { return true; }
    public boolean isEnabled( int position ) { return true; }
    public int getCount() { return m_count; }
    public long getItemId(int position) { return position; }
    public int getItemViewType(int position) { return 0; }
    public int getViewTypeCount() { return 1; }
    public boolean hasStableIds() { return true; }
    public boolean isEmpty() { return getCount() == 0; }
    public void registerDataSetObserver(DataSetObserver observer) {}
    public void unregisterDataSetObserver(DataSetObserver observer) {}
}