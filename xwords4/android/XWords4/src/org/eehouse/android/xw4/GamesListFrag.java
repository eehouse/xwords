/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2014 by Eric House (xwords@eehouse.org).  All rights
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


import android.content.Context;
import android.os.Handler;
import android.app.Activity;
import android.app.ListActivity;
import android.os.Build;
import android.os.Bundle;
import android.support.v4.app.ListFragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;

import junit.framework.Assert;

import org.eehouse.android.xw4.jni.CommonPrefs;
import org.eehouse.android.xw4.loc.LocUtils;

public class GamesListFrag extends XWFragment implements GamesListDelegator {

    private GamesListDelegate m_dlgt;

    // public GamesListFrag( FragActivity activity )
    // {
    //     m_activity = activity;
    // }

    @Override
    public void onCreate( Bundle savedInstanceState )
    {
        DbgUtils.logf( "GamesListFrag.onCreate()" );
        m_dlgt = new GamesListDelegate( this, savedInstanceState );
        super.onCreate( m_dlgt, savedInstanceState );
    }

    @Override
    public View onCreateView( LayoutInflater inflater, ViewGroup container, 
                              Bundle savedInstanceState ) 
    {
        View root = inflater.inflate( R.layout.game_list, container, false );
        LocUtils.xlateView( getActivity(), root );
        return root;
    }

    @Override
    public void onActivityCreated( Bundle savedInstanceState )
    {
        super.onActivityCreated( savedInstanceState );
        setHasOptionsMenu( true );
    }

    //////////////////////////////////////////////////////////////////////
    // ListDelegator interface
    //////////////////////////////////////////////////////////////////////
    public void launchGame( long rowid, boolean invited )
    {
        DbgUtils.logf( "GamesListFrag.launchGame(%d)", rowid );
        FragActivity.launchGame( rowid, invited );
    }

}
