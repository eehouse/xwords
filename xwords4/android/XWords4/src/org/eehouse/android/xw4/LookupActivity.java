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
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ListView;
import java.util.ArrayList;

import junit.framework.Assert;

public class LookupActivity extends XWListActivity
    implements View.OnClickListener,
               AdapterView.OnItemClickListener {

    public static final String WORDS = "WORDS";
    public static final String LANG = "LANG";

    private static String[] s_langCodes;
    private static String[] s_lookupNames;
    private static String[] s_lookupUrls;
    private static ArrayAdapter<String> s_urlsAdapter;
    private static final int LIST_LAYOUT = 
        // android.R.layout.simple_spinner_item;
        // android.R.layout.select_dialog_item
        android.R.layout.simple_list_item_1
        ;

    private static int s_lang = -1;

    private String[] m_words;
    private static int m_lang;
    private int m_wordIndex = 0;
    private int m_urlIndex = 0;
    private int m_state;
    private ArrayAdapter<String> m_wordsAdapter;
    private ArrayAdapter<String> m_shown;
    private Button m_doneButton;

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );
        getBundledData( savedInstanceState );

        Intent intent = getIntent();
        m_words = intent.getStringArrayExtra( WORDS );
        m_lang = intent.getIntExtra( LANG, -1 );
        setLang( m_lang );

        setContentView( R.layout.lookup );

        m_wordsAdapter = new ArrayAdapter<String>( this, LIST_LAYOUT, 
                                                   m_words );
        getListView().setOnItemClickListener( this );

        m_doneButton = (Button)findViewById( R.id.button_done );
        m_doneButton.setOnClickListener( this );

        m_state = 0;
        adjustForState();
    }

    @Override
    protected void onSaveInstanceState( Bundle outState ) 
    {
        super.onSaveInstanceState( outState );
        // if ( null != m_words ) {
        //     outState.putStringArray( WORDS, m_words );
        // }
    }

    /* View.OnClickListener -- just the Done button */
    public void onClick( View view ) 
    {
        --m_state;
        adjustForState();
    }

    /* AdapterView.OnItemClickListener */
    public void onItemClick( AdapterView<?> parent, View view, 
                             int position, long id )
    {
        if ( m_shown == m_wordsAdapter ) {
            m_wordIndex = position;
            Utils.logf( "%s selected", m_words[position] );
        } else if ( m_shown == s_urlsAdapter ) {
            m_urlIndex = position;
            Utils.logf( "%s selected", s_lookupUrls[position] );
        } else {
            Assert.fail();
        }
        ++m_state;
        adjustForState();
    }

    private void getBundledData( Bundle bundle )
    {
        // if ( null != bundle ) {
        //     m_words = bundle.getStringArray( WORDS );
        // }
    }

    private void adjustForState()
    {
        if ( 0 > m_state ) {
            finish();
        } else { 
            switch( m_state ) {
            case 0:
                if ( 1 < m_words.length ) {
                    m_shown = m_wordsAdapter;
                    getListView().setAdapter( m_wordsAdapter );
                    setTitle( R.string.title_lookup );
                    m_doneButton.setText( R.string.button_done );
                    break;
                }
            case 1:
                if ( 1 < s_lookupUrls.length ) {
                    m_shown = s_urlsAdapter;
                    getListView().setAdapter( s_urlsAdapter );
                    setTitle( m_words[m_wordIndex] );
                    String txt = Utils.format( this, R.string.button_donef,
                                               m_words[m_wordIndex] );
                    m_doneButton.setText( txt );
                    break;
                }
            case 2:
                lookupWord( m_words[m_wordIndex], s_lookupUrls[m_urlIndex] );
                if ( 0 >= --m_state ) {
                    finish();
                }
                break;
            }
        }
    }

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

    private void setTitle( String word )
    {
        String title = Utils.format( this, R.string.pick_url_titlef, word );
        super.setTitle( title );
    }

}
