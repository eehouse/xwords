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

import android.app.Activity;
import android.app.Dialog;
import android.content.Intent;
import android.os.Bundle;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MenuItem;
import android.view.View;
import android.widget.ListAdapter;
import android.widget.ListView;

import junit.framework.Assert;

import junit.framework.Assert;

public class XWActivity extends Activity implements Delegator {

    private DelegateBase m_dlgt;

    protected void onCreate( Bundle savedInstanceState, DelegateBase dlgt )
    {
        if ( XWApp.LOG_LIFECYLE ) {
            DbgUtils.logf( "%s.onCreate(this=%H)", getClass().getName(), this );
        }
        super.onCreate( savedInstanceState );
        m_dlgt = dlgt;

        int layoutID = m_dlgt.getLayoutID();
        if ( 0 < layoutID ) {
            m_dlgt.setContentView( layoutID );
        }

        dlgt.init( savedInstanceState );
    }

    @Override
    protected void onSaveInstanceState( Bundle outState ) 
    {
        super.onSaveInstanceState( outState );
        m_dlgt.onSaveInstanceState( outState );
    }

    @Override
    protected void onPause()
    {
        if ( XWApp.LOG_LIFECYLE ) {
            DbgUtils.logf( "%s.onPause(this=%H)", getClass().getName(), this );
        }
        m_dlgt.onPause();
        super.onPause();
    }

    @Override
    protected void onResume()
    {
        if ( XWApp.LOG_LIFECYLE ) {
            DbgUtils.logf( "%s.onResume(this=%H)", getClass().getName(), this );
        }
        super.onResume();
        m_dlgt.onResume();
    }

    @Override
    protected void onStart()
    {
        if ( XWApp.LOG_LIFECYLE ) {
            DbgUtils.logf( "%s.onStart(this=%H)", getClass().getName(), this );
        }
        super.onStart();
        m_dlgt.onStart();
    }

    @Override
    protected void onStop()
    {
        if ( XWApp.LOG_LIFECYLE ) {
            DbgUtils.logf( "%s.onStop(this=%H)", getClass().getName(), this );
        }
        m_dlgt.onStop();
        super.onStop();
    }

    @Override
    protected void onDestroy()
    {
        if ( XWApp.LOG_LIFECYLE ) {
            DbgUtils.logf( "%s.onDestroy(this=%H)", getClass().getName(), this );
        }
        m_dlgt.onDestroy();
        super.onDestroy();
    }

    @Override
    public void onWindowFocusChanged( boolean hasFocus )
    {
        super.onWindowFocusChanged( hasFocus );
        m_dlgt.onWindowFocusChanged( hasFocus );
    }

    @Override
    public void onBackPressed() {
        if ( !m_dlgt.onBackPressed() ) {
            super.onBackPressed();
        }
    }

    @Override
    public boolean onCreateOptionsMenu( Menu menu ) 
    {
        return m_dlgt.onCreateOptionsMenu( menu );
    }

    @Override
    public boolean onPrepareOptionsMenu( Menu menu ) 
    {
        return m_dlgt.onPrepareOptionsMenu( menu )
            || super.onPrepareOptionsMenu( menu );
    } // onPrepareOptionsMenu

    @Override
    public boolean onOptionsItemSelected( MenuItem item ) 
    {
        return m_dlgt.onOptionsItemSelected( item )
            || super.onOptionsItemSelected( item );
    }

    @Override
    public void onCreateContextMenu( ContextMenu menu, View view,
                                     ContextMenuInfo menuInfo )
    {
        m_dlgt.onCreateContextMenu( menu, view, menuInfo );
    }

    @Override
    public boolean onContextItemSelected( MenuItem item )
    {
        return m_dlgt.onContextItemSelected( item );
    }

    @Override
    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = super.onCreateDialog( id );
        Assert.assertNull( dialog );
        if ( null == dialog ) {
            dialog = m_dlgt.onCreateDialog( id );
        }
        return dialog;
    } // onCreateDialog

    @Override
    public void onPrepareDialog( int id, Dialog dialog )
    {
        super.onPrepareDialog( id, dialog );
        m_dlgt.prepareDialog( DlgID.values()[id], dialog );
    }

    @Override
    protected void onActivityResult( int requestCode, int resultCode, 
                                     Intent data )
    {
        RequestCode rc = RequestCode.values()[requestCode]; 
        m_dlgt.onActivityResult( rc, resultCode, data );
    }

    //////////////////////////////////////////////////////////////////////
    // Delegator interface
    //////////////////////////////////////////////////////////////////////
    public Activity getActivity()
    {
        return this;
    }

    public Bundle getArguments()
    {
        return getIntent().getExtras();
    }

    public ListView getListView()
    {
        ListView view = (ListView)findViewById( android.R.id.list );
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
