/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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
import android.widget.ArrayAdapter;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListAdapter;
import android.widget.SectionIndexer;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import java.util.Arrays;

import junit.framework.Assert;

import org.eehouse.android.xw4.jni.JNIUtilsImpl;
import org.eehouse.android.xw4.jni.XwJNI;

public class DictBrowseActivity extends XWListActivity
    implements View.OnClickListener, OnItemSelectedListener {

    public static final String DICT_NAME = "DICT_NAME";
    public static final String DICT_MIN = "DICT_MIN";
    public static final String DICT_MAX = "DICT_MAX";
    public static final String DICT_COUNTS = "DICT_COUNTS";

    private static final int MIN_LEN = 2;
    private static final int FINISH_ACTION = 1;

    private int m_dictClosure = 0;
    private int m_lang;
    private String m_name;
    private Spinner m_minSpinner;
    private Spinner m_maxSpinner;
    private int m_minShown;
    private int m_maxShown;
    private int m_minAvail;
    private int m_maxAvail;
    private int[] m_counts;


// - Steps to reproduce the problem:
// Create ListView, set custom adapter which implements ListAdapter and
// SectionIndexer but do not extends BaseAdapter. Enable fast scroll in
// layout. This will effect in ClassCastException.


    private class DictListAdapter extends BaseAdapter
        implements SectionIndexer {

        private String[] m_prefixes;
        private int[] m_indices;
        private int m_nWords;

        public DictListAdapter()
        {
            super();

            XwJNI.dict_iter_setMinMax( m_dictClosure, m_minShown, m_maxShown );
            m_nWords = XwJNI.dict_iter_wordCount( m_dictClosure );

            int format = m_minShown == m_maxShown ?
                R.string.dict_browse_title1f : R.string.dict_browse_titlef;
            setTitle( Utils.format( DictBrowseActivity.this, format,
                                    m_name, m_nWords, m_minShown, m_maxShown ));
        }

        public Object getItem( int position ) 
        {
            TextView text =
                (TextView)Utils.inflate( DictBrowseActivity.this,
                                         android.R.layout.simple_list_item_1 );
            String str = XwJNI.dict_iter_nthWord( m_dictClosure, position );
            if ( null != str ) {
                text.setText( str );
                text.setOnClickListener( DictBrowseActivity.this );
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

            String[] names = { name };
            DictUtils.DictPairs pairs = DictUtils.openDicts( this, names );
            m_dictClosure = XwJNI.dict_iter_init( pairs.m_bytes[0], 
                                                  pairs.m_paths[0],
                                                  JNIUtilsImpl.get() );

            m_counts = intent.getIntArrayExtra( DICT_COUNTS );
            if ( null == m_counts ) {
                m_counts = XwJNI.dict_iter_getCounts( m_dictClosure );
            }
            if ( null == m_counts ) {
                // empty dict?  Just close down for now.  Later if
                // this is extended to include tile info -- it should
                // be -- then use an empty list elem and disable
                // search/minmax stuff.
                String msg = Utils.format( this, R.string.alert_empty_dictf,
                                           name );
                showOKOnlyDialogThen( msg, FINISH_ACTION );
            } else {
                figureMinMax();

                setContentView( R.layout.dict_browser );

                Button button = (Button)findViewById( R.id.search_button );
                button.setOnClickListener( new View.OnClickListener() {
                        public void onClick( View view )
                        {
                            findButtonClicked();
                        }
                    } );

                m_minShown = intent.getIntExtra( DICT_MIN, m_minAvail );
                m_maxShown = intent.getIntExtra( DICT_MAX, m_maxAvail );
                setUpSpinners();

                setListAdapter( new DictListAdapter() );
                getListView().setFastScrollEnabled( true );
            }
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
            DbgUtils.logf( "%s", err.toString() );
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


    //////////////////////////////////////////////////
    // AdapterView.OnItemSelectedListener interface
    //////////////////////////////////////////////////
    public void onItemSelected( AdapterView<?> parent, View view, 
                                int position, long id )
    {
        TextView text = (TextView)view;
        int newval = Integer.parseInt( text.getText().toString() );
        if ( parent == m_minSpinner ) {
            setMinMax( newval, m_maxShown );
        } else if ( parent == m_maxSpinner ) {
            setMinMax( m_minShown, newval );
        }
    }

    public void onNothingSelected( AdapterView<?> parent )
    {
    }

    //////////////////////////////////////////////////
    // DlgDelegate.DlgClickNotify interface
    //////////////////////////////////////////////////
    @Override
    public void dlgButtonClicked( int id, int which )
    {
        Assert.assertTrue( FINISH_ACTION == id ); 
        finish();
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
                DbgUtils.showf( this, R.string.dict_browse_nowordsf, 
                                m_name, text );
            }
        }
    }

    private void setMinMax( int min, int max )
    {
        // I can't make a second call to setListAdapter() work, nor
        // does notifyDataSetChanged do anything toward refreshing the
        // adapter/making it recognized a changed dataset.  So, as a
        // workaround, relaunch the activity with different
        // parameters.
        if ( m_minShown != min || m_maxShown != max ) {
            Intent intent = getIntent();
            intent.putExtra( DICT_MIN, min );
            intent.putExtra( DICT_MAX, max );
            intent.putExtra( DICT_COUNTS, m_counts );
            startActivity( intent );

            finish();
        }
    }

    private void figureMinMax()
    {
        Assert.assertTrue( m_counts.length == XwJNI.MAX_COLS_DICT + 1 );
        m_minAvail = 0;
        while ( 0 == m_counts[m_minAvail] ) {
            ++m_minAvail;
        }
        m_maxAvail = XwJNI.MAX_COLS_DICT;
        while ( 0 == m_counts[m_maxAvail] ) { // 
            --m_maxAvail;
        }
    }

    private void makeAdapter( Spinner spinner, int min, int max, int cur )
    {
        int sel = -1;
        String[] nums = new String[max - min + 1];
        for ( int ii = 0; ii < nums.length; ++ii ) {
            int val = min + ii;
            if ( val == cur ) {
                sel = ii;
            }
            nums[ii] = String.format( "%d", min + ii );
        }
        ArrayAdapter<String> adapter = new
            ArrayAdapter<String>( this, 
                                  //android.R.layout.simple_spinner_dropdown_item,
                                  android.R.layout.simple_spinner_item,
                                  nums );
        adapter.setDropDownViewResource( android.R.layout.
                                         simple_spinner_dropdown_item );
        spinner.setAdapter( adapter );
        spinner.setSelection( sel );
    }

    private void setUpSpinners()
    {
        // Min and max-length spinners.  To avoid empty lists,
        // don't allow min to exceed max.  Do that by making the
        // current max the largest min allowed, and the current
        // min the smallest max allowed.
        m_minSpinner = (Spinner)findViewById( R.id.wordlen_min );
        makeAdapter( m_minSpinner, m_minAvail, m_maxShown, m_minShown );
        m_minSpinner.setOnItemSelectedListener( this );

        m_maxSpinner = (Spinner)findViewById( R.id.wordlen_max );
        makeAdapter( m_maxSpinner, m_minShown, m_maxAvail, m_maxShown );
        m_maxSpinner.setOnItemSelectedListener( this );
    }


    public static void launch( Context caller, String name )
    {
        Intent intent = new Intent( caller, DictBrowseActivity.class );
        intent.putExtra( DICT_NAME, name );
        caller.startActivity( intent );
    }
}
