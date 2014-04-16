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
import android.os.Bundle;
import android.app.Dialog;
import android.view.KeyEvent;

public class GameConfigActivity extends Activity {

    private GameConfigDelegate m_dlgt;

    @Override
    public void onCreate( Bundle savedInstanceState )
    {
        super.onCreate( savedInstanceState );
        m_dlgt = new GameConfigDelegate( this, savedInstanceState );
        m_dlgt.init( savedInstanceState );
    } // onCreate

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
    protected void onPrepareDialog( int id, Dialog dialog )
    { 
        m_dlgt.onPrepareDialog( id, dialog );
        super.onPrepareDialog( id, dialog );
    }
    @Override
    protected void onStart()
    {
        super.onStart();
        m_dlgt.onStart();
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
        m_dlgt.onPause();
        super.onPause();
    }

    @Override
    protected void onSaveInstanceState( Bundle outState ) 
    {
        super.onSaveInstanceState( outState );
        m_dlgt.onSaveInstanceState( outState );
    }

    @Override
    public boolean onKeyDown( int keyCode, KeyEvent event )
    {
        boolean consumed = m_dlgt.onKeyDown( keyCode, event );
        return consumed || super.onKeyDown( keyCode, event );
    }
}
