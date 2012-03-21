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
import android.content.ContentResolver;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.ContactsContract.CommonDataKinds;
import android.provider.ContactsContract.CommonDataKinds.Phone;
import android.provider.ContactsContract;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;
import java.util.ArrayList;

import junit.framework.Assert;

public class NBSInviteActivity extends InviteActivity
    implements View.OnClickListener {
    public static final String DEVS = "DEVS";
    public static final String INTENT_KEY_NMISSING = "NMISSING";

    private static final int GET_CONTACT = 1;

    private ArrayList<String> m_names;
    private ArrayList<String> m_phones;
    private NBSPhonesAdapter m_adapter;

    @Override
    protected void onCreate( Bundle savedInstanceState )
    {
        super.onCreate( savedInstanceState, R.layout.nbsinviter,
                        R.id.button_invite, R.id.button_add, 
                        R.id.button_clear, R.id.invite_desc,
                        R.string.invite_nbs_descf );

        m_names = new ArrayList<String>();
        m_phones = new ArrayList<String>();
    }
    
    @Override
    protected void onActivityResult( int requestCode, int resultCode, 
                                     Intent data )
    {
        super.onActivityResult(requestCode, resultCode, data);
        if ( Activity.RESULT_CANCELED != resultCode && data != null ) {
            switch (requestCode) {
            case GET_CONTACT:
                addPhoneNumbers( data );
                break;
            }
        }
    }

    protected void scan() {
        Intent intent = new Intent( Intent.ACTION_PICK, 
                                    ContactsContract.Contacts.CONTENT_URI );
        startActivityForResult( intent, GET_CONTACT );
    }

    protected void clearSelected()
    {
        DbgUtils.logf( "NBSInviteActivity.clearSelected()" );
        ListView list = (ListView)findViewById( android.R.id.list );
        int count = list.getChildCount();
        for ( int ii = count - 1; ii >= 0; --ii ) {
            NBSListItem item = (NBSListItem)list.getChildAt( ii );
            if ( item.isChecked() ) {
                m_phones.remove( ii );
                m_names.remove( ii );
            }
        }
        rebuildList();
        // int index = m_phones.size();
        // while ( index-- > 0 ) {
        //     if ( m_adapter.isChecked( index ) ) {
        //         m_phones.remove( index );
        //         m_names.remove( index );
        //     }
        // }
    }

    protected String[] listSelected() {
        DbgUtils.logf( "NBSInviteActivity.listSelected()" );
        ListView list = (ListView)findViewById( android.R.id.list );
        String[] result = new String[m_checkCount];
        int count = list.getChildCount();
        int index = 0;
        for ( int ii = 0; ii < count; ++ii ) {
            NBSListItem item = (NBSListItem)list.getChildAt( ii );
            if ( item.isChecked() ) {
                result[index++] = item.getNumber();
            }
        }
        return result;
    }

    private void addPhoneNumbers( Intent intent )
    {
        Uri data = intent.getData();
        Cursor cursor = managedQuery( data, null, null, null, null );
        if ( cursor.moveToFirst() ) {
            int len_before = m_phones.size();
            int index = cursor.getColumnIndex(ContactsContract.Contacts._ID );
            if ( 0 <= index ) {
                String id = cursor.getString( index );
                ContentResolver resolver = getContentResolver();
                Cursor pc = 
                    resolver.query( CommonDataKinds.Phone.CONTENT_URI, 
                                    null,
                                    CommonDataKinds.Phone.CONTACT_ID + " = ?", 
                                    new String[] { id }, null );

                while ( pc.moveToNext() ) {
                    String name = 
                        pc.getString( pc.getColumnIndex( CommonDataKinds.
                                                         Phone.DISPLAY_NAME));
                    String number = 
                        pc.getString( pc.getColumnIndex( CommonDataKinds.
                                                         Phone.NUMBER ) );
                    int type = 
                        pc.getInt( pc.getColumnIndex( CommonDataKinds.
                                                      Phone.TYPE ) );

                    if ( Phone.TYPE_MOBILE == type && 0 < number.length() ) {
                        m_names.add( name );
                        m_phones.add( number );
                    }
                }
                if ( len_before != m_phones.size() ) {
                    rebuildList();
                }
            }
        }
    }

    private void rebuildList()
    {
        m_adapter = new NBSPhonesAdapter();
        setListAdapter( m_adapter );
        m_checkCount = 0;
        tryEnable();
    }

    private class NBSPhonesAdapter extends XWListAdapter {
        private NBSListItem[] m_items;

        public NBSPhonesAdapter()
        {
            super( m_phones.size() );
            m_items = new NBSListItem[m_phones.size()];
        }

        public Object getItem( int position ) 
        { 
            DbgUtils.logf( "getItem called (phone %s)", m_phones.get(position) );
            NBSListItem item = 
                (NBSListItem)Utils.inflate( NBSInviteActivity.this,
                                            R.layout.nbsinviter_item );
            item.setOnCheckedChangeListener( NBSInviteActivity.this );
            item.setContents( m_names.get(position), m_phones.get(position) );
            m_items[position] = item;
            return item;
        }

        public View getView( final int position, View convertView, 
                             ViewGroup parent ) {
            return (View)getItem( position );
        }

        public boolean isChecked( int index ) 
        {
            boolean checked = m_items[index].isChecked();
            DbgUtils.logf( "item %d (for %s) checked: %b", 
                           index, m_items[index].getNumber(), checked );
            return checked;
        }
    }
}
