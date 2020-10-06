/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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

package org.eehouse.android.xw4;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.ListAdapter;
import android.widget.ListView;

public abstract class ListDelegateBase extends DelegateBase {

    private Activity m_activity;
    private Delegator m_delegator;

    protected ListDelegateBase( Delegator delegator, Bundle savedInstanceState,
                                int layoutID )
    {
        this( delegator, savedInstanceState, layoutID, R.menu.empty );
    }

    protected ListDelegateBase( Delegator delegator, Bundle savedInstanceState,
                                int layoutID, int menuID )
    {
        super( delegator, savedInstanceState, layoutID, menuID );
        m_delegator = delegator;
        m_activity = delegator.getActivity();
    }

    protected void setListAdapter( ListAdapter adapter )
    {
        m_delegator.setListAdapter( adapter );
    }

    protected ListAdapter setListAdapter()
    {
        return m_delegator.getListAdapter();
    }

    protected ListView getListView()
    {
        return m_delegator.getListView();
    }

    protected void setListAdapterKeepScroll( ListAdapter adapter )
    {
        ListView listView = getListView();
        int pos = listView.getFirstVisiblePosition();
        View child = listView.getChildAt( 0 );
        int top = (null == child) ? 0 : child.getTop();

        setListAdapter( adapter );
        listView.setSelectionFromTop( pos, top );
    }
}
