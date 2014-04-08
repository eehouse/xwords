/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2011 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.view.View;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MenuInflater;
import android.widget.LinearLayout;

public class ChatActivity extends Activity {

    private ChatDelegate m_dlgt;

    @Override
    public void onCreate( Bundle savedInstanceState ) 
    {
        if ( BuildConstants.CHAT_SUPPORTED ) {
            super.onCreate( savedInstanceState );

            setContentView( R.layout.chat );

            m_dlgt = new ChatDelegate( this, savedInstanceState );
        }
    }

    @Override
    public boolean onCreateOptionsMenu( Menu menu )
    {
        return m_dlgt.onCreateOptionsMenu( menu );
    }

    @Override
    public boolean onOptionsItemSelected( MenuItem item ) 
    {
        return m_dlgt.onOptionsItemSelected( item )
            || super.onOptionsItemSelected( item );
    }
}
