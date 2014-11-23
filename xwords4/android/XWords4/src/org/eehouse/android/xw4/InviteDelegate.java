/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009-2014 by Eric House (xwords@eehouse.org).  All
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
import android.app.ListActivity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import junit.framework.Assert;

abstract class InviteDelegate extends ListDelegateBase
    implements View.OnClickListener {

    public static final String DEVS = "DEVS";
    protected static final String INTENT_KEY_NMISSING = "NMISSING";

    protected int m_nMissing;
    protected Button m_okButton;
    protected Button m_rescanButton;
    protected Button m_clearButton;
    private Activity m_activity;

    public InviteDelegate( ListDelegator delegator, Bundle savedInstanceState,
                           int layoutID )
    {
        super( delegator, savedInstanceState, layoutID, R.menu.empty );
        m_activity = delegator.getActivity();
        Intent intent = getIntent();
        m_nMissing = intent.getIntExtra( INTENT_KEY_NMISSING, -1 );
    }

    protected void init( int button_invite, int button_rescan, 
                         int button_clear, int desc_id, String descTxt )
    {
        m_okButton = (Button)findViewById( button_invite );
        m_okButton.setOnClickListener( this );
        m_rescanButton = (Button)findViewById( button_rescan );
        m_rescanButton.setOnClickListener( this );
        m_clearButton = (Button)findViewById( button_clear );
        m_clearButton.setOnClickListener( this );

        TextView descView = (TextView)findViewById( desc_id );
        descView.setText( descTxt );

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

    abstract void tryEnable() ;
    abstract String[] listSelected();
    abstract void scan();
    abstract void clearSelected();
}
