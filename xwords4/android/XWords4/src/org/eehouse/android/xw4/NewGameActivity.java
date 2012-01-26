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
import android.app.ProgressDialog;
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


public class NewGameActivity extends XWActivity
    implements BTConnection.BTStateChangeListener {

    private static final int NEW_GAME_ACTION = 1;

    private static final int PICK_BTDEV_DLG = DlgDelegate.DIALOG_LAST + 1;

    private boolean m_showsOn;
    private Handler m_handler = null;
    private ProgressDialog m_progress;
    private int m_chosen;

    @Override
    protected void onCreate(Bundle savedInstanceState) 
    {
        super.onCreate( savedInstanceState );

        setContentView( R.layout.new_game );

        TextView desc = (TextView)findViewById( R.id.newgame_local_desc );
        String fmt = getString( R.string.newgame_local_descf );
        String dict = CommonPrefs.getDefaultHumanDict( this );
        String lang = DictLangCache.getLangName( this, dict );
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
                            String msg = getString( R.string.scan_progress );
                            m_progress = 
                                ProgressDialog.show( NewGameActivity.this, msg,
                                                     null, true, true );
                            BTConnection.rescan( NewGameActivity.this,
                                                 getHandler() );
                        }
                    };
                final String[] btDevs = BTConnection.listPairedWithXwords();
                OnClickListener okLstnr =
                    new OnClickListener() {
                        public void onClick( DialogInterface dlg, 
                                             int whichButton ) {
                            if ( 0 <= m_chosen ) {
                                if ( m_chosen < btDevs.length ) {
                                    int gameID = GameUtils.newGameID();
                                    BTConnection.
                                        inviteRemote( btDevs[m_chosen],
                                                      gameID, getHandler() );
                                }
                            }
                        }
                    };
                ab = new AlertDialog.Builder( this )
                    .setTitle( R.string.bt_pick_title )
                    .setPositiveButton( R.string.button_ok, okLstnr )
                    .setNegativeButton( R.string.bt_pick_rescan_button, 
                                        scanLstnr );

                if ( null != btDevs && 0 < btDevs.length ) {
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
                    ab.setSingleChoiceItems( btDevs, m_chosen, 
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
        BTConnection.setBTStateChangeListener( this );
    }

    @Override
    protected void onPause() {
        BTConnection.setBTStateChangeListener( null );
        super.onPause();
    }

    // BTConnection.BTStateChangeListener
    public void stateChanged( boolean nowEnabled )
    {
        checkEnableBT( false );
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
        if ( useDefaults ) {
            showDialog( PICK_BTDEV_DLG );
        } else {
            Utils.notImpl( this );
        }
    }

    private void checkEnableBT( boolean force )
    {
        boolean enabled = BTConnection.BTEnabled();

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

    private Handler getHandler()
    {
        if ( null == m_handler ) {
            m_handler = new Handler() {
                    public void handleMessage( Message msg ) {
                        switch( msg.what ) {
                        case BTConnection.CONNECT_ACCEPTED:
                            GameUtils.makeNewBTGame( NewGameActivity.this,
                                                     msg.arg1, null );
                            finish();
                            break;
                        case BTConnection.CONNECT_REFUSED:
                        case BTConnection.CONNECT_FAILED:
                            break;
                        case BTConnection.SCAN_DONE:
                            if ( null != m_progress ) {
                                m_progress.cancel();
                                m_progress = null;
                            }
                            showDialog( PICK_BTDEV_DLG );
                            break;
                        }
                    }
                };
        }
        return m_handler;
    }
}
