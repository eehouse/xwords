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

import android.view.ViewGroup;
import android.widget.ListView;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.ClipboardManager;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Spinner;
import java.util.Arrays;
import java.util.HashSet;

import junit.framework.Assert;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.jni.GameSummary;

public class StudyList extends XWListActivity 
    implements OnItemSelectedListener, SelectableItem {

    public static final int NO_LANG = -1;

    private static final String START_LANG = "START_LANG";
    
    private Spinner m_spinner;
    private View m_pickView;    // LinearLayout, actually
    private int[] m_langCodes;
    private String[] m_words;
    private HashSet<Integer> m_checkeds;
    private int m_langPosition;
    private SLWordsAdapter<String> m_adapter;
    private ListView m_list;
    private CharSequence m_origTitle;

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );

        setContentView( R.layout.studylist );
        m_list = (ListView)findViewById( android.R.id.list );

        m_spinner = (Spinner)findViewById( R.id.pick_lang_spinner );
        m_pickView = findViewById( R.id.pick_lang );

        initOrFinish( getIntent() );
    }

    @Override
    public boolean onCreateOptionsMenu( Menu menu )
    {
        getMenuInflater().inflate( R.menu.studylist, menu );
        return true;
    }

    // @Override
    // public boolean onPrepareOptionsMenu( Menu menu ) 
    // {
    //     return true;
    // }

    public boolean onOptionsItemSelected( MenuItem item )
    {
        boolean handled = true;
        switch ( item.getItemId() ) {
        case R.id.copy_all:
            showNotAgainDlgThen( R.string.not_again_studycopy,
                                 R.string.key_na_studycopy, 
                                 Action.SL_COPY_ACTION );
            break;
        case R.id.clear_all:
            showConfirmThen( R.string.confirm_studylist_clear, 
                             Action.SL_CLEAR_ACTION );
            break;

        case R.id.select_all:
            for ( int ii = 0; ii < m_words.length; ++ii ) {
                m_checkeds.add( ii );
            }
            makeAdapter();
            setTitleBar();
            break;
        case R.id.deselect_all:
            m_checkeds.clear();
            makeAdapter();
            setTitleBar();
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
                DBUtils.studyListClear( this, m_langCodes[m_langPosition] );
                initOrFinish( null );
                break;
            case SL_COPY_ACTION:
                ClipboardManager clipboard = (ClipboardManager)
                    getSystemService( Context.CLIPBOARD_SERVICE );
                clipboard.setText( TextUtils.join( "\n", m_words ) );

                String msg  = getString( R.string.paste_donef, m_words.length );
                Utils.showToast( this, msg );
                break;
            default:
                Assert.fail();
                break;
            }
        }
    }

    @Override
    public void onListItemClick( ListView lv, View view, int position, long id )
    {
        String[] words = { m_words[position] };
        launchLookup( words, m_langCodes[m_langPosition], true );
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
    // SelectableItem interface
    //////////////////////////////////////////////////
    public void itemClicked( SelectableItem.LongClickHandler clicked,
                             GameSummary summary )
    {
        Assert.assertTrue( clicked instanceof XWListItem );
        m_checkeds.add( ((XWListItem)clicked).getPosition() );
    }

    public void itemToggled( SelectableItem.LongClickHandler toggled, 
                             boolean selected )
    {
        Assert.assertTrue( toggled instanceof XWListItem );
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
        Assert.assertTrue( obj instanceof XWListItem );
        return m_checkeds.contains( ((XWListItem)obj).getPosition() );
    }

    private void loadList()
    {
        int lang = m_langCodes[m_langPosition];
        m_words = DBUtils.studyListWords( this, lang );
        m_checkeds = new HashSet<Integer>();

        makeAdapter();

        String langName = DictLangCache.getLangNames( this )[lang];
        m_origTitle = getString( R.string.studylist_titlef, langName );
        setTitleBar();
    }

    private void makeAdapter()
    {
        m_adapter = new SLWordsAdapter<String>( this, 0, m_words );
        setListAdapter( m_adapter );
    }

    private void initOrFinish( Intent startIntent )
    {
        m_langCodes = DBUtils.studyListLangs( this );
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

            String[] names = DictLangCache.getLangNames( this );
            String[] myNames = new String[m_langCodes.length];
            for ( int ii = 0; ii < m_langCodes.length; ++ii ) {
                int lang = m_langCodes[ii];
                myNames[ii] = names[lang];
                if ( lang == startLang ) {
                    startIndex = ii;
                }
            }

            ArrayAdapter<String> adapter = new
                ArrayAdapter<String>( this, 
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
        CharSequence newTitle;
        int nSels = m_checkeds.size();
        DbgUtils.logf( "setTitleBar: nSels=%d", nSels );
        if ( 0 == nSels ) {
            newTitle = m_origTitle;
        } else {
            newTitle = getString( R.string.sel_wordsf, nSels );
        }
        setTitle( newTitle );
    }

    public static void launchOrAlert( Context context, int lang, 
                                      DlgDelegate.HasDlgDelegate dlg )
    {
        String msg = null;
        if ( 0 == DBUtils.studyListLangs( context ).length ) {
            msg = context.getString( R.string.study_no_lists );
        } else if ( NO_LANG != lang && 
                    0 == DBUtils.studyListWords( context, lang ).length ) {
            String langname = DictLangCache.getLangName( context, lang );
            msg = context.getString( R.string.study_no_langf, langname );
        } else {
            Intent intent = new Intent( context, StudyList.class );
            if ( NO_LANG != lang ) {
                intent.putExtra( START_LANG, lang );
            }
            context.startActivity( intent );
        }

        if ( null != msg ) {
            dlg.showOKOnlyDialog( msg );
        }
    }

    private class SLWordsAdapter<String> extends ArrayAdapter<String> {
        // public SLWordsAdapter()
        // {
        //     super( m_words.length );
        // }

        public SLWordsAdapter( Context context, int ignored, String[] strings) {
            super( context, ignored, strings );
        }

        public SLWordsAdapter( Context context, int resource ) {
            super( context, resource );
        }

        public View getView( int position, View convertView, 
                             ViewGroup parent ) {
            XWListItem item = XWListItem.inflate( StudyList.this, StudyList.this );
            item.setPosition( position );
            item.setText( m_words[position] );
            item.setSelected( m_checkeds.contains(position) );
            DbgUtils.logf( "getView(position=%d) => %H", position, item );
            return item;
        }
    }

}
