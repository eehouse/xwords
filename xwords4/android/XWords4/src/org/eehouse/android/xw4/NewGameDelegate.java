/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009 - 2013 by Eric House (xwords@eehouse.org).  All
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
import android.app.AlertDialog;
import android.app.Dialog;
import android.bluetooth.BluetoothAdapter;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import java.util.Random;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.CurGameInfo;
import org.eehouse.android.xw4.jni.CommonPrefs;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.XwJNI;

public class NewGameDelegate extends DelegateBase {

    // private static final String SAVE_DEVNAMES = "DEVNAMES";
    private static final String SAVE_REMOTEGAME = "REMOTEGAME";
    private static final String SAVE_GAMEID = "GAMEID";
    private static final String SAVE_NAMEFOR = "SAVE_NAMEFOR";
    private static final String GROUPID_EXTRA = "groupid";
    private static final int CONFIG_FOR_BT = 1;
    private static final int CONFIG_FOR_SMS = 2;
    private static final int INVITE_FOR_BT = 3;
    private static final int INVITE_FOR_SMS = 4;

    private boolean m_showsOn;
    private boolean m_nameForBT;
    private boolean m_firingPrefs = false;
    private int m_chosen;
    private int m_lang = 0;
    private String m_dict = null;
    private long m_newRowID = -1;
    private String m_gameName;
    private int m_gameID;
    private long m_groupID;
    private String m_remoteDev;
    private Activity m_activity;

    protected NewGameDelegate( Delegator delegator, Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState, R.layout.new_game );
        m_activity = delegator.getActivity();
    }

    protected void init( Bundle savedInstanceState ) 
    {
        getBundledData( savedInstanceState );

        m_groupID = getIntent().getLongExtra( GROUPID_EXTRA, -1 );

        TextView desc = (TextView)findViewById( R.id.newgame_local_desc );
        m_dict = CommonPrefs.getDefaultHumanDict( m_activity );
        String lang = DictLangCache.getLangName( m_activity, m_dict );
        m_lang = DictLangCache.getLangLangCode( m_activity, lang );
        desc.setText( getString( R.string.newgame_local_desc_fmt, lang ) );
        
        Button button = (Button)findViewById( R.id.newgame_local );
        button.setOnClickListener( new View.OnClickListener() {
                @Override
                public void onClick( View v ) {
                    makeNewGame( false, true );
                }
            } );
        button = (Button)findViewById( R.id.newgame_local_config );
        button.setOnClickListener( new View.OnClickListener() {
                @Override
                public void onClick( View v ) {
                    makeNewGame( false, false );
                }
            } );

        button = (Button)findViewById( R.id.newgame_invite_net );
        button.setOnClickListener( new View.OnClickListener() {
                @Override
                public void onClick( View v ) {
                    makeNewGame( true, true );
                }
            } );

        button = (Button)findViewById( R.id.newgame_net_config );
        button.setOnClickListener( new View.OnClickListener() {
                @Override
                public void onClick( View v ) {
                    makeNewGame( true, false );
                }
            } );

        checkEnableBT( true );
        checkEnableSMS();
    }

    protected void onSaveInstanceState( Bundle outState ) 
    {
        outState.putString( SAVE_REMOTEGAME, m_remoteDev );
        outState.putInt( SAVE_GAMEID, m_gameID );
        outState.putBoolean( SAVE_NAMEFOR, m_nameForBT );
    }

    private void getBundledData( Bundle bundle )
    {
        if ( null != bundle ) {
            m_remoteDev = bundle.getString( SAVE_REMOTEGAME );
            m_gameID = bundle.getInt( SAVE_GAMEID );
            m_nameForBT = bundle.getBoolean( SAVE_NAMEFOR );
        }
    }

    protected void onWindowFocusChanged( boolean hasFocus )
    {
        if ( hasFocus && m_firingPrefs ) {
            m_firingPrefs = false;
            checkEnableSMS();
        }
    }

    // DlgDelegate.DlgClickNotify interface
    @Override
    public void dlgButtonClicked( DlgDelegate.Action action, int which, Object[] params )
    {
        switch( action ) {
        case NEW_GAME_ACTION:
            if ( DlgDelegate.DISMISS_BUTTON != which ) {
                makeNewGame( true, true, which );
            }
            break;
        default:
            Assert.fail();
        }
    }
    
    @Override
    protected void onResume() {
        super.onResume();
        checkEnableBT( false );
    }

    protected void onActivityResult( int requestCode, int resultCode, 
                                     Intent data )
    {
        if ( Activity.RESULT_CANCELED != resultCode ) {
            switch ( requestCode ) {
            case CONFIG_FOR_BT:
            case CONFIG_FOR_SMS:
                if ( Activity.RESULT_CANCELED == resultCode ) {
                    DBUtils.deleteGame( m_activity, m_newRowID );
                } else {
                    // We'll leave it up to BoardActivity to detect that
                    // it's not had any remote connections yet.
                    GameUtils.launchGame( m_activity, m_newRowID );
                    finish();
                }
                break;
            case INVITE_FOR_BT:     // user selected device 
            case INVITE_FOR_SMS:
                if ( Activity.RESULT_CANCELED != resultCode ) {
                    m_nameForBT = INVITE_FOR_BT == requestCode;
                    String[] remoteDevs =
                        data.getStringArrayExtra( InviteDelegate.DEVS );
                    Assert.assertTrue( 1 == remoteDevs.length );
                    m_remoteDev = remoteDevs[0];

                    m_gameID = GameUtils.newGameID();
                    m_gameName = getString( R.string.dft_name_fmt, 
                                            m_gameID & 0xFFFF );
                    showDialog( DlgID.NAME_GAME );
                }
                break;
            }
        }
    }

    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = null;
        DlgID dlgID = DlgID.values()[id];
        switch( dlgID ) {
        case NAME_GAME:
            final GameNamer namerView = (GameNamer)inflate( R.layout.rename_game );
            namerView.setLabel( m_nameForBT ? R.string.btname_label
                                : R.string.smsname_label );
            namerView.setName( m_gameName );

            OnClickListener lstnr =
                new DialogInterface.OnClickListener() {
                    public void onClick( DialogInterface dlg, int itm ) {
                        m_gameName = namerView.getName();
                        if ( m_nameForBT ) {
                            BTService.inviteRemote( m_activity, m_remoteDev,
                                                    m_gameID, m_gameName, 
                                                    m_lang, m_dict, 2, 1 );
                            startProgress( R.string.invite_progress );
                        } else {
                            SMSService.inviteRemote( m_activity, m_remoteDev,
                                                     m_gameID, m_gameName, 
                                                     m_lang, m_dict, 2, 1 );
                            long rowid = GameUtils.
                                makeNewSMSGame( m_activity, m_groupID, m_gameID, 
                                                null, m_lang, m_dict, 2, 1 );
                            DBUtils.setName( m_activity, rowid, m_gameName );
                            GameUtils.launchGame( m_activity, rowid, true );
                            finish();
                        }
                    }
                };

            dialog = makeAlertBuilder()
                .setTitle( m_nameForBT ? R.string.game_btname_title 
                           : R.string.game_smsname_title )
                .setNegativeButton( R.string.button_cancel, null )
                .setPositiveButton( R.string.button_ok, lstnr )
                .setView( namerView )
                .create();
            setRemoveOnDismiss( dialog, dlgID );

            break;
        case ENABLE_NFC:
            dialog = NFCUtils.makeEnableNFCDialog( m_activity );
            break;
        default:
            dialog = super.onCreateDialog( id );
            break;
        }
        return dialog;
    }

    // MultiService.MultiEventListener interface
    @Override
    public void eventOccurred( MultiService.MultiEvent event, 
                               final Object ... args )
    {
        switch( event ) {
        case BT_ENABLED:
        case BT_DISABLED:
            post( new Runnable() {
                    public void run() {
                        checkEnableBT( false );
                    }
                });
            break;
        case NEWGAME_FAILURE:
            post( new Runnable() {
                    public void run() {
                        stopProgress();
                        DbgUtils.showf( m_activity, 
                                        "Remote failed to create game" );
                    } 
                } );
            break;
        case NEWGAME_SUCCESS:
            final int gameID = (Integer)args[0];
            post( new Runnable() {
                    public void run() {
                        long rowid = 
                            GameUtils.makeNewBTGame( m_activity, m_groupID, 
                                                     gameID, null, m_lang, 
                                                     m_dict, 2, 1 );
                        DBUtils.setName( m_activity, rowid, m_gameName );
                        GameUtils.launchGame( m_activity, rowid, true );
                        finish();
                    }
                } );
            break;
        default:
            super.eventOccurred( event, args );
            break;
        }
    } // MultiService.MultiEventListener.eventOccurred

    private void makeNewGame( boolean networked, boolean launch )
    {
        if ( launch && networked ) {
            // Let 'em cancel before we make the game
            showInviteChoicesThen( DlgDelegate.Action.NEW_GAME_ACTION );
        } else {
            makeNewGame( networked, launch, DlgDelegate.SMS_BTN );
        }
    }

    private void makeNewGame( boolean networked, boolean launch,
                              int chosen )
    {
        boolean viaNFC = DlgDelegate.NFC_BTN == chosen;
        if ( viaNFC && !NFCUtils.nfcAvail( m_activity )[1] ) {
            showDialog( DlgID.ENABLE_NFC );
        } else {
            String room = null;
            String inviteID = null;
            long rowid;
            int[] lang = {0};
            String[] dict = {null};
            final int nPlayers = 2; // hard-coded for no-configure case

            if ( networked ) {
                room = GameUtils.makeRandomID();
                inviteID = GameUtils.makeRandomID();
                rowid = GameUtils.makeNewNetGame( m_activity, m_groupID, room, inviteID, 
                                                  lang, dict, nPlayers, 1 );
            } else {
                rowid = GameUtils.saveNew( m_activity, new CurGameInfo( m_activity ), 
                                           m_groupID );
            }

            if ( launch ) {
                GameUtils.launchGame( m_activity, rowid, networked );
                if ( networked ) {
                    GameUtils.launchInviteActivity( m_activity, chosen, room, 
                                                    inviteID, lang[0], dict[0],
                                                    nPlayers );
                }
            } else {
                GameUtils.doConfig( m_activity, rowid, GameConfigActivity.class );
            }

            finish();
        }
    }

    private void makeNewBTGame( boolean useDefaults )
    {
        if ( XWApp.BTSUPPORTED ) {
            int gameID = GameUtils.newGameID();
            if ( !useDefaults ) {
                m_newRowID = GameUtils.makeNewBTGame( m_activity, 
                                                      m_groupID, gameID, null, 
                                                      m_lang, m_dict, 2, 1 );
                Intent intent = new Intent( m_activity, GameConfigActivity.class );
                intent.setAction( Intent.ACTION_EDIT );
                intent.putExtra( GameUtils.INTENT_KEY_ROWID, m_newRowID );
                intent.putExtra( GameUtils.INTENT_FORRESULT_ROWID, true );
                startActivityForResult( intent, CONFIG_FOR_BT );
            } else {
                BTInviteDelegate.launchForResult( m_activity, 1, INVITE_FOR_BT );
            }
        }
    }

    private void makeNewSMSGame( boolean useDefaults )
    {
        int gameID = GameUtils.newGameID();
        if ( !useDefaults ) {
            m_newRowID = GameUtils.makeNewSMSGame( m_activity, 
                                                   m_groupID, gameID, null, 
                                                   m_lang, m_dict, 2, 1 );
            String name = getString( R.string.dft_sms_name_fmt, gameID & 0xFFFF );
            DBUtils.setName( m_activity, m_newRowID, name );

            Intent intent = new Intent( m_activity, GameConfigActivity.class );
            intent.setAction( Intent.ACTION_EDIT );
            intent.putExtra( GameUtils.INTENT_KEY_ROWID, m_newRowID );
            intent.putExtra( GameUtils.INTENT_FORRESULT_ROWID, true );
            startActivityForResult( intent, CONFIG_FOR_SMS );
        } else {
            SMSInviteDelegate.launchForResult( m_activity, 1, INVITE_FOR_SMS );
        }
    }

    private void checkEnableBT( boolean force )
    {
        if ( XWApp.BTSUPPORTED ) {
            boolean enabled = BTService.BTEnabled();

            if ( force || enabled != m_showsOn ) {
                m_showsOn = enabled;

                findViewById( R.id.bt_separator ).setVisibility( View.VISIBLE );

                findViewById( R.id.bt_disabled ).
                    setVisibility( enabled ? View.GONE : View.VISIBLE  );
                findViewById( R.id.bt_stuff ).
                    setVisibility( enabled ? View.VISIBLE : View.GONE  );

                Button button;
                if ( enabled ) {
                    button = (Button)findViewById( R.id.newgame_invite_bt );
                    button.setOnClickListener( new View.OnClickListener() {
                            @Override
                                public void onClick( View v ) {
                                makeNewBTGame( true );
                            }
                        } );
                    button = (Button)findViewById( R.id.newgame_bt_config );
                    button.setOnClickListener( new View.OnClickListener() {
                            @Override
                                public void onClick( View v ) {
                                makeNewBTGame( false );
                            }
                        } );
                } else {
                    button = (Button)findViewById( R.id.newgame_enable_bt );
                    button.setOnClickListener( new View.OnClickListener() {
                            @Override
                                public void onClick( View v ) {
                                Intent enableBtIntent = 
                                    new Intent(BluetoothAdapter.
                                               ACTION_REQUEST_ENABLE);
                                startActivityForResult( enableBtIntent, 0 );
                            }
                        } );
                }
            }
        }
    }

    private void checkEnableSMS()
    { 
        if ( XWApp.SMSSUPPORTED && Utils.deviceSupportsSMS(m_activity) ) {
            boolean enabled = XWPrefs.getSMSEnabled( m_activity );
            findViewById( R.id.sms_separator ).
                setVisibility( View.VISIBLE );

            findViewById( R.id.sms_disabled ).
                setVisibility( enabled ? View.GONE : View.VISIBLE  );
            findViewById( R.id.sms_stuff ).
                setVisibility( enabled ? View.VISIBLE : View.GONE  );

            Button button;
            if ( enabled ) {
                button = (Button)findViewById( R.id.newgame_invite_sms );
                button.setOnClickListener( new View.OnClickListener() {
                        @Override
                            public void onClick( View v ) {
                            makeNewSMSGame( true );
                        }
                    } );
                button = (Button)findViewById( R.id.newgame_sms_config );
                button.setOnClickListener( new View.OnClickListener() {
                        @Override
                            public void onClick( View v ) {
                            makeNewSMSGame( false );
                        }
                    } );
            } else {
                button = (Button)findViewById( R.id.newgame_enable_sms );
                button.setOnClickListener( new View.OnClickListener() {
                        @Override
                        public void onClick( View v ) {
                            m_firingPrefs = true;
                            Utils.launchSettings( m_activity );
                        }
                    } );
            }
        }
    }

    public static void startActivity( Activity parent, long groupID )
    {
        Bundle extras = new Bundle();
        extras.putLong( GROUPID_EXTRA, groupID );
        Intent intent = new Intent( parent, NewGameActivity.class );
        intent.putExtras( extras );
        parent.startActivity( intent );
    }
}
