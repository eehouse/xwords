/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
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
import android.content.Intent;
import android.os.Bundle;
import android.text.TextWatcher;
import android.text.method.KeyListener;
import android.widget.EditText;
import android.widget.TextView;
import android.view.View;
import android.view.KeyEvent;
import android.text.Editable;
import android.net.Uri;

public class ChatActivity extends XWActivity {

    private String m_path;

    @Override
    public void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );

        setContentView( R.layout.chat );

        Intent intent = getIntent();
        m_path = intent.getData().getPath();
        if ( m_path.charAt(0) == '/' ) {
            m_path = m_path.substring( 1 );
        }
        
        String history = DBUtils.getChatHistory( this, m_path );
        Utils.logf( "got history: %s", null == history? "null" : history );
        TextView textView = (TextView)findViewById( R.id.chat_history );
        textView.setText( history );

        final EditText edit = (EditText)findViewById( R.id.chat_edit );
        edit.setOnKeyListener( new View.OnKeyListener() {
                public boolean onKey(View v, int keyCode, KeyEvent event) {
                    boolean consumed = KeyEvent.ACTION_DOWN == event.getAction()
                        && KeyEvent.KEYCODE_ENTER == keyCode;
                    if ( consumed ) {
                        String text = edit.getText().toString();
                        if ( null == text || text.length() == 0 ) {
                            setResult( Activity.RESULT_CANCELED );
                        } else {
                            DBUtils.appendChatHistory( ChatActivity.this, 
                                                       m_path, text, true );

                            Intent result = new Intent();
                            result.putExtra( "chat", text );
                            setResult( Activity.RESULT_OK, result );
                        }
                        finish();
                    }
                    return consumed;
                } 
            } );
    }
}
