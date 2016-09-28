/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009-2014 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.Activity;
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

import junit.framework.Assert;

import org.eehouse.android.xw4.loc.LocUtils;

import java.util.ArrayList;

public class LookupAlert extends LinearLayout
    implements View.OnClickListener, Dialog.OnKeyListener,
               AdapterView.OnItemClickListener {

    public static final String WORDS = "WORDS";
    public static final String LANG = "LANG";
    public static final String NOSTUDY = "NOSTUDY";
    private static final String FORCELIST = "FORCELIST";
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
    private Activity m_parent;
    private ListView m_listView;
    private String[] m_words;
    private boolean m_forceList;
    private boolean m_studyOn;
    private int m_wordIndex = 0;
    private int m_urlIndex = 0;
    private int m_state;
    private ArrayAdapter<String> m_wordsAdapter;
    private Button m_doneButton;
    private Button m_studyButton;
    private TextView m_summary;

    public LookupAlert( Context context, AttributeSet as ) {
        super( context, as );
        m_context = context;
    }

    // @Override
    // protected void onCreate( Bundle savedInstanceState )
    // {
    //     super.onCreate( savedInstanceState );
    //     requestWindowFeature( Window.FEATURE_NO_TITLE );
    //     setContentView( R.layout.lookup );

    //     Intent intent = getIntent();
    //     m_words = intent.getStringArrayExtra( WORDS );
    //     setLang( intent.getIntExtra( LANG, -1 ) );
    //     m_forceList = intent.getBooleanExtra( FORCELIST, false );
    //     m_studyOn = XWPrefs.getStudyEnabled( this );
    //     if ( m_studyOn ) {
    //         m_studyOn = !intent.getBooleanExtra( NOSTUDY, false );
    //     }

    //     m_state = STATE_DONE;
    //     adjustState( 1 );

    //     m_wordsAdapter = new ArrayAdapter<String>( this, LIST_LAYOUT,
    //                                                m_words );
    //     getListView().setOnItemClickListener( this );

    //     m_doneButton = (Button)findViewById( R.id.button_done );
    //     m_doneButton.setOnClickListener( this );
    //     m_studyButton = (Button)findViewById( R.id.button_study );
    //     if ( m_studyOn ) {
    //         m_studyButton.setOnClickListener( this );
    //     } else {
    //         m_studyButton.setVisibility( View.GONE );
    //     }

    //     m_summary = (TextView)findViewById( R.id.summary );

    //     switchState();
    // }

    private void init( Activity activity, Bundle params )
    {
        m_parent = activity;
        m_words = params.getStringArray( WORDS );
        setLang( activity, params.getInt( LANG, -1 ) );
        m_forceList = params.getBoolean( FORCELIST, false );
        m_studyOn = XWPrefs.getStudyEnabled( m_context );
        if ( m_studyOn ) {
            m_studyOn = !params.getBoolean( NOSTUDY, false );
        }

        m_state = STATE_DONE;
        adjustState( 1 );

        m_wordsAdapter = new ArrayAdapter<String>( m_context, LIST_LAYOUT,
                                                   m_words );
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
    }

    // @Override
    // protected void onSaveInstanceState( Bundle outState )
    // {
    //     super.onSaveInstanceState( outState );
    //     outState.putInt( STATE, m_state );
    //     outState.putInt( WORDINDEX, m_wordIndex );
    //     outState.putInt( URLINDEX, m_urlIndex );
    // }

    // private void getBundledData( Bundle bundle )
    // {
    //     if ( null == bundle ) {
    //         m_state = STATE_DONE;
    //         adjustState( 1 );
    //     } else {
    //         m_state = bundle.getInt( STATE );
    //         m_wordIndex = bundle.getInt( WORDINDEX );
    //         m_urlIndex = bundle.getInt( URLINDEX );
    //     }
    // }

    //////////////////////////////////////////////////////////////////////
    // View.OnClickListener
    //////////////////////////////////////////////////////////////////////
    public void onClick( View view )
    {
        if ( view == m_doneButton ) {
            switchState( -1 );
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
    public void onItemClick( AdapterView<?> parentView, View view,
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

    private void adjustState( int incr )
    {
        m_state += incr;
        for ( ; ; ) {
            int curState = m_state;
            if ( STATE_WORDS == m_state && 1 >= m_words.length ) {
                m_state += incr;
            }
            if ( STATE_URLS == m_state &&
                ( 1 >= s_lookupUrls.length && !m_forceList && !m_studyOn ) ) {
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
            m_parent.removeDialog( DlgID.LOOKUP.ordinal() );
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
            switchState( -1 );
            break;
        default:
            Assert.fail();
            break;
        }
    } // switchState

    private static void lookupWord( Context context, String word, String fmt )
    {
        if ( false ) {
            DbgUtils.logf( "skipping lookupWord(%s)", word );
        } else {
            String langCode = s_langCodes[s_lang];
            String dict_url = String.format( fmt, langCode, word );
            Uri uri = Uri.parse( dict_url );
            Intent intent = new Intent( Intent.ACTION_VIEW, uri );
            intent.setFlags( Intent.FLAG_ACTIVITY_NEW_TASK );

            try {
                context.startActivity( intent );
            } catch ( android.content.ActivityNotFoundException anfe ) {
                DbgUtils.logex( anfe );
            }
        }
    } // lookupWord

    private static void setLang( Context context, int lang )
    {
        if ( null == s_langCodes ) {
            s_langCodes = context.getResources().getStringArray( R.array.language_codes );
        }

        if ( s_lang != lang ) {
            String[] urls = context.getResources().getStringArray( R.array.lookup_urls );
            ArrayList<String> tmpUrls = new ArrayList<String>();
            ArrayList<String> tmpNames = new ArrayList<String>();
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
            s_urlsAdapter = new ArrayAdapter<String>( context, LIST_LAYOUT,
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
            switchState( -1 );
        }
        return handled;
    }

    public static boolean needAlert( Context context, String[] words,
                                     int langCode, boolean noStudy )
    {
        boolean result = !noStudy || 1 < words.length;
        if ( !result ) {
            setLang( context, langCode );
            result = 1 < s_lookupUrls.length;
        }
        return result;
    }

    public static Bundle makeParams( String[] words, int lang,
                                     boolean noStudyOption )
    {
        Bundle bundle = new Bundle();
        bundle.putStringArray( WORDS, words );
        bundle.putInt( LANG, lang );
        bundle.putBoolean( NOSTUDY, noStudyOption );
        return bundle;
    }

    public static Dialog makeDialog( Activity parent, Bundle bundle )
    {
        LookupAlert view = (LookupAlert)
            LocUtils.inflate( parent, R.layout.lookup );
        view.init( parent, bundle );

        Dialog result = LocUtils.makeAlertBuilder( parent )
            .setTitle( R.string.lookup_title )
            .setView( view )
            .create();
        result.setOnKeyListener( view );
        return result;
    }

    protected static void launchWordLookup( Context context, String word,
                                            int langCode )
    {
        setLang( context, langCode );
        lookupWord( context, word, s_lookupUrls[0] );
    }
}
