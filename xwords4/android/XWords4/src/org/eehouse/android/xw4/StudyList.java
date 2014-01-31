/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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

import junit.framework.Assert;

public class StudyList extends XWListActivity 
    implements OnItemSelectedListener {

    public static final int NO_LANG = -1;

    private static final int CLEAR_ACTION = 1;
    private static final int COPY_ACTION = 2;
    private static final String START_LANG = "START_LANG";
    
    private Spinner m_spinner;
    private View m_pickView;    // LinearLayout, actually
    private int[] m_langCodes;
    private String[] m_words;
    private int m_position;

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );

        setContentView( R.layout.studylist );

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
                                 COPY_ACTION );
            break;
        case R.id.clear_all:
            showConfirmThen( R.string.confirm_studylist_clear, CLEAR_ACTION );
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
    public void dlgButtonClicked( int id, int which, Object[] params )
    {
        if ( AlertDialog.BUTTON_POSITIVE == which ) {
            switch ( id ) {
            case CLEAR_ACTION:
                DBUtils.studyListClear( this, m_langCodes[m_position] );
                initOrFinish( null );
                break;
            case COPY_ACTION:
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
        launchLookup( words, m_langCodes[m_position], true );
    }

    //////////////////////////////////////////////////
    // AdapterView.OnItemSelectedListener interface
    //////////////////////////////////////////////////
    public void onItemSelected( AdapterView<?> parent, View view, 
                                int position, long id )
    {
        m_position = position;
        loadList();
    }

    public void onNothingSelected( AdapterView<?> parent )
    {
    }

    private void loadList()
    {
        int lang = m_langCodes[m_position];
        m_words = DBUtils.studyListWords( this, lang );
        ArrayAdapter<String> adapter = new ArrayAdapter<String>
            ( this, android.R.layout.simple_list_item_1 );
        for ( String word : m_words ) {
            adapter.add( word );
        }
        // adapter.sort();

        setListAdapter( adapter );

        String langName = DictLangCache.getLangNames( this )[lang];
        String title = getString( R.string.studylist_titlef, langName );
        setTitle( title );
    }

    private void initOrFinish( Intent startIntent )
    {
        m_langCodes = DBUtils.studyListLangs( this );
        if ( 0 == m_langCodes.length ) {
            finish();
        } else if ( 1 == m_langCodes.length ) {
            m_pickView.setVisibility( View.GONE );
            m_position = 0;
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

}
