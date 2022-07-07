/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2020 by Eric House (xwords@eehouse.org).  All
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
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ListView;
import android.widget.SectionIndexer;
import android.widget.Spinner;
import android.widget.TableLayout;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.ExpandImageButton.ExpandChangeListener;
import org.eehouse.android.xw4.jni.DictInfo;
import org.eehouse.android.xw4.jni.JNIUtilsImpl;
import org.eehouse.android.xw4.jni.XwJNI.DictWrapper;
import org.eehouse.android.xw4.jni.XwJNI.IterWrapper;
import org.eehouse.android.xw4.jni.XwJNI.PatDesc;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;
import org.eehouse.android.xw4.Utils.ISOCode;

import java.util.Arrays;
import java.io.Serializable;

public class DictBrowseDelegate extends DelegateBase
    implements View.OnClickListener, View.OnLongClickListener,
               PatTableRow.EnterPressed {
    private static final String TAG = DictBrowseDelegate.class.getSimpleName();
    private static final String DELIM = ".";
    private static final boolean SHOW_NUM = false;
    private static final String[] FAQ_PARAMS = {"filters", "intro"};

    private static final String DICT_NAME = "DICT_NAME";
    private static final String DICT_LOC = "DICT_LOC";

    private static final int MIN_LEN = 2;
    private static final int MAX_LEN = 15;

    // Struct to show both what user's configuring AND what's been
    // successfully fed to create the current iterator. The config setting
    // become the filter params when the user presses the Apply Filter button
    // and corrects any tile problems.
    private static class DictBrowseState implements Serializable {
        public int m_chosenMin, m_chosenMax;
        public int m_passedMin, m_passedMax;
        public int m_pos;
        public int m_top;
        public PatDesc[] m_pats;
        public int[] m_counts;
        public boolean m_expanded;
        public String m_delim;

        public DictBrowseState()
        {
            m_chosenMin = MIN_LEN;
            m_chosenMax = MAX_LEN;
            m_pats = new PatDesc[3];
            for ( int ii = 0; ii < m_pats.length; ++ii ) {
                m_pats[ii] = new PatDesc();
            }
        }

        private void onFilterAccepted( DictWrapper dict, String delim )
        {
            m_passedMin = m_chosenMin;
            m_passedMax = m_chosenMax;

            for ( PatDesc desc : m_pats ) {
                String str = XwJNI.dict_tilesToStr( dict, desc.tilePat, delim );
                desc.strPat = str;
            }
        }

        @Override
        public String toString()
        {
            StringBuilder sb = new StringBuilder("{pats:[");
            for ( PatDesc pd : m_pats ) {
                sb.append(pd).append(",");
            }
            sb.append("],");
            sb.append( "passedMin:").append(m_passedMin).append(",")
                .append( "passedMax:").append(m_passedMax).append(",")
                .append( "chosenMin:").append(m_chosenMin).append(",")
                .append( "chosenMax:").append(m_chosenMax).append(",")
                ;
            sb.append("}");
            return sb.toString();
        }
    }

    private Activity m_activity;
    private ISOCode m_lang;
    private String m_name;
    private String mAboutStr;
    private DictUtils.DictLoc m_loc;
    private DictBrowseState m_browseState;
    private int m_minAvail;
    private int m_maxAvail;
    private ListView m_list;
    private IterWrapper m_diClosure;
    private DictWrapper m_dict;
    private DictInfo mDictInfo;
    private PatTableRow m_rows[] = { null, null, null };
    private Spinner m_spinnerMin;
    private Spinner m_spinnerMax;
    private boolean m_filterAlertShown;
    private Runnable mResetChecker;

    private class DictListAdapter extends BaseAdapter
        implements SectionIndexer {

        private String[] m_prefixes;
        private int[] m_indices;
        private int m_nWords;

        public DictListAdapter()
        {
            super();

            m_nWords = XwJNI.di_wordCount( m_diClosure );
            // Log.d( TAG, "making DictListAdapter; have %d words", m_nWords );
        }

        public Object getItem( int position )
        {
            TextView text = (TextView)
                inflate( android.R.layout.simple_list_item_1 );
            text.setOnClickListener( DictBrowseDelegate.this );
            text.setOnLongClickListener( DictBrowseDelegate.this );

            String str = XwJNI.di_nthWord( m_diClosure, position, m_browseState.m_delim );
            if ( null != str ) {
                if ( SHOW_NUM ) {
                    str = String.format( "%1$d %2$s", 1+position, str );
                }
            } else if ( SHOW_NUM ) {
                str = String.format( "%1$d <null>", 1+position );
            }
            if ( null != str ) {
                text.setText( str );
            }
            return text;
        }

        public View getView( int position, View convertView, ViewGroup parent ) {
            return (View)getItem( position );
        }

        public long getItemId( int position ) { return position; }

        public int getCount() {
            Assert.assertTrueNR( m_nWords == XwJNI.di_wordCount( m_diClosure ) );
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

        @Override
        public Object[] getSections()
        {
            m_prefixes = XwJNI.di_getPrefixes( m_diClosure );
            m_indices = XwJNI.di_getIndices( m_diClosure );
            return m_prefixes;
        }
    }

    protected DictBrowseDelegate( Delegator delegator, Bundle sis )
    {
        super( delegator, sis, R.layout.dict_browser,
               R.menu.dict_browse_menu );
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
            mAboutStr = getString( R.string.show_note_menu_fmt, name );
            m_name = name;
            m_loc =
                DictUtils.DictLoc.values()[args.getInt( DICT_LOC, 0 )];
            m_lang = DictLangCache.getDictISOCode( m_activity, name );

            findTableRows();
            m_spinnerMin = ((LabeledSpinner)findViewById( R.id.spinner_min ))
                .getSpinner();
            m_spinnerMax = ((LabeledSpinner)findViewById( R.id.spinner_max ))
                .getSpinner();

            loadBrowseState();

            String[] names = { m_name };
            DictUtils.DictPairs pairs = DictUtils.openDicts( m_activity, names );
            Assert.assertNotNull( m_browseState );
            m_dict = XwJNI.makeDict( pairs.m_bytes[0], m_name, pairs.m_paths[0] );

            mDictInfo = XwJNI.dict_getInfo( m_dict, false );
            setTitle( getString( R.string.dict_browse_title_fmt, m_name, mDictInfo.wordCount ) );

            ExpandImageButton eib = (ExpandImageButton)findViewById( R.id.expander );
            eib.setOnExpandChangedListener( new ExpandChangeListener() {
                    @Override
                    public void expandedChanged( boolean nowExpanded )
                    {
                        m_browseState.m_expanded = nowExpanded;
                        setShowConfig();
                        if ( !nowExpanded ) {
                            hideSoftKeyboard();
                        }
                    }
                } )
                .setExpanded( m_browseState.m_expanded );

            int[] ids = { R.id.button_useconfig, R.id.button_addBlank,
                          R.id.button_clear, };
            for ( int id : ids ) {
                findViewById( id ).setOnClickListener(this);
            }

            setShowConfig();
            replaceIter( true );
        }
    } // init

    @Override
    protected void onPause()
    {
        scrapeBrowseState();
        storeBrowseState();
        enableResetChecker( false );
        super.onPause();
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        loadBrowseState();
        setFindPats( m_browseState.m_pats );
    }

    @Override
    protected Dialog makeDialog( DBAlert alert, Object[] params )
    {
        Dialog dialog = null;
        DlgID dlgID = alert.getDlgID();
        switch ( dlgID ) {
        case CHOOSE_TILES:
            final byte[][] choices = (byte[][])params[0];
            final int indx = (Integer)params[1];
            final String[] strs = new String[choices.length];
            for ( int ii = 0; ii < choices.length; ++ii ) {
                strs[ii] = XwJNI.dict_tilesToStr( m_dict, choices[ii], DELIM );
            }
            String title = getString( R.string.pick_tiles_title_fmt,
                                      m_rows[indx].getFieldName() );
            final int[] chosen = {0};
            dialog = makeAlertBuilder()
                .setSingleChoiceItems( strs, chosen[0], new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick( DialogInterface dialog, int which )
                        {
                            chosen[0] = which;
                        }
                    } )
                .setPositiveButton( android.R.string.ok,
                                    new DialogInterface.OnClickListener() {
                                        @Override
                                        public void onClick( DialogInterface dialog, int which )
                                        {
                                            if ( 0 <= chosen[0] ) {
                                                int sel = chosen[0];
                                                useButtonClicked( indx, choices[sel] );
                                            }
                                        }
                                    } )
                .setTitle( title )
                .create();
            break;
        case SHOW_TILES:
            String info = (String)params[0];
            View tilesView = inflate( R.layout.tiles_table );
            addTileRows( tilesView, info );

            String langName = DictLangCache.getLangNameForISOCode( m_activity, m_lang );
            title = getString( R.string.show_tiles_title_fmt, langName );
            dialog = makeAlertBuilder()
                .setView( tilesView )
                .setPositiveButton( android.R.string.ok, null )
                .setTitle( title )
                .create();
            break;
        default:
            dialog = super.makeDialog( alert, params );
            break;
        }
        return dialog;
    }

    @Override
    public boolean onPrepareOptionsMenu( Menu menu )
    {
        Utils.setItemVisible( menu, R.id.dicts_shownote,
                              null != getDesc() );
        MenuItem item = menu.findItem( R.id.dicts_shownote );
        item.setTitle( mAboutStr );
        return true;
    }

    @Override
    public boolean onOptionsItemSelected( MenuItem item )
    {
        boolean handled = true;

        switch ( item.getItemId() ) {
        case R.id.dicts_showtiles:
            showTiles();
            break;
        case R.id.dicts_showfaq:
            showFaq( FAQ_PARAMS );
            break;
        case R.id.dicts_shownote:
            makeOkOnlyBuilder( getDesc() )
                .setTitle( mAboutStr )
                .show();
            break;
        default:
            handled = false;
        }
        return handled;
    }

    //////////////////////////////////////////////////
    // View.OnLongClickListener interface
    //////////////////////////////////////////////////
    @Override
    public boolean onLongClick( View view )
    {
        boolean success = view instanceof TextView;
        if ( success ) {
            TextView text = (TextView)view;
            String word = text.getText().toString();
            Utils.stringToClip( m_activity, word );

            String msg = LocUtils
                .getString( m_activity, R.string.word_to_clip_fmt, word );
            showToast( msg );
        }
        return success;
    }

    //////////////////////////////////////////////////
    // View.OnClickListener interface
    //////////////////////////////////////////////////
    @Override
    public void onClick( View view )
    {
        switch ( view.getId() ) {
        case R.id.button_useconfig:
            useButtonClicked();
            break;
        case R.id.button_addBlank:
            addBlankButtonClicked();
            break;
        case R.id.button_clear:
            resetClicked();
            break;
        default:
            if ( view instanceof TextView ) {
                TextView text = (TextView)view;
                String[] words = { text.getText().toString() };
                launchLookup( words, m_lang, true );
            } else {
                Assert.failDbg();
            }
            break;
        }
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
        case SHOW_TILES:
            showTiles();
            break;
        default:
            handled = super.onPosButton( action, params );
            break;
        }
        return handled;
    }

    //////////////////////////////////////////////////
    // PatTableRow.EnterPressed
    //////////////////////////////////////////////////
    @Override
    public boolean enterPressed()
    {
        useButtonClicked();
        return true;
    }

    private void scrapeBrowseState()
    {
        Assert.assertTrueNR( null != m_browseState );
        m_browseState.m_chosenMin = MIN_LEN + m_spinnerMin.getSelectedItemPosition();
        m_browseState.m_chosenMax = MIN_LEN + m_spinnerMax.getSelectedItemPosition();
        if ( null != m_list ) { // there are words? (don't NPE on empty dict)
            m_browseState.m_pos = m_list.getFirstVisiblePosition();
            View view = m_list.getChildAt( 0 );
            m_browseState.m_top = (view == null) ? 0 : view.getTop();
        }

        // Get the strings (not bytes) from the rows
        for ( int ii = 0; ii < m_rows.length; ++ii ) {
            m_rows[ii].getToDesc(m_browseState.m_pats[ii]);
            // .updateFrom( desc );
        }
    }

    private static final int[] sTileRowIDs = {R.id.face, R.id.count, R.id.value };
    private void addTileRows( View view, String info )
    {
        ViewGroup table = view.findViewById( R.id.table );
        if ( null != table ) {
            String[] tiles = TextUtils.split( info, "\n" );
            for ( String row : tiles ) {
                String[] fields = TextUtils.split( row, "\t" );
                if ( 3 == fields.length ) {
                    ViewGroup rowView = (ViewGroup)inflate( R.layout.tiles_row );
                    for ( int ii = 0; ii < sTileRowIDs.length; ++ii ) {
                        TextView tv = (TextView)rowView.findViewById( sTileRowIDs[ii] );
                        tv.setText( fields[ii] );
                    }
                    table.addView( rowView );
                }
            }
        }
    }

    private void showTiles()
    {
        String info = XwJNI.getTilesInfo( m_dict );
        showDialogFragment( DlgID.SHOW_TILES, info );
    }

    private String m_stateKey = null;
    private String getStateKey()
    {
        if ( null == m_stateKey ) {
            m_stateKey = String.format( "KEY_%s_%d", m_name, m_loc.ordinal() );
        }
        return m_stateKey;
    }

    // We'll enable the button as soon as any row gets focus, since once one
    // of them has focus one always will.
    private boolean mBlankButtonEnabled = false;
    private Runnable mFocusGainedProc = new Runnable() {
            @Override
            public void run() {
                if ( !mBlankButtonEnabled ) {
                    mBlankButtonEnabled = true;
                    findViewById(R.id.button_addBlank)
                        .setEnabled( true );
                }
            }
        };

    private void findTableRows()
    {
        TableLayout table = (TableLayout)findViewById( R.id.table );
        int count = table.getChildCount();
        int nFound = 0;
        for ( int ii = 0; ii < count && nFound < m_rows.length; ++ii ) {
            View child = table.getChildAt( ii );
            if ( child instanceof PatTableRow ) {
                PatTableRow row = (PatTableRow)child;
                m_rows[nFound++] = row;
                row.setOnFocusGained( mFocusGainedProc );
                row.setOnEnterPressed(this);
            }
        }
        Assert.assertTrueNR( nFound == m_rows.length );
    }

    private void loadBrowseState()
    {
        boolean newState = false;
        if ( null == m_browseState ) {
            Serializable obj = DBUtils.getSerializableFor( m_activity, getStateKey() );
            if ( null != obj && obj instanceof DictBrowseState ) {
                m_browseState = (DictBrowseState)obj;
                if ( null == m_browseState.m_pats ) { // remove if unneeded
                    m_browseState = null;
                    Assert.failDbg();
                }
            }
            if ( null == m_browseState ) {
                m_browseState = new DictBrowseState();
            }
        }
        // Log.d( TAG, "loadBrowseState() => %s", m_browseState );
    }

    private void storeBrowseState()
    {
        if ( null != m_browseState ) {
            DBUtils.setSerializableFor( m_activity, getStateKey(), m_browseState );
        }
    }

    private void useButtonClicked() { useButtonClicked( -1, null ); }

    private void useButtonClicked( int justFixed, byte[] fixedTiles )
    {
        if ( -1 == justFixed ) {
            // Hungarian fix: when we're called via button, clear state so we
            // can know later when we have a tile pattern that it came from
            // the user making a choice and we needn't offer it
            // again. Otherwise if more than one of the lines is ambiguous
            // (results in CHOOSE_TILES call) we loop forever.
            scrapeBrowseState();
            for ( PatDesc desc : m_browseState.m_pats ) {
                desc.tilePat = null;
            }
        }

        boolean pending = false;
        if ( m_browseState.m_chosenMin > m_browseState.m_chosenMax ) {
            pending = true;
            makeOkOnlyBuilder( R.string.error_min_gt_max ).show();
        }

        PatDesc[] pats = m_browseState.m_pats;
        for ( int ii = 0; ii < pats.length && !pending; ++ii ) {
            final PatDesc thisPats = pats[ii];
            if ( justFixed == ii ) {
                Assert.assertTrueNR( null != fixedTiles );
                thisPats.tilePat = fixedTiles;
            } else if ( null == thisPats.tilePat ) {
                String strPat = thisPats.strPat;
                if ( null != strPat && 0 < strPat.length() ) {
                    byte[][] choices = XwJNI.dict_strToTiles( m_dict, strPat );
                    if ( null == choices || 0 == choices.length ) {
                        String langName = DictLangCache.getLangNameForISOCode( m_activity, m_lang );
                        String msg = getString( R.string.no_tiles_exist, strPat, langName );
                        makeOkOnlyBuilder( msg )
                            .setActionPair( Action.SHOW_TILES, R.string.show_tiles_button )
                            .show();
                        pending = true;
                    } else if ( 1 == choices.length
                                || !XwJNI.dict_hasDuplicates( m_dict ) ) {
                        // Pick the shortest option, i.e. when there's a
                        // choice between using one or several tiles to spell
                        // something choose one.
                        thisPats.tilePat = choices[0];
                        for ( int jj = 1; jj < choices.length; ++jj ) {
                            byte[] tilePat = choices[jj];
                            if ( tilePat.length < thisPats.tilePat.length ) {
                                thisPats.tilePat = tilePat;
                            }
                        }
                    } else {
                        m_browseState.m_delim = DELIM;
                        showDialogFragment( DlgID.CHOOSE_TILES, (Object)choices, ii );
                        pending = true;
                    }
                }
            }
        }

        if ( !pending ) {
            storeBrowseState();
            replaceIter( false );
            hideSoftKeyboard();
        }
    }

    private void addBlankButtonClicked()
    {
        boolean handled = false;
        for ( PatTableRow row : m_rows ) {
            handled = handled || row.addBlankToFocussed( "_" );
        }
    }

    private void resetClicked()
    {
        m_browseState = new DictBrowseState();
        storeBrowseState();
        loadBrowseState();
        setFindPats( m_browseState.m_pats );
    }

    private void setShowConfig()
    {
        boolean expanded = m_browseState.m_expanded;
        findViewById(R.id.config).setVisibility( expanded ? View.VISIBLE : View.GONE );
        enableResetChecker( expanded );
    }

    private void setFindPats( PatDesc[] descs )
    {
        if ( null != descs && descs.length == m_rows.length ) {
            for ( int ii = 0; ii < m_rows.length; ++ii ) {
                m_rows[ii].setFromDesc( descs[ii] );
            }
        }
        setUpSpinners();
    }

    private String formatPats( PatDesc[] pats, String delim )
    {
        Assert.assertTrueNR( null != m_diClosure );
        List<String> strs = new ArrayList<>();
        for ( int ii = 0; ii < pats.length; ++ii ) {
            PatDesc desc = pats[ii];
            String str = desc.strPat;
            if ( null == str && (ii == 0 || ii == pats.length - 1) ) {
                str = "";
            }
            if ( null != str ) {
                strs.add(str);
            }
        }
        String result = TextUtils.join( "…", strs );
        // Log.d( TAG, "formatPats() => %s", result );
        return result;
    }
    
    private String[] m_nums;
    private void makeSpinnerAdapter( Spinner spinner, int curVal )
    {
        ArrayAdapter<String> adapter = new
            ArrayAdapter<String>( m_activity,
                                  android.R.layout.simple_spinner_item,
                                  m_nums );
        adapter.setDropDownViewResource( android.R.layout.
                                         simple_spinner_dropdown_item );
        spinner.setAdapter( adapter );
        spinner.setSelection( curVal - MIN_LEN );
    }

    private void setUpSpinners()
    {
        if ( null == m_nums ) {
            m_nums = new String[MAX_LEN - MIN_LEN + 1];
            for ( int ii = MIN_LEN; ii <= MAX_LEN; ++ii ) {
                m_nums[ii - MIN_LEN] = String.format( "%d", ii );
            }
        }

        makeSpinnerAdapter( m_spinnerMin, m_browseState.m_chosenMin );
        makeSpinnerAdapter( m_spinnerMax, m_browseState.m_chosenMax );
    }

    private String[] mDescWrap = null;
    private String getDesc()
    {
        if ( null == mDescWrap ) {
            String desc = XwJNI.dict_getDesc( m_dict );
            if ( BuildConfig.NON_RELEASE ) {
                String[] sums = DictLangCache.getDictMD5Sums( m_activity, m_name );
                if ( null != desc ) {
                    desc += "\n\n";
                } else {
                    desc = "";
                }
                desc += "md5s: " + sums[0] + "\n" + sums[1];
            }
            mDescWrap = new String[] { desc };
        }
        return mDescWrap[0];
    }

    private FrameLayout removeList()
    {
        m_list = null;
        FrameLayout parent = (FrameLayout)findViewById(R.id.list_container);
        parent.removeAllViews();

        return parent;
    }

    private void replaceIter( boolean useOldVals )
    {
        Assert.assertNotNull( m_browseState );
        Assert.assertNotNull( m_dict );
        int min = useOldVals ? m_browseState.m_passedMin : m_browseState.m_chosenMin;
        int max = useOldVals ? m_browseState.m_passedMax : m_browseState.m_chosenMax;

        String title = getString( R.string.filter_title_fmt, m_name );
        String msg = getString( R.string.filter_progress_fmt, mDictInfo.wordCount );
        startProgress( title, msg );

        XwJNI.di_init( m_dict, m_browseState.m_pats, min, max,
                       new XwJNI.DictIterProcs() {
                           @Override
                           public void onIterReady( final IterWrapper wrapper )
                           {
                               runOnUiThread( new Runnable() {
                                       @Override
                                       public void run() {
                                           stopProgress();

                                           if ( null != wrapper ) {
                                               m_browseState.onFilterAccepted( m_dict, null );
                                               initList( wrapper );
                                               setFindPats( m_browseState.m_pats );
                                           } else {
                                               makeOkOnlyBuilder(R.string.alrt_bad_filter )
                                                   .show();
                                           }
                                           newFeatureAlert();
                                       }
                                   } );
                           }
                       } );
    }

    private void newFeatureAlert()
    {
        if ( ! m_filterAlertShown ) {
            m_filterAlertShown = true;
            makeNotAgainBuilder( R.string.new_feature_filter, R.string.key_na_newFeatureFilter )
                .setActionPair( Action.SHOW_FAQ, R.string.button_faq )
                .setParams( (Object)FAQ_PARAMS )
                .show();
        }
    }
    
    private void initList( IterWrapper newIter )
    {
        FrameLayout parent = removeList();

        m_list = (ListView)inflate( R.layout.dict_browser_list );

        Assert.assertNotNull( m_browseState );
        Assert.assertNotNull( m_dict );
        m_diClosure = newIter;
        
        DictListAdapter dla = new DictListAdapter();

        m_list.setAdapter( dla );
        m_list.setFastScrollEnabled( true );
        m_list.setSelectionFromTop( m_browseState.m_pos, m_browseState.m_top );

        parent.addView( m_list );

        updateFilterString();
    }

    private void updateFilterString()
    {
        PatDesc[] pats = m_browseState.m_pats;
        Assert.assertNotNull( pats );
        String summary;
        String pat = formatPats( pats, null );
        int nWords = XwJNI.di_wordCount( m_diClosure );
        int[] minMax = XwJNI.di_getMinMax( m_diClosure );
        summary = getString( R.string.filter_sum_pat_fmt, pat,
                             minMax[0], minMax[1],
                             nWords );
        TextView tv = (TextView)findViewById( R.id.filter_summary );
        tv.setText( summary );
    }

    private void hideSoftKeyboard()
    {
        View hasFocus = m_activity.getCurrentFocus();
        if ( null != hasFocus ) {
            InputMethodManager imm = (InputMethodManager)
                m_activity.getSystemService( Activity.INPUT_METHOD_SERVICE );
            imm.hideSoftInputFromWindow( hasFocus.getWindowToken(), 0 );
        }
    }

    final private static int sResetCheckMS = 500;
    private void enableResetChecker( boolean enable )
    {
        DbgUtils.assertOnUIThread();
        if ( !enable ) {
            mResetChecker = null;
        } else if ( null == mResetChecker ) {
            final Handler handler = new Handler();
            final Button resetButton = (Button)findViewById(R.id.button_clear);
            mResetChecker = new Runnable() {
                    @Override
                    public void run() {
                        if ( null != mResetChecker ) {
                            int curMin = MIN_LEN + m_spinnerMin.getSelectedItemPosition();
                            int curMax = MIN_LEN + m_spinnerMax.getSelectedItemPosition();
                            boolean hasState = curMin != MIN_LEN || curMax != MAX_LEN;
                            for ( int ii = 0; !hasState && ii < m_rows.length; ++ii ) {
                                hasState = m_rows[ii].hasState();
                            }
                            resetButton.setEnabled( hasState );

                            handler.postDelayed( mResetChecker, sResetCheckMS );
                        }
                    }
                };
            handler.postDelayed( mResetChecker, sResetCheckMS );
        }
    }

    private static void launch( Delegator delegator, Bundle bundle )
    {
        delegator.addFragment( DictBrowseFrag.newInstance( delegator ),
                               bundle );
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
