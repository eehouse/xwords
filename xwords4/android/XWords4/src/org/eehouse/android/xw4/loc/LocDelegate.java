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
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.Spinner;

import org.eehouse.android.xw4.DbgUtils;
import org.eehouse.android.xw4.ListDelegateBase;
import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.Utils;

public class LocDelegate extends ListDelegateBase 
    implements View.OnClickListener,
               OnItemSelectedListener {

    private ListActivity m_activity;
    private LocListAdapter m_adapter;
    private EditText m_searchField;
    private Spinner m_filterBy;
    private ImageButton m_searchButton;
    private LocSearcher m_searcher;
    private String m_curSearch;

    protected LocDelegate( ListActivity activity, Bundle savedInstanceState )
    {
        super( activity, savedInstanceState );
        m_activity = activity;
        init( savedInstanceState );
    }

    protected boolean onBackPressed()
    {
        LocUtils.saveLocalData( m_activity );
        return false;
    }

    @Override
    public void onClick( View view ) 
    {
        String newText = m_searchField.getText().toString();
        if ( null == m_curSearch || ! m_curSearch.equals( newText ) ) {
            m_curSearch = newText;
            m_searcher.start( newText ); // synchronous for now
            makeNewAdapter();
        }
    }

    private void makeNewAdapter()
    {
        ListView listview = getListView();
        m_adapter = new LocListAdapter( m_activity, listview, m_searcher );
        m_activity.setListAdapter( m_adapter );
    }

    protected void init( Bundle savedInstanceState ) 
    {
        setContentView( R.layout.loc_main );

        m_searchButton = (ImageButton)findViewById( R.id.loc_search_button );
        m_searchButton.setOnClickListener( this );

        m_searchField = (EditText)findViewById( R.id.loc_search_field );
        m_filterBy = (Spinner)findViewById( R.id.filters );
        m_filterBy.setOnItemSelectedListener( this );

        LocSearcher.Pair[] pairs = LocUtils.makePairs( m_activity );
        m_searcher = new LocSearcher( m_activity, pairs );

        makeNewAdapter();
    }

    //////////////////////////////////////////////////
    // AdapterView.OnItemSelectedListener interface
    //////////////////////////////////////////////////
    public void onItemSelected( AdapterView<?> parent, View view, 
                                int position, long id )
    {
        m_searcher.start( position );
        makeNewAdapter();
    }

    public void onNothingSelected( AdapterView<?> parent )
    {
    }
}
