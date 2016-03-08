/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2011 by Eric House (xwords@eehouse.org).  All rights
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
import android.app.AlertDialog;
import android.content.Intent;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.EditText;
import android.widget.ScrollView;
import android.widget.TableLayout;
import android.widget.TableRow;
import android.widget.TextView;

import junit.framework.Assert;

import org.eehouse.android.xw4.DlgDelegate.Action;

public class ChatDelegate extends DelegateBase {

    private static final String INTENT_KEY_PLAYER = "intent_key_player";
    private static final String INTENT_KEY_NAMES = "intent_key_names";
    private static final String INTENT_KEY_LOCS = "intent_key_locs";
    
    private long m_rowid;
    private int m_curPlayer;
    private Activity m_activity;
    private EditText mEdit;

    public ChatDelegate( Delegator delegator, Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState, R.layout.chat, R.menu.chat_menu );
        m_activity = delegator.getActivity();
    }

    @Override
    protected void init( Bundle savedInstanceState ) 
    {
        if ( BuildConstants.CHAT_SUPPORTED ) {
            mEdit = (EditText)findViewById( R.id.chat_edit );
            mEdit.addTextChangedListener( new TextWatcher() {
                    public void afterTextChanged( Editable s ) {
                        invalidateOptionsMenuIf();
                    }
                    public void beforeTextChanged( CharSequence s, int st, 
                                                   int cnt, int a ) {}
                    public void onTextChanged( CharSequence s, int start, 
                                               int before, int count ) {}
                } );

            Intent intent = getIntent();
            m_rowid = intent.getLongExtra( GameUtils.INTENT_KEY_ROWID, -1 );
            m_curPlayer = intent.getIntExtra( INTENT_KEY_PLAYER, -1 );
            String[] names = intent.getStringArrayExtra( INTENT_KEY_NAMES );
            boolean[] locals = intent.getBooleanArrayExtra( INTENT_KEY_LOCS );
     
            DBUtils.HistoryPair[] pairs
                = DBUtils.getChatHistory( m_activity, m_rowid, locals );
            if ( null != pairs ) {
                TableLayout layout = (TableLayout)
                    findViewById( R.id.chat_history );

                for ( DBUtils.HistoryPair pair : pairs ) {
                    TableRow row = (TableRow)inflate( R.layout.chat_row );
                    if ( m_curPlayer == pair.playerIndx ) {
                        row.setBackgroundColor(0xFF202020);
                    }
                    TextView view = (TextView)row.findViewById( R.id.chat_row_text );
                    view.setText( pair.msg );
                    view = (TextView)row.findViewById( R.id.chat_row_name );
                    view.setText( names[pair.playerIndx] );
                    layout.addView( row );
                }
            }

            final ScrollView scroll = (ScrollView)findViewById( R.id.scroll );
            scroll.post(new Runnable() {            
                    @Override
                    public void run() {
                        scroll.fullScroll(View.FOCUS_DOWN);              
                    }
                });

            String title = getString( R.string.chat_title_fmt, 
                                      GameUtils.getName( m_activity, m_rowid ) );
            setTitle( title );
        } else {
            // Should really assert....
            finish();
        }
    }

    @Override
    public boolean onPrepareOptionsMenu( Menu menu )
    {
        String text = mEdit.getText().toString();
        boolean haveText = null != text && 0 < text.length();
        Utils.setItemVisible( menu, R.id.chat_menu_send, haveText );
        return true;
    }

    @Override
    public boolean onOptionsItemSelected( MenuItem item ) 
    {
        boolean handled = true;
        switch ( item.getItemId() ) {
        case R.id.chat_menu_clear:
            if ( handled ) {
                showConfirmThen( R.string.confirm_clear_chat, Action.CLEAR_ACTION );
            }
            break;
        case R.id.chat_menu_send:
            String text = mEdit.getText().toString();
            if ( null == text || text.length() == 0 ) {
                setResult( Activity.RESULT_CANCELED );
            } else {
                DBUtils.appendChatHistory( m_activity, m_rowid, text, m_curPlayer );

                Intent result = new Intent();
                result.putExtra( BoardDelegate.INTENT_KEY_CHAT, text );
                setResult( Activity.RESULT_OK, result );
            }
            finish();
            break;
        default:
            handled = false;
            break;
        }
        return handled;
    }

    @Override
    public void dlgButtonClicked( Action action, int which, Object[] params )
    {
        switch ( action ) {
        case CLEAR_ACTION:
            if ( AlertDialog.BUTTON_POSITIVE == which ) {
                DBUtils.clearChatHistory( m_activity, m_rowid );
                TableLayout layout = 
                    (TableLayout)findViewById( R.id.chat_history );
                layout.removeAllViews();
            }
            break;
        default:
            super.dlgButtonClicked( action, which, params );
        }
    }


    public static void startForResult( Activity parent, RequestCode requestCode,
                                       long rowID, int curPlayer, 
                                       String[] names, boolean[] locs )
    {
        Assert.assertFalse( -1 == curPlayer );
        Intent intent = new Intent( parent, ChatActivity.class );
        intent.putExtra( GameUtils.INTENT_KEY_ROWID, rowID );
        intent.putExtra( INTENT_KEY_PLAYER, curPlayer );
        intent.putExtra( INTENT_KEY_NAMES, names );
        intent.putExtra( INTENT_KEY_LOCS, locs );
        parent.startActivityForResult( intent, requestCode.ordinal() );
    }
}
