/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ListView;
import android.widget.SectionIndexer;
import android.widget.Spinner;
import android.widget.TextView;


import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.jni.JNIUtilsImpl;
import org.eehouse.android.xw4.jni.XwJNI;

import java.util.Arrays;

public class DictBrowseDelegate extends DelegateBase
    implements View.OnClickListener, OnItemSelectedListener {
    private static final String TAG = DictBrowseDelegate.class.getSimpleName();

    private static final String DICT_NAME = "DICT_NAME";
    private static final String DICT_LOC = "DICT_LOC";

    private static final int MIN_LEN = 2;

    private Activity m_activity;
    private long m_dictClosure = 0L;
    private int m_lang;
    private String m_name;
    private DictUtils.DictLoc m_loc;
    private DBUtils.DictBrowseState m_browseState;
    private int m_minAvail;
    private int m_maxAvail;
    private ListView m_list;


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

            XwJNI.dict_iter_setMinMax( m_dictClosure, m_browseState.m_minShown,
                                       m_browseState.m_maxShown );
            m_nWords = XwJNI.dict_iter_wordCount( m_dictClosure );

            int format = m_browseState.m_minShown == m_browseState.m_maxShown ?
                R.string.dict_browse_title1_fmt : R.string.dict_browse_title_fmt;
            setTitle( getString( format, m_name, m_nWords,
                                 m_browseState.m_minShown,
                                 m_browseState.m_maxShown ));

            String desc = XwJNI.dict_iter_getDesc( m_dictClosure );
            if ( null != desc ) {
                TextView view = (TextView)findViewById( R.id.desc );
                Assert.assertNotNull( view );
                view.setVisibility( View.VISIBLE );
                view.setText( desc );
            }
        }

        public Object getItem( int position )
        {
            TextView text = (TextView)
                inflate( android.R.layout.simple_list_item_1 );
            String str = XwJNI.dict_iter_nthWord( m_dictClosure, position );
            if ( null != str ) {
                text.setText( str );
                text.setOnClickListener( DictBrowseDelegate.this );
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
            if ( section >= m_indices.length ) {
                section = m_indices.length - 1;
            }
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

    protected DictBrowseDelegate( Delegator delegator, Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState, R.layout.dict_browser );
        m_activity = delegator.getActivity();
    }

    @Override
    protected void init( Bundle savedInstanceState )
    {
        Bundle args = getArguments();
        String name = null == args? null : args.getString( DICT_NAME );
        Assert.assertNotNull( name );
        if ( null == name ) {
            finish();
        } else {
            m_name = name;
            m_loc =
                DictUtils.DictLoc.values()[args.getInt( DICT_LOC, 0 )];
            m_lang = DictLangCache.getDictLangCode( m_activity, name );

            String[] names = { name };
            DictUtils.DictPairs pairs = DictUtils.openDicts( m_activity, names );
            m_dictClosure = XwJNI.dict_iter_init( pairs.m_bytes[0],
                                                  name, pairs.m_paths[0] );

            m_browseState = DBUtils.dictsGetOffset( m_activity, name, m_loc );
            boolean newState = null == m_browseState;
            if ( newState ) {
                m_browseState = new DBUtils.DictBrowseState();
                m_browseState.m_pos = 0;
                m_browseState.m_top = 0;
            }
            if ( null == m_browseState.m_counts ) {
                m_browseState.m_counts =
                    XwJNI.dict_iter_getCounts( m_dictClosure );
            }

            if ( null == m_browseState.m_counts ) {
                // empty dict?  Just close down for now.  Later if
                // this is extended to include tile info -- it should
                // be -- then use an empty list elem and disable
                // search/minmax stuff.
                String msg = getString( R.string.alert_empty_dict_fmt, name );
                makeOkOnlyBuilder(msg).setAction(Action.FINISH_ACTION).show();
            } else {
                figureMinMax( m_browseState.m_counts );
                if ( newState ) {
                    m_browseState.m_minShown = m_minAvail;
                    m_browseState.m_maxShown = m_maxAvail;
                }

                Button button = (Button)findViewById( R.id.search_button );
                button.setOnClickListener( new View.OnClickListener() {
                        public void onClick( View view )
                        {
                            findButtonClicked();
                        }
                    } );

                setUpSpinners();

                initList();
            }
        }
    } // init

    protected void onPause()
    {
        if ( null != m_browseState ) { // already saved?
            m_browseState.m_pos = m_list.getFirstVisiblePosition();
            View view = m_list.getChildAt( 0 );
            m_browseState.m_top = (view == null) ? 0 : view.getTop();
            m_browseState.m_prefix = getFindText();
            DBUtils.dictsSetOffset( m_activity, m_name, m_loc, m_browseState );
            m_browseState = null;
        }
        super.onPause();
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        if ( null == m_browseState ) {
            m_browseState = DBUtils.dictsGetOffset( m_activity, m_name, m_loc );
        }
        setFindText( m_browseState.m_prefix );
    }


    protected void onDestroy()
    {
        XwJNI.dict_iter_destroy( m_dictClosure );
        m_dictClosure = 0;
    }

    // Just in case onDestroy didn't get called....
    @Override
    public void finalize()
    {
        XwJNI.dict_iter_destroy( m_dictClosure );
        try {
            super.finalize();
        } catch ( java.lang.Throwable err ){
            Log.i( TAG, "%s", err.toString() );
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
        // null text seems to have generated at least one google play report
        if ( null != text && null != m_browseState ) {
            int newval = Integer.parseInt( text.getText().toString() );
            switch ( parent.getId() ) {
            case R.id.wordlen_min:
                if ( newval != m_browseState.m_minShown ) {
                    setMinMax( newval, m_browseState.m_maxShown );
                }
                break;
            case R.id.wordlen_max:
                if ( newval != m_browseState.m_maxShown ) {
                    setMinMax( m_browseState.m_minShown, newval );
                }
                break;
            }
        }
    }

    public void onNothingSelected( AdapterView<?> parent )
    {
    }

    //////////////////////////////////////////////////
    // DlgDelegate.DlgClickNotify interface
    //////////////////////////////////////////////////
    @Override
    public boolean onPosButton( Action action, Object[] params )
    {
        boolean handled = false;
        switch( action ) {
        case FINISH_ACTION:
            handled = true;
            finish();
            break;
        default:
            handled = super.onPosButton( action, params );
            break;
        }
        return handled;
    }

    private void findButtonClicked()
    {
        String text = getFindText();
        if ( null != text && 0 < text.length() ) {
            m_browseState.m_prefix = text;
            showPrefix();
        }
    }

    private String getFindText()
    {
        EditText edit = (EditText)findViewById( R.id.word_edit );
        return edit.getText().toString();
    }

    private void setFindText( String text )
    {
        EditText edit = (EditText)findViewById( R.id.word_edit );
        edit.setText( text );
    }

    private void showPrefix()
    {
        String text = m_browseState.m_prefix;
        if ( null != text && 0 < text.length() ) {
            int pos = XwJNI.dict_iter_getStartsWith( m_dictClosure, text );
            if ( 0 <= pos ) {
                m_list.setSelection( pos );
            } else {
                DbgUtils.showf( m_activity, R.string.dict_browse_nowords_fmt,
                                m_name, text );
            }
        }
    }

    private void setMinMax( int min, int max )
    {
        // I can't make a second call to setListAdapter() work, nor does
        // notifyDataSetChanged do anything toward refreshing the
        // adapter/making it recognize a changed dataset.  So, as a
        // workaround, relaunch the activity with different parameters.
        if ( m_browseState.m_minShown != min ||
             m_browseState.m_maxShown != max ) {

            m_browseState.m_pos = 0;
            m_browseState.m_top = 0;
            m_browseState.m_minShown = min;
            m_browseState.m_maxShown = max;
            m_browseState.m_prefix = getFindText();
            DBUtils.dictsSetOffset( m_activity, m_name, m_loc, m_browseState );

            setUpSpinners();

            initList();
        }
    }

    private void figureMinMax( int[] counts )
    {
        Assert.assertTrue( counts.length == XwJNI.MAX_COLS_DICT + 1 );
        m_minAvail = 0;
        while ( 0 == counts[m_minAvail] ) {
            ++m_minAvail;
        }
        m_maxAvail = XwJNI.MAX_COLS_DICT;
        while ( 0 == counts[m_maxAvail] ) { //
            --m_maxAvail;
        }
    }

    private void makeSpinnerAdapter( int resID, int min, int max, int cur )
    {
        Spinner spinner = (Spinner)findViewById( resID );
        Assert.assertTrue( min <= max );

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
            ArrayAdapter<String>( m_activity,
                                  //android.R.layout.simple_spinner_dropdown_item,
                                  android.R.layout.simple_spinner_item,
                                  nums );
        adapter.setDropDownViewResource( android.R.layout.
                                         simple_spinner_dropdown_item );
        spinner.setAdapter( adapter );
        spinner.setSelection( sel );
        spinner.setOnItemSelectedListener( this );
    }

    private void setUpSpinners()
    {
        // Min and max-length spinners.  To avoid empty lists,
        // don't allow min to exceed max.  Do that by making the
        // current max the largest min allowed, and the current
        // min the smallest max allowed.
        makeSpinnerAdapter( R.id.wordlen_min, m_minAvail,
                            m_browseState.m_maxShown, m_browseState.m_minShown );
        makeSpinnerAdapter( R.id.wordlen_max, m_browseState.m_minShown,
                            m_maxAvail, m_browseState.m_maxShown );
    }

    private void initList()
    {
        FrameLayout parent = (FrameLayout)findViewById(R.id.list_container);
        parent.removeAllViews();

        m_list = (ListView)inflate( R.layout.dict_browser_list );
        m_list.setAdapter( new DictListAdapter() );
        m_list.setFastScrollEnabled( true );
        m_list.setSelectionFromTop( m_browseState.m_pos, m_browseState.m_top );

        parent.addView( m_list );
    }

    private static void launch( Delegator delegator, Bundle bundle )
    {
        if ( delegator.inDPMode() ) {
            delegator.addFragment( DictBrowseFrag.newInstance( delegator ),
                                   bundle );
        } else {
            Activity activity = delegator.getActivity();
            Intent intent = new Intent( activity, DictBrowseActivity.class );
            intent.putExtras( bundle );
            activity.startActivity( intent );
        }
    }

    public static void launch( Delegator delegator, String name,
                               DictUtils.DictLoc loc )
    {
        Bundle bundle = new Bundle();
        bundle.putString( DICT_NAME, name );
        bundle.putInt( DICT_LOC, loc.ordinal() );
        launch( delegator, bundle );
    }

    public static void launch( Delegator delegator, String name )
    {
        DictUtils.DictLoc loc
            = DictUtils.getDictLoc( delegator.getActivity(), name );
        if ( null == loc ) {
            Log.w( TAG, "launch(): DictLoc null; try again?" );
        } else {
            launch( delegator, name, loc );
        }
    }
}
