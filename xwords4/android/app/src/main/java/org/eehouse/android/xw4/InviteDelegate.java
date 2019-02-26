/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
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

import java.io.Serializable;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.eehouse.android.xw4.DBUtils.SentInvitesInfo;

abstract class InviteDelegate extends ListDelegateBase
    implements View.OnClickListener,
               ViewGroup.OnHierarchyChangeListener {
    private static final String TAG = InviteDelegate.class.getSimpleName();

    protected interface InviterItem {
        boolean equals(InviterItem item);
        String getDev();        // the string that identifies this item in results
    }

    protected static class TwoStringPair implements InviterItem, Serializable {
        private String mDev;
        public String str2;

        public TwoStringPair( String dev, String str2 ) {
            mDev = dev; this.str2 = str2;
        }

        public String getDev() { return mDev; }

        public boolean equals( InviterItem item )
        {
            boolean result = false;
            if ( null != item ) {
                TwoStringPair pair = (TwoStringPair)item;
                result = mDev.equals( pair.mDev )
                    && ((null == str2 && null == pair.str2)
                        || str2.equals( pair.str2 ) );
                Log.d( TAG, "%s.equals(%s) => %b", mDev, pair.mDev, result );
            }
            return result;
        }
    }

    public static final String DEVS = "DEVS";
    public static final String COUNTS = "COUNTS";
    public static final String RAR = "RAR";
    private static final String INTENT_KEY_NMISSING = "NMISSING";
    protected static final String INTENT_KEY_LASTDEV = "LDEV";

    protected int m_nMissing;
    protected String m_lastDev;
    protected Button m_inviteButton;
    private Activity m_activity;
    private ListView m_lv;
    private TextView m_ev;
    private InviteItemsAdapter m_adapter;
    protected Map<InviterItem, Integer> m_counts;
    protected Set<InviterItem> m_checked;
    private boolean m_setChecked;
    private boolean m_remotesAreRobots;

    public static Intent makeIntent( Activity activity, Class target,
                                     int nMissing, SentInvitesInfo info )
    {
        Intent intent = new Intent( activity, target )
            .putExtra( INTENT_KEY_NMISSING, nMissing );
        if ( null != info ) {
            intent.putExtra( RAR, info.getRemotesRobots() );
        }
        return intent;
    }

    public InviteDelegate( Delegator delegator, Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState, R.layout.inviter, R.menu.empty );
        m_activity = delegator.getActivity();
        Intent intent = getIntent();
        m_nMissing = intent.getIntExtra( INTENT_KEY_NMISSING, -1 );
        m_lastDev = intent.getStringExtra( INTENT_KEY_LASTDEV );
        m_remotesAreRobots = intent.getBooleanExtra( RAR, false );
        m_counts = new HashMap<InviterItem, Integer>();
        m_checked = new HashSet<InviterItem>();
    }

    protected void init( String descTxt, int emptyMsgId )
    {
        m_inviteButton = (Button)findViewById( R.id.button_invite );
        m_inviteButton.setOnClickListener( this );

        TextView descView = (TextView)findViewById( R.id.invite_desc );
        descView.setText( descTxt );

        int extraID = getExtra();
        if ( 0 != extraID ) {
            TextView extraView = (TextView)findViewById( R.id.invite_extra );
            extraView.setText( getString( extraID ) );
            extraView.setVisibility( View.VISIBLE );
        }

        m_lv = (ListView)findViewById( android.R.id.list );
        m_ev = (TextView)findViewById( android.R.id.empty );
        if ( null != m_lv && null != m_ev && 0 != emptyMsgId ) {
            m_ev.setText( getString( emptyMsgId ) );
            m_lv.setOnHierarchyChangeListener( this );
            showEmptyIfEmpty();
        }

        tryEnable();
    }

    // Children implement ...
    abstract void onChildAdded( View child, InviterItem item );

    // Implement this if you want to insert descriptive text
    int getExtra() { return 0; }

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

        tryEnable();
    }

    protected void updateListAdapter( List<? extends InviterItem> items )
    {
        updateListAdapter( R.layout.two_strs_item, items );
    }

    protected void updateListAdapter( int itemId,
                                      List<? extends InviterItem> items )
    {
        updateChecked( items );
        m_adapter = new InviteItemsAdapter( itemId, items );
        setListAdapter( m_adapter );
    }

    protected void listSelected( InviterItem[] selected, String[] devs )
    {
        for ( int ii = 0; ii < selected.length; ++ii ) {
            devs[ii] = selected[ii].getDev();
        }
    }

    abstract void onBarButtonClicked( int id );

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
            intent.putExtra( RAR, m_remotesAreRobots );
            setResult( Activity.RESULT_OK, intent );
            finish();
        }
    }

    private InviterItem[] getSelItems()
    {
        int ii = 0;
        InviterItem[] result = new InviterItem[m_checked.size()];
        for ( InviterItem checked : m_checked ) {
            result[ii++] = checked;
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
    }

    final Set<InviterItem> getChecked() { return m_checked; }

    protected void clearChecked() { m_checked.clear(); }

    // Figure which previously-checked items belong in the new set.
    private void updateChecked( List<? extends InviterItem> newItems )
    {
        Set<InviterItem> old = new HashSet<InviterItem>();
        old.addAll( m_checked );
        m_checked.clear();

        for ( Iterator<InviterItem> iter = old.iterator(); iter.hasNext(); ) {
            InviterItem oldItem = iter.next();
            for ( InviterItem item : newItems ) {
                if ( item.equals( oldItem ) ) {
                    m_checked.add( item );
                    break;
                }
            }
        }
    }

    // callbacks made by InviteItemsAdapter
    protected void onItemChecked( InviterItem item, boolean checked )
    {
        if ( checked ) {
            m_checked.add( item );
        } else {
            m_checked.remove( item );
        }
    }

    private InviteItemsAdapter getAdapter()
    {
        return m_adapter;
    }

    private class InviteItemsAdapter extends XWListAdapter {
        private InviterItem[] m_items;
        private int m_itemId;

        public InviteItemsAdapter( int itemID, List<? extends InviterItem> items )
        {
            super( null == items? 0 : items.size() );
            m_itemId = itemID;
            if ( null != items ) {
                m_items = items.toArray( new InviterItem[items.size()] );
            }
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
                            m_checked.add( item );
                        } else {
                            m_checked.remove( item );
                        //     // User's now making changes; don't check new views
                        //     m_setChecked = false;
                        }
                        onItemChecked( item, isChecked );

                        tryEnable();
                    }
                };
            box.setOnCheckedChangeListener( listener );

            if ( m_setChecked || m_checked.contains( item ) ) {
                box.setChecked( true );
            } else if ( null != m_lastDev && m_lastDev.equals(item.getDev()) ) {
                m_lastDev = null;
                box.setChecked( true );
            }
            return layout;
        }

        public String getAddr( CheckBox box ) { return (String)box.getTag(); }
        public String getName( CheckBox box ) { return box.getText().toString(); }
    }

}
