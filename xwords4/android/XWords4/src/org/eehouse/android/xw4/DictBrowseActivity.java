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

import android.content.Intent;
import android.database.DataSetObserver;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListAdapter;
import android.widget.TextView;

import junit.framework.Assert;

import org.eehouse.android.xw4.jni.JNIUtilsImpl;
import org.eehouse.android.xw4.jni.XwJNI;

public class DictBrowseActivity extends XWListActivity {

    public static final String DICT_NAME = "DICT_NAME";

    private int m_dictClosure = 0;

    private class DictListAdapter extends XWListAdapter {

        public Object getItem( int position ) 
        {
            TextView text = new TextView( DictBrowseActivity.this );
            String str = XwJNI.dict_iter_nthWord( m_dictClosure, position );
            if ( null != str ) {
                str = String.format( "%d %s", position, str );
                text.setText( str );
            }
            return text;
        }

        public View getView( int position, View convertView, ViewGroup parent ) {
            return (View)getItem( position );
        }

        public int getCount() { 
            Assert.assertTrue( 0 != m_dictClosure );
            return XwJNI.dict_iter_wordCount( m_dictClosure );
        }

    }

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );

        Intent intent = getIntent();
        String name = null == intent? null:intent.getStringExtra( DICT_NAME );
        if ( null == name ) {
            finish();
        }
        String[] names = { name };
        DictUtils.DictPairs pairs = DictUtils.openDicts( this, names );
        m_dictClosure = XwJNI.dict_iter_init( pairs.m_bytes[0], pairs.m_paths[0],
                                              JNIUtilsImpl.get() );

        setContentView( R.layout.dict_browser );
        setListAdapter( new DictListAdapter() );
    }

    @Override
    protected void onDestroy()
    {
        XwJNI.dict_iter_destroy( m_dictClosure );
        m_dictClosure = 0;
        super.onDestroy();
    }


    // Just in case onDestroy didn't get called....
    @Override
    public void finalize()
    {
        XwJNI.dict_iter_destroy( m_dictClosure );
        try {
            super.finalize();
        } catch ( java.lang.Throwable err ){
            Utils.logf( "%s", err.toString() );
        }
    }

}
