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
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.ListView;
import android.os.Handler;

import junit.framework.Assert;

public class BTInviteActivity extends XWListActivity 
    implements View.OnClickListener, 
               CompoundButton.OnCheckedChangeListener {

    public static final String DEVS = "DEVS";
    public static final String INTENT_KEY_NMISSING = "NMISSING";

    private Button m_okButton;
    private Button m_rescanButton;
    private Button m_reconfigureButton;
    private String[] m_btDevNames;
    private int m_nMissing;
    private Handler m_handler;
    private int m_checkCount = 0;

    @Override
    protected void onCreate( Bundle savedInstanceState )
    {
        super.onCreate( savedInstanceState );

        Intent intent = getIntent();
        m_nMissing = intent.getIntExtra( INTENT_KEY_NMISSING, -1 );

        setContentView( R.layout.btinviter );

        m_okButton = (Button)findViewById( R.id.button_ok );
        m_okButton.setOnClickListener( this );
        m_rescanButton = (Button)findViewById( R.id.button_rescan );
        m_rescanButton.setOnClickListener( this );
        m_reconfigureButton = (Button)findViewById( R.id.button_reconfigure );
        m_reconfigureButton.setOnClickListener( this );

        m_handler = new Handler();

        m_checkCount = 0;
        tryEnable();
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        if ( null == m_btDevNames ) {
            rescan();
        }
    }

    public void onClick( View view ) 
    {
        DbgUtils.logf( "onClick" );
        if ( m_okButton == view ) {
            DbgUtils.logf( "OK BUTTON" );
            Intent intent = new Intent();
            String[] devs = listSelected();
            intent.putExtra( DEVS, devs );
            setResult( Activity.RESULT_OK, intent );
            finish();
        } else if ( m_rescanButton == view ) {
            rescan();
        } else if ( m_reconfigureButton == view ) {
        }
    }

    // /* AdapterView.OnItemClickListener */
    // public void onItemClick( AdapterView<?> parent, View view, 
    //                          int position, long id )
    // {
    //     DbgUtils.logf( "BTInviteActivity.onItemClick(position=%d)", position );
    // }

    public void onCheckedChanged( CompoundButton buttonView, 
                                  boolean isChecked )
    {
        DbgUtils.logf( "BTInviteActivity.onCheckedChanged( isChecked=%b )",
                       isChecked );
        if ( isChecked ) {
            ++m_checkCount;
        } else {
            --m_checkCount;
        }
        tryEnable();
    }

    // BTService.BTEventListener interface
    @Override
    public void eventOccurred( BTService.BTEvent event, final Object ... args )
    {
        switch( event ) {
        case SCAN_DONE:
            m_handler.post( new Runnable() {
                    public void run() {
                        synchronized( BTInviteActivity.this ) {
                            stopProgress();
                            if ( 0 < args.length ) {
                                m_btDevNames = (String[])(args[0]);
                            }
                            setListAdapter( new BTDevsAdapter( m_btDevNames ) );
                            m_checkCount = 0;
                            tryEnable();
                        }
                    }
                } );
            break;
        default:
            super.eventOccurred( event, args );
        }
    }

    private void rescan()
    {
        startProgress( R.string.scan_progress );
        BTService.rescan( this );
    }

    private String[] listSelected()
    {
        ListView list = (ListView)findViewById( android.R.id.list );
        String[] result = new String[m_checkCount];
        int count = list.getChildCount();
        int index = 0;
        for ( int ii = 0; ii < count; ++ii ) {
            CheckBox box = (CheckBox)list.getChildAt( ii );
            if ( box.isChecked() ) {
                result[index++] = box.getText().toString();
            }
        }
        return result;
    }

    private void tryEnable() 
    {
        m_okButton.setEnabled( m_checkCount == m_nMissing );
    }


    private class BTDevsAdapter extends XWListAdapter {
        private String[] m_devs;
        public BTDevsAdapter( String[] devs )
        {
            super( devs.length );
            m_devs = devs;
        }

        public Object getItem( int position) { return m_devs[position]; }
        public View getView( final int position, View convertView, 
                             ViewGroup parent ) {
            CheckBox box = (CheckBox)
                Utils.inflate( BTInviteActivity.this, R.layout.btinviter_item );
            box.setText( m_devs[position] );
            box.setOnCheckedChangeListener( BTInviteActivity.this );
            return box;
        }

    }

}
