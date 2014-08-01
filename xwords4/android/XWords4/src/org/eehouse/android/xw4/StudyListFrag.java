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

import android.os.Bundle;
import android.view.View;

import org.eehouse.android.xw4.loc.LocUtils;

public class StudyListFrag extends XWListFragment implements ListDelegator {
    private StudyListDelegate m_dlgt;

    @Override
    public void onCreate( Bundle savedInstanceState )
    {
        DbgUtils.logf( "StudyListFrag.onCreate()" );
        m_dlgt = new StudyListDelegate( this, savedInstanceState );
        super.onCreate( m_dlgt, savedInstanceState, R.layout.studylist );
    }

    @Override
    public void onActivityCreated( Bundle savedInstanceState )
    {
        super.onActivityCreated( savedInstanceState );
        setHasOptionsMenu( true );
    }
}
