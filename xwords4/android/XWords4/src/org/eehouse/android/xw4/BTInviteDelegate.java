/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009-2015 by Eric House (xwords@eehouse.org).  All rights
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
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.Spinner;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.Iterator;

import junit.framework.Assert;

import org.eehouse.android.xw4.DlgDelegate.Action;

public class BTInviteDelegate extends InviteDelegate {

    private Activity m_activity;
    private Set<String> m_checked;
    private Map<String, Integer> m_counts;
    private boolean m_setChecked;
    private BTDevsAdapter m_adapter;

    public static void launchForResult( Activity activity, int nMissing, 
                                        int requestCode )
    {
        Assert.assertTrue( 0 < nMissing ); // don't call if nMissing == 0
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
        m_checked = new HashSet<String>();
        m_counts = new HashMap<String, Integer>();

        String msg = getString( R.string.bt_pick_addall_button );
        msg = getString( R.string.invite_bt_desc_fmt, m_nMissing, msg );
        super.init( R.id.button_invite, R.id.button_rescan, 
                    R.id.button_clear, R.id.invite_desc, msg );
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
        String[][] selected = new String[1][];
        listSelected( selected, null );
        BTService.clearDevices( m_activity, selected[0] );
    }

    protected void listSelected( String[][] devsP, int[][] countsP )
    {
        int size = m_checked.size();
        int[] counts = null;
        String[] devs = new String[size];
        devsP[0] = devs;
        if ( null != countsP ) {
            counts = new int[size];
            countsP[0] = counts;
        }

        int nxt = 0;
        for ( Iterator<String> iter = m_checked.iterator();
              iter.hasNext(); ) {
            String btAddr = iter.next();
            devs[nxt] = btAddr;
            if ( null != counts ) {
                counts[nxt] = m_counts.get( btAddr );
            }
            ++nxt;
        }
    }

    protected void tryEnable() 
    {
        String[][] devs = new String[1][];
        int[][] counts = new int[1][];
        listSelected( devs, counts );

        m_clearButton.setEnabled( 0 < devs[0].length );

        int count = 0;
        for ( int one : counts[0] ) {
            count += one;
        }
        m_okButton.setEnabled( 0 < count && count <= m_nMissing );
    }

    private class BTDevsAdapter extends XWListAdapter {
        private String[] m_devAddrs;
        private String[] m_devNames;
        public BTDevsAdapter( String[] btAddrs, String[] btNames )
        {
            super( null == btAddrs? 0 : btAddrs.length );
            m_devAddrs = btAddrs;
            m_devNames = btNames;
        }

        public Object getItem( int position ) { return m_devNames[position]; }

        public View getView( int position, View convertView, ViewGroup parent ) {
            final String btAddr = m_devAddrs[position];
            LinearLayout layout = (LinearLayout)inflate( R.layout.btinviter_item );
            CheckBox box = (CheckBox)layout.findViewById( R.id.inviter_check );
            box.setText( m_devNames[position] );
            box.setTag( btAddr );

            m_counts.put( btAddr, 1 );
            if ( XWPrefs.getCanInviteMulti( m_activity ) && 1 < m_nMissing ) {
                Spinner spinner = (Spinner)
                    layout.findViewById(R.id.nperdev_spinner);
                ArrayAdapter<String> adapter = 
                    new ArrayAdapter<String>( m_activity, android.R.layout
                                              .simple_spinner_item );
                for ( int ii = 1; ii <= m_nMissing; ++ii ) {
                    String str = getString( R.string.nplayers_fmt, ii );
                    adapter.add( str );
                }
                spinner.setAdapter( adapter );
                spinner.setVisibility( View.VISIBLE );
                spinner.setOnItemSelectedListener( new OnItemSelectedListener() {
                        public void onItemSelected( AdapterView<?> parent, 
                                                    View view, int pos, 
                                                    long id )
                        {
                            m_counts.put( btAddr, 1 + pos );
                            tryEnable();
                        }

                        public void onNothingSelected( AdapterView<?> parent ) {}
                    } );
            }

            CompoundButton.OnCheckedChangeListener listener = 
                new CompoundButton.OnCheckedChangeListener() {
                    public void onCheckedChanged( CompoundButton buttonView, 
                                                  boolean isChecked )
                    {
                        if ( isChecked ) {
                            m_checked.add( btAddr );
                        } else {
                            m_checked.remove( btAddr );
                            // User's now making changes; don't check new views
                            m_setChecked = false;
                        }
                        tryEnable();
                    }
                };
            box.setOnCheckedChangeListener( listener );

            if ( m_setChecked || m_checked.contains( btAddr ) ) {
                box.setChecked( true );
            }
            return layout;
        }

        public String getBTAddr( CheckBox box ) { return (String)box.getTag(); }
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
