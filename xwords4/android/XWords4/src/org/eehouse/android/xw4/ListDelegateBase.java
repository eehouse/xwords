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

package org.eehouse.android.xw4;

import android.app.ListActivity;
import android.os.Bundle;
import android.widget.ListAdapter;
import android.widget.ListView;

public class ListDelegateBase extends DelegateBase {
    
    private ListActivity m_activity;

    protected ListDelegateBase( ListActivity activity, Bundle savedInstanceState,
                                int menuID )
    {
        super( activity, savedInstanceState, menuID );
        m_activity = activity;
    }

    protected ListDelegateBase( ListActivity activity, Bundle savedState )
    {
        super( activity, savedState );
        m_activity = activity;
    }

    protected void setListAdapter( ListAdapter adapter )
    {
        m_activity.setListAdapter( adapter );
    }

    protected ListAdapter setListAdapter()
    {
        return m_activity.getListAdapter();
    }

    protected ListView getListView()
    {
        return m_activity.getListView();
    }
}
