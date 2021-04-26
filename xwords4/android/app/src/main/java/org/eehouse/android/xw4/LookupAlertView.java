/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009-2017 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;


import org.eehouse.android.xw4.loc.LocUtils;

import java.util.ArrayList;

public class LookupAlertView extends LinearLayout
    implements View.OnClickListener, Dialog.OnKeyListener,
               AdapterView.OnItemClickListener {
    private static final String TAG = LookupAlertView.class.getSimpleName();

    public interface OnDoneListener {
        void onDone();
    }

    public static final String WORDS = "WORDS";
    public static final String LANG = "LANG";
    public static final String STUDY_ON = "STUDY_ON";
    private static final String STATE = "STATE";
    private static final String WORDINDEX = "WORDINDEX";
    private static final String URLINDEX = "URLINDEX";

    private static final int STATE_DONE = 0;
    private static final int STATE_WORDS = 1;
    private static final int STATE_URLS = 2;
    private static final int STATE_LOOKUP = 3;

    private static String[] s_langCodes;
    private static String[] s_lookupNames;
    private static String[] s_lookupUrls;
    private static ArrayAdapter<String> s_urlsAdapter;
    private static final int LIST_LAYOUT = android.R.layout.simple_list_item_1;

    private static int s_lang = -1;
    private static String s_langName;

    // These two are probably always the same object
    private Context m_context;
    private OnDoneListener m_onDone;
    private ListView m_listView;
    private String[] m_words;
    private boolean m_studyOn;
    private int m_wordIndex = 0;
    private int m_urlIndex = 0;
    private int m_state;
    private ArrayAdapter<String> m_wordsAdapter;
    private Button m_doneButton;
    private Button m_studyButton;
    private TextView m_summary;

    public LookupAlertView( Context context, AttributeSet as ) {
        super( context, as );
        m_context = context;
    }

    protected void init( OnDoneListener lstn, Bundle bundle )
    {
        m_onDone = lstn;
        m_words = bundle.getStringArray( WORDS );
        int lang = bundle.getInt( LANG, 0 );
        setLang( m_context, lang  );
        m_studyOn = XWPrefs.getStudyEnabled( m_context )
            && bundle.getBoolean( STUDY_ON, true );

        m_state = bundle.getInt( STATE, STATE_WORDS );
        m_wordIndex = bundle.getInt( WORDINDEX, 0 );
        m_urlIndex = bundle.getInt( URLINDEX, 0 );

        m_wordsAdapter = new ArrayAdapter<>( m_context, LIST_LAYOUT, m_words );
        m_listView = (ListView)findViewById( android.R.id.list );
        m_listView.setOnItemClickListener( this );

        m_doneButton = (Button)findViewById( R.id.button_done );
        m_doneButton.setOnClickListener( this );
        m_studyButton = (Button)findViewById( R.id.button_study );
        if ( m_studyOn ) {
            m_studyButton.setOnClickListener( this );
        } else {
            m_studyButton.setVisibility( View.GONE );
        }

        m_summary = (TextView)findViewById( R.id.summary );

        switchState();
        if ( 1 == m_words.length ) {
            // imitate onItemClick() on the 0th elem
            Assert.assertTrueNR( STATE_WORDS == m_state );
            Assert.assertTrueNR( m_wordIndex == 0 );
            switchState( true );
        }
    }

    // NOT @Override!!!
    protected void saveInstanceState( Bundle bundle )
    {
        addParams( bundle, m_words, s_lang, m_studyOn );
        bundle.putInt( STATE, m_state );
        bundle.putInt( WORDINDEX, m_wordIndex );
        bundle.putInt( URLINDEX, m_urlIndex );
    }

    //////////////////////////////////////////////////////////////////////
    // View.OnClickListener
    //////////////////////////////////////////////////////////////////////
    @Override
    public void onClick( View view )
    {
        if ( view == m_doneButton ) {
            switchState( false );
        } else if ( view == m_studyButton ) {
            String word = m_words[m_wordIndex];
            DBUtils.addToStudyList( m_context, word, s_lang );

            String msg = LocUtils.getString( m_context, R.string.add_done_fmt,
                                             word, s_langName );
            Utils.showToast( m_context, msg );
        }
    }

    //////////////////////////////////////////////////////////////////////
    // AdapterView.OnItemClickListener
    //////////////////////////////////////////////////////////////////////
    @Override
    public void onItemClick( AdapterView<?> parentView, View view,
                             int position, long id )
    {
        if ( STATE_WORDS == m_state ) {
            m_wordIndex = position;
        } else if ( STATE_URLS == m_state ) {
            m_urlIndex = position;
        } else {
            Assert.failDbg();
        }
        switchState( true );
    }

    private void adjustState( boolean forward )
    {
        int incr = forward ? 1 : -1;
        m_state += incr;
        for ( ; ; ) {
            int curState = m_state;
            if ( STATE_WORDS == m_state && 1 >= m_words.length ) {
                m_state += incr;
            }
            if ( STATE_URLS == m_state &&
                ( 1 >= s_lookupUrls.length && !m_studyOn ) ) {
                m_state += incr;
            }
            if ( m_state == curState ) {
                break;
            }
        }
    }

    private void switchState( boolean forward )
    {
        adjustState( forward );
        switchState();
    }

    private void switchState()
    {
        switch( m_state ) {
        case STATE_DONE:
            m_onDone.onDone();
            break;
        case STATE_WORDS:
            m_listView.setAdapter( m_wordsAdapter );
            setSummary( m_studyOn ?
                        R.string.title_lookup_study : R.string.title_lookup );
            m_doneButton.setText( R.string.button_done );
            m_studyButton.setVisibility( View.GONE );
            break;
        case STATE_URLS:
            m_listView.setAdapter( s_urlsAdapter );
            setSummary( m_words[m_wordIndex] );
            String txt = LocUtils.getString( m_context, R.string.button_done_fmt,
                                             m_words[m_wordIndex] );
            m_doneButton.setText( txt );
            txt = LocUtils.getString( m_context, R.string.add_to_study_fmt,
                                      m_words[m_wordIndex] );
            if ( m_studyOn ) {
                m_studyButton.setVisibility( View.VISIBLE );
                m_studyButton.setText( txt );
            }
            break;
        case STATE_LOOKUP:
            lookupWord( m_context, m_words[m_wordIndex],
                        s_lookupUrls[m_urlIndex] );
            switchState( false );
            break;
        default:
            Assert.failDbg();
            break;
        }
    } // switchState

    private void lookupWord( Context context, String word, String fmt )
    {
        String langCode = s_langCodes[s_lang];
        String dict_url = String.format( fmt, langCode, word );
        Uri uri = Uri.parse( dict_url );
        Intent intent = new Intent( Intent.ACTION_VIEW, uri );
        intent.setFlags( Intent.FLAG_ACTIVITY_NEW_TASK );

        try {
            context.startActivity( intent );
        } catch ( android.content.ActivityNotFoundException anfe ) {
            Log.ex( TAG, anfe );
        }
    } // lookupWord

    private void setLang( Context context, int lang )
    {
        if ( null == s_langCodes ) {
            s_langCodes = context.getResources().getStringArray( R.array.language_codes );
        }

        if ( s_lang != lang ) {
            String[] urls = context.getResources().getStringArray( R.array.lookup_urls );
            ArrayList<String> tmpUrls = new ArrayList<>();
            ArrayList<String> tmpNames = new ArrayList<>();
            String langCode = String.format( ":%s:", s_langCodes[lang] );
            for ( int ii = 0; ii < urls.length; ii += 3 ) {
                String codes = urls[ii+1];
                if ( 0 == codes.length() || codes.contains( langCode ) ) {
                    String url = urls[ii+2];
                    if ( ! tmpUrls.contains( url ) ) {
                        tmpNames.add( urls[ii] );
                        tmpUrls.add( url );
                    }
                }
            }
            s_lookupNames = tmpNames.toArray( new String[tmpNames.size()] );
            s_lookupUrls = tmpUrls.toArray( new String[tmpUrls.size()] );
            s_urlsAdapter = new ArrayAdapter<>( context, LIST_LAYOUT,
                                                s_lookupNames );
            s_lang = lang;
            String langName = DictLangCache.getLangName( context, lang );
            s_langName = LocUtils.xlateLang( context, langName );
        }
    }

    private void setSummary( int id )
    {
        m_summary.setText( LocUtils.getString( m_context, id ) );
    }

    private void setSummary( String word )
    {
        String title =
            LocUtils.getString( m_context, R.string.pick_url_title_fmt, word );
        m_summary.setText( title );
    }

    //////////////////////////////////////////////////////////////////////
    // Dialog.OnKeyListener interface
    //////////////////////////////////////////////////////////////////////
    public boolean onKey( DialogInterface arg0, int keyCode, KeyEvent event )
    {
        boolean handled = keyCode == KeyEvent.KEYCODE_BACK
            && KeyEvent.ACTION_UP == event.getAction();
        if ( handled ) {
            switchState( false );
        }
        return handled;
    }

    private static void addParams( Bundle bundle, String[] words, int lang,
                                   boolean studyOn )
    {
        bundle.putStringArray( WORDS, words );
        bundle.putInt( LANG, lang );
        bundle.putBoolean( STUDY_ON, studyOn );
    }

    protected static Bundle makeParams( String[] words, int lang,
                                        boolean noStudyOption )
    {
        Bundle bundle = new Bundle();
        addParams( bundle, words, lang, !noStudyOption );
        return bundle;
    }
}
