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
    private static final String SAVE_DEVNAMES = "DEVNAMES";
    private static final int PICK_BTDEV_DLG = DlgDelegate.DIALOG_LAST + 1;

    private boolean m_showsOn;
    private Handler m_handler = null;
    private int m_chosen;
    private String[] m_btDevNames;
    private int m_lang = 0;

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );
        getBundledData( savedInstanceState );

        m_handler = new Handler();

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
    }

    @Override
    protected Dialog onCreateDialog( final int id )
    {
        Dialog dialog = super.onCreateDialog( id );
        if ( null == dialog ) {
            AlertDialog.Builder ab;
            OnClickListener lstnr;

            switch( id ) {
            case PICK_BTDEV_DLG:
                OnClickListener scanLstnr =
                    new OnClickListener() {
                        public void onClick( DialogInterface dlg, 
                                             int whichButton ) {
                            startProgress( R.string.scan_progress );
                            BTService.rescan( NewGameActivity.this );
                        }
                    };
                OnClickListener okLstnr =
                    new OnClickListener() {
                        public void onClick( DialogInterface dlg, 
                                             int whichButton ) {
                            if ( 0 <= m_chosen ) {
                                if ( m_chosen < m_btDevNames.length ) {
                                    int gameID = GameUtils.newGameID();
                                    BTService.
                                        inviteRemote( NewGameActivity.this,
                                                      m_btDevNames[m_chosen],
                                                      gameID, m_lang, 2, 1 );
                                    startProgress( R.string.invite_progress );
                                }
                            }
                        }
                    };
                ab = new AlertDialog.Builder( this )
                    .setTitle( R.string.bt_pick_title )
                    .setPositiveButton( R.string.button_ok, okLstnr )
                    .setNegativeButton( R.string.bt_pick_rescan_button, 
                                        scanLstnr );

                if ( null != m_btDevNames && 0 < m_btDevNames.length ) {
                    OnClickListener devChosenLstnr =
                        new OnClickListener() {
                            public void onClick( DialogInterface dlgi, 
                                                 int whichButton ) {
                                AlertDialog dlg = (AlertDialog)dlgi;
                                Button btn = 
                                    dlg.getButton( AlertDialog.BUTTON_POSITIVE ); 
                                btn.setEnabled( 0 <= whichButton );

                                m_chosen = whichButton;
                            }
                        };
                    m_chosen = -1;
                    ab.setSingleChoiceItems( m_btDevNames, m_chosen, 
                                             devChosenLstnr );
                }
                dialog = ab.create();
                Utils.setRemoveOnDismiss( this, dialog, PICK_BTDEV_DLG );
                break;
            }
        }
        return dialog;
    }

    @Override
    protected void onPrepareDialog( int id, Dialog dialog )
    {
        super.onPrepareDialog( id, dialog );
        if ( PICK_BTDEV_DLG == id ) {
            ((AlertDialog)dialog).getButton( AlertDialog.BUTTON_POSITIVE )
                .setEnabled( false );
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
    protected void onSaveInstanceState( Bundle outState ) 
    {
        super.onSaveInstanceState( outState );
        outState.putStringArray( SAVE_DEVNAMES, m_btDevNames );
    }

    private void getBundledData( Bundle bundle )
    {
        if ( null != bundle ) {
            m_btDevNames = bundle.getStringArray( SAVE_DEVNAMES );
        }
    }

    // BTService.BTEventListener interface
    @Override
    public void eventOccurred( BTService.BTEvent event, final Object ... args )
    {
        switch( event ) {
        case SCAN_DONE:
            m_handler.post( new Runnable() {
                    public void run() {
                        synchronized( NewGameActivity.this ) {
                            stopProgress();
                            if ( 0 < args.length ) {
                                m_btDevNames = (String[])(args[0]);
                            }
                            showDialog( PICK_BTDEV_DLG );
                        }
                    }
                } );
            break;
        case BT_ENABLED:
        case BT_DISABLED:
            m_handler.post( new Runnable() {
                    public void run() {
                        checkEnableBT( false );
                    }
                });
            break;
        case NEWGAME_FAILURE:
            m_handler.post( new Runnable() {
                    public void run() {
                        stopProgress();
                        DbgUtils.showf( NewGameActivity.this,
                                        "Remote failed to create game" );
                    } 
                } );
            break;
        case NEWGAME_SUCCESS:
            final int gameID = (Integer)args[0];
            m_handler.post( new Runnable() {
                    public void run() {
                        GameUtils.makeNewBTGame( NewGameActivity.this, gameID, 
                                                 null, m_lang, 2, 1 );
                        finish();
                    }
                } );
            break;
        default:
            DbgUtils.logf( "unexpected event %s", event.toString() );
            break;
        }
    }

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
            DbgUtils.showf( this, "For debugging only..." );
            GameUtils.makeNewBTGame( NewGameActivity.this, gameID, 
                                     null, m_lang, 2, 1 );
            finish();
        } else if ( null == m_btDevNames || 0 == m_btDevNames.length ) {
            startProgress( R.string.scan_progress );
            BTService.rescan( this );
        } else {
            showDialog( PICK_BTDEV_DLG );
        }
    }

    private void checkEnableBT( boolean force )
    {
        boolean enabled = BTService.BTEnabled();

        if ( force || enabled != m_showsOn ) {
            m_showsOn = enabled;

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
                                new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
                            startActivityForResult( enableBtIntent, 0 );
                        }
                    } );
            }
        }
    }
}
