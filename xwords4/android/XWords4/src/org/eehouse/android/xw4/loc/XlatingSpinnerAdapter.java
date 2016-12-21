/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2014 by Eric House (xwords@eehouse.org).  All rights reserved.
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

package org.eehouse.android.xw4.loc;

import android.content.Context;
import android.database.DataSetObserver;
import android.view.View;
import android.view.ViewGroup;
import android.widget.SpinnerAdapter;

public class XlatingSpinnerAdapter implements SpinnerAdapter {
    private static final String TAG = XlatingSpinnerAdapter.class.getSimpleName();

    private SpinnerAdapter m_adapter;
    private Context m_context;

    protected XlatingSpinnerAdapter( Context context, SpinnerAdapter adapter )
    {
        m_adapter = adapter;
        m_context = context;
    }

    public View getDropDownView( int position, View convertView, ViewGroup parent )
    {
        View view = m_adapter.getDropDownView( position, convertView, parent );
        LocUtils.xlateView( m_context, view );
        return view;
    }

    public View getView( int position, View convertView, ViewGroup parent )
    {
        View view = m_adapter.getView( position, convertView, parent );
        LocUtils.xlateView( m_context, view );
        return view;
    }

    public int getCount() { return m_adapter.getCount(); }
    public Object getItem(int position) { return m_adapter.getItem(position); }
    public long getItemId(int position) { return m_adapter.getItemId(position); }
    public int getItemViewType(int position) { return m_adapter.getItemViewType(position); }
    public int getViewTypeCount() { return m_adapter.getViewTypeCount(); }
    public boolean hasStableIds() { return m_adapter.hasStableIds(); }
    public boolean isEmpty() { return m_adapter.isEmpty(); }
    public void registerDataSetObserver(DataSetObserver observer) {
        m_adapter.registerDataSetObserver(observer);
    }
    public void unregisterDataSetObserver(DataSetObserver observer) {
        m_adapter.unregisterDataSetObserver(observer);
    }
}
