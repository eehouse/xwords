/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009 - 2014 by Eric House (xwords@eehouse.org).  All
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

package org.eehouse.android.xw4.loc;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

import org.eehouse.android.xw4.XWListAdapter;

public class LocListAdapter extends XWListAdapter {
    private static final String TAG = LocListAdapter.class.getSimpleName();

    private Context m_context;
    private ListView m_listView;
    private LocSearcher m_searcher;
    private View.OnClickListener m_listener;

    protected LocListAdapter( Context context, ListView listView,
                              LocSearcher searcher,
                              View.OnClickListener listener )
    {
        m_context = context;
        m_listView = listView;
        m_searcher = searcher;
        m_listener = listener;
    }

    @Override
    public int getCount()
    {
        int count = m_searcher.matchSize();
        // DbgUtils.logf(" LocListAdapter.getCount() => %d", count );
        return count;
    }

    public View getView( int position, View convertView, ViewGroup parent )
    {
        // DbgUtils.logf( "LocListAdapter.getView(position=%d)", position );
        LocSearcher.Pair pair = m_searcher.getNthMatch( position );
        View view = LocListItem.create( m_context, pair, position );
        view.setOnClickListener( m_listener );
        return view;
    }
}
