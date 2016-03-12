/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2014 by Eric House (xwords@eehouse.org).  All rights
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

package org.eehouse.android.xw4;

import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListAdapter;
import android.widget.ListView;

import junit.framework.Assert;

public class XWFragment extends Fragment implements Delegator {

    private DelegateBase m_dlgt;

    public void onCreate( DelegateBase dlgt, Bundle sis )
    {
        DbgUtils.logdf( "%s.onCreate() called", this.getClass().getName() );
        super.onCreate( sis );
        m_dlgt = dlgt;
    }

    @Override
    public View onCreateView( LayoutInflater inflater, ViewGroup container, 
                              Bundle savedInstanceState ) 
    {
        DbgUtils.logdf( "%s.onCreateView() called", this.getClass().getName() );
        return m_dlgt.inflateView( inflater, container );
    }

    @Override
    public void onActivityCreated( Bundle savedInstanceState )
    {
        DbgUtils.logdf( "%s.onActivityCreated() called", this.getClass().getName() );
        m_dlgt.init( savedInstanceState );
        super.onActivityCreated( savedInstanceState );
    }

    @Override
    public void onPause()
    {
        DbgUtils.logdf( "%s.onPause() called", this.getClass().getName() );
        m_dlgt.onPause();
        super.onPause();
    }

    @Override
    public void onResume()
    {
        super.onResume();
        m_dlgt.onResume();
    }

    @Override
    public void onStart()
    {
        DbgUtils.logdf( "%s.onStart() called", this.getClass().getName() );
        super.onStart();
        m_dlgt.onStart();
    }

    @Override
    public void onStop()
    {
        DbgUtils.logdf( "%s.onStop() called", this.getClass().getName() );
        m_dlgt.onStop();
        super.onStop();
    }

    @Override
    public void onDestroy()
    {
        m_dlgt.onDestroy();
        super.onDestroy();
    }

    @Override
    public void onPrepareOptionsMenu( Menu menu )
    {
        m_dlgt.onPrepareOptionsMenu( menu );
    }

    @Override
    public void onCreateOptionsMenu( Menu menu, MenuInflater inflater )
    {
        m_dlgt.onCreateOptionsMenu( menu, inflater );
    }

    @Override
    public boolean onOptionsItemSelected( MenuItem item ) 
    {
        return m_dlgt.onOptionsItemSelected( item );
    }

    public void finish()
    {
        Assert.fail();
    }

    public ListView getListView()
    {
        ListView view = (ListView)m_dlgt.findViewById( android.R.id.list );
        return view;
    }

    public void setListAdapter( ListAdapter adapter )
    { 
        getListView().setAdapter( adapter );
    }

    public ListAdapter getListAdapter()
    {
        return getListView().getAdapter();
    }
}


