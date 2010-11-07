/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
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

import android.app.ListActivity;
import android.app.Dialog;
import android.app.AlertDialog;
import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.DialogInterface;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ContextMenu.ContextMenuInfo;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.Button;
import android.view.MenuInflater;
import java.io.File;
import android.preference.PreferenceManager;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;

public class GamesList extends XWListActivity 
    implements DispatchNotify.HandleRelaysIface,
               RefreshMsgsTask.RefreshMsgsIface {

    private GameListAdapter m_adapter;
    private String m_invalPath = null;
    private String m_missingDict;
    private Handler m_handler;

    @Override
    protected void onCreate(Bundle savedInstanceState) 
    {
        super.onCreate(savedInstanceState);

        m_handler = new Handler();

        setContentView(R.layout.game_list);

        boolean isUpgrade = FirstRunDialog.show( this, false );
        PreferenceManager.setDefaultValues( this, R.xml.xwprefs, isUpgrade );

        // setDefaultKeyMode(DEFAULT_KEYS_SHORTCUT);

        registerForContextMenu( getListView() );

        Button newGameB = (Button)findViewById(R.id.new_game);
        newGameB.setOnClickListener( new View.OnClickListener() {
                @Override
                public void onClick( View v ) {
                    addGame( false );
                    showNotAgainDlgThen( R.string.not_again_newgame, 
                                         R.string.key_notagain_newgame, null );
                }
            });
        newGameB = (Button)findViewById(R.id.new_game_net);
        newGameB.setOnClickListener( new View.OnClickListener() {
                @Override
                public void onClick( View v ) {
                    String path = addGame( true );
                    GameUtils.doConfig( GamesList.this, path, 
                                        RelayGameActivity.class );
                    m_invalPath = path;
                }
            });

        m_adapter = new GameListAdapter( this );
        setListAdapter( m_adapter );

        RelayReceiver.RestartTimer( this );
    }

    @Override
    protected void onStart()
    {
        super.onStart();
        DispatchNotify.SetRelayIDsHandler( this );
    }

    @Override
    protected void onStop()
    {
        super.onStop();
        DispatchNotify.SetRelayIDsHandler( null );
    }

    // DispatchNotify.HandleRelaysIface interface
    public void HandleRelaysIDs( final String[] relayIDs )
    {
        m_handler.post( new Runnable() {
                public void run() {
                    if ( null == relayIDs ) {
                        Utils.logf( "relayIDs null" );
                    } else if ( relayIDs.length == 0 ) {
                        Utils.logf( "relayIDs empty" );
                    } else {
                        for ( String relayID : relayIDs ) {
                            Utils.logf( "HandleRelaysIDs: got %s", relayID );
                            String path = DBUtils.getPathFor( GamesList.this,
                                                              relayID );
                            m_adapter.inval( path );
                        }
                        onContentChanged();
                    }
                }
            } );
    }

    // RefreshMsgsTask.RefreshMsgsIface interface
    public void RefreshMsgsResults( String[] relayIDs )
    {
        HandleRelaysIDs( relayIDs );
    }

    // @Override
    // protected void onNewIntent( Intent intent )
    // {
    //     RelayService.CancelNotification();

    //     Utils.logf( "onNewIntent called" );
    //     String[] relayIDs = intent.
    //         getStringArrayExtra( getString(R.string.relayids_extra) );
    //     HandleRelaysIDs( relayIDs );
    // }

    @Override
    public void onWindowFocusChanged( boolean hasFocus )
    {
        super.onWindowFocusChanged( hasFocus );
        if ( hasFocus && null != m_invalPath ) {
            m_adapter.inval( m_invalPath );
            m_invalPath = null;
            onContentChanged();
        }
    }

    @Override
    public void onCreateContextMenu( ContextMenu menu, View view, 
                                     ContextMenuInfo menuInfo ) 
    {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate( R.menu.games_list_item_menu, menu );
    }
        
    @Override
    public boolean onContextItemSelected( MenuItem item ) 
    {
        AdapterView.AdapterContextMenuInfo info;
        try {
            info = (AdapterView.AdapterContextMenuInfo) item.getMenuInfo();
        } catch (ClassCastException e) {
            Utils.logf( "bad menuInfo:" + e.toString() );
            return false;
        }

        return handleMenuItem( item.getItemId(), info.position );
    } // onContextItemSelected

    public boolean onCreateOptionsMenu( Menu menu )
    {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate( R.menu.games_list_menu, menu );
        return true;
    }

    private void doSyncMenuitem()
    {
        if ( null == DBUtils.getRelayIDNoMsgs( this ) ) {
            showOKOnlyDialog( R.string.no_games_to_refresh );
        } else {
            new RefreshMsgsTask( this, this ).execute();
        }
    }

    public boolean onOptionsItemSelected( MenuItem item )
    {
        boolean handled = true;
        Intent intent;

        switch (item.getItemId()) {
        case R.id.gamel_menu_delete_all:
            if ( GameUtils.gamesList( this ).length > 0 ) {
                DialogInterface.OnClickListener lstnr =
                    new DialogInterface.OnClickListener() {
                        public void onClick( DialogInterface dlg, int item ) {
                            for( String game:GameUtils.gamesList(GamesList.this)) {
                                GameUtils.deleteGame( GamesList.this, game  );
                            }
                            m_adapter = new GameListAdapter( GamesList.this );
                            setListAdapter( m_adapter );
                        }
                    };
                showConfirmThen( R.string.confirm_delete_all, lstnr );
            }
            handled = true;
            break;

        case R.id.gamel_menu_dicts:
            intent = new Intent( this, DictsActivity.class );
            startActivity( intent );
            break;

        case R.id.gamel_menu_checkmoves:
            showNotAgainDlgThen( R.string.not_again_sync,
                                 R.string.key_notagain_sync,
                                 new Runnable() {
                                     public void run() {
                                         doSyncMenuitem();
                                     }
                                 } );
            break;

        case R.id.gamel_menu_prefs:
            intent = new Intent( this, PrefsActivity.class );
            startActivity( intent );
            break;

        case R.id.gamel_menu_about:
            showAboutDialog();
            break;

        // case R.id.gamel_menu_view_hidden:
        //     Utils.notImpl( this );
        //     break;
        default:
            handled = false;
        }

        return handled;
    }

    @Override
    protected void onListItemClick( ListView l, View v, int position, long id )
    {
        super.onListItemClick( l, v, position, id );
        String path = GameUtils.gamesList( this )[position];

        // We need a way to let the user get back to the basic-config
        // dialog in case it was dismissed.  That way it to check for
        // an empty room name.
        GameSummary summary = DBUtils.getSummary( this, path );
        if ( summary.conType == CommsAddrRec.CommsConnType.COMMS_CONN_RELAY
             && summary.roomName.length() == 0 ) {
            // If it's unconfigured and of the type RelayGameActivity
            // can handle send it there, otherwise use the full-on
            // config.
            Class clazz;
            if ( RelayGameActivity.isSimpleGame( summary ) ) {
                clazz = RelayGameActivity.class;
            } else {
                clazz = GameConfig.class;
            }
            GameUtils.doConfig( this, path, clazz );
        } else {
            File file = new File( path );
            Uri uri = Uri.fromFile( file );
            Intent intent = new Intent( Intent.ACTION_EDIT, uri,
                                        this, BoardActivity.class );
            startActivity( intent );
        }
        m_invalPath = path;
    }

    private boolean handleMenuItem( int menuID, int position ) 
    {
        boolean handled = true;

        final String path = GameUtils.gamesList( this )[position];
    
        if ( R.id.list_item_delete == menuID ) {
            DialogInterface.OnClickListener lstnr =
                new DialogInterface.OnClickListener() {
                    public void onClick( DialogInterface dlg, int ii ) {
                        GameUtils.deleteGame( GamesList.this, path );
                        m_adapter.inval( path );
                        onContentChanged();
                    }
                };
            showConfirmThen( R.string.confirm_delete, lstnr );
        } else if ( R.id.list_item_reset == menuID ) {
            DialogInterface.OnClickListener lstnr =
                new DialogInterface.OnClickListener() {
                    public void onClick( DialogInterface dlg, int ii ) {
                        GameUtils.resetGame( GamesList.this, 
                                             path, path );
                        m_adapter.inval( path );
                        onContentChanged();
                    }
                };
            showConfirmThen( R.string.confirm_reset, lstnr );
        } else {
            String invalPath = null;
            String[] missingName = new String[1];
            int[] missingLang = new int[1];
            boolean hasDict = GameUtils.gameDictHere( this, path, 
                                                      missingName, missingLang );
            if ( !hasDict ) {
                showNoDict( missingName[0], missingLang[0] );
            } else {
                switch ( menuID ) {
                case R.id.list_item_config:
                    GameUtils.doConfig( this, path, GameConfig.class );
                    m_invalPath = path;
                    break;

                case R.id.list_item_new_from:
                    String newName = GameUtils.resetGame( this, path );  
                    invalPath = newName;
                    break;

                case R.id.list_item_copy:
                    GameSummary summary = DBUtils.getSummary( this, path );
                    if ( summary.inNetworkGame() ) {
                        showOKOnlyDialog( R.string.no_copy_network );
                    } else {
                        byte[] stream = GameUtils.savedGame( this, path );
                        newName = GameUtils.saveGame( this, stream );
                        DBUtils.saveSummary( this, newName, summary );
                    }
                    break;

                    // These require some notion of predictable sort order.
                    // Maybe put off until I'm using a db?
                    // case R.id.list_item_hide:
                    // case R.id.list_item_move_up:
                    // case R.id.list_item_move_down:
                    // case R.id.list_item_move_to_top:
                    // case R.id.list_item_move_to_bottom:
                    // Utils.notImpl( this );
                    // break;
                default:
                    handled = false;
                    break;
                }
            }

            if ( null != invalPath ) {
                m_adapter.inval( invalPath );
            }
            if ( handled ) {
                onContentChanged();
            }
        }

        return handled;
    } // handleMenuItem

    private String saveNew( CurGameInfo gi )
    {
        String path = null;
        byte[] bytes = XwJNI.gi_to_stream( gi );
        if ( null != bytes ) {
            path = GameUtils.saveGame( this, bytes );
        }
        return path;
    }

    private String addGame( boolean networked )
    {
        String path = saveNew( new CurGameInfo( this, networked ) );
        GameUtils.resetGame( this, path, path );
        onContentChanged();
        return path;
    }

}
