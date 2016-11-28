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
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.Spinner;
import android.widget.TextView;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

import junit.framework.Assert;

abstract class InviteDelegate extends ListDelegateBase
    implements View.OnClickListener,
               ViewGroup.OnHierarchyChangeListener {
    private static final String TAG = InviteDelegate.class.getSimpleName();

    public static final String DEVS = "DEVS";
    public static final String COUNTS = "COUNTS";
    protected static final String INTENT_KEY_NMISSING = "NMISSING";
    protected static final String INTENT_KEY_LASTDEV = "LDEV";

    protected int m_nMissing;
    protected String m_lastDev;
    protected Button m_inviteButton;
    protected Button m_rescanButton;
    protected Button m_clearButton;
    private Activity m_activity;
    private ListView m_lv;
    private View m_ev;
    private boolean m_showAddrs;
    private InviteItemsAdapter m_adapter;
    protected Map<String, Integer> m_counts;
    protected Set<Integer> m_checked;
    private boolean m_setChecked;
    private LinearLayout[] m_items;

    public InviteDelegate( Delegator delegator, Bundle savedInstanceState,
                           int layoutID )
    {
        super( delegator, savedInstanceState, layoutID, R.menu.empty );
        m_activity = delegator.getActivity();
        Intent intent = getIntent();
        m_nMissing = intent.getIntExtra( INTENT_KEY_NMISSING, -1 );
        m_lastDev = intent.getStringExtra( INTENT_KEY_LASTDEV );
        m_counts = new HashMap<String, Integer>();
        m_checked = new HashSet<Integer>();
    }

    protected void init( int button_invite, int desc_id, String descTxt )
    {
        init( button_invite, 0, 0, desc_id, descTxt );
    }

    protected void init( int button_invite, int button_rescan,
                         int button_clear, int desc_id, String descTxt )
    {
        m_inviteButton = (Button)findViewById( button_invite );
        m_inviteButton.setOnClickListener( this );
        if ( 0 != button_rescan ) {
            m_rescanButton = (Button)findViewById( button_rescan );
            m_rescanButton.setOnClickListener( this );
        }
        if ( 0 != button_clear ) {
            m_clearButton = (Button)findViewById( button_clear );
            m_clearButton.setOnClickListener( this );
        }

        TextView descView = (TextView)findViewById( desc_id );
        descView.setText( descTxt );

        m_lv = (ListView)findViewById( android.R.id.list );
        m_ev = findViewById( android.R.id.empty );
        if ( null != m_lv && null != m_ev ) {
            m_lv.setOnHierarchyChangeListener( this );
            showEmptyIfEmpty();
        }

        tryEnable();
    }

    protected void updateListAdapter( int itemId, String[] names, String[] addrs,
                                      boolean showAddrs )
    {
        m_showAddrs = showAddrs;
        m_adapter = new InviteItemsAdapter( itemId, names, addrs );
        setListAdapter( m_adapter );
    }

    ////////////////////////////////////////
    // View.OnClickListener
    ////////////////////////////////////////
    public void onClick( View view )
    {
        if ( m_inviteButton == view ) {
            Intent intent = new Intent();
            String[][] devs = new String[1][];
            int[][] counts = new int[1][];
            listSelected( devs, counts );
            intent.putExtra( DEVS, devs[0] );
            intent.putExtra( COUNTS, counts[0] );
            setResult( Activity.RESULT_OK, intent );
            finish();
        } else if ( m_rescanButton == view ) {
            scan();
        } else if ( m_clearButton == view ) {
            clearSelected( makeCheckedArray() );
        }
    }

    ////////////////////////////////////////
    // ViewGroup.OnHierarchyChangeListener
    ////////////////////////////////////////
    public void onChildViewAdded( View parent, View child )
    {
        showEmptyIfEmpty();
    }
    public void onChildViewRemoved( View parent, View child )
    {
        showEmptyIfEmpty();
    }

    private void showEmptyIfEmpty()
    {
        m_ev.setVisibility( 0 == m_lv.getChildCount()
                            ? View.VISIBLE : View.GONE );
    }

    protected void listSelected( String[][] devsP, int[][] countsP )
    {
        int[] counts = null;
        int len = m_checked.size();
        Assert.assertTrue( 0 < len );

        if ( null != countsP ) {
            counts = new int[len];
            countsP[0] = counts;
        }

        String[] checked = new String[len];
        Iterator<Integer> iter = m_checked.iterator();
        for ( int ii = 0; iter.hasNext(); ++ii ) {
            int index = iter.next();
            String addr = m_adapter.getAddrs()[index];
            Assert.assertNotNull( addr ); // fired!!!
            checked[ii] = addr;
            if ( null != counts ) {
                counts[ii] = m_counts.get( addr );
            }
        }
        DbgUtils.logd( TAG, "listSelected() adding %s",
                       checked.toString() );
        devsP[0] = checked;
    }

    protected void tryEnable()
    {
        int count = m_checked.size();
        m_inviteButton.setEnabled( count > 0 && count <= m_nMissing );
        if ( null != m_clearButton ) {
            m_clearButton.setEnabled( count > 0 );
        }
    }

    protected void scan() {}

    protected void clearSelected( Integer[] itemIndices )
    {
        for ( Iterator<Integer> iter = m_checked.iterator();
              iter.hasNext(); ) {
            int index = iter.next();
            LinearLayout item = m_items[index];
            CheckBox box = (CheckBox)item.findViewById( R.id.inviter_check );
            if ( null != box ) {
                box.setChecked( false );
            }
            m_checked.remove( iter );
        }
    }

    // callbacks made by InviteItemsAdapter

    protected void onItemChecked( int index, boolean checked )
    {
        DbgUtils.logd( TAG, "onItemChecked(%d, %b)", index, checked );
        if ( checked ) {
            m_checked.add( index );
        } else {
            m_checked.remove( index );
        }
    }

    protected InviteItemsAdapter getAdapter()
    {
        return m_adapter;
    }

    private Integer[] makeCheckedArray()
    {
        return m_checked.toArray( new Integer[m_checked.size()] );
    }

    private class InviteItemsAdapter extends XWListAdapter {
        private String[] m_devAddrs;
        private String[] m_devNames;
        private int m_itemId;

        public InviteItemsAdapter( int itemID, String[] names, String[] addrs )
        {
            super( null == addrs? 0 : addrs.length );
            m_itemId = itemID;
            m_devAddrs = addrs;
            m_devNames = names;
            m_items = new LinearLayout[getCount()];
        }

        public String[] getAddrs() { return m_devAddrs; }

        @Override
        public Object getItem( int position ) { return m_devNames[position]; }

        @Override
        public View getView( final int position, View convertView,
                             ViewGroup parent )
        {
            final String addr = m_devAddrs[position];
            final LinearLayout layout = (LinearLayout)inflate( m_itemId );
            CheckBox box = (CheckBox)layout.findViewById( R.id.inviter_check );
            box.setText( m_devNames[position] );
            box.setTag( addr );

            m_counts.put( addr, 1 );
            if ( XWPrefs.getCanInviteMulti( m_activity ) && 1 < m_nMissing ) {
                Spinner spinner = (Spinner)
                    layout.findViewById(R.id.nperdev_spinner);
                ArrayAdapter<String> adapter =
                    new ArrayAdapter<String>( m_activity, android.R.layout
                                              .simple_spinner_item );
                for ( int ii = 1; ii <= m_nMissing; ++ii ) {
                    String str = getQuantityString( R.plurals.nplayers_fmt, ii, ii );
                    adapter.add( str );
                }
                spinner.setAdapter( adapter );
                spinner.setVisibility( View.VISIBLE );
                spinner.setOnItemSelectedListener( new OnItemSelectedListener() {
                        public void onItemSelected( AdapterView<?> parent,
                                                    View view, int pos,
                                                    long id )
                        {
                            m_counts.put( addr, 1 + pos );
                            tryEnable();
                        }

                        public void onNothingSelected( AdapterView<?> parent ) {}
                    } );
            }

            CompoundButton.OnCheckedChangeListener listener =
                new CompoundButton.OnCheckedChangeListener() {
                    public void onCheckedChanged( CompoundButton buttonView,
                                                  boolean isChecked ) {
                        if ( !isChecked ) {
                            m_setChecked = false;
                        }
                        // if ( isChecked ) {
                        //     m_checked.add( position );
                        // } else {
                        //     m_checked.remove( position );
                        //     // User's now making changes; don't check new views
                        //     m_setChecked = false;
                        // }
                        onItemChecked( position, isChecked );

                        tryEnable();
                    }
                };
            box.setOnCheckedChangeListener( listener );

            if ( m_setChecked || m_checked.contains( position ) ) {
                box.setChecked( true );
            } else if ( null != m_lastDev && m_lastDev.equals( addr ) ) {
                m_lastDev = null;
                box.setChecked( true );
            }
            m_items[position] = layout;
            return layout;
        }

        public String getAddr( CheckBox box ) { return (String)box.getTag(); }
        public String getName( CheckBox box ) { return box.getText().toString(); }
    }
}
