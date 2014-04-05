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

import android.app.ListActivity;
import android.os.Bundle;
import android.widget.ListView;

import org.eehouse.android.xw4.DelegateBase;
import org.eehouse.android.xw4.R;


public class LocDelegate extends DelegateBase {

    private ListActivity m_activity;
    private LocListAdapter m_adapter;

    protected LocDelegate( ListActivity activity, Bundle savedInstanceState )
    {
        super( activity, savedInstanceState );
        m_activity = activity;
        init( savedInstanceState );
    }

    protected boolean onBackPressed()
    {
        LocUtils.saveData( m_activity );
        return false;
    }

    private void init( Bundle savedInstanceState ) 
    {
        m_activity.setContentView( R.layout.loc_main );

        ListView listview = m_activity.getListView();
        m_adapter = new LocListAdapter( m_activity, listview );
        m_activity.setListAdapter( m_adapter );
    }

}
