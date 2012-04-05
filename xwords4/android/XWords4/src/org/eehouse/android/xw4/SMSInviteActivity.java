/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2012 by Eric House (xwords@eehouse.org).  All rights
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
import android.content.ContentResolver;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.ContactsContract.CommonDataKinds.Phone;
import android.provider.ContactsContract.CommonDataKinds;
import android.provider.ContactsContract;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.ListView;
import java.util.ArrayList;
import java.util.Iterator;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.CommonPrefs;

public class SMSInviteActivity extends InviteActivity {

    private static final int GET_CONTACT = 1;

    private ArrayList<String> m_names;
    private ArrayList<String> m_phones;
    private ArrayList<Boolean> m_checks;
    private SMSPhonesAdapter m_adapter;
    private EditText m_manualField;

    @Override
    protected void onCreate( Bundle savedInstanceState )
    {
        super.onCreate( savedInstanceState, R.layout.smsinviter,
                        R.id.button_invite, R.id.button_add, 
                        R.id.button_clear, R.id.invite_desc,
                        R.string.invite_sms_descf );

        m_manualField = (EditText)findViewById( R.id.phone_edit );
        ImageButton button = (ImageButton)findViewById( R.id.manual_add_button );
        button.setOnClickListener( new View.OnClickListener() {
                public void onClick( View view )
                {
                    String number = m_manualField.getText().toString();
                    if ( 0 < number.length() ) {
                        m_manualField.setText("");
                        add( getString( R.string.manual_owner_name ), number );
                        saveState();
                        rebuildList();
                    }
                }
            } );

        getSavedState();
        rebuildList();
        tryEnable();
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

    protected void scan()
    {
        Intent intent = new Intent( Intent.ACTION_PICK, 
                                    ContactsContract.Contacts.CONTENT_URI );
        startActivityForResult( intent, GET_CONTACT );
    }

    protected void clearSelected()
    {
        int count = m_adapter.getCount();
        for ( int ii = count - 1; ii >= 0; --ii ) {
            if ( m_checks.get( ii ) ) {
                remove( ii );
            }
        }
        saveState();
        rebuildList();
    }

    protected String[] listSelected()
    {
        int count = m_adapter.getCount();
        String[] result = new String[countChecks()];
        int index = 0;
        Iterator<Boolean> iter = m_checks.iterator();
        for ( int ii = 0; iter.hasNext(); ++ii ) {
            if ( iter.next() ) {
                result[index++] = 
                    ((SMSListItem)m_adapter.getItem(ii)).getNumber();
            }
        }
        return result;
    }

    @Override
    protected void tryEnable() 
    {
        int count = countChecks();
        m_okButton.setEnabled( count == m_nMissing );
        m_clearButton.setEnabled( 0 < count );
    }

    private int countChecks()
    {
        int count = 0;
        if ( null != m_checks ) {
            Iterator<Boolean> iter = m_checks.iterator();
            while ( iter.hasNext() ) {
                if ( iter.next() ) {
                    ++count;
                }
            }
        }
        return count;
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
                    resolver.query( Phone.CONTENT_URI, null,
                                    Phone.CONTACT_ID + " = ?", 
                                    new String[] { id }, null );
                String name = "";
                while ( pc.moveToNext() ) {
                    name = 
                        pc.getString( pc.getColumnIndex( Phone.DISPLAY_NAME));
                    String number = 
                        pc.getString( pc.getColumnIndex( Phone.NUMBER ) );
                    // int type = 
                    //     pc.getInt( pc.getColumnIndex( Phone.TYPE ) );

                    if ( /*Phone.TYPE_MOBILE == type && */0 < number.length() ) {
                        add( name, number );
                    }
                }
                if ( len_before != m_phones.size() ) {
                    saveState();
                    rebuildList();
                } else {
                    int resid = null != name && 0 < name.length()
                        ?  R.string.sms_nomobilef
                        : R.string.sms_nomobile;
                    String msg = Utils.format( this, resid, name );
                    showOKOnlyDialog( msg );
                }
            }
        }
    }

    private void rebuildList()
    {
        m_adapter = new SMSPhonesAdapter();
        setListAdapter( m_adapter );
        tryEnable();
    }

    private void getSavedState()
    {
        m_names = CommonPrefs.getSMSNames( this );
        m_phones = CommonPrefs.getSMSPhones( this );

        int size = m_phones.size();
        m_checks = new ArrayList<Boolean>(size);
        for ( int ii = 0; ii < size; ++ii ) {
            m_checks.add( false );
        }
    }

    private void saveState()
    {
        CommonPrefs.setSMSNames( this, m_names );
        CommonPrefs.setSMSPhones( this, m_phones );
    }

    private void add( String name, String number )
    {
        m_names.add( name );
        m_phones.add( number );
        m_checks.add( false );
    }

    private void remove( int index )
    {
        m_phones.remove( index );
        m_names.remove( index );
        m_checks.remove( index );
    }

    private class SMSPhonesAdapter extends XWListAdapter {
        private SMSListItem[] m_items;

        public SMSPhonesAdapter()
        {
            super( m_phones.size() );
            m_items = new SMSListItem[m_phones.size()];
        }

        public Object getItem( final int position ) 
        { 
            // For some reason I can't cache items to be returned.
            // Checking/unchecking breaks for some but not all items,
            // with some relation to whether they were scrolled into
            // view.  So build them anew each time (but still cache
            // for by-index access.)

            SMSListItem item = 
                (SMSListItem)Utils.inflate( SMSInviteActivity.this,
                                                R.layout.smsinviter_item );
            item.setChecked( m_checks.get( position ) );

            CompoundButton.OnCheckedChangeListener lstnr =
                new CompoundButton.OnCheckedChangeListener() {
                    public void onCheckedChanged( CompoundButton bv, 
                                                  boolean isChecked ) {
                        m_checks.set( position, isChecked );
                        tryEnable();
                    }
                };
            item.setOnCheckedChangeListener( lstnr );
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
            SMSListItem item = m_items[index];
            boolean checked = null != item && item.isChecked();
            return checked;
        }
    }
}
