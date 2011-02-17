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
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;

public class ChatActivity extends XWActivity implements View.OnClickListener {

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
        LinearLayout layout = (LinearLayout)findViewById( R.id.chat_history );
        LayoutInflater factory = LayoutInflater.from( this );
        String local = getString( R.string.chat_local_id );

        for ( String str : parseHistory( history ) ) {
            TextView view = 
                (TextView)factory.inflate( str.startsWith(local)
                                           ? R.layout.chat_history_local
                                           : R.layout.chat_history_remote, 
                                           null );
            view.setText( str );
            layout.addView( view );
        }

        ((Button)findViewById( R.id.send_button )).setOnClickListener( this );
    }

    @Override
    public void onClick( View view ) 
    {
        EditText edit = (EditText)findViewById( R.id.chat_edit );
        String text = edit.getText().toString();
        if ( null == text || text.length() == 0 ) {
            setResult( Activity.RESULT_CANCELED );
        } else {
            DBUtils.appendChatHistory( this, m_path, text, true );

            Intent result = new Intent();
            result.putExtra( "chat", text );
            setResult( Activity.RESULT_OK, result );
        }
        finish();
    }

    private String[] parseHistory( String history )
    {
        return history.split( "\n" );
    }

}
