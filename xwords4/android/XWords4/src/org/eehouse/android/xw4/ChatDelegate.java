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
import android.view.MenuItem;
import android.view.View;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import org.eehouse.android.xw4.DlgDelegate.Action;

public class ChatDelegate extends DelegateBase {

    private long m_rowid;
    private Activity m_activity;

    public ChatDelegate( Delegator delegator, Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState, R.layout.chat, R.menu.chat_menu );
        m_activity = delegator.getActivity();
    }

    @Override
    protected void init( Bundle savedInstanceState ) 
    {
        if ( BuildConstants.CHAT_SUPPORTED ) {
            m_rowid = getIntent().getLongExtra( GameUtils.INTENT_KEY_ROWID, -1 );
     
            DBUtils.HistoryPair[] pairs = DBUtils.getChatHistory( m_activity, m_rowid );
            if ( null != pairs ) {
                LinearLayout layout = (LinearLayout)
                    findViewById( R.id.chat_history );

                for ( DBUtils.HistoryPair pair : pairs ) {
                    TextView view = (TextView)
                        inflate( pair.sourceLocal
                                 ? R.layout.chat_history_local
                                 : R.layout.chat_history_remote );
                    view.setText( pair.msg );
                    layout.addView( view );
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
            EditText edit = (EditText)findViewById( R.id.chat_edit );
            String text = edit.getText().toString();
            if ( null == text || text.length() == 0 ) {
                setResult( Activity.RESULT_CANCELED );
            } else {
                DBUtils.appendChatHistory( m_activity, m_rowid, text, true );

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
                LinearLayout layout = 
                    (LinearLayout)findViewById( R.id.chat_history );
                layout.removeAllViews();
            }
            break;
        default:
            super.dlgButtonClicked( action, which, params );
        }
    }
}
