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
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;

public class XWActivity extends Activity {

    private DelegateBase m_dlgt;

    protected void onCreate( Bundle savedInstanceState, DelegateBase dlgt )
    {
        DbgUtils.logf( "<eeh>XWActivity.onCreate()" );
        super.onCreate( savedInstanceState );
        m_dlgt = dlgt;
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
        m_dlgt.onPause();
        super.onPause();
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        m_dlgt.onResume();
    }

    @Override
    protected void onStart()
    {
        super.onStart();
        m_dlgt.onStart();
    }

    @Override
    protected void onStop()
    {
        m_dlgt.onStop();
        super.onStop();
    }

    @Override
    protected void onDestroy()
    {
        m_dlgt.onDestroy();
        super.onDestroy();
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
        DbgUtils.logf( "XWListActivity.onCreateOptionsMenu called" );
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
    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = super.onCreateDialog( id );
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
}
