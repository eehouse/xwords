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

import android.app.AlertDialog;
import android.app.ListActivity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.ClipboardManager;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.Spinner;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Iterator;

import junit.framework.Assert;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.loc.LocUtils;

public class StudyListDelegate extends ListDelegateBase
    implements OnItemSelectedListener, SelectableItem,
               View.OnLongClickListener, View.OnClickListener {

    public static final int NO_LANG = -1;

    protected static final String START_LANG = "START_LANG";
    
    private ListActivity m_activity;
    private Spinner m_spinner;
    private View m_pickView;    // LinearLayout, actually
    private int[] m_langCodes;
    private String[] m_words;
    private HashSet<Integer> m_checkeds;
    private int m_langPosition;
    private SLWordsAdapter m_adapter;
    private ListView m_list;
    private String m_origTitle;

    protected StudyListDelegate( ListActivity activity, Bundle savedInstanceState )
    {
        super( activity, savedInstanceState, R.menu.studylist );
        m_activity = activity;
    }

    protected void init( Bundle savedInstanceState ) 
    {
        setContentView( R.layout.studylist );
        m_list = (ListView)findViewById( android.R.id.list );

        m_spinner = (Spinner)findViewById( R.id.pick_lang_spinner );
        m_pickView = findViewById( R.id.pick_lang );

        initOrFinish( getIntent() );
    }

    protected boolean backPressed() 
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
            showNotAgainDlgThen( R.string.not_again_studycopy,
                                 R.string.key_na_studycopy, 
                                 Action.SL_COPY_ACTION );
            break;
        case R.id.slmenu_clear_sel:
            String msg = getString( R.string.confirm_studylist_clear_fmt, 
                                    m_checkeds.size() );
            showConfirmThen( msg, Action.SL_CLEAR_ACTION );
            break;

        case R.id.slmenu_select_all:
            for ( int ii = 0; ii < m_words.length; ++ii ) {
                m_checkeds.add( ii );
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

    //////////////////////////////////////////////////
    // DlgDelegate.DlgClickNotify interface
    //////////////////////////////////////////////////
    @Override
    public void dlgButtonClicked( Action action, int which, Object[] params )
    {
        if ( AlertDialog.BUTTON_POSITIVE == which ) {
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
                ClipboardManager clipboard = (ClipboardManager)
                    m_activity.getSystemService( Context.CLIPBOARD_SERVICE );
                clipboard.setText( TextUtils.join( "\n", selWords ) );

                String msg = getString( R.string.paste_done_fmt, 
                                        selWords.length );
                Utils.showToast( m_activity, msg );
                break;
            default:
                Assert.fail();
                break;
            }
        }
    }

    //////////////////////////////////////////////////
    // AdapterView.OnItemSelectedListener interface
    //////////////////////////////////////////////////
    public void onItemSelected( AdapterView<?> parent, View view, 
                                int position, long id )
    {
        m_langPosition = position;
        loadList();             // because language has changed
    }

    public void onNothingSelected( AdapterView<?> parent )
    {
    }

    //////////////////////////////////////////////////
    // View.OnLongClickListener interface
    //////////////////////////////////////////////////
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
    public void onClick( View view ) 
    {
        XWListItem item = (XWListItem)view;
        String[] words = { m_words[item.getPosition()] };
        launchLookup( words, m_langCodes[m_langPosition], true );
    }

    //////////////////////////////////////////////////
    // SelectableItem interface
    //////////////////////////////////////////////////
    public void itemClicked( SelectableItem.LongClickHandler clicked,
                             GameSummary summary )
    {
        m_checkeds.add( ((XWListItem)clicked).getPosition() );
    }

    public void itemToggled( SelectableItem.LongClickHandler toggled, 
                             boolean selected )
    {
        int position = ((XWListItem)toggled).getPosition();
        if ( selected ) {
            m_checkeds.add( position );
        } else {
            m_checkeds.remove( position );
        }
        setTitleBar();
    }

    public boolean getSelected( SelectableItem.LongClickHandler obj )
    {
        return m_checkeds.contains( ((XWListItem)obj).getPosition() );
    }

    private void loadList()
    {
        int lang = m_langCodes[m_langPosition];
        m_words = DBUtils.studyListWords( m_activity, lang );
        m_checkeds = new HashSet<Integer>();

        makeAdapter();

        String langName = DictLangCache.getLangNames( m_activity )[lang];
        m_origTitle = getString( R.string.studylist_title_fmt, langName );
        setTitleBar();
    }

    private void makeAdapter()
    {
        m_adapter = new SLWordsAdapter();
        setListAdapter( m_adapter );
    }

    private void initOrFinish( Intent startIntent )
    {
        m_langCodes = DBUtils.studyListLangs( m_activity );
        if ( 0 == m_langCodes.length ) {
            finish();
        } else if ( 1 == m_langCodes.length ) {
            m_pickView.setVisibility( View.GONE );
            m_langPosition = 0;
            loadList();
        } else {
            int startLang = NO_LANG;
            int startIndex = -1;
            if ( null != startIntent ) {
                startLang = startIntent.getIntExtra( START_LANG, NO_LANG );
            }

            String[] names = DictLangCache.getLangNames( m_activity );
            String[] myNames = new String[m_langCodes.length];
            for ( int ii = 0; ii < m_langCodes.length; ++ii ) {
                int lang = m_langCodes[ii];
                myNames[ii] = names[lang];
                if ( lang == startLang ) {
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

        ABUtils.invalidateOptionsMenuIf( m_activity );
    }

    private String[] getSelWords()
    {
        String[] result;
        int nSels = m_checkeds.size();
        if ( nSels == m_words.length ) {
            result = m_words;
        } else {
            result = new String[nSels];
            Iterator<Integer> iter = m_checkeds.iterator();
            for ( int ii = 0; iter.hasNext(); ++ii ) {
                result[ii] = m_words[iter.next()];
            }
        }
        return result;
    }

    private void clearSels()
    {
        m_checkeds.clear();
        makeAdapter();
        setTitleBar();
    }

    public static void launchOrAlert( Context context, int lang, 
                                      DlgDelegate.HasDlgDelegate dlg )
    {
        String msg = null;
        if ( 0 == DBUtils.studyListLangs( context ).length ) {
            msg = LocUtils.getString( context, R.string.study_no_lists );
        } else if ( NO_LANG != lang && 
                    0 == DBUtils.studyListWords( context, lang ).length ) {
            String langname = DictLangCache.getLangName( context, lang );
            msg = LocUtils.getString( context, R.string.study_no_lang_fmt, 
                                      langname );
        } else {
            Intent intent = new Intent( context, StudyListActivity.class );
            if ( NO_LANG != lang ) {
                intent.putExtra( START_LANG, lang );
            }
            context.startActivity( intent );
        }

        if ( null != msg ) {
            dlg.showOKOnlyDialog( msg );
        }
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
            item.setText( m_words[position] );
            item.setSelected( m_checkeds.contains(position) );
            item.setOnLongClickListener( StudyListDelegate.this );
            item.setOnClickListener( StudyListDelegate.this );
            return item;
        }
    }
}
