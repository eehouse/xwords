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
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.ContentResolver;
import android.content.DialogInterface;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.ContactsContract.CommonDataKinds.Phone;
import android.provider.ContactsContract.CommonDataKinds;
import android.provider.ContactsContract;
import android.text.method.DialerKeyListener;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CompoundButton;
import android.widget.ImageButton;
import android.widget.ListView;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.Iterator;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.CommonPrefs;

public class SMSInviteActivity extends InviteActivity {

    private static final int GET_CONTACT = 1;
    private static final int GET_NUMBER = DlgDelegate.DIALOG_LAST + 1;
    private static final String SAVE_NAME = "SAVE_NAME";
    private static final String SAVE_NUMBER = "SAVE_NUMBER";

    private static final int CLEAR_ACTION = 1;
    private static final int USE_IMMOBILE_ACTION = 2;

    private ArrayList<PhoneRec> m_phoneRecs;
    private SMSPhonesAdapter m_adapter;
    private ImageButton m_addButton;
    private String m_pendingName;
    private String m_pendingNumber;

    @Override
    protected void onCreate( Bundle savedInstanceState )
    {
        super.onCreate( savedInstanceState, R.layout.smsinviter,
                        R.id.button_invite, R.id.button_add, 
                        R.id.button_clear, R.id.invite_desc,
                        R.string.invite_sms_descf );
        getBundledData( savedInstanceState );

        m_addButton = (ImageButton)findViewById( R.id.manual_add_button );
        m_addButton.setOnClickListener( new View.OnClickListener() {
                public void onClick( View view )
                {
                    showDialog( GET_NUMBER );
                }
            } );

        getSavedState();
        rebuildList( true );
    }

    @Override
    protected void onSaveInstanceState( Bundle outState ) 
    {
        super.onSaveInstanceState( outState );
        outState.putString( SAVE_NAME, m_pendingName );
        outState.putString( SAVE_NUMBER, m_pendingNumber );
    }

    private void getBundledData( Bundle bundle )
    {
        if ( null != bundle ) {
            m_pendingName = bundle.getString( SAVE_NAME );
            m_pendingNumber = bundle.getString( SAVE_NUMBER );
        }
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

    @Override
    protected Dialog onCreateDialog( int id )
    {        
        Dialog dialog = super.onCreateDialog( id );
        if ( null == dialog ) {
            DialogInterface.OnClickListener lstnr;
            switch( id ) {
            case GET_NUMBER:
                final GameNamer namerView =
                    (GameNamer)Utils.inflate( this, R.layout.rename_game );
                namerView.setLabel( R.string.get_sms_number );
                namerView.setKeyListener(DialerKeyListener.getInstance());
                lstnr = new DialogInterface.OnClickListener() {
                        public void onClick( DialogInterface dlg, int item ) {
                            String number = namerView.getName();
                            String name = 
                                getString( R.string.manual_owner_name );
                            PhoneRec rec = new PhoneRec( name, number );
                            addChecked( rec );
                            saveAndRebuild();
                        }
                    };
                dialog = new AlertDialog.Builder( this )
                    .setNegativeButton( R.string.button_cancel, null )
                    .setPositiveButton( R.string.button_ok, lstnr )
                    .setView( namerView )
                    .create();
                break;
            }
            Utils.setRemoveOnDismiss( this, dialog, id );
        }
        return dialog;
    }

    protected void scan()
    {
        Intent intent = new Intent( Intent.ACTION_PICK, 
                                    ContactsContract.Contacts.CONTENT_URI );
        intent.setType( Phone.CONTENT_TYPE );
        startActivityForResult( intent, GET_CONTACT );
    }

    protected void clearSelected()
    {
        showConfirmThen( getString( R.string.confirm_clear ), CLEAR_ACTION );
    }

    protected String[] listSelected()
    {
        int count = m_adapter.getCount();
        String[] result = new String[countChecks()];
        int index = 0;
        Iterator<PhoneRec> iter = m_phoneRecs.iterator();
        for ( int ii = 0; iter.hasNext(); ++ii ) {
            if ( iter.next().m_checked ) {
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

    // DlgDelegate.DlgClickNotify interface
    @Override
    public void dlgButtonClicked( int id, int which )
    {
        if ( AlertDialog.BUTTON_POSITIVE == which ) {
            switch( id ) {
            case CLEAR_ACTION:
                clearSelectedImpl();
                break;
            case USE_IMMOBILE_ACTION:
                addChecked( new PhoneRec( m_pendingName, m_pendingNumber ) );
                saveAndRebuild();
                break;
            }
        }
    }

    private int countChecks()
    {
        int count = 0;
        if ( null != m_phoneRecs ) {
            Iterator<PhoneRec> iter = m_phoneRecs.iterator();
            while ( iter.hasNext() ) {
                if ( iter.next().m_checked ) {
                    ++count;
                }
            }
        }
        return count;
    }

    private void addPhoneNumbers( Intent intent )
    {
        Uri data = intent.getData();
        Cursor cursor = managedQuery( data, 
                                      new String[] { Phone.DISPLAY_NAME, 
                                                     Phone.NUMBER, 
                                                     Phone.TYPE },
                                      null, null, null );
        if ( cursor.moveToFirst() ) {
            String name = 
                cursor.getString( cursor.getColumnIndex( Phone.DISPLAY_NAME));
            String number = 
                cursor.getString( cursor.getColumnIndex( Phone.NUMBER ) );

            int type = cursor.getInt( cursor.getColumnIndex( Phone.TYPE ) );
            if ( Phone.TYPE_MOBILE == type ) {
                addChecked( new PhoneRec( name, number ) );
                saveAndRebuild();
            } else {
                m_pendingName = name;
                m_pendingNumber = number;
                String msg = Utils.format( this, R.string.warn_nomobilef,
                                           number, name );
                showConfirmThen( msg, R.string.button_yes, USE_IMMOBILE_ACTION );
            }
            cursor.close();
        }
    } // addPhoneNumbers

    private void rebuildList( boolean checkIfAll )
    {
        Collections.sort(m_phoneRecs,new Comparator<PhoneRec>() {
                public int compare( PhoneRec rec1, PhoneRec rec2 ) {
                    return rec1.m_name.compareTo(rec2.m_name);
                }
            });
        m_adapter = new SMSPhonesAdapter();
        setListAdapter( m_adapter );
        if ( checkIfAll && m_phoneRecs.size() == m_nMissing ) {
            Iterator<PhoneRec> iter = m_phoneRecs.iterator();
            while ( iter.hasNext() ) {
                iter.next().m_checked = true;
            }
        }
        tryEnable();
    }

    private void getSavedState()
    {
        String[] names = CommonPrefs.getSMSNames( this );
        String[] phones = CommonPrefs.getSMSPhones( this );
        int size = phones.length;

        m_phoneRecs = new ArrayList<PhoneRec>(size);
        for ( int ii = 0; ii < size; ++ii ) {
            PhoneRec rec = new PhoneRec( names[ii], phones[ii] );
            m_phoneRecs.add( rec );
        }
    }

    private void saveAndRebuild()
    {
        String[] names = new String[m_phoneRecs.size()];
        String[] phones = new String[m_phoneRecs.size()];
        Iterator<PhoneRec> iter = m_phoneRecs.iterator();
        for ( int ii = 0; iter.hasNext(); ++ii ) {
            PhoneRec rec = iter.next();
            names[ii] = rec.m_name;
            phones[ii] = rec.m_phone;
        }
        CommonPrefs.setSMSNames( this, names );
        CommonPrefs.setSMSPhones( this, phones );

        rebuildList( false );
    }

    private void addChecked( PhoneRec rec )
    {
        Iterator<PhoneRec> iter = m_phoneRecs.iterator();
        while ( iter.hasNext() ) {
            iter.next().m_checked = false;
        }

        rec.m_checked = true;
        m_phoneRecs.add( rec );
    }

    private void clearSelectedImpl()
    {
        int count = m_adapter.getCount();
        for ( int ii = count - 1; ii >= 0; --ii ) {
            if ( m_phoneRecs.get( ii ).m_checked ) {
                m_phoneRecs.remove( ii );
            }
        }
        saveAndRebuild();
    }

    private class PhoneRec {
        public String m_phone;
        public String m_name;
        public boolean m_checked;
        public PhoneRec( String name, String phone )
        {
            this( name, phone, false );
        }

        public PhoneRec( String name, String phone, boolean checked )
        {
            m_name = name;
            m_phone = phone;
            m_checked = checked;
        }
    }

    private class SMSPhonesAdapter extends XWListAdapter {
        private SMSListItem[] m_items;

        public SMSPhonesAdapter()
        {
            super( m_phoneRecs.size() );
            m_items = new SMSListItem[m_phoneRecs.size()];
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
            item.setChecked( m_phoneRecs.get(position).m_checked );

            CompoundButton.OnCheckedChangeListener lstnr =
                new CompoundButton.OnCheckedChangeListener() {
                    public void onCheckedChanged( CompoundButton bv, 
                                                  boolean isChecked ) {
                        m_phoneRecs.get(position).m_checked = isChecked;
                        tryEnable();
                    }
                };
            item.setOnCheckedChangeListener( lstnr );
            PhoneRec rec = m_phoneRecs.get( position );
            item.setContents( rec.m_name, rec.m_phone );
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
