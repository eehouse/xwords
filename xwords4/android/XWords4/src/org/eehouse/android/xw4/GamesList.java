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
// import android.telephony.PhoneStateListener;
// import android.telephony.TelephonyManager;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;

public class GamesList extends XWListActivity 
    implements DispatchNotify.HandleRelaysIface,
               RefreshMsgsTask.RefreshMsgsIface {

    private static final int WARN_NODICT       = DlgDelegate.DIALOG_LAST + 1;
    private static final int WARN_NODICT_SUBST = WARN_NODICT + 1;
    private static final int SHOW_SUBST        = WARN_NODICT + 2;

    private GameListAdapter m_adapter;
    private String m_invalPath = null;
    private String m_missingDict;
    private Handler m_handler;
    private String m_missingDictName;
    private String m_missingDictPath;
    private String[] m_sameLangDicts;
    private int m_missingDictLang;

    // private XWPhoneStateListener m_phoneStateListener;
    // private class XWPhoneStateListener extends PhoneStateListener {
    //     @Override
    //     public void onDataConnectionStateChanged( int state )
    //     {
    //         Utils.logf( "onDataConnectionStateChanged(%d)", state );
    //         if ( TelephonyManager.DATA_CONNECTED == state ) {
    //             NetUtils.informOfDeaths( GamesList.this );
    //         }
    //     }
    // }

    @Override
    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = super.onCreateDialog( id );
        if ( null == dialog ) {
            AlertDialog.Builder ab;
            switch ( id ) {
            case WARN_NODICT:
            case WARN_NODICT_SUBST:
                ab = new AlertDialog.Builder( this )
                    .setTitle( R.string.no_dict_title )
                    .setMessage( "" ) // required to get to change it later
                    .setPositiveButton( R.string.button_ok, null )
                    .setNegativeButton( R.string.button_download, null ) // change
                    ;
                if ( WARN_NODICT_SUBST == id ) {
                    DialogInterface.OnClickListener lstnr = 
                        new DialogInterface.OnClickListener() {
                            public void onClick( DialogInterface dlg, int item ) {
                                showDialog( SHOW_SUBST );
                            }
                        };
                    ab.setNeutralButton( R.string.button_substdict, lstnr );
                }
                dialog = ab.create();
                break;
            case SHOW_SUBST:
                m_sameLangDicts = 
                    DictLangCache.getHaveLangCounts( this, m_missingDictLang );
                ab = new AlertDialog.Builder( this )
                    .setTitle( R.string.subst_dict_title )
                    .setNegativeButton( R.string.button_cancel, null )
                    .setItems( m_sameLangDicts,
                               new DialogInterface.OnClickListener() {
                                   public void onClick( DialogInterface dlg,
                                                        int which ) {
                                       String dict = m_sameLangDicts[which];
                                       dict = DictLangCache.stripCount( dict );
                                       GameUtils.replaceDict( GamesList.this,
                                                              m_missingDictPath,
                                                              dict );
                                   }
                               })
                    ;
                dialog = ab.create();
                // Force destruction so onCreateDialog() will get
                // called next time and we can insert a different
                // list.  There seems to be no way to change the list
                // inside onPrepareDialog().
                dialog.setOnDismissListener(new DialogInterface.
                                            OnDismissListener() {
                        public void onDismiss(DialogInterface dlg) {
                            removeDialog( SHOW_SUBST );
                        }
                    });
                break;
            default:
                Assert.fail();
                break;
            }
        }
        return dialog;
    }

    @Override
    protected void onPrepareDialog( int id, Dialog dialog )
    {
        DialogInterface.OnClickListener lstnr;
        AlertDialog ad;
        
        switch( id ) {
        case WARN_NODICT:
        case WARN_NODICT_SUBST:
            lstnr = new DialogInterface.OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        Intent intent = 
                            Utils.mkDownloadActivity( GamesList.this,
                                                      m_missingDictName,
                                                      m_missingDictLang );
                        startActivity( intent );
                    }
                };
            ad = (AlertDialog)dialog;
            ad.setButton( AlertDialog.BUTTON_NEGATIVE, 
                          getString( R.string.button_download ), lstnr );
            int fmtId = WARN_NODICT == id? R.string.no_dictf
                : R.string.no_dict_substf;
            ad.setMessage( String.format( getString( fmtId ),
                                          m_missingDictName ) );
            break;
        default:
            super.onPrepareDialog( id, dialog );
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) 
    {
        super.onCreate(savedInstanceState);

        m_handler = new Handler();

        setContentView(R.layout.game_list);
        ListView listView = getListView();
        listView.setEmptyView( findViewById( R.id.empty_games_list ) );
        registerForContextMenu( listView );

        boolean isUpgrade = FirstRunDialog.show( this, false );
        PreferenceManager.setDefaultValues( this, R.xml.xwprefs, isUpgrade );

        // setDefaultKeyMode(DEFAULT_KEYS_SHORTCUT);

        Button newGameB = (Button)findViewById(R.id.new_game);
        newGameB.setOnClickListener( new View.OnClickListener() {
                @Override
                public void onClick( View v ) {
                    addGame( false );
                    showNotAgainDlg( R.string.not_again_newgame, 
                                     R.string.key_notagain_newgame );
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

        GameConverter.convert( this );

        m_adapter = new GameListAdapter( this );
        setListAdapter( m_adapter );

        RelayReceiver.RestartTimer( this );
        NetUtils.informOfDeaths( this );
    }

    @Override
    protected void onStart()
    {
        super.onStart();
        DispatchNotify.SetRelayIDsHandler( this );

        // TelephonyManager mgr = 
        //     (TelephonyManager)getSystemService( Context.TELEPHONY_SERVICE );
        // m_phoneStateListener = new XWPhoneStateListener();
        // mgr.listen( m_phoneStateListener,
        //             PhoneStateListener.LISTEN_DATA_CONNECTION_STATE );
    }

    @Override
    protected void onStop()
    {
        // TelephonyManager mgr = 
        //     (TelephonyManager)getSystemService( Context.TELEPHONY_SERVICE );
        // mgr.listen( m_phoneStateListener, PhoneStateListener.LISTEN_NONE );
        // m_phoneStateListener = null;

        DispatchNotify.SetRelayIDsHandler( null );

        super.onStop();
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
            if ( DBUtils.gamesList( this ).length > 0 ) {
                DialogInterface.OnClickListener lstnr =
                    new DialogInterface.OnClickListener() {
                        public void onClick( DialogInterface dlg, int item ) {
                            String[] games = 
                                DBUtils.gamesList( GamesList.this );
                            for ( int ii = games.length - 1; ii >= 0; --ii ) {
                                GameUtils.deleteGame( GamesList.this, games[ii], 
                                                      ii == 0  );
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
        String path = DBUtils.gamesList( this )[position];

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
            if ( checkWarnNoDict( path ) ) {
                File file = new File( path );
                Uri uri = Uri.fromFile( file );
                Intent intent = new Intent( Intent.ACTION_EDIT, uri,
                                            this, BoardActivity.class );
                startActivity( intent );
            }
        }
        m_invalPath = path;
    }

    private boolean handleMenuItem( int menuID, int position ) 
    {
        boolean handled = true;
        DialogInterface.OnClickListener lstnr;

        final String path = DBUtils.gamesList( this )[position];
    
        if ( R.id.list_item_delete == menuID ) {
            lstnr = new DialogInterface.OnClickListener() {
                    public void onClick( DialogInterface dlg, int ii ) {
                        GameUtils.deleteGame( GamesList.this, path, true );
                        m_adapter.inval( path );
                        onContentChanged();
                    }
                };
            showConfirmThen( R.string.confirm_delete, lstnr );
        } else {
            String invalPath = null;
            if ( checkWarnNoDict( path ) ) {
                switch ( menuID ) {
                case R.id.list_item_reset:
                    lstnr = new DialogInterface.OnClickListener() {
                            public void onClick( DialogInterface dlg, int ii ) {
                                GameUtils.resetGame( GamesList.this, path );
                                m_adapter.inval( path );
                                onContentChanged();
                            }
                        };
                    showConfirmThen( R.string.confirm_reset, lstnr );
                    break;
                case R.id.list_item_config:
                    GameUtils.doConfig( this, path, GameConfig.class );
                    m_invalPath = path;
                    break;

                case R.id.list_item_new_from:
                    String newName = GameUtils.dupeGame( this, path );  
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

    private boolean checkWarnNoDict( String path )
    {
        String[] missingName = new String[1];
        int[] missingLang = new int[1];
        boolean hasDict = GameUtils.gameDictHere( this, path, 
                                                  missingName, missingLang );
        if ( !hasDict ) {
            m_missingDictName = missingName[0];
            m_missingDictLang = missingLang[0];
            m_missingDictPath = path;
            if ( 0 == DictLangCache.getLangCount( this, m_missingDictLang ) ) {
                showDialog( WARN_NODICT );
            } else {
                showDialog( WARN_NODICT_SUBST );
            }
        }
        return hasDict;
    }

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
        GameUtils.resetGame( this, path );
        onContentChanged();
        return path;
    }
}
