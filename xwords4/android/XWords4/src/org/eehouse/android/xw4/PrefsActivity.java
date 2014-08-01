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

import org.eehouse.android.xw4.loc.LocUtils;

public class PrefsActivity extends PreferenceActivity implements Delegator {

    private PrefsDelegate m_dlgt;

    @Override
    protected Dialog onCreateDialog( int id )
    {
        return m_dlgt.onCreateDialog( id );
    }

    @Override
    protected void onCreate( Bundle savedInstanceState )
    {
        super.onCreate( savedInstanceState );
        m_dlgt = new PrefsDelegate( this, this, savedInstanceState );
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
}
