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
import android.widget.FrameLayout;
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

    protected interface InviterItem {
    }

    protected static class TwoStringPair implements InviterItem {
        public String str1;
        public String str2;

        public TwoStringPair( String str1, String str2 ) {
            this.str1 = str1; this.str2 = str2;
        }

        public static TwoStringPair[] make( String[] names, String[] addrs )
        {
            TwoStringPair[] pairs = new TwoStringPair[names.length];
            for ( int ii = 0; ii < pairs.length; ++ii ) {
                pairs[ii] = new TwoStringPair( names[ii], addrs[ii] );
            }
            return pairs;
        }
    }

    // Children implement ...
    abstract void onChildAdded( View child, InviterItem item );
    abstract void listSelected( InviterItem[] selected, String[] devs );

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
    private TextView m_ev;
    private boolean m_showAddrs;
    private InviteItemsAdapter m_adapter;
    protected Map<InviterItem, Integer> m_counts;
    protected Set<Integer> m_checked;
    private boolean m_setChecked;
    // private LinearLayout[] m_items;

    public InviteDelegate( Delegator delegator, Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState, R.layout.inviter, R.menu.empty );
        m_activity = delegator.getActivity();
        Intent intent = getIntent();
        m_nMissing = intent.getIntExtra( INTENT_KEY_NMISSING, -1 );
        m_lastDev = intent.getStringExtra( INTENT_KEY_LASTDEV );
        m_counts = new HashMap<InviterItem, Integer>();
        m_checked = new HashSet<Integer>();
    }

    protected void init( String descTxt, int emptyMsgId )
    {
        m_inviteButton = (Button)findViewById( R.id.button_invite );
        m_inviteButton.setOnClickListener( this );

        TextView descView = (TextView)findViewById( R.id.invite_desc );
        descView.setText( descTxt );

        m_lv = (ListView)findViewById( android.R.id.list );
        m_ev = (TextView)findViewById( android.R.id.empty );
        if ( null != m_lv && null != m_ev && 0 != emptyMsgId ) {
            m_ev.setText( getString( emptyMsgId ) );
            m_lv.setOnHierarchyChangeListener( this );
            showEmptyIfEmpty();
        }

        tryEnable();
    }

    // Subclasses are meant to call this
    protected void addButtonBar( int buttonBarId, int[] buttonBarItemIds )
    {
        FrameLayout container = (FrameLayout)findViewById( R.id.button_bar );
        ViewGroup bar = (ViewGroup)inflate( buttonBarId );
        container.addView( bar );

        View.OnClickListener listener = new View.OnClickListener() {
                @Override
                public void onClick( View view ) {
                    onBarButtonClicked( view.getId() );
                }
            };

        for ( int id : buttonBarItemIds ) {
            bar.findViewById( id ).setOnClickListener( listener );
        }
    }

    protected void updateListAdapter( int itemId, InviterItem[] items )
    {
        // m_items = items;
        m_adapter = new InviteItemsAdapter( itemId, items );
        setListAdapter( m_adapter );
    }

    protected void onBarButtonClicked( int id )
    {
        Assert.fail();          // subclass must implement
    }

    // Subclasses can do something here
    protected void addToButtonBar( FrameLayout container ) {}

    ////////////////////////////////////////
    // View.OnClickListener
    ////////////////////////////////////////
    public void onClick( View view )
    {
        if ( m_inviteButton == view ) {
            int len = m_checked.size();
            String[] devs = new String[len];

            InviterItem[] items = getSelItems();
            listSelected( items, devs );

            int[] counts = new int[len];
            for ( int ii = 0; ii < len; ++ii ) {
                counts[ii] = m_counts.get( items[ii] );
            }

            Intent intent = new Intent();
            intent.putExtra( DEVS, devs );
            intent.putExtra( COUNTS, counts );
            setResult( Activity.RESULT_OK, intent );
            finish();
        }
    }

    private InviterItem[] getSelItems()
    {
        int ii = 0;
        InviterItem[] result = new InviterItem[m_checked.size()];
        InviterItem[] src = getAdapter().getItems();
        for ( int checked : m_checked ) {
            result[ii++] = src[checked];
        }
        return result;
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

    protected void tryEnable()
    {
        int count = m_checked.size();
        m_inviteButton.setEnabled( count > 0 && count <= m_nMissing );
        // if ( null != m_clearButton ) {
        //     m_clearButton.setEnabled( count > 0 );
        // }
    }

    final Set<Integer> getChecked() { return m_checked; }

    protected void clearChecked() { m_checked.clear(); }

    // protected void scan() {}

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
        private InviterItem[] m_items;
        private int m_itemId;

        public InviteItemsAdapter( int itemID, InviterItem[] items )
        {
            super( null == items? 0 : items.length );
            m_itemId = itemID;
            m_items = items;
            // m_items = new LinearLayout[getCount()];
        }

        public InviterItem[] getItems() { return m_items; }

        // public String[] getAddrs() { return m_devAddrs; }

        @Override
        public Object getItem( int position ) { return m_items[position]; }

        @Override
        public View getView( final int position, View convertView,
                             ViewGroup parent )
        {
            final InviterItem item = m_items[position];
            final LinearLayout layout = (LinearLayout)
                inflate( R.layout.inviter_item_frame );
            CheckBox box = (CheckBox)layout.findViewById( R.id.inviter_check );

            // Give subclass a chance to install and populate its view
            FrameLayout frame = (FrameLayout)layout.findViewById( R.id.frame );
            View child = inflate( m_itemId );
            frame.addView( child );
            onChildAdded( child, m_items[position] );

            m_counts.put( item, 1 );
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
                            m_counts.put( item, 1 + pos );
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
                        if ( isChecked ) {
                            m_checked.add( position );
                        } else {
                            m_checked.remove( position );
                        //     // User's now making changes; don't check new views
                        //     m_setChecked = false;
                        }
                        onItemChecked( position, isChecked );

                        tryEnable();
                    }
                };
            box.setOnCheckedChangeListener( listener );

            if ( m_setChecked || m_checked.contains( position ) ) {
                box.setChecked( true );
            } else if ( null != m_lastDev && m_lastDev.equals( item ) ) {
                m_lastDev = null;
                box.setChecked( true );
            }
            // m_items[position] = layout;
            return layout;
        }

        public String getAddr( CheckBox box ) { return (String)box.getTag(); }
        public String getName( CheckBox box ) { return box.getText().toString(); }
    }

}
