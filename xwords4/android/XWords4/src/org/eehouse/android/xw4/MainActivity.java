/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009 - 2014 by Eric House (xwords@eehouse.org).  All
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

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import org.eehouse.android.xw4.jni.CurGameInfo;

import junit.framework.Assert;

public class MainActivity extends XWActivity {
    private GamesListDelegate m_dlgt;

    @Override
    protected void onCreate( Bundle savedInstanceState )
    {
        m_dlgt = new GamesListDelegate( this, savedInstanceState );
        super.onCreate( savedInstanceState, m_dlgt );

        // Trying to debug situation where two of this activity are running at
        // once. finish()ing when Intent.FLAG_ACTIVITY_BROUGHT_TO_FRONT is
        // passed is not the fix, but perhaps there's another
        // int flags = getIntent().getFlags();
        // DbgUtils.logf( "MainActivity.onCreate(this=%H): flags=0x%x", 
        //                this, flags );
    } // onCreate

    // called when we're brought to the front (probably as a result of
    // notification)
    @Override
    protected void onNewIntent( Intent intent )
    {
        super.onNewIntent( intent );
        m_dlgt.onNewIntent( intent );
    }

    //////////////////////////////////////////////////////////////////////
    // GamesListDelegator interface
    //////////////////////////////////////////////////////////////////////
    public void launchGame( long rowID, boolean invited )
    {
        GameUtils.launchGame( this, rowID, invited );
    }
}
