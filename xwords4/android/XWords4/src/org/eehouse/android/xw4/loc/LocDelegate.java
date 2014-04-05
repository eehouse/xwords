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

package org.eehouse.android.xw4.loc;

import android.app.ListActivity;
import android.os.Bundle;

import org.eehouse.android.xw4.DelegateBase;
import org.eehouse.android.xw4.R;


public class LocDelegate extends DelegateBase {

    private ListActivity m_activity;

    protected LocDelegate( ListActivity activity, Bundle savedInstanceState )
    {
        super( activity, savedInstanceState );
        m_activity = activity;
        init( savedInstanceState );
    }

    private void init( Bundle savedInstanceState ) 
    {
        m_activity.setContentView( R.layout.loc_main );
    }

}
