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
import android.provider.ContactsContract.CommonDataKinds.Phone;
import android.provider.ContactsContract;
import android.telephony.PhoneNumberUtils;
import android.text.method.DialerKeyListener;
import android.view.View;
import android.widget.Button;

import junit.framework.Assert;

import org.eehouse.android.xw4.DBUtils.SentInvitesInfo;
import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.Perms23.Perm;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.Iterator;
import java.util.Set;

import org.json.JSONObject;
import org.json.JSONException;

public class SMSInviteDelegate extends InviteDelegate {
    private static final String TAG = SMSInviteDelegate.class.getSimpleName();
    private static int[] BUTTONIDS = {
        R.id.button_add,
        R.id.manual_add_button,
        R.id.button_clear,
    };

    private ArrayList<PhoneRec> m_phoneRecs;
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
        super( delegator, savedInstanceState );
        m_activity = delegator.getActivity();
    }

    protected void init( Bundle savedInstanceState )
    {
        String msg = getString( R.string.button_invite );
        msg = getQuantityString( R.plurals.invite_sms_desc_fmt, m_nMissing,
                                 m_nMissing, msg );
        super.init( msg, R.string.empty_sms_inviter );
        addButtonBar( R.layout.sms_buttons, BUTTONIDS );

        getSavedState();
        rebuildList( true );

        askContactsPermission();
    }

    @Override
    protected void onBarButtonClicked( int id )
    {
        switch( id ) {
        case R.id.button_add:
            Intent intent = new Intent( Intent.ACTION_PICK,
                                        ContactsContract.Contacts.CONTENT_URI );
            intent.setType( Phone.CONTENT_TYPE );
            startActivityForResult( intent, RequestCode.GET_CONTACT );
            break;
        case R.id.manual_add_button:
            showDialog( DlgID.GET_NUMBER );
            break;
        case R.id.button_clear:
            int count = getChecked().size();
            String msg = getQuantityString( R.plurals.confirm_clear_sms_fmt,
                                            count, count );
            makeConfirmThenBuilder( msg, Action.CLEAR_ACTION ).show();
            break;
        }
    }

    @Override
    protected void onActivityResult( RequestCode requestCode, int resultCode,
                                     Intent data )
    {
        if ( Activity.RESULT_CANCELED != resultCode && data != null ) {
            switch ( requestCode ) {
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
                            self.makeConfirmThenBuilder( R.string.warn_unlimited,
                                                         Action.POST_WARNING_ACTION )
                                .setPosButton( R.string.button_yes )
                                .setParams( number, null )
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

    @Override
    protected void onChildAdded( View child, InviterItem data )
    {
        PhoneRec rec = (PhoneRec)data;
        ((TwoStrsItem)child).setStrings( rec.m_name, rec.m_phone );
    }

    @Override
    protected void tryEnable()
    {
        super.tryEnable();

        Button button = (Button)findViewById( R.id.button_clear );
        if ( null != button ) { // may not be there yet
            button.setEnabled( 0 < getChecked().size() );
        }
    }

    // DlgDelegate.DlgClickNotify interface
    @Override
    public void onPosButton( Action action, Object[] params )
    {
        switch ( action ) {
        case CLEAR_ACTION:
            clearSelectedImpl();
            break;
        case USE_IMMOBILE_ACTION:
            m_immobileConfirmed = true;
            break;
        case POST_WARNING_ACTION:
            PhoneRec rec = new PhoneRec( (String)params[1],
                                         (String)params[0] );
            m_phoneRecs.add( rec );
            clearChecked();
            onItemChecked( rec, true );
            saveAndRebuild();
            break;
        default:
            super.onPosButton( action, params );
        }
    }

    @Override
    public void onNegButton( Action action, final Object[] params )
    {
        switch ( action ) {
        case USE_IMMOBILE_ACTION:
            if ( m_immobileConfirmed ) {
                // Putting up a new alert from inside another's handler
                // confuses things. So post instead.
                post( new Runnable() {
                        @Override
                        public void run() {
                            makeConfirmThenBuilder( R.string.warn_unlimited,
                                                    Action.POST_WARNING_ACTION )
                                .setPosButton( R.string.button_yes )
                                .setParams( params )
                                .show();
                        }
                    } );
            }
            break;
        default:
            super.onNegButton( action, params );
        }
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

                DlgDelegate.ConfirmThenBuilder builder;
                if ( Phone.TYPE_MOBILE == type ) {
                    builder = makeConfirmThenBuilder( R.string.warn_unlimited,
                                                      Action.POST_WARNING_ACTION );
                } else {
                    m_immobileConfirmed = false;
                    String msg = getString( R.string.warn_nomobile_fmt,
                                            number, name );
                    builder = makeConfirmThenBuilder( msg, Action.USE_IMMOBILE_ACTION );
                }
                builder.setPosButton( R.string.button_yes )
                    .setParams( number, name )
                    .show();
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

        updateListAdapter( m_phoneRecs.toArray( new PhoneRec[m_phoneRecs.size()] ) );
        tryEnable();
    }

    private void getSavedState()
    {
        JSONObject phones = XWPrefs.getSMSPhones( m_activity );

        m_phoneRecs = new ArrayList<PhoneRec>();
        for ( Iterator<String> iter = phones.keys(); iter.hasNext(); ) {
            String phone = iter.next();
            String name = phones.optString( phone, null );
            PhoneRec rec = new PhoneRec( name, phone );
            m_phoneRecs.add( rec );
        }
    }

    private void saveAndRebuild()
    {
        JSONObject phones = new JSONObject();
        Iterator<PhoneRec> iter = m_phoneRecs.iterator();
        while ( iter.hasNext() ) {
            PhoneRec rec = iter.next();
            try {
                phones.put( rec.m_phone, rec.m_name );
            } catch ( JSONException ex ) {
                DbgUtils.logex( TAG, ex );
            }
        }
        XWPrefs.setSMSPhones( m_activity, phones );

        rebuildList( false );
    }

    private void clearSelectedImpl()
    {
        Set<InviterItem> checked = getChecked();
        Iterator<PhoneRec> iter = m_phoneRecs.iterator();
        for ( ; iter.hasNext(); ) {
            if ( checked.contains( iter.next() ) ) {
                iter.remove();
            }
        }
        clearChecked();
        saveAndRebuild();
    }

    private void askContactsPermission()
    {
        // We want to ask, and to give the rationale, but behave the same
        // regardless of the answers given. So SKIP_CALLBACK.
        Perms23.tryGetPerms( this, Perm.READ_CONTACTS,
                             R.string.contacts_rationale,
                             Action.SKIP_CALLBACK, this );
    }

    private class PhoneRec implements InviterItem {
        public String m_phone;
        public String m_name;

        public PhoneRec( String phone )
        {
            this( null, phone );
        }

        public String getDev() { return m_phone; }

        public boolean equals( InviterItem item )
        {
            boolean result = false;
            if ( null != item &&  item instanceof PhoneRec ) {
                PhoneRec rec = (PhoneRec)item;
                result = m_name.equals(rec.m_name)
                    && PhoneNumberUtils.compare( m_phone, rec.m_phone );
            }
            return result;
        }

        private PhoneRec( String name, String phone )
        {
            m_phone = phone;

            if ( null == name ) {
                name = getString( R.string.contact_not_found );
            }
            m_name = name;
        }
    }
}
