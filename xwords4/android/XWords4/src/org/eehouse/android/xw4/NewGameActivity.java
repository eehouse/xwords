/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2009-2010 by Eric House (xwords@eehouse.org).  All
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


public class NewGameActivity extends XWActivity {

    private static final int NEW_GAME_ACTION = 1;
    // private static final String SAVE_DEVNAMES = "DEVNAMES";
    private static final String SAVE_REMOTEGAME = "REMOTEGAME";
    private static final String SAVE_GAMEID = "GAMEID";
    private static final int CONFIG_FOR_BT = 1;
    private static final int CONFIG_FOR_NBS = 2;
    private static final int INVITE_FOR_BT = 3;
    private static final int INVITE_FOR_NBS = 4;

    // Dialogs
    private static final int NAME_GAME = DlgDelegate.DIALOG_LAST + 1;

    private boolean m_showsOn;
    private int m_chosen;
    private int m_lang = 0;
    private long m_newRowID = -1;
    private String m_gameName;
    private int m_gameID;
    private String m_remoteDev;

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );
        getBundledData( savedInstanceState );

        setContentView( R.layout.new_game );

        TextView desc = (TextView)findViewById( R.id.newgame_local_desc );
        String fmt = getString( R.string.newgame_local_descf );
        String dict = CommonPrefs.getDefaultHumanDict( this );
        String lang = DictLangCache.getLangName( this, dict );
        m_lang = DictLangCache.getLangLangCode( this, lang );
        desc.setText( String.format( fmt, lang ) );
        
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

        button = (Button)findViewById( R.id.newgame_invite );
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
        checkEnableNBS();
    }

    @Override
    protected void onSaveInstanceState( Bundle outState ) 
    {
        super.onSaveInstanceState( outState );
        outState.putString( SAVE_REMOTEGAME, m_remoteDev );
        outState.putInt( SAVE_GAMEID, m_gameID );
    }

    private void getBundledData( Bundle bundle )
    {
        if ( null != bundle ) {
            m_remoteDev = bundle.getString( SAVE_REMOTEGAME );
            m_gameID = bundle.getInt( SAVE_GAMEID );
        }
    }

    // DlgDelegate.DlgClickNotify interface
    @Override
    public void dlgButtonClicked( int id, int which )
    {
        switch( id ) {
        case NEW_GAME_ACTION:
            if ( DlgDelegate.DISMISS_BUTTON != which ) {
                makeNewGame( true, true, DlgDelegate.EMAIL_BTN == which );
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

    @Override
    protected void onActivityResult( int requestCode, int resultCode, 
                                     Intent data )
    {
        if ( Activity.RESULT_CANCELED != resultCode ) {
            switch ( requestCode ) {
            case CONFIG_FOR_BT:
            case CONFIG_FOR_NBS:
                if ( Activity.RESULT_CANCELED == resultCode ) {
                    DBUtils.deleteGame( this, m_newRowID );
                } else {
                    // We'll leave it up to BoardActivity to detect that
                    // it's not had any remote connections yet.
                    GameUtils.launchGame( this, m_newRowID );
                    finish();
                }
                break;
            case INVITE_FOR_BT:     // user selected device 
                if ( Activity.RESULT_CANCELED != resultCode ) {
                    String[] remoteDevs =
                        data.getStringArrayExtra( BTInviteActivity.DEVS );
                    Assert.assertTrue( 1 == remoteDevs.length );
                    m_remoteDev = remoteDevs[0];

                    m_gameID = GameUtils.newGameID();
                    m_gameName = Utils.format( this, R.string.dft_bt_namef, 
                                               m_gameID & 0xFFFF );
                    showDialog( NAME_GAME );
                }
                break;

            case INVITE_FOR_NBS:
                if ( Activity.RESULT_CANCELED != resultCode ) {
                    String[] phones =
                        data.getStringArrayExtra( NBSInviteActivity.DEVS );
                    Assert.assertTrue( 1 == phones.length );

                    m_gameID = GameUtils.newGameID();
                    m_gameName = Utils.format( this, R.string.dft_nbs_namef, 
                                               m_gameID & 0xFFFF );
                    NBSReceiver.inviteRemote( NewGameActivity.this, phones[0],
                                              m_gameID, m_gameName, 
                                              m_lang, 2, 1 );
                    long rowid = 
                        GameUtils.makeNewNBSGame( NewGameActivity.this, 
                                                  m_gameID, null, m_lang, 
                                                  2, 1 );
                    GameUtils.launchGame( NewGameActivity.this, rowid, true );
                    finish();
                }
                break;
            }
        }
    }

    @Override
    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = super.onCreateDialog( id );
        if ( null == dialog ) {
            switch( id ) {
            case NAME_GAME:
                final GameNamer namerView =
                    (GameNamer)Utils.inflate( this, R.layout.rename_game );
                namerView.setLabel( R.string.btname_label );
                namerView.setName( m_gameName );

                OnClickListener lstnr =
                    new DialogInterface.OnClickListener() {
                        public void onClick( DialogInterface dlg, int itm ) {
                            m_gameName = namerView.getName();
                            BTService.inviteRemote( NewGameActivity.this, 
                                                    m_remoteDev,
                                                    m_gameID, m_gameName, 
                                                    m_lang, 2, 1 );
                            startProgress( R.string.invite_progress );
                        }
                    };

                dialog = new AlertDialog.Builder( this )
                    .setTitle( R.string.game_btname_title )
                    .setNegativeButton( R.string.button_cancel, null )
                    .setPositiveButton( R.string.button_ok, lstnr )
                    .setView( namerView )
                    .create();
                Utils.setRemoveOnDismiss( this, dialog, id );

                break;
            }
        }
        return dialog;
    }

    // BTService.BTEventListener interface
    @Override
    public void eventOccurred( BTService.BTEvent event, final Object ... args )
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
                        DbgUtils.showf( NewGameActivity.this,
                                        "Remote failed to create game" );
                    } 
                } );
            break;
        case NEWGAME_SUCCESS:
            final int gameID = (Integer)args[0];
            post( new Runnable() {
                    public void run() {
                        long rowid = 
                            GameUtils.makeNewBTGame( NewGameActivity.this, 
                                                     gameID, null, m_lang, 
                                                     2, 1 );
                        DBUtils.setName( NewGameActivity.this, 
                                         rowid, m_gameName );
                        GameUtils.launchGame( NewGameActivity.this, 
                                              rowid, true );
                        finish();
                    }
                } );
            break;
        default:
            super.eventOccurred( event, args );
            break;
        }
    } // BTService.BTEventListener.eventOccurred

    private void makeNewGame( boolean networked, boolean launch )
    {
        if ( launch && networked ) {
            // Let 'em cancel before we make the game
            showEmailOrSMSThen( NEW_GAME_ACTION );
        } else {
            makeNewGame( networked, launch, false );
        }
    }

    private void makeNewGame( boolean networked, boolean launch,
                              boolean choseEmail )
    {
        String room = null;
        String inviteID = null;
        long rowid;
        int[] lang = {0};
        final int nPlayers = 2; // hard-coded for no-configure case

        if ( networked ) {
            room = GameUtils.makeRandomID();
            inviteID = GameUtils.makeRandomID();
            rowid = GameUtils.makeNewNetGame( this, room, inviteID, lang, 
                                              nPlayers, 1 );
        } else {
            rowid = GameUtils.saveNew( this, new CurGameInfo( this ) );
        }

        if ( launch ) {
            GameUtils.launchGame( this, rowid, networked );
            if ( networked ) {
                GameUtils.launchInviteActivity( this, choseEmail, room, 
                                                inviteID, lang[0], nPlayers );
            }
        } else {
            GameUtils.doConfig( this, rowid, GameConfig.class );
        }

        finish();
    }

    private void makeNewBTGame( boolean useDefaults )
    {
        int gameID = GameUtils.newGameID();
        if ( !useDefaults ) {
            m_newRowID = GameUtils.makeNewBTGame( NewGameActivity.this, 
                                                 gameID, null, m_lang, 
                                                 2, 1 ); // initial defaults
            Intent intent = new Intent( this, GameConfig.class );
            intent.setAction( Intent.ACTION_EDIT );
            intent.putExtra( GameUtils.INTENT_KEY_ROWID, m_newRowID );
            intent.putExtra( GameUtils.INTENT_FORRESULT_ROWID, true );
            startActivityForResult( intent, CONFIG_FOR_BT );
        } else {
            GameUtils.launchBTInviter( this, 1, INVITE_FOR_BT );
        }
    }

    private void makeNewNBSGame( boolean useDefaults )
    {
        int gameID = GameUtils.newGameID();
        if ( !useDefaults ) {
            m_newRowID = GameUtils.makeNewNBSGame( NewGameActivity.this, 
                                                   gameID, null, m_lang, 
                                                   2, 1 ); // initial defaults
            Intent intent = new Intent( this, GameConfig.class );
            intent.setAction( Intent.ACTION_EDIT );
            intent.putExtra( GameUtils.INTENT_KEY_ROWID, m_newRowID );
            intent.putExtra( GameUtils.INTENT_FORRESULT_ROWID, true );
            startActivityForResult( intent, CONFIG_FOR_NBS );
        } else {
            GameUtils.launchNBSInviter( this, 1, INVITE_FOR_NBS );
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

    private void checkEnableNBS()
    { 
        if ( XWApp.NBSSUPPORTED ) {
            boolean enabled = true; // is the phone on
            findViewById( R.id.nbs_separator ).setVisibility( View.VISIBLE );

            findViewById( R.id.nbs_disabled ).
                setVisibility( enabled ? View.GONE : View.VISIBLE  );
            findViewById( R.id.nbs_stuff ).
                setVisibility( enabled ? View.VISIBLE : View.GONE  );

            Button button;
            if ( enabled ) {
                button = (Button)findViewById( R.id.newgame_invite_nbs );
                button.setOnClickListener( new View.OnClickListener() {
                        @Override
                            public void onClick( View v ) {
                            makeNewNBSGame( true );
                        }
                    } );
                button = (Button)findViewById( R.id.newgame_nbs_config );
                button.setOnClickListener( new View.OnClickListener() {
                        @Override
                            public void onClick( View v ) {
                            makeNewNBSGame( false );
                        }
                    } );
            } else {
                button = (Button)findViewById( R.id.newgame_enable_nbs );
                button.setOnClickListener( new View.OnClickListener() {
                        @Override
                            public void onClick( View v ) {
                            Utils.notImpl( NewGameActivity.this );
                        }
                    } );
            }
        }
    }
}
