/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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
import android.widget.EditText;


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
    private Activity m_activity;

    public static void launchForResult( Activity activity, int nMissing,
                                        SentInvitesInfo info,
                                        RequestCode requestCode )
    {

        Intent intent = InviteDelegate
            .makeIntent( activity, SMSInviteActivity.class,
                         nMissing, info );
        if ( null != info ) {
            String lastDev = info.getLastDev( InviteMeans.SMS_DATA );
            intent.putExtra( INTENT_KEY_LASTDEV, lastDev );
        }
        activity.startActivityForResult( intent, requestCode.ordinal() );
    }

    public SMSInviteDelegate( Delegator delegator, Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState );
        m_activity = delegator.getActivity();
    }

    @Override
    protected void init( Bundle savedInstanceState )
    {
        super.init( savedInstanceState );

        String msg = getString( R.string.button_invite );
        msg = getQuantityString( R.plurals.invite_sms_desc_fmt, m_nMissing,
                                 m_nMissing, msg );
        init( msg, R.string.empty_sms_inviter );
        addButtonBar( R.layout.sms_buttons, BUTTONIDS );

        getSavedState();
        rebuildList( true );

        askContactsPermission();
    }

    @Override
    int getExtra() { return R.string.invite_nbs_desc; }

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
            showDialogFragment( DlgID.GET_NUMBER );
            break;
        case R.id.button_clear:
            int count = getChecked().size();
            String msg = getQuantityString( R.plurals.confirm_clear_sms_fmt,
                                            count, count );
            makeConfirmThenBuilder( Action.CLEAR_ACTION, msg ).show();
            break;
        }
    }

    @Override
    protected void onActivityResult( RequestCode requestCode, int resultCode,
                                     final Intent data )
    {
        if ( Activity.RESULT_CANCELED != resultCode && data != null ) {
            switch ( requestCode ) {
            case GET_CONTACT:
                post( new Runnable() {
                        @Override
                        public void run() {
                            addPhoneNumbers( data );
                        }
                    } );
                break;
            }
        }
    }

    @Override
    protected Dialog makeDialog( DBAlert alert, Object... params )
    {
        Assert.assertVarargsNotNullNR(params);
        Dialog dialog;
        DialogInterface.OnClickListener lstnr;
        switch( alert.getDlgID() ) {
        case GET_NUMBER: {
            final View getNumView = inflate( R.layout.get_sms );
            ((EditText)getNumView.findViewById( R.id.num_field )).setKeyListener(DialerKeyListener.getInstance());
            lstnr = new DialogInterface.OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        String number
                            = ((EditText)getNumView.findViewById(R.id.num_field))
                            .getText().toString();
                        if ( null != number && 0 < number.length() ) {
                            String name
                                = ((EditText)getNumView.findViewById(R.id.name_field))
                                .getText().toString();
                            postSMSCostWarning( number, name );
                        }
                    }
                };
            dialog = makeAlertBuilder()
                .setTitle( R.string.get_sms_title )
                .setView( getNumView )
                .setPositiveButton( android.R.string.ok, lstnr )
                .setNegativeButton( android.R.string.cancel, null )
                .create();
        }
            break;
        default:
            dialog = super.makeDialog( alert, params );
            break;
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
    public boolean onPosButton( Action action, Object... params )
    {
        Assert.assertVarargsNotNullNR(params);
        boolean handled = true;
        switch ( action ) {
        case CLEAR_ACTION:
            clearSelectedImpl();
            break;
        case USE_IMMOBILE_ACTION:
            postSMSCostWarning( (String)params[0], (String)params[1] );
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
            handled = super.onPosButton( action, params );
        }
        return handled;
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

                if ( Phone.TYPE_MOBILE == type ) {
                    postSMSCostWarning( number, name );
                } else {
                    postConfirmMobile( number, name );
                }
            }
        }
    } // addPhoneNumbers

    private void postSMSCostWarning( String number, String name )
    {
        makeConfirmThenBuilder( Action.POST_WARNING_ACTION,
                                R.string.warn_unlimited )
            .setPosButton( R.string.button_yes )
            .setParams( number, name )
            .show();
    }

    private void postConfirmMobile( String number, String name )
    {
        makeConfirmThenBuilder( Action.USE_IMMOBILE_ACTION,
                                R.string.warn_nomobile_fmt, number, name )
            .setPosButton( R.string.button_yes )
            .setParams( number, name )
            .show();
    }

    private void rebuildList( boolean checkIfAll )
    {
        Collections.sort( m_phoneRecs, new Comparator<PhoneRec>() {
                public int compare( PhoneRec rec1, PhoneRec rec2 ) {
                    return rec1.m_name.compareTo(rec2.m_name);
                }
            });

        updateList( m_phoneRecs );
        tryEnable();
    }

    private void getSavedState()
    {
        JSONObject phones = XWPrefs.getSMSPhones( m_activity );

        m_phoneRecs = new ArrayList<>();
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
                Log.ex( TAG, ex );
            }
        }
        XWPrefs.setSMSPhones( m_activity, phones );

        rebuildList( false );
    }

    private void clearSelectedImpl()
    {
        Set<String> checked = getChecked();
        Iterator<PhoneRec> iter = m_phoneRecs.iterator();
        for ( ; iter.hasNext(); ) {
            if ( checked.contains( iter.next().getDev() ) ) {
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
                             Action.SKIP_CALLBACK );
    }

    private class PhoneRec implements InviterItem {
        public String m_phone;
        public String m_name;

        public PhoneRec( String phone )
        {
            this( null, phone );
        }

        @Override
        public String getDev() { return m_phone; }

        @Override
        public boolean equals( InviterItem item )
        {
            boolean result = false;
            if ( null != item && item instanceof PhoneRec ) {
                PhoneRec rec = (PhoneRec)item;
                result = m_name == rec.m_name
                    && PhoneNumberUtils.compare( m_phone, rec.m_phone );
            }
            return result;
        }

        private PhoneRec( String name, String phone )
        {
            m_phone = phone;
            m_name = name;
        }
    }
}
