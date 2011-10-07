/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
/*
 * Copyright 2009-2011 by Eric House (xwords@eehouse.org).  All
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
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.view.Window;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ListView;
import android.widget.TextView;
import android.app.AlertDialog;
import java.util.ArrayList;

import junit.framework.Assert;

public class LookupActivity extends XWListActivity
    implements View.OnClickListener,
               AdapterView.OnItemClickListener {

    public static final String WORDS = "WORDS";
    public static final String LANG = "LANG";
    public static final String STATE = "STATE";
    public static final String WORDINDEX = "WORDINDEX";
    public static final String URLINDEX = "URLINDEX";

    private static final int STATE_DONE = 0;
    private static final int STATE_WORDS = 1;
    private static final int STATE_URLS = 2;
    private static final int STATE_LOOKUP = 3;

    private static final int LOOKUP_ACTION = 1;

    private static String[] s_langCodes;
    private static String[] s_lookupNames;
    private static String[] s_lookupUrls;
    private static ArrayAdapter<String> s_urlsAdapter;
    private static final int LIST_LAYOUT = android.R.layout.simple_list_item_1;

    private static int s_lang = -1;

    private String[] m_words;
    private static int m_lang;
    private int m_wordIndex = 0;
    private int m_urlIndex = 0;
    private int m_state;
    private ArrayAdapter<String> m_wordsAdapter;
    private Button m_doneButton;
    private TextView m_summary;

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );

        requestWindowFeature( Window.FEATURE_NO_TITLE );

        Intent intent = getIntent();
        m_words = intent.getStringArrayExtra( WORDS );
        m_lang = intent.getIntExtra( LANG, -1 );
        setLang( m_lang );

        getBundledData( savedInstanceState );

        setContentView( R.layout.lookup );

        m_wordsAdapter = new ArrayAdapter<String>( this, LIST_LAYOUT, 
                                                   m_words );
        getListView().setOnItemClickListener( this );

        m_doneButton = (Button)findViewById( R.id.button_done );
        m_doneButton.setOnClickListener( this );
        m_summary = (TextView)findViewById( R.id.summary );

        switchState();
    }

    /* View.OnClickListener -- just the Done button */
    public void onClick( View view ) 
    {
        switchState( -1 );
    }

    /* AdapterView.OnItemClickListener */
    public void onItemClick( AdapterView<?> parent, View view, 
                             int position, long id )
    {
        if ( STATE_WORDS == m_state ) {
            m_wordIndex = position;
        } else if ( STATE_URLS == m_state ) {
            m_urlIndex = position;
        } else {
            Assert.fail();
        }
        switchState( 1 );
    }

    @Override
    protected void onSaveInstanceState( Bundle outState ) 
    {
        super.onSaveInstanceState( outState );
        outState.putInt( STATE, m_state );
        outState.putInt( WORDINDEX, m_wordIndex );
        outState.putInt( URLINDEX, m_urlIndex );
    }

    //////////////////////////////////////////////////
    // DlgDelegate.DlgClickNotify interface
    //////////////////////////////////////////////////
    @Override
    public void dlgButtonClicked( int id, int which )
    {
        if ( LOOKUP_ACTION == id 
             && AlertDialog.BUTTON_POSITIVE == which ) {
            lookupWord( m_words[m_wordIndex], s_lookupUrls[m_urlIndex] );
            switchState( -1 );
        }
    }

    private void getBundledData( Bundle bundle )
    {
        if ( null == bundle ) {
            m_state = STATE_DONE;
            adjustState( 1 );
        } else {
            m_state = bundle.getInt( STATE );
            m_wordIndex = bundle.getInt( WORDINDEX );
            m_urlIndex = bundle.getInt( URLINDEX );
        }
    }

    private void adjustState( int incr )
    {
        m_state += incr;
        for ( ; ; ) {
            int curState = m_state;
            if ( STATE_WORDS == m_state && 1 >= m_words.length ) {
                m_state += incr;
            }
            if ( STATE_URLS == m_state && 1 >= s_lookupUrls.length ) {
                m_state += incr;
            }
            if ( m_state == curState ) {
                break;
            }
        }
    }

    private void switchState( int incr )
    {
        adjustState( incr );
        switchState();
    }

    private void switchState()
    {
        switch( m_state ) {
        case STATE_DONE:
            finish();
            break;
        case STATE_WORDS:
            getListView().setAdapter( m_wordsAdapter );
            setSummary( R.string.title_lookup );
            m_doneButton.setText( R.string.button_done );
            break;
        case STATE_URLS:
            getListView().setAdapter( s_urlsAdapter );
            setSummary( m_words[m_wordIndex] );
            String txt = Utils.format( this, R.string.button_donef,
                                       m_words[m_wordIndex] );
            m_doneButton.setText( txt );
            break;
        case STATE_LOOKUP:
            if ( 1 >= s_lookupUrls.length ) {
                showNotAgainDlgThen( R.string.not_again_needUrlsForLang, 
                                     R.string.key_na_needUrlsForLang,
                                     LOOKUP_ACTION );
            } else {
                dlgButtonClicked( LOOKUP_ACTION, AlertDialog.BUTTON_POSITIVE );
            }
            break;
        default:
            Assert.fail();
            break;
        }
    } // adjustState

    private void lookupWord( String word, String fmt )
    {
        if ( false ) {
            Utils.logf( "skipping lookupWord(%s)", word );
        } else {
            String langCode = s_langCodes[s_lang];
            String dict_url = String.format( fmt, langCode, word );
            Uri uri = Uri.parse( dict_url );
            Intent intent = new Intent( Intent.ACTION_VIEW, uri );
            intent.setFlags( Intent.FLAG_ACTIVITY_NEW_TASK );
        
            try {
                startActivity( intent );
            } catch ( android.content.ActivityNotFoundException anfe ) {
                Utils.logf( "%s", anfe.toString() );
            }
        }
    } // lookupWord

    public void setLang( int lang )
    {
        if ( null == s_langCodes ) {
            s_langCodes = getResources().getStringArray( R.array.language_codes );
        }

        if ( s_lang != lang ) {
            String[] urls = getResources().getStringArray( R.array.lookup_urls );
            ArrayList<String> tmpUrls = new ArrayList<String>();
            ArrayList<String> tmpNames = new ArrayList<String>();
            String langCode = String.format( ":%s:", s_langCodes[lang] );
            for ( int ii = 0; ii < urls.length; ii += 3 ) {
                String codes = urls[ii+1];
                if ( 0 == codes.length() || codes.contains( langCode ) ) {
                    tmpNames.add( urls[ii] );
                    tmpUrls.add( urls[ii+2] );
                }
            }
            s_lookupNames = tmpNames.toArray( new String[tmpNames.size()] );
            s_lookupUrls = tmpUrls.toArray( new String[tmpUrls.size()] );
            s_urlsAdapter = new ArrayAdapter<String>( this, LIST_LAYOUT, 
                                                      s_lookupNames );
            s_lang = lang;
        } // initLookup
    }

    private void setSummary( int id )
    {
        m_summary.setText( getString( id ) );
    }

    private void setSummary( String word )
    {
        String title = Utils.format( this, R.string.pick_url_titlef, word );
        m_summary.setText( title );
    }

}
