/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
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

package org.eehouse.android.xw4.loc;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.widget.EditText;
import android.widget.TextView;

import org.eehouse.android.xw4.DelegateBase;
import org.eehouse.android.xw4.R;

public class LocItemEditDelegate extends DelegateBase {

    private static final String KEY = "KEY";
    private Activity m_activity;
    private String m_key;
    private EditText m_edit;

    protected LocItemEditDelegate( Activity activity, Bundle savedInstanceState )
    {
        super( activity, savedInstanceState );
        m_activity = activity;
    }

    protected void init( Bundle savedInstanceState )
    {
        setContentView( R.layout.loc_item_edit );

        String key = getIntent().getStringExtra( KEY );
        m_key = key;

        TextView view = (TextView)findViewById( R.id.english_view );
        view.setText( key );
        view = (TextView)findViewById( R.id.xlated_view_blessed );
        view.setText( LocUtils.getBlessedXlation( m_activity, key, true ) );
        m_edit = (EditText)findViewById( R.id.xlated_view_local );
        m_edit.setText( LocUtils.getLocalXlation( m_activity, key, true ) );

        setLabel( R.id.english_label, R.string.loc_main_english );
        setLabel( R.id.blessed_label, R.string.loc_main_yourlang );
        setLabel( R.id.local_label, R.string.loc_main_yourlang );
    }

    @Override
    protected void onPause()
    {
        // Save any local translation
        CharSequence txt = m_edit.getText();
        if ( null != txt && 0 < txt.length() ) {
            LocUtils.setXlation( m_activity, m_key, txt.toString() );
        }

        super.onPause();
    }

    private void setLabel( int viewID, int strID )
    {
        TextView view = (TextView)findViewById( viewID );
        view.setText( LocUtils.getString( m_activity, strID ) );
    }

    protected static void launch( Context context, LocSearcher.Pair pair )
    {
        Intent intent = new Intent( context, LocItemEditActivity.class );
        String key = pair.getKey();

        intent.putExtra( KEY, key );

        context.startActivity( intent );
    }
}
