/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;

import junit.framework.Assert;

public class NBSInviteActivity extends XWActivity
    implements View.OnClickListener {
    public static final String DEVS = "DEVS";
    public static final String INTENT_KEY_NMISSING = "NMISSING";

    private Button m_okButton;
    private EditText m_edit;
    private int m_nMissing;

    @Override
    protected void onCreate( Bundle savedInstanceState )
    {
        super.onCreate( savedInstanceState );

        Intent intent = getIntent();
        m_nMissing = intent.getIntExtra( INTENT_KEY_NMISSING, -1 );
        Assert.assertTrue( 1 == m_nMissing );

        setContentView( R.layout.nbsinviter );

        m_okButton = (Button)findViewById( R.id.button_invite );
        m_okButton.setOnClickListener( this );
    }

    public void onClick( View view ) 
    {
        if ( m_okButton == view ) {
            EditText edit = (EditText)findViewById( R.id.phone_edit );
            String phone = edit.getText().toString();
            if ( null != phone && 0 < phone.length() ) {
                Intent intent = new Intent();
                String[] devs = { phone };
                intent.putExtra( DEVS, devs );
                setResult( Activity.RESULT_OK, intent );
                finish();
            } else {
                showOKOnlyDialog( R.string.err_no_phone );
            }
        }
    }
}
