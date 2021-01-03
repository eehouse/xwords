/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.MenuItem;
import android.view.View;

import org.eehouse.android.xw4.DlgDelegate.Action;

public class DualpaneDelegate extends DelegateBase {
    private static final String TAG = DualpaneDelegate.class.getSimpleName();
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
    protected Dialog makeDialog( DBAlert alert, Object[] params )
    {
        Dialog dialog = null;
        MainActivity main = (MainActivity)m_activity;
        XWFragment[] frags = main.getFragments( false );
        for ( XWFragment frag : frags ) {
            dialog = frag.getDelegate().makeDialog( alert, params );
            if ( null != dialog ) {
                break;
            }
        }
        return dialog;
    }

    @Override
    protected void handleNewIntent( Intent intent )
    {
        MainActivity main = (MainActivity)m_activity;
        main.dispatchNewIntent( intent );
        Log.i( TAG, "handleNewIntent()" );
    }

    @Override
    protected boolean handleBackPressed()
    {
        MainActivity main = (MainActivity)m_activity;
        boolean handled = main.dispatchBackPressed();
        Log.i( TAG, "handleBackPressed() => %b", handled );
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

    @Override
    public boolean onPosButton( Action action, Object[] params )
    {
        boolean handled = false;
        MainActivity main = (MainActivity)m_activity;
        XWFragment[] frags = main.getVisibleFragments();
        for ( XWFragment frag : frags ) {
            handled = frag.getDelegate().onPosButton( action, params );
            if ( handled ) {
                break;
            }
        }
        return handled;
    }

    @Override
    public boolean onNegButton( Action action, Object[] params )
    {
        boolean handled = false;
        MainActivity main = (MainActivity)m_activity;
        XWFragment[] frags = main.getVisibleFragments();
        for ( XWFragment frag : frags ) {
            handled = frag.getDelegate().onNegButton( action, params );
            if ( handled ) {
                break;
            }
        }
        return handled;
    }

    @Override
    public boolean onDismissed( Action action, Object[] params )
    {
        boolean handled = false;
        MainActivity main = (MainActivity)m_activity;
        XWFragment[] frags = main.getVisibleFragments();
        for ( XWFragment frag : frags ) {
            handled = frag.getDelegate().onDismissed( action, params );
            if ( handled ) {
                break;
            }
        }
        return handled;
    }

    @Override
    public void inviteChoiceMade( Action action, InviteMeans means,
                                  Object[] params )
    {
        MainActivity main = (MainActivity)m_activity;
        XWFragment[] frags = main.getVisibleFragments();
        for ( XWFragment frag : frags ) {
            frag.getDelegate().inviteChoiceMade( action, means, params );
        }
    }
}
