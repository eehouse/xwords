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

import android.content.Context;
import android.content.Intent;
import android.database.DataSetObserver;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListAdapter;
import android.widget.SectionIndexer;
import android.widget.TextView;
import java.util.Arrays;

import junit.framework.Assert;

import org.eehouse.android.xw4.jni.JNIUtilsImpl;
import org.eehouse.android.xw4.jni.XwJNI;

public class DictBrowseActivity extends XWListActivity
    implements View.OnClickListener {

    public static final String DICT_NAME = "DICT_NAME";

    private int m_dictClosure = 0;
    private int m_lang;
    private String m_name;
    private int m_nWords;
    private float m_textSize;

// - Steps to reproduce the problem:
// Create ListView, set custom adapter which implements ListAdapter and
// SectionIndexer but do not extends BaseAdapter. Enable fast scroll in
// layout. This will effect in ClassCastException.


    private class DictListAdapter extends BaseAdapter
        implements SectionIndexer {

        private String[] m_prefixes;
        private int[] m_indices;

        public Object getItem( int position ) 
        {
            TextView text = new TextView( DictBrowseActivity.this );
            String str = XwJNI.dict_iter_nthWord( m_dictClosure, position );
            if ( null != str ) {
                text.setText( str );
                text.setOnClickListener( DictBrowseActivity.this );
                text.setTextSize( m_textSize );
            }
            return text;
        }

        public View getView( int position, View convertView, ViewGroup parent ) {
            return (View)getItem( position );
        }

        public long getItemId( int position ) { return position; }

        public int getCount() { 
            Assert.assertTrue( 0 != m_dictClosure );
            return m_nWords;
        }

        // SectionIndexer
        public int getPositionForSection( int section )
        {
            return m_indices[section];
        }
        
        public int getSectionForPosition( int position )
        {
            int section = Arrays.binarySearch( m_indices, position );
            if ( section < 0 ) {
                section *= -1;
            }
            if ( section >= m_indices.length ) {
                section = m_indices.length - 1;
            }
            return section;
        }
        
        public Object[] getSections() 
        {
            m_prefixes = XwJNI.dict_iter_getPrefixes( m_dictClosure );
            m_indices = XwJNI.dict_iter_getIndices( m_dictClosure );
            return m_prefixes;
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
        } else {
            m_name = name;
            m_lang = DictLangCache.getDictLangCode( this, name );

            m_textSize = 2.0f + new TextView( this ).getTextSize();

            String[] names = { name };
            DictUtils.DictPairs pairs = DictUtils.openDicts( this, names );
            m_dictClosure = XwJNI.dict_iter_init( pairs.m_bytes[0], 
                                                  pairs.m_paths[0],
                                                  JNIUtilsImpl.get() );
            m_nWords = XwJNI.dict_iter_wordCount( m_dictClosure );

            setTitle( Utils.format( this, R.string.dict_browse_titlef,
                                    name, m_nWords ) );

            Utils.logf( "calling makeIndex" );
            XwJNI.dict_iter_makeIndex( m_dictClosure );
            Utils.logf( "makeIndex done" );

            setContentView( R.layout.dict_browser );
            setListAdapter( new DictListAdapter() );
            getListView().setFastScrollEnabled( true );

            Button button = (Button)findViewById( R.id.search_button );
            button.setOnClickListener( new View.OnClickListener() {
                    public void onClick( View view )
                    {
                        findButtonClicked();
                    }
                } );
        }
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

    //////////////////////////////////////////////////
    // View.OnClickListener interface
    //////////////////////////////////////////////////
    @Override
    public void onClick( View view )
    {
        TextView text = (TextView)view;
        String[] words = { text.getText().toString() };
        launchLookup( words, m_lang, true );
    }

    private void findButtonClicked()
    {
        EditText edit = (EditText)findViewById( R.id.word_edit );
        String text = edit.getText().toString();
        if ( null != text && 0 < text.length() ) {
            int pos = XwJNI.dict_iter_getStartsWith( m_dictClosure, text );
            if ( 0 <= pos ) {
                getListView().setSelection( pos );
            } else {
                Utils.showf( this, R.string.dict_browse_nowordsf, 
                             m_name, text );
            }
        }
    }

    public static void launch( Context caller, String name )
    {
        Intent intent = new Intent( caller, DictBrowseActivity.class );
        intent.putExtra( DICT_NAME, name );
        caller.startActivity( intent );
    }
}
