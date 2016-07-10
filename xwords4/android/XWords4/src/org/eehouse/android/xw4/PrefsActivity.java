/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009-2010 by Eric House (xwords@eehouse.org).  All
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
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceActivity;

import junit.framework.Assert;

import org.eehouse.android.xw4.loc.LocUtils;
import org.eehouse.android.xw4.DlgDelegate.Action;

public class PrefsActivity extends PreferenceActivity
    implements Delegator, DlgDelegate.HasDlgDelegate {

    private PrefsDelegate m_dlgt;

    @Override
    protected void onCreate( Bundle savedInstanceState )
    {
        super.onCreate( savedInstanceState );
        m_dlgt = new PrefsDelegate( this, this, savedInstanceState );

        int layoutID = m_dlgt.getLayoutID();
        if ( 0 < layoutID ) {
            m_dlgt.setContentView( layoutID );
        }

        m_dlgt.init( savedInstanceState );
    }
    
    @Override
    protected void onStart() 
    {
        LocUtils.xlatePreferences( this );
        super.onStart();
    }

    @Override
    protected void onResume() 
    {
        super.onResume();
        m_dlgt.onResume();
   }

    @Override
    protected void onPause() 
    {
        super.onPause();
        m_dlgt.onPause();
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
    protected Dialog onCreateDialog( int id )
    {
        return m_dlgt.onCreateDialog( id );
    }

    @Override
    public void onPrepareDialog( int id, Dialog dialog )
    {
        super.onPrepareDialog( id, dialog );
        m_dlgt.prepareDialog( DlgID.values()[id], dialog );
    }

    public void showOKOnlyDialog( int msgID )
    {
        m_dlgt.showOKOnlyDialog( msgID );
    }

    public void showOKOnlyDialog( String msg )
    {
        m_dlgt.showOKOnlyDialog( msg );
    }

    public void showNotAgainDlgThen( int msgID, int prefsKey,
                                     DlgDelegate.Action action )
    {
        m_dlgt.showNotAgainDlgThen( msgID, prefsKey, action );
    }

    protected void showConfirmThen( int msg, int posButton, int negButton,
                                    Action action )
    {
        m_dlgt.showConfirmThen( msg, posButton, negButton, action );
    }

    protected void showConfirmThen( String msg, int posButton, int negButton,
                                    Action action )
    {
        m_dlgt.showConfirmThen( msg, posButton, negButton, action );
    }

    protected void showSMSEnableDialog( Action action )
    {
        m_dlgt.showSMSEnableDialog( action );
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

    public boolean inDPMode() { Assert.fail(); return false; }
    public void addFragment( XWFragment fragment, Bundle extras ) { Assert.fail(); }
}
