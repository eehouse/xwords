/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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

import android.content.Intent;
import android.os.Bundle;
import androidx.fragment.app.Fragment;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListAdapter;
import android.widget.ListView;

import java.util.HashSet;
import java.util.Set;


abstract class XWFragment extends Fragment implements Delegator {
    private static final String TAG = XWFragment.class.getSimpleName();
    private static final String PARENT_NAME = "PARENT_NAME";
    private static final String COMMIT_ID = "COMMIT_ID";

    private DelegateBase m_dlgt;
    private String m_parentName;
    private boolean m_hasOptionsMenu = false;
    private int m_commitID;

    private static Set<XWFragment> sActiveFrags = new HashSet<>();
    public static XWFragment findOwnsView( View view )
    {
        XWFragment result = null;
        DbgUtils.assertOnUIThread();
        for ( XWFragment frag : sActiveFrags ) {
           if ( frag.getView() == view ) {
               Assert.assertNull( result );
               result = frag;
               // break;  <-- put this back eventually
            }
        }

        return result;
    }

    public XWFragment setParentName( Delegator parent )
    {
        m_parentName = null == parent ? "<none>"
            : parent.getClass().getSimpleName();
        return this;
    }

    public String getParentName()
    {
        Assert.assertNotNull( m_parentName );
        return m_parentName;
    }

    public void setCommitID( int id ) { m_commitID = id; }
    public int getCommitID() { return m_commitID; }

    protected void onCreate( DelegateBase dlgt, Bundle sis, boolean hasOptionsMenu )
    {
        Log.d( TAG, "%H/%s.onCreate() called", this, getClass().getSimpleName() );
        m_hasOptionsMenu = hasOptionsMenu;
        this.onCreate( dlgt, sis );
    }

    @Override
    public void onSaveInstanceState( Bundle outState )
    {
        Log.d( TAG, "%H/%s.onSaveInstanceState() called", this, getClass().getSimpleName() );
        Assert.assertNotNull( m_parentName );
        outState.putString( PARENT_NAME, m_parentName );
        outState.putInt( COMMIT_ID, m_commitID );
        m_dlgt.onSaveInstanceState( outState );
        super.onSaveInstanceState( outState );
    }

    protected void onCreate( DelegateBase dlgt, Bundle sis )
    {
        Log.d( TAG, "%H/%s.onCreate() called", this, getClass().getSimpleName() );
        super.onCreate( sis );
        if ( null != sis ) {
            m_parentName = sis.getString( PARENT_NAME );
            Assert.assertNotNull( m_parentName );
            m_commitID = sis.getInt( COMMIT_ID );
        }
        Assert.assertNull( m_dlgt );
        m_dlgt = dlgt;
    }

    // This is supposed to be the first call we can use to start hooking stuff
    // up.
    // @Override
    // public void onAttach( Activity activity )
    // {
    //     Log.df( "%s.onAttach() called",
    //                     this.getClass().getSimpleName() );
    //     super.onAttach( activity );
    // }

    @Override
    public View onCreateView( LayoutInflater inflater, ViewGroup container,
                              Bundle savedInstanceState )
    {
        Log.d( TAG, "%H/%s.onCreateView() called", this, getClass().getSimpleName() );
        sActiveFrags.add(this);
        return m_dlgt.inflateView( inflater, container );
    }

    @Override
    public void onActivityCreated( Bundle savedInstanceState )
    {
        Log.d( TAG, "%H/%s.onActivityCreated() called", this, getClass().getSimpleName() );
        m_dlgt.init( savedInstanceState );
        super.onActivityCreated( savedInstanceState );
        if ( m_hasOptionsMenu ) {
            setHasOptionsMenu( true );
        }
    }

    @Override
    public void onPause()
    {
        Log.d( TAG, "%H/%s.onPause() called", this, getClass().getSimpleName() );
        m_dlgt.onPause();
        super.onPause();
    }

    @Override
    public void onResume()
    {
        Log.d( TAG, "%H/%s.onResume() called", this, getClass().getSimpleName() );
        super.onResume();
        m_dlgt.onResume();
    }

    @Override
    public void onStart()
    {
        Log.d( TAG, "%H/%s.onStart() called", this, getClass().getSimpleName() );
        super.onStart();
        m_dlgt.onStart();
    }

    @Override
    public void onStop()
    {
        Log.d( TAG, "%H/%s.onStop() called", this, getClass().getSimpleName() );
        m_dlgt.onStop();
        super.onStop();
    }

    @Override
    public void onDestroy()
    {
        Log.d( TAG, "%H/%s.onDestroy() called", this, getClass().getSimpleName() );
        m_dlgt.onDestroy();
        sActiveFrags.remove( this );
        super.onDestroy();
    }

    @Override
    public void onActivityResult( int requestCode, int resultCode, Intent data )
    {
        Log.d( TAG, "%H/%s.onActivityResult() called", this, getClass().getSimpleName() );
        m_dlgt.onActivityResult( RequestCode.values()[requestCode], 
                                 resultCode, data );
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
        Assert.failDbg();
    }

    public void setTitle() { m_dlgt.setTitle(); }

    @Override
    public void addFragment( XWFragment fragment, Bundle extras )
    {
        MainActivity main = (MainActivity)getActivity();
        if ( null != main ) {   // I've seen this come back null
            main.addFragment( fragment, extras );
        }
    }

    @Override
    public void addFragmentForResult( XWFragment fragment, Bundle extras,
                                      RequestCode code )
    {
        MainActivity main = (MainActivity)getActivity();
        main.addFragmentForResult( fragment, extras, code, this );
    }

    public DelegateBase getDelegate() { return m_dlgt; }

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
