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

import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.view.Window;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;
import android.app.AlertDialog;
import java.util.ArrayList;
import android.util.AttributeSet;

import junit.framework.Assert;

public class LookupView extends LinearLayout
    implements View.OnClickListener,
               AdapterView.OnItemClickListener,
               DialogInterface.OnDismissListener {

    private static final int STATE_DONE = 0;
    private static final int STATE_WORDS = 1;
    private static final int STATE_URLS = 2;
    private static final int STATE_LOOKUP = 3;

    private static String[] s_langCodes;
    private static String[] s_lookupNames;
    private static String[] s_lookupUrls;
    private static ArrayAdapter<String> s_urlsAdapter;
    private static final int LIST_LAYOUT = // android.R.layout.simple_list_item_1;
        // android.R.layout.select_dialog_item;
        R.layout.select_dialog_item;
    
    private static int s_lang = -1;

    private String[] m_words;
    private boolean m_forceList;
    private static int m_lang;
    private int m_wordIndex = 0;
    private int m_urlIndex = 0;
    private int m_state;
    private ArrayAdapter<String> m_wordsAdapter;
    private Button m_doneButton;
    private TextView m_summary;
    private Dialog m_dialog;
    private Context m_context;
    private int m_dlgId;
    private ListView m_list;

    public LookupView( Context cx, AttributeSet as ) {
        super( cx, as );
        m_context = cx;
    }

    public void setWords( String[] words, int lang, boolean forceList ) 
    {
        m_words = words;
        m_forceList = forceList;
        setLang( lang );

        m_state = STATE_DONE;
        adjustState( 1 );

        m_wordsAdapter = new ArrayAdapter<String>( m_context, LIST_LAYOUT, 
                                                   m_words );
        m_list = (ListView)findViewById( R.id.list );
        m_list.setOnItemClickListener( this );

        m_doneButton = (Button)findViewById( R.id.button_done );
        m_doneButton.setOnClickListener( this );
        m_summary = (TextView)findViewById( R.id.summary );

        switchState();
    }

    public void setDialog( Dialog dialog, int id )
    {
        m_dialog = dialog;
        m_dlgId = id;
        m_dialog.setOnDismissListener( this );
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

    /* DialogInterface.OnDismissListener interface */
    public void onDismiss( DialogInterface di ) 
    {
        m_dialog.getOwnerActivity().removeDialog( m_dlgId );
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
                ( 1 >= s_lookupUrls.length && !m_forceList ) ) {
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
            m_dialog.dismiss();
            break;
        case STATE_WORDS:
            m_list.setAdapter( m_wordsAdapter );
            setSummary( R.string.title_lookup );
            m_doneButton.setText( R.string.button_done );
            break;
        case STATE_URLS:
            m_list.setAdapter( s_urlsAdapter );
            setSummary( m_words[m_wordIndex] );
            String txt = Utils.format( m_context, R.string.button_donef,
                                       m_words[m_wordIndex] );
            m_doneButton.setText( txt );
            break;
        case STATE_LOOKUP:
            lookupWord( m_words[m_wordIndex], s_lookupUrls[m_urlIndex] );
            switchState( -1 );
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
                m_context.startActivity( intent );
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
            s_urlsAdapter = new ArrayAdapter<String>( m_context, LIST_LAYOUT, 
                                                      s_lookupNames );
            s_lang = lang;
        } // initLookup
    }

    private void setSummary( int id )
    {
        m_summary.setText( m_context.getString( id ) );
    }

    private void setSummary( String word )
    {
        String title = Utils.format( m_context, R.string.pick_url_titlef, word );
        m_summary.setText( title );
    }
}
