/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009 - 2016 by Eric House (xwords@eehouse.org).  All
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

import android.app.Activity;
import android.app.Dialog;
import android.content.Intent;
import android.os.Bundle;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.ContextMenu;
import android.view.MenuItem;
import android.view.View;

public class DualpaneDelegate extends DelegateBase {
    private Activity m_activity;

    public DualpaneDelegate( Delegator delegator, Bundle sis )
    {
        super( delegator, sis, R.layout.dualcontainer );
        m_activity = delegator.getActivity();
    }

    @Override
    protected void init( Bundle savedInstanceState )
    {
    }

    @Override
    protected Dialog onCreateDialog( int id )
    {
        return DlgDelegate.onCreateDialog( id );
    }

    @Override
    protected void prepareDialog( DlgID dlgId, Dialog dialog )
    {
        DlgDelegate.onPrepareDialog( dlgId.ordinal(), dialog );
    }

    @Override
    protected boolean handleNewIntent( Intent intent )
    {
        MainActivity main = (MainActivity)m_activity;
        boolean handled = main.dispatchNewIntent( intent );
        DbgUtils.logf( "DualpaneDelegate.handleNewIntent() => %b", handled );
        return handled;
    }

    @Override
    protected boolean handleBackPressed()
    {
        MainActivity main = (MainActivity)m_activity;
        boolean handled = main.dispatchBackPressed();
        DbgUtils.logf( "DualpaneDelegate.handleBackPressed() => %b", handled );
        return handled;
    }

    @Override
    protected void onActivityResult( RequestCode requestCode, int resultCode, Intent data )
    {
        MainActivity main = (MainActivity)m_activity;
        main.dispatchOnActivityResult( requestCode, resultCode, data );
    }

    @Override
    protected void onCreateContextMenu( ContextMenu menu, View view, 
                                        ContextMenuInfo menuInfo )
    {
        MainActivity main = (MainActivity)m_activity;
        main.dispatchOnCreateContextMenu( menu, view, menuInfo );
    }

    @Override
    protected boolean onContextItemSelected( MenuItem item )
    {
        MainActivity main = (MainActivity)m_activity;
        return main.dispatchOnContextItemSelected( item );
    }

}
