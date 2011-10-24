/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
/*
 * Copyright 2009 - 2011 by Eric House (xwords@eehouse.org).  All
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

import android.database.DataSetObserver;
import android.os.Bundle;
import android.widget.ListAdapter;
import android.widget.TextView;
import android.view.View;
import android.view.ViewGroup;

public class DictBrowseActivity extends XWListActivity {

    public static final String DICT_NAME = "DICT_NAME";
    public static final String DICT_LOC = "DICT_LOC";

    private class DictListAdapter extends XWListAdapter {

        public DictListAdapter()
        {
            // 510-528-3769
            super( DictBrowseActivity.this, 1000 );
        }

        public Object getItem( int position ) 
        {
            String str = String.format( "Word %d", position );
            Utils.logf( "returning %s", str );
            TextView text = new TextView( DictBrowseActivity.this );
            text.setText( str );
            return text;
        }

        public View getView( int position, View convertView, ViewGroup parent ) {
            return (View)getItem( position );
        }

    }

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );

        setContentView( R.layout.dict_browser );
        setListAdapter( new DictListAdapter() );
    }

}
