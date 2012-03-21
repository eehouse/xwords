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
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.CompoundButton;
import android.widget.ListView;
import android.widget.TextView;
import android.os.Handler;

import junit.framework.Assert;

abstract class InviteActivity extends XWListActivity 
    implements View.OnClickListener, 
               CompoundButton.OnCheckedChangeListener {

    public static final String DEVS = "DEVS";
    public static final String INTENT_KEY_NMISSING = "NMISSING";

    protected int m_nMissing;
    protected Button m_okButton;
    protected Button m_rescanButton;
    protected Button m_clearButton;
    protected int m_checkCount = 0;

    protected void onCreate( Bundle savedInstanceState, int view_id,
                             int button_invite, int button_rescan, 
                             int button_clear, int desc_id, int desc_strf )
    {
        super.onCreate( savedInstanceState );
        setContentView( view_id );

        Intent intent = getIntent();
        m_nMissing = intent.getIntExtra( INTENT_KEY_NMISSING, -1 );

        m_okButton = (Button)findViewById( button_invite );
        m_okButton.setOnClickListener( this );
        m_rescanButton = (Button)findViewById( button_rescan );
        m_rescanButton.setOnClickListener( this );
        m_clearButton = (Button)findViewById( button_clear );
        m_clearButton.setOnClickListener( this );

        TextView desc = (TextView)findViewById( desc_id );
        desc.setText( Utils.format( this, desc_strf, m_nMissing ) );

        m_checkCount = 0;
        tryEnable();
    }

    public void onClick( View view ) 
    {
        if ( m_okButton == view ) {
            Intent intent = new Intent();
            String[] devs = listSelected();
            intent.putExtra( DEVS, devs );
            setResult( Activity.RESULT_OK, intent );
            finish();
        } else if ( m_rescanButton == view ) {
            scan();
        } else if ( m_clearButton == view ) {
            clearSelected();
        }
    }

    public void onCheckedChanged( CompoundButton buttonView, 
                                  boolean isChecked )
    {
        DbgUtils.logf( "InviteActivity.onCheckedChanged( isChecked=%b )",
                       isChecked );
        if ( isChecked ) {
            ++m_checkCount;
        } else {
            --m_checkCount;
        }
        tryEnable();
    }

    protected void tryEnable() 
    {
        m_okButton.setEnabled( m_checkCount == m_nMissing );
        m_clearButton.setEnabled( 0 < m_checkCount );
    }

    abstract String[] listSelected();
    abstract void scan();
    abstract void clearSelected();
}