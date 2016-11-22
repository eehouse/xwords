/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2012 - 2016 by Eric House (xwords@eehouse.org).  All rights
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
import android.content.DialogInterface;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.ContactsContract;
import android.provider.ContactsContract.CommonDataKinds.Phone;
import android.text.method.DialerKeyListener;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.CompoundButton;
import android.widget.ImageButton;
import android.widget.Spinner;

import junit.framework.Assert;

import org.eehouse.android.xw4.DBUtils.SentInvitesInfo;
import org.eehouse.android.xw4.DlgDelegate.Action;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.Iterator;

public class SMSInviteDelegate extends InviteDelegate {

    private static final String SAVE_NAME = "SAVE_NAME";
    private static final String SAVE_NUMBER = "SAVE_NUMBER";

    private ArrayList<PhoneRec> m_phoneRecs;
    private ImageButton m_addButton;
    private String m_pendingName;
    private String m_pendingNumber;
    private boolean m_immobileConfirmed;
    private Activity m_activity;

    public static void launchForResult( Activity activity, int nMissing,
                                        SentInvitesInfo info,
                                        RequestCode requestCode )
    {
        Intent intent = new Intent( activity, SMSInviteActivity.class );
        intent.putExtra( INTENT_KEY_NMISSING, nMissing );
        if ( null != info ) {
            String lastDev = info.getLastDev( InviteMeans.SMS );
            intent.putExtra( INTENT_KEY_LASTDEV, lastDev );
        }
        activity.startActivityForResult( intent, requestCode.ordinal() );
    }

    public SMSInviteDelegate( Delegator delegator, Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState, R.layout.smsinviter );
        m_activity = delegator.getActivity();
    }

    protected void init( Bundle savedInstanceState )
    {
        String msg = getString( R.string.button_invite );
        msg = getQuantityString( R.plurals.invite_sms_desc_fmt, m_nMissing,
                                 m_nMissing, msg );
        super.init( R.id.button_invite, R.id.button_add, R.id.button_clear,
                    R.id.invite_desc, msg );

        getBundledData( savedInstanceState );

        m_addButton = (ImageButton)findViewById( R.id.manual_add_button );
        m_addButton.setOnClickListener( new View.OnClickListener() {
                public void onClick( View view )
                {
                    showDialog( DlgID.GET_NUMBER );
                }
            } );

        getSavedState();
        rebuildList( true );
    }

    @Override
    protected void onSaveInstanceState( Bundle outState )
    {
        outState.putString( SAVE_NAME, m_pendingName );
        outState.putString( SAVE_NUMBER, m_pendingNumber );
        super.onSaveInstanceState( outState );
    }

    private void getBundledData( Bundle bundle )
    {
        if ( null != bundle ) {
            m_pendingName = bundle.getString( SAVE_NAME );
            m_pendingNumber = bundle.getString( SAVE_NUMBER );
        }
    }

    @Override
    protected void onActivityResult( RequestCode requestCode, int resultCode,
                                     Intent data )
    {
        // super.onActivityResult( requestCode, resultCode, data );
        if ( Activity.RESULT_CANCELED != resultCode && data != null ) {
            switch (requestCode) {
            case GET_CONTACT:
                addPhoneNumbers( data );
                break;
            }
        }
    }

    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = super.onCreateDialog( id );
        if ( null == dialog ) {
            DialogInterface.OnClickListener lstnr;
            DlgID dlgID = DlgID.values()[id];
            switch( dlgID ) {
            case GET_NUMBER:
                final GameNamer namerView =
                    (GameNamer)inflate( R.layout.rename_game );
                namerView.setLabel( R.string.get_sms_number );
                namerView.setKeyListener(DialerKeyListener.getInstance());
                lstnr = new DialogInterface.OnClickListener() {
                        public void onClick( DialogInterface dlg, int item ) {
                            SMSInviteDelegate self = (SMSInviteDelegate)curThis();
                            String number = namerView.getName();
                            PhoneRec rec = new PhoneRec( number );
                            self.m_pendingNumber = number;
                            self.m_pendingName = null;
                            self.makeConfirmThenBuilder( R.string.warn_unlimited,
                                                         Action.POST_WARNING_ACTION )
                                .setPosButton( R.string.button_yes )
                                .show();
                        }
                    };
                dialog = makeAlertBuilder()
                    .setNegativeButton( android.R.string.cancel, null )
                    .setPositiveButton( android.R.string.ok, lstnr )
                    .setView( namerView )
                    .create();
                break;
            }
            setRemoveOnDismiss( dialog, dlgID );
        }
        return dialog;
    }

    protected void scan()
    {
        Intent intent = new Intent( Intent.ACTION_PICK,
                                    ContactsContract.Contacts.CONTENT_URI );
        intent.setType( Phone.CONTENT_TYPE );
        startActivityForResult( intent, RequestCode.GET_CONTACT );
    }

    @Override
    protected void clearSelected( Integer[] checked )
    {
        int count = checked.length;
        String msg = getQuantityString( R.plurals.confirm_clear_sms_fmt,
                                        count, count );
        makeConfirmThenBuilder( msg, Action.CLEAR_ACTION ).show();
    }

    protected void listSelected( String[][] devsP, int[][] countsP )
    {
        XWListAdapter adapter = getAdapter();
        int count = adapter.getCount();
        String[] result = new String[countChecks()];
        int[] counts = new int[result.length];

        int index = 0;
        Iterator<PhoneRec> iter = m_phoneRecs.iterator();
        for ( int ii = 0; iter.hasNext(); ++ii ) {
            PhoneRec rec = iter.next();
            if ( rec.m_isChecked ) {
                counts[index] = rec.m_nPlayers;
                result[index] = ((SMSListItem)adapter.getItem(ii)).getNumber();
                index++;
            }
        }
        devsP[0] = result;
        if ( null != countsP ) {
            countsP[0] = counts;
        }
    }

    @Override
    protected void tryEnable()
    {
        if ( null != m_phoneRecs ) {
            int nPlayers = 0;
            int nDevs = 0;
            Iterator<PhoneRec> iter = m_phoneRecs.iterator();
            while ( iter.hasNext() ) {
                PhoneRec rec = iter.next();
                if ( rec.m_isChecked ) {
                    ++nDevs;
                    nPlayers += rec.m_nPlayers;
                }
            }
            m_inviteButton.setEnabled( 0 < nPlayers && nPlayers <= m_nMissing );
            m_clearButton.setEnabled( 0 < nDevs );
        }
    }

    // DlgDelegate.DlgClickNotify interface
    @Override
    public void dlgButtonClicked( Action action, int which, Object[] params )
    {
        switch( which ) {
        case AlertDialog.BUTTON_POSITIVE:
            switch( action ) {
            case CLEAR_ACTION:
                clearSelectedImpl();
                break;
            case USE_IMMOBILE_ACTION:
                m_immobileConfirmed = true;
                break;
            case POST_WARNING_ACTION:
                addChecked( new PhoneRec( m_pendingName, m_pendingNumber ) );
                saveAndRebuild();
                break;
            }
            break;
        case DlgDelegate.DISMISS_BUTTON:
            if ( Action.USE_IMMOBILE_ACTION == action && m_immobileConfirmed ) {
                makeConfirmThenBuilder( R.string.warn_unlimited,
                                        Action.POST_WARNING_ACTION )
                    .setPosButton( R.string.button_yes )
                    .show();
            }
            break;
        }
    }

    private int countChecks()
    {
        int count = 0;
        if ( null != m_phoneRecs ) {
            Iterator<PhoneRec> iter = m_phoneRecs.iterator();
            while ( iter.hasNext() ) {
                if ( iter.next().m_isChecked ) {
                    ++count;
                }
            }
        }
        return count;
    }

    private void addPhoneNumbers( Intent intent )
    {
        Uri data = intent.getData();
        Cursor cursor = m_activity
            .managedQuery( data,
                           new String[] { Phone.DISPLAY_NAME,
                                          Phone.NUMBER,
                                          Phone.TYPE },
                           null, null, null );
        // Have seen a crash reporting
        // "android.database.StaleDataException: Attempted to access a
        // cursor after it has been closed." when the query takes a
        // long time to return.  Be safe.
        if ( null != cursor && !cursor.isClosed() ) {
            if ( cursor.moveToFirst() ) {
                String name =
                    cursor.getString( cursor.
                                      getColumnIndex( Phone.DISPLAY_NAME));
                String number =
                    cursor.getString( cursor.
                                      getColumnIndex( Phone.NUMBER ) );

                int type = cursor.getInt( cursor.
                                          getColumnIndex( Phone.TYPE ) );
                m_pendingName = name;
                m_pendingNumber = number;
                if ( Phone.TYPE_MOBILE == type ) {
                    makeConfirmThenBuilder( R.string.warn_unlimited,
                                            Action.POST_WARNING_ACTION )
                        .setPosButton( R.string.button_yes )
                        .show();
                } else {
                    m_immobileConfirmed = false;
                    String msg = getString( R.string.warn_nomobile_fmt,
                                            number, name );
                    makeConfirmThenBuilder( msg, Action.USE_IMMOBILE_ACTION )
                        .setPosButton( R.string.button_yes )
                        .show();
                }
            }
        }
    } // addPhoneNumbers

    private void rebuildList( boolean checkIfAll )
    {
        Collections.sort( m_phoneRecs, new Comparator<PhoneRec>() {
                public int compare( PhoneRec rec1, PhoneRec rec2 ) {
                    return rec1.m_name.compareTo(rec2.m_name);
                }
            });
        String[] phones = new String[m_phoneRecs.size()];
        String[] names = new String[m_phoneRecs.size()];
        for ( int ii = 0; ii < m_phoneRecs.size(); ++ii ) {
            PhoneRec rec = m_phoneRecs.get( ii );
            phones[ii] = rec.m_phone;
            names[ii] = rec.m_name;
        }

        updateListAdapter( R.layout.smsinviter_item, phones, names, true );
        tryEnable();
    }

    private void getSavedState()
    {
        String[] phones = XWPrefs.getSMSPhones( m_activity );

        m_phoneRecs = new ArrayList<PhoneRec>(phones.length);
        for ( String phone : phones ) {
            boolean matches = phone.equals( m_lastDev );
            PhoneRec rec = new PhoneRec( null, phone, matches );
            m_phoneRecs.add( rec );
        }
    }

    private void saveAndRebuild()
    {
        String[] phones = new String[m_phoneRecs.size()];
        Iterator<PhoneRec> iter = m_phoneRecs.iterator();
        for ( int ii = 0; iter.hasNext(); ++ii ) {
            PhoneRec rec = iter.next();
            phones[ii] = rec.m_phone;
        }
        XWPrefs.setSMSPhones( m_activity, phones );

        rebuildList( false );
    }

    private void addChecked( PhoneRec rec )
    {
        if ( m_nMissing <= countChecks() ) {
            Iterator<PhoneRec> iter = m_phoneRecs.iterator();
            while ( iter.hasNext() ) {
                iter.next().m_isChecked = false;
            }
        }

        rec.m_isChecked = true;
        m_phoneRecs.add( rec );
    }

    private void clearSelectedImpl()
    {
        XWListAdapter adapter = getAdapter();
        int count = adapter.getCount();
        for ( int ii = count - 1; ii >= 0; --ii ) {
            if ( m_phoneRecs.get( ii ).m_isChecked ) {
                m_phoneRecs.remove( ii );
            }
        }
        saveAndRebuild();
    }

    private class PhoneRec {
        public String m_phone;
        public String m_name;
        public boolean m_isChecked;
        public int m_nPlayers;
        public PhoneRec( String name, String phone )
        {
            this( name, phone, false );
        }
        public PhoneRec( String phone )
        {
            this( null, phone, false );
        }

        private PhoneRec( String name, String phone, boolean checked )
        {
            m_phone = phone;
            m_isChecked = checked;
            m_nPlayers = 1;

            if ( null == name ) {
                name = Utils.phoneToContact( m_activity, phone, false );
                if ( null == name ) {
                    name = getString( R.string.manual_owner_name );
                }
            }
            m_name = name;
        }
    }
}
