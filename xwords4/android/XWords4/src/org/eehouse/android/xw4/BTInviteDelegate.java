/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
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
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.ListActivity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import junit.framework.Assert;

import org.eehouse.android.xw4.DlgDelegate.Action;

public class BTInviteDelegate extends InviteDelegate {

    private Activity m_activity;
    private boolean m_firstScan;
    private Set<Integer> m_checked;
    private boolean m_setChecked;
    private BTDevsAdapter m_adapter;

    public static void launchForResult( Activity activity, int nMissing, 
                                        int requestCode )
    {
        Intent intent = new Intent( activity, BTInviteActivity.class );
        intent.putExtra( INTENT_KEY_NMISSING, nMissing );
        activity.startActivityForResult( intent, requestCode );
    }

    protected BTInviteDelegate( ListDelegator delegator, Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState, R.layout.btinviter );
        m_activity = delegator.getActivity();
    }

    @Override
    protected void init( Bundle savedInstanceState )
    {
        m_checked = new HashSet<Integer>();
        super.init( R.id.button_invite, R.id.button_rescan, 
                    R.id.button_clear, R.id.invite_desc,
                    R.string.invite_bt_desc_fmt );
        m_firstScan = true;
        BTService.clearDevices( m_activity, null ); // will return names
    }

    // MultiService.MultiEventListener interface
    @Override
    public void eventOccurred( MultiService.MultiEvent event, final Object ... args )
    {
        switch( event ) {
        case SCAN_DONE:
            post( new Runnable() {
                    public void run() {
                        synchronized( BTInviteDelegate.this ) {

                            String[] btDevAddrs = null;
                            String[] btDevNames = null;
                            if ( 0 < args.length ) {
                                btDevAddrs = (String[])(args[0]);
                                btDevNames = (String[])(args[1]);
                                if ( null != btDevNames
                                     && 0 == btDevNames.length ) {
                                    btDevNames = null;
                                    btDevAddrs = null;
                                }
                            }

                            m_setChecked = null != btDevNames
                                && m_nMissing == btDevNames.length;
                            m_adapter = new BTDevsAdapter( btDevAddrs, btDevNames );
                            setListAdapter( m_adapter );
                            m_checked.clear();
                            tryEnable();
                            m_firstScan = false;
                        }
                    }
                } );
            break;
        default:
            super.eventOccurred( event, args );
        }
    }

    protected void scan()
    {
        int count = BTService.getPairedCount( m_activity );
        if ( 0 < count ) {
            BTService.scan( m_activity );
        } else {
            showConfirmThen( R.string.bt_no_devs, R.string.button_go_settings, 
                             Action.OPEN_BT_PREFS_ACTION );
        }
    }

    protected void clearSelected()
    {
        BTService.clearDevices( m_activity, listSelected() );
    }

    protected String[] listSelected()
    {
        ListView list = (ListView)findViewById( android.R.id.list );
        String[] result = new String[m_checked.size()];
        int count = list.getChildCount();
        int index = 0;
        for ( int ii = 0; ii < count; ++ii ) {
            CheckBox box = (CheckBox)list.getChildAt( ii );
            if ( box.isChecked() ) {
                String btAddr = m_adapter.getBTAddr( box );
                String btName = m_adapter.getBTName( box );
                Assert.assertTrue( box.getText().toString().equals( btName ) );
                result[index++] = btAddr;
            }
        }
        return result;
    }

    protected void tryEnable() 
    {
        int size = m_checked.size();
        m_okButton.setEnabled( size == m_nMissing );
        m_clearButton.setEnabled( 0 < size );
    }

    private class BTDevsAdapter extends XWListAdapter {
        private String[] m_devAddrs;
        private String[] m_devNames;
        private Map<CheckBox, String> m_boxAddrs;
        public BTDevsAdapter( String[] btAddrs, String[] btNames )
        {
            super( null == btAddrs? 0 : btAddrs.length );
            m_boxAddrs = new HashMap<CheckBox, String>();
            m_devAddrs = btAddrs;
            m_devNames = btNames;
        }

        public Object getItem( int position) { return m_devNames[position]; }
        public View getView( final int position, View convertView, 
                             ViewGroup parent ) {
            CheckBox box = (CheckBox)inflate( R.layout.btinviter_item );
            box.setText( m_devNames[position] );
            m_boxAddrs.put( box, m_devAddrs[position] );

            CompoundButton.OnCheckedChangeListener listener = 
                new CompoundButton.OnCheckedChangeListener() {
                    public void onCheckedChanged( CompoundButton buttonView, 
                                                  boolean isChecked )
                    {
                        if ( isChecked ) {
                            m_checked.add( position );
                        } else {
                            m_checked.remove( position );
                            // User's now making changes; don't check new views
                            m_setChecked = false;
                        }
                        tryEnable();
                    }
                };

            box.setOnCheckedChangeListener( listener );
            if ( m_setChecked ) {
                box.setChecked( true );
            }
            return box;
        }

        public String getBTAddr( CheckBox box ) { return m_boxAddrs.get(box); }
        public String getBTName( CheckBox box ) { return box.getText().toString(); }
    }

    // DlgDelegate.DlgClickNotify interface
    @Override
    public void dlgButtonClicked( Action action, int which, Object[] params )
    {
        switch( action ) {
        case OPEN_BT_PREFS_ACTION:
            if ( AlertDialog.BUTTON_POSITIVE == which ) {
                BTService.openBTSettings( m_activity );
            }
            break;
        default:
            super.dlgButtonClicked( action, which, params );
        }
    }
}
