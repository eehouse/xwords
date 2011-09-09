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
import android.widget.EditText;
import android.widget.LinearLayout;
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
               DBUtils.DBChangeListener,
               GameListAdapter.LoadItemCB {

    private static final int WARN_NODICT       = DlgDelegate.DIALOG_LAST + 1;
    private static final int WARN_NODICT_SUBST = WARN_NODICT + 1;
    private static final int SHOW_SUBST        = WARN_NODICT + 2;
    private static final int GET_NAME          = WARN_NODICT + 3;
    private static final int RENAME_GAME       = WARN_NODICT + 4;

    private static final String SAVE_ROWID = "SAVE_ROWID";

    private static final int NEW_NET_GAME_ACTION = 1;
    private static final int RESET_GAME_ACTION = 2;
    private static final int DELETE_GAME_ACTION = 3;
    private static final int DELETE_ALL_ACTION = 4;
    private static final int SYNC_MENU_ACTION = 5;
    private static final int DUPE_GAME_ACTION = 6;

    private GameListAdapter m_adapter;
    private String m_missingDict;
    private Handler m_handler;
    private String[] m_missingDictNames;
    private long m_missingDictRowId;
    private String[] m_sameLangDicts;
    private int m_missingDictLang;
    private long m_rowid;
    private String m_nameField;
    private NetLaunchInfo m_netLaunchInfo;

    @Override
    protected Dialog onCreateDialog( int id )
    {
        DialogInterface.OnClickListener lstnr;
        LinearLayout layout;

        Dialog dialog = super.onCreateDialog( id );
        if ( null == dialog ) {
            AlertDialog.Builder ab;
            switch ( id ) {
            case WARN_NODICT:
            case WARN_NODICT_SUBST:
                lstnr = new DialogInterface.OnClickListener() {
                        public void onClick( DialogInterface dlg, int item ) {
                            for ( String name : m_missingDictNames ) {
                                DictsActivity.
                                    launchAndDownload( GamesList.this, 
                                                       m_missingDictLang,
                                                       name );
                                break; // just do one
                            }
                        }
                    };
                String message;
                String langName = 
                    DictLangCache.getLangName( this, m_missingDictLang );
                String gameName = GameUtils.getName( this, m_rowid );
                if ( WARN_NODICT == id ) {
                    message = String.format( getString( R.string.no_dictf ),
                                             gameName, langName );
                } else {
                    message = String.format( getString(R.string.no_dict_substf),
                                             gameName, m_missingDictNames[0], 
                                             langName );
                }

                ab = new AlertDialog.Builder( this )
                    .setTitle( R.string.no_dict_title )
                    .setMessage( message )
                    .setPositiveButton( R.string.button_ok, null )
                    .setNegativeButton( R.string.button_download, lstnr )
                    ;
                if ( WARN_NODICT_SUBST == id ) {
                    lstnr = new DialogInterface.OnClickListener() {
                            public void onClick( DialogInterface dlg, int item ) {
                                showDialog( SHOW_SUBST );
                            }
                        };
                    ab.setNeutralButton( R.string.button_substdict, lstnr );
                }
                dialog = ab.create();
                Utils.setRemoveOnDismiss( this, dialog, id );
                break;
            case SHOW_SUBST:
                m_sameLangDicts = 
                    DictLangCache.getHaveLangCounts( this, m_missingDictLang );
                lstnr = new DialogInterface.OnClickListener() {
                        public void onClick( DialogInterface dlg,
                                             int which ) {
                            int pos = ((AlertDialog)dlg).getListView().
                                getCheckedItemPosition();
                            String dict = m_sameLangDicts[pos];
                            dict = DictLangCache.stripCount( dict );
                            GameUtils.replaceDicts( GamesList.this,
                                                    m_missingDictRowId,
                                                    m_missingDictNames[0],
                                                    dict );
                        }
                    };
                dialog = new AlertDialog.Builder( this )
                    .setTitle( R.string.subst_dict_title )
                    .setPositiveButton( R.string.button_substdict, lstnr )
                    .setNegativeButton( R.string.button_cancel, null )
                    .setSingleChoiceItems( m_sameLangDicts, 0, null )
                    .create();
                    ;
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

            case RENAME_GAME:
                layout =
                    (LinearLayout)Utils.inflate( this, R.layout.rename_game );
                lstnr = new DialogInterface.OnClickListener() {
                        public void onClick( DialogInterface dlgi, int item ) {
                            Dialog dlg = (Dialog)dlgi;
                            EditText txt = 
                                (EditText)dlg.findViewById( R.id.name_edit );
                            String name = txt.getText().toString();
                            DBUtils.setName( GamesList.this, m_rowid, name );
                            m_adapter.inval( m_rowid );
                            onContentChanged();
                        }
                    };
                dialog = new AlertDialog.Builder( this )
                    .setTitle( R.string.game_rename_title )
                    .setNegativeButton( R.string.button_cancel, null )
                    .setPositiveButton( R.string.button_ok, lstnr )
                    .setView( layout )
                    .create();
                break;

            case GET_NAME:
                layout = 
                    (LinearLayout)Utils.inflate( this, R.layout.dflt_name );
                final EditText etext =
                    (EditText)layout.findViewById( R.id.name_edit );
                etext.setText( CommonPrefs.getDefaultPlayerName( this, 0, 
                                                                 true ) );
                dialog = new AlertDialog.Builder( this )
                    .setTitle( R.string.default_name_title )
                    .setMessage( R.string.default_name_message )
                    .setPositiveButton( R.string.button_ok, null )
                    .setView( layout )
                    .create();
                dialog.setOnDismissListener(new DialogInterface.
                                            OnDismissListener() {
                        public void onDismiss( DialogInterface dlg ) {
                            String name = etext.getText().toString();
                            if ( 0 == name.length() ) {
                                name = CommonPrefs.
                                    getDefaultPlayerName( GamesList.this,
                                                          0, true );
                            }
                            CommonPrefs.setDefaultPlayerName( GamesList.this,
                                                              name );
                        }
                    });
                break;
            default:
                // just drop it; super.onCreateDialog likely failed
                break;
            }
        }
        return dialog;
    } // onCreateDialog

    @Override
    public void onPrepareDialog( int id, Dialog dialog )
    {
        switch( id ) {
        case RENAME_GAME:
            String name = GameUtils.getName( this, m_rowid );
            EditText txt = (EditText)dialog.findViewById( R.id.name_edit );
            txt.setText( name );
            break;
        default:
            super.onPrepareDialog( id, dialog );
            break;
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) 
    {
        super.onCreate( savedInstanceState );
        getBundledData( savedInstanceState );

        m_handler = new Handler();

        setContentView(R.layout.game_list);
        registerForContextMenu( getListView() );
        DBUtils.setDBChangeListener( this );

        boolean isUpgrade = FirstRunDialog.show( this, false );
        PreferenceManager.setDefaultValues( this, R.xml.xwprefs, isUpgrade );

        // setDefaultKeyMode(DEFAULT_KEYS_SHORTCUT);

        Button newGameB = (Button)findViewById(R.id.new_game);
        newGameB.setOnClickListener( new View.OnClickListener() {
                @Override
                public void onClick( View v ) {
                    // addGame( false );
                    startNewGameActivity();
                    // showNotAgainDlg( R.string.not_again_newgame, 
                    //                  R.string.key_notagain_newgame );
                }
            });

        m_adapter = new GameListAdapter( this, this );
        setListAdapter( m_adapter );

        NetUtils.informOfDeaths( this );

        Intent intent = getIntent();
        startFirstHasDict( intent );
        startNewNetGame( intent );

        askDefaultNameIf();
    } // onCreate

    @Override
    // called when we're brought to the front (probably as a result of
    // notification)
    protected void onNewIntent( Intent intent )
    {
        super.onNewIntent( intent );
        Assert.assertNotNull( intent );
        invalRelayIDs( intent.
                       getStringArrayExtra( DispatchNotify.RELAYIDS_EXTRA ) );
        startFirstHasDict( intent );
        startNewNetGame( intent );
    }

    @Override
    protected void onStart()
    {
        super.onStart();
        DispatchNotify.SetRelayIDsHandler( this );

        boolean hide = CommonPrefs.getHideIntro( this );
        int hereOrGone = hide ? View.GONE : View.VISIBLE;
        for ( int id : new int[]{ R.id.empty_games_list, 
                                  R.id.new_game } ) {
            View view = findViewById( id );
            view.setVisibility( hereOrGone );
        }
        View empty = findViewById( R.id.empty_list_msg );
        empty.setVisibility( hide ? View.VISIBLE : View.GONE );
        getListView().setEmptyView( hide? empty : null );

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

    @Override
    protected void onDestroy()
    {
        DBUtils.clearDBChangeListener( this );
        super.onDestroy();
    }

    @Override
    protected void onSaveInstanceState( Bundle outState ) 
    {
        super.onSaveInstanceState( outState );
        outState.putLong( SAVE_ROWID, m_rowid );
        if ( null != m_netLaunchInfo ) {
            m_netLaunchInfo.putSelf( outState );
        }
    }

    private void getBundledData( Bundle bundle )
    {
        if ( null != bundle ) {
            m_rowid = bundle.getLong( SAVE_ROWID );
            m_netLaunchInfo = new NetLaunchInfo( bundle );
        }
    }

    @Override
    public void onWindowFocusChanged( boolean hasFocus )
    {
        super.onWindowFocusChanged( hasFocus );
        if ( hasFocus ) {
            updateField();
        }
    }

    // DispatchNotify.HandleRelaysIface interface
    public void HandleRelaysIDs( final String[] relayIDs )
    {
        m_handler.post( new Runnable() {
                public void run() {
                    invalRelayIDs( relayIDs );
                    startFirstHasDict( relayIDs );
                }
            } );
    }

    public void HandleInvite( final Uri invite )
    {
        final NetLaunchInfo nli = new NetLaunchInfo( invite );
        if ( nli.isValid() ) {
            m_handler.post( new Runnable() {
                    @Override
                    public void run() {
                        startNewNetGame( nli );
                    }
                } );
        }
    }

    // DBUtils.DBChangeListener interface
    public void gameSaved( final long rowid )
    {
        m_handler.post( new Runnable() {
                public void run() {
                    m_adapter.inval( rowid );
                    onContentChanged();
                }
            } );
    }

    // GameListAdapter.LoadItemCB interface
    public void itemLoaded( long rowid )
    {
        onContentChanged();
    }

    // DlgDelegate.DlgClickNotify interface
    @Override
    public void dlgButtonClicked( int id, int which )
    {
        if ( AlertDialog.BUTTON_POSITIVE == which ) {
            switch( id ) {
            case NEW_NET_GAME_ACTION:
                long rowid = GameUtils.makeNewNetGame( this, m_netLaunchInfo );
                GameUtils.launchGame( this, rowid, true );
                break;
            case RESET_GAME_ACTION:
                GameUtils.resetGame( this, m_rowid );
                break;
            case DELETE_GAME_ACTION:
                GameUtils.deleteGame( this, m_rowid, true );
                break;
            case DELETE_ALL_ACTION:
                long[] games = DBUtils.gamesList( this );
                for ( int ii = games.length - 1; ii >= 0; --ii ) {
                    GameUtils.deleteGame( this, games[ii], ii == 0  );
                    m_adapter.inval( games[ii] );
                }
                break;
            case SYNC_MENU_ACTION:
                doSyncMenuitem();
                break;
            case DUPE_GAME_ACTION:
                long newid = GameUtils.dupeGame( GamesList.this, m_rowid );
                if ( null != m_adapter ) {
                    m_adapter.inval( newid );
                }
                break;
            default:
                Assert.fail();
            }
        }
    }

    public void itemClicked( long rowid )
    {
        // We need a way to let the user get back to the basic-config
        // dialog in case it was dismissed.  That way it to check for
        // an empty room name.
        GameSummary summary = DBUtils.getSummary( this, rowid, true );
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
            GameUtils.doConfig( this, rowid, clazz );
        } else {
            if ( checkWarnNoDict( rowid ) ) {
                GameUtils.launchGame( this, rowid );
            }
        }
    }

    @Override
    public void onCreateContextMenu( ContextMenu menu, View view, 
                                     ContextMenuInfo menuInfo ) 
    {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate( R.menu.games_list_item_menu, menu );

        AdapterView.AdapterContextMenuInfo info = 
            (AdapterView.AdapterContextMenuInfo)menuInfo;
        int position = info.position;
        long rowid = DBUtils.gamesList( this )[position];
        String title = GameUtils.getName( this, rowid );
        String fmt = getString(R.string.game_item_menu_titlef );
        menu.setHeaderTitle( String.format( fmt, title ) );
    }
        
    @Override
    public boolean onContextItemSelected( MenuItem item ) 
    {
        AdapterView.AdapterContextMenuInfo info;
        try {
            info = (AdapterView.AdapterContextMenuInfo) item.getMenuInfo();
        } catch (ClassCastException e) {
            Utils.logf( "bad menuInfo: %s", e.toString() );
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

    public boolean onOptionsItemSelected( MenuItem item )
    {
        boolean handled = true;
        Intent intent;

        switch (item.getItemId()) {
        case R.id.gamel_menu_newgame:
            startNewGameActivity();
            break;

        case R.id.gamel_menu_delete_all:
            if ( DBUtils.gamesList( this ).length > 0 ) {
                showConfirmThen( R.string.confirm_delete_all, 
                                 R.string.button_delete, DELETE_ALL_ACTION );
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
                                 SYNC_MENU_ACTION );
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

    private boolean handleMenuItem( int menuID, int position ) 
    {
        boolean handled = true;
        DialogInterface.OnClickListener lstnr;

        m_rowid = DBUtils.gamesList( this )[position];
        
        if ( R.id.list_item_delete == menuID ) {
            showConfirmThen( R.string.confirm_delete, R.string.button_delete, 
                             DELETE_GAME_ACTION );
        } else {
            if ( checkWarnNoDict( m_rowid ) ) {
                switch ( menuID ) {
                case R.id.list_item_reset:
                    showConfirmThen( R.string.confirm_reset, 
                                     R.string.button_reset, RESET_GAME_ACTION );
                    break;
                case R.id.list_item_config:
                    GameUtils.doConfig( this, m_rowid, GameConfig.class );
                    break;
                case R.id.list_item_rename:
                    showDialog( RENAME_GAME );
                    break;

                case R.id.list_item_new_from:
                    showNotAgainDlgThen( R.string.not_again_newfrom,
                                         R.string.key_notagain_newfrom, 
                                         DUPE_GAME_ACTION );
                    break;

                case R.id.list_item_copy:
                    GameSummary summary = DBUtils.getSummary( this, 
                                                              m_rowid, true );
                    if ( summary.inNetworkGame() ) {
                        showOKOnlyDialog( R.string.no_copy_network );
                    } else {
                        byte[] stream = GameUtils.savedGame( this, m_rowid );
                        GameUtils.GameLock lock = 
                            GameUtils.saveNewGame( this, stream );
                        DBUtils.saveSummary( this, lock, summary );
                        lock.unlock();
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
        }

        return handled;
    } // handleMenuItem

    private boolean checkWarnNoDict( long rowid )
    {
        String[][] missingNames = new String[1][];
        int[] missingLang = new int[1];
        boolean hasDicts = GameUtils.gameDictsHere( this, rowid,
                                                    missingNames, 
                                                    missingLang );
        if ( !hasDicts ) {
            m_missingDictNames = missingNames[0];
            m_missingDictLang = missingLang[0];
            m_missingDictRowId = rowid;
            if ( 0 == DictLangCache.getLangCount( this, m_missingDictLang ) ) {
                showDialog( WARN_NODICT );
            } else {
                showDialog( WARN_NODICT_SUBST );
            }
        }
        return hasDicts;
    }

    private void invalRelayIDs( String[] relayIDs ) 
    {
        if ( null != relayIDs ) {
            for ( String relayID : relayIDs ) {
                long rowid = DBUtils.getRowIDFor( this, relayID );
                m_adapter.inval( rowid );
            }
            onContentChanged();
        }
    }

    // Launch the first of these for which there's a dictionary
    // present.
    private void startFirstHasDict( String[] relayIDs )
    {
        if ( null != relayIDs ) {
            for ( String relayID : relayIDs ) {
                long rowid = DBUtils.getRowIDFor( this, relayID );
                if ( -1 != rowid && GameUtils.gameDictsHere( this, rowid ) ) {
                    GameUtils.launchGame( this, rowid );
                    break;
                }
            }
        }
    }

    private void startFirstHasDict( Intent intent )
    {
        if ( null != intent ) {
            String[] relayIDs =
                intent.getStringArrayExtra( DispatchNotify.RELAYIDS_EXTRA );
            startFirstHasDict( relayIDs );
        }
    }

    private void startNewGameActivity()
    {
        startActivity( new Intent( this, NewGameActivity.class ) );
    }

    private void startNewNetGame( NetLaunchInfo info )
    {
        long rowid = DBUtils.getRowIDForOpen( this, info.room, info.lang, 
                                              info.nPlayers );

        if ( -1 != rowid ) {
            rowid = GameUtils.makeNewNetGame( this, info );
            GameUtils.launchGame( this, rowid, true );
        } else {
            String fmt = getString( R.string.dup_game_queryf );
            String msg = String.format( fmt, info.room );
            m_netLaunchInfo = info;
            showConfirmThen( msg, NEW_NET_GAME_ACTION );
        }
    } // startNewNetGame

    private void startNewNetGame( Intent intent )
    {
        Uri data = intent.getData();
        if ( null != data ) {
            NetLaunchInfo info = new NetLaunchInfo( data );
            if ( info.isValid() ) {
                startNewNetGame( info );
            }
        }
    } // startNewNetGame

    private void askDefaultNameIf()
    {
        if ( null == CommonPrefs.getDefaultPlayerName( this, 0, false ) ) {
            String name = CommonPrefs.getDefaultPlayerName( this, 0, true );
            CommonPrefs.setDefaultPlayerName( GamesList.this, name );
            showDialog( GET_NAME );
        }
    }

    private void updateField()
    {
        String newField = CommonPrefs.getSummaryField( this );
        if ( ! newField.equals( m_nameField ) ) {
            m_nameField = newField;
            m_adapter.setField( newField );
            onContentChanged();
        }
    }

}
