/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009 - 2013 by Eric House (xwords@eehouse.org).  All
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
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.Window;

import org.eehouse.android.xw4.jni.CommonPrefs;

public class BoardActivity extends XWActivity {

    private BoardDelegate m_dlgt;

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        if ( CommonPrefs.getHideTitleBar( this )
             && ABUtils.haveMenuKey( this ) ) {
            requestWindowFeature( Window.FEATURE_NO_TITLE );
        }
        
        m_dlgt = new BoardDelegate( this, savedInstanceState );
        super.onCreate( savedInstanceState, m_dlgt );

        int orientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
        if ( XWPrefs.getIsTablet( this ) ) {
            orientation = ActivityInfo.SCREEN_ORIENTATION_USER;
        } else if ( 9 <= Integer.valueOf( android.os.Build.VERSION.SDK ) ) {
            orientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT;
        }
        if ( ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED != orientation ) {
            setRequestedOrientation( orientation );
        }
    } // onCreate

    @Override
    public boolean onKeyDown( int keyCode, KeyEvent event )
    {
        return m_dlgt.onKeyDown( keyCode, event )
            || super.onKeyDown( keyCode, event );
    }

    @Override
    public boolean onKeyUp( int keyCode, KeyEvent event )
    {
        return m_dlgt.onKeyUp( keyCode, event ) || super.onKeyUp( keyCode, event );
    }
} // class BoardActivity
