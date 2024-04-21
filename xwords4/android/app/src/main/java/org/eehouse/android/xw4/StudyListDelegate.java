/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.ClipboardManager;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.Spinner;


import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.Utils.ISOCode;
import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.loc.LocUtils;

import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

public class StudyListDelegate extends ListDelegateBase
    implements OnItemSelectedListener, SelectableItem,
               View.OnLongClickListener, View.OnClickListener,
               DBUtils.StudyListListener {
    private static final String TAG = StudyListDelegate.class.getSimpleName();

    protected static final String START_LANG = "START_LANG";

    private static final String CHECKED_KEY = "CHECKED_KEY";

    private Activity m_activity;
    private Spinner m_spinner;
    private LabeledSpinner m_pickView;
    private ISOCode[] m_langCodes;
    private String[] m_words;
    private Set<String> m_checkeds;
    private int m_langPosition;
    private SLWordsAdapter m_adapter;
    private String m_origTitle;

    protected StudyListDelegate( Delegator delegator, Bundle sis )
    {
        super( delegator, sis, R.layout.studylist, R.menu.studylist );
        m_activity = delegator.getActivity();
    }

    @Override
    protected void init( Bundle sis )
    {
        m_pickView = (LabeledSpinner)findViewById( R.id.pick_lang );
        m_spinner = m_pickView.getSpinner();
        m_checkeds = new HashSet<>();
        m_words = new String[0];

        getBundledData( sis );

        initOrFinish( getArguments() );
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        DBUtils.addStudyListChangedListener( this );
    }

    @Override
    protected void onPause()
    {
        DBUtils.removeStudyListChangedListener( this );
        super.onPause();
    }

    @Override
    protected boolean handleBackPressed()
    {
        boolean handled = 0 < m_checkeds.size();
        if ( handled ) {
            clearSels();
        }
        return handled;
    }

    @Override
    public boolean onPrepareOptionsMenu( Menu menu )
    {
        int nSel = m_checkeds.size();
        Utils.setItemVisible( menu, R.id.slmenu_copy_sel, 0 < nSel );
        Utils.setItemVisible( menu, R.id.slmenu_clear_sel, 0 < nSel );
        Utils.setItemVisible( menu, R.id.slmenu_select_all,
                              m_words.length > nSel );
        Utils.setItemVisible( menu, R.id.slmenu_deselect_all, 0 < nSel );
        boolean enable = 1 == nSel;
        if ( enable ) {
            String title = getString( R.string.button_lookup_fmt,
                                      getSelWords()[0] );
            menu.findItem( R.id.slmenu_lookup_sel ).setTitle( title );
        }
        Utils.setItemVisible( menu, R.id.slmenu_lookup_sel, enable );
        return true;
    }

    @Override
    public boolean onOptionsItemSelected( MenuItem item )
    {
        boolean handled = true;
        switch ( item.getItemId() ) {
        case R.id.slmenu_copy_sel:
            makeNotAgainBuilder( R.string.key_na_studycopy,
                                 Action.SL_COPY_ACTION,
                                 R.string.not_again_studycopy )
                .show();
            break;
        case R.id.slmenu_clear_sel:
            String msg = getQuantityString( R.plurals.confirm_studylist_clear_fmt,
                                            m_checkeds.size(), m_checkeds.size() );
            makeConfirmThenBuilder( Action.SL_CLEAR_ACTION, msg ).show();
            break;

        case R.id.slmenu_select_all:
            for ( String word : m_words ) {
                m_checkeds.add( word );
            }
            makeAdapter();
            setTitleBar();
            break;
        case R.id.slmenu_deselect_all:
            clearSels();
            break;
        case R.id.slmenu_lookup_sel:
            String[] oneWord = new String[]{ getSelWords()[0] };
            launchLookup( oneWord, m_langCodes[m_langPosition], true );
            break;
        default:
            handled = false;
        }
        return handled;
    }

    @Override
    protected void onSaveInstanceState( Bundle outState )
    {
        outState.putSerializable( CHECKED_KEY, (HashSet)m_checkeds );
    }

    private void getBundledData( Bundle sis )
    {
        if ( null != sis ) {
            m_checkeds = (HashSet)sis.getSerializable( CHECKED_KEY );
        }
    }

    //////////////////////////////////////////////////
    // DBUtils.StudyListListener
    //////////////////////////////////////////////////
    @Override
    public void onWordAdded( String word, ISOCode isoCode )
    {
        if ( isoCode.equals( m_langCodes[m_langPosition] ) ) {
            loadList();
        }
    }

    //////////////////////////////////////////////////
    // DlgDelegate.DlgClickNotify interface
    //////////////////////////////////////////////////
    @Override
    public boolean onPosButton( Action action, Object... params )
    {
        Assert.assertVarargsNotNullNR(params);
        boolean handled = true;
        switch ( action ) {
        case SL_CLEAR_ACTION:
            String[] selWords = getSelWords();
            if ( selWords.length == m_words.length ) {
                selWords = null; // all: easier on DB :-)
            }
            DBUtils.studyListClear( m_activity, m_langCodes[m_langPosition],
                                    selWords );
            initOrFinish( null );
            break;
        case SL_COPY_ACTION:
            selWords = getSelWords();
            Utils.stringToClip( m_activity, TextUtils.join( "\n", selWords ) );

            String msg = getQuantityString( R.plurals.paste_done_fmt,
                                            selWords.length, selWords.length );
            showToast( msg );
            break;
        default:
            Log.d( TAG, "not handling: %s", action );
            handled = false;
            break;
        }
        return handled;
    }

    //////////////////////////////////////////////////
    // AdapterView.OnItemSelectedListener interface
    //////////////////////////////////////////////////
    @Override
    public void onItemSelected( AdapterView<?> parent, View view,
                                int position, long id )
    {
        m_langPosition = position;
        m_checkeds.clear();
        loadList();             // because language has changed
    }

    @Override
    public void onNothingSelected( AdapterView<?> parent )
    {
    }

    //////////////////////////////////////////////////
    // View.OnLongClickListener interface
    //////////////////////////////////////////////////
    @Override
    public boolean onLongClick( View view )
    {
        boolean success = view instanceof SelectableItem.LongClickHandler;
        if ( success ) {
            ((SelectableItem.LongClickHandler)view).longClicked();
        }
        return success;
    }

    //////////////////////////////////////////////////
    // View.OnClickListener interface
    //////////////////////////////////////////////////
    @Override
    public void onClick( View view )
    {
        XWListItem item = (XWListItem)view;
        String[] words = { m_words[item.getPosition()] };
        launchLookup( words, m_langCodes[m_langPosition], true );
    }

    //////////////////////////////////////////////////
    // SelectableItem interface
    //////////////////////////////////////////////////
    @Override
    public void itemClicked( SelectableItem.LongClickHandler clicked,
                             GameSummary summary )
    {
        m_checkeds.add( ((XWListItem)clicked).getText() );
    }

    @Override
    public void itemToggled( SelectableItem.LongClickHandler toggled,
                             boolean selected )
    {
        String word = ((XWListItem)toggled).getText();
        if ( selected ) {
            m_checkeds.add( word );
        } else {
            m_checkeds.remove( word );
        }
        setTitleBar();
    }

    @Override
    public boolean getSelected( SelectableItem.LongClickHandler obj )
    {
        return m_checkeds.contains( ((XWListItem)obj).getText() );
    }

    private void loadList()
    {
        ISOCode isoCode = m_langCodes[m_langPosition];
        m_words = DBUtils.studyListWords( m_activity, isoCode );

        makeAdapter();

        String langName = DictLangCache.getLangNameForISOCode( m_activity, isoCode );
        m_origTitle = getString( R.string.studylist_title_fmt,
                                 langName );
        setTitleBar();
    }

    private void makeAdapter()
    {
        m_adapter = new SLWordsAdapter();
        setListAdapter( m_adapter );
    }

    private void initOrFinish( Bundle args )
    {
        m_langCodes = DBUtils.studyListLangs( m_activity );
        if ( 0 == m_langCodes.length ) {
            finish();
        } else if ( 1 == m_langCodes.length ) {
            m_pickView.setVisibility( View.GONE );
            m_langPosition = 0;
            loadList();
        } else {
            String startLang = null;
            int startIndex = -1;
            if ( null != args ) {
                startLang = args.getString( START_LANG );
            }

            String[] myNames = new String[m_langCodes.length];
            for ( int ii = 0; ii < m_langCodes.length; ++ii ) {
                ISOCode isoCode = m_langCodes[ii];
                myNames[ii] = DictLangCache.getLangNameForISOCode( m_activity, isoCode );
                if ( isoCode.equals( startLang ) ) {
                    startIndex = ii;
                }
            }

            ArrayAdapter<String> adapter = new
                ArrayAdapter<String>( m_activity,
                                      android.R.layout.simple_spinner_item,
                                      myNames );
            adapter.setDropDownViewResource( android.R.layout.
                                             simple_spinner_dropdown_item );
            m_spinner.setAdapter( adapter );
            m_spinner.setOnItemSelectedListener( this );
            if ( -1 != startIndex ) {
                m_spinner.setSelection( startIndex );
            }
        }
    }

    private void setTitleBar()
    {
        String newTitle;
        int nSels = m_checkeds.size();
        if ( 0 == nSels ) {
            newTitle = m_origTitle;
        } else {
            newTitle = getString( R.string.sel_items_fmt, nSels );
        }
        setTitle( newTitle );

        invalidateOptionsMenuIf();
    }

    private String[] getSelWords()
    {
        String[] result;
        int nSels = m_checkeds.size();
        if ( nSels == m_words.length ) {
            result = m_words;
        } else {
            result = m_checkeds.toArray( new String[nSels] );
        }
        return result;
    }

    private void clearSels()
    {
        m_checkeds.clear();
        makeAdapter();
        setTitleBar();
    }

    public static void launch( Delegator delegator )
    {
        launch( delegator, null );
    }

    public static void launch( Delegator delegator, ISOCode isoCode )
    {
        Activity activity = delegator.getActivity();
        if ( null == isoCode ) {
            Assert.assertTrueNR( 0 < DBUtils.studyListLangs( activity ).length );
        } else {
            Assert.assertTrueNR( 0 < DBUtils
                                 .studyListWords( activity, isoCode ).length );
        }

        Bundle bundle = new Bundle();
        if ( null != isoCode ) {
            bundle.putString( START_LANG, isoCode.toString() );
        }

        delegator.addFragment( StudyListFrag.newInstance( delegator ),
                               bundle );
    }

    private class SLWordsAdapter extends XWListAdapter {

        public SLWordsAdapter()
        {
            super( m_words.length );
        }

        public View getView( int position, View convertView, ViewGroup parent ){
            XWListItem item =
                XWListItem.inflate( m_activity, StudyListDelegate.this );
            item.setPosition( position );
            String word = m_words[position];
            item.setText( word );
            item.setSelected( m_checkeds.contains( word ) );
            item.setOnLongClickListener( StudyListDelegate.this );
            item.setOnClickListener( StudyListDelegate.this );
            return item;
        }
    }
}
