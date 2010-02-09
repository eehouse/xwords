/* -*- compile-command: "cd ../../../../../; ant reinstall"; -*- */
/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.eehouse.android.xw4;

import android.app.ListActivity;
import android.content.ComponentName;
import android.content.ContentUris;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ContextMenu.ContextMenuInfo;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.SimpleCursorAdapter;
import android.content.res.AssetManager;
import java.io.InputStream;
import android.widget.Button;
import android.view.MenuInflater;
import java.io.FileOutputStream;
import java.io.File;

import org.eehouse.android.xw4.jni.*;

import org.eehouse.android.xw4.XWords4.Games; // for constants

public class GamesList extends ListActivity implements View.OnClickListener {
    private static final String TAG = "GamesList";
    private GameListAdapter m_adapter;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        CommonPrefs.setContext( this );

        setContentView(R.layout.game_list);

        // setDefaultKeyMode(DEFAULT_KEYS_SHORTCUT);

        registerForContextMenu( getListView() );

        Button newGameB = (Button)findViewById(R.id.new_game);
        newGameB.setOnClickListener( this );

        // If no data was given in the intent (because we were started
        // as a MAIN activity), then use our default content provider.
        Intent intent = getIntent();
        if (intent.getData() == null) {
            intent.setData(Games.CONTENT_URI);
        }

        m_adapter = new GameListAdapter( this );
        setListAdapter( m_adapter );
    }

    @Override
    public void onWindowFocusChanged( boolean hasFocus )
    {
        super.onWindowFocusChanged( hasFocus );
        if ( hasFocus ) {
            onContentChanged();
        }
    }

    @Override
    public void onCreateContextMenu( ContextMenu menu, View view, 
                                     ContextMenuInfo menuInfo ) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate( R.menu.games_list_item_menu, menu );
    }
        
    @Override
    public boolean onContextItemSelected( MenuItem item ) 
    {
        boolean handled = true;
        byte[] stream;

        AdapterView.AdapterContextMenuInfo info;
        try {
            info = (AdapterView.AdapterContextMenuInfo) item.getMenuInfo();
        } catch (ClassCastException e) {
            Utils.logf( "bad menuInfo:" + e.toString() );
            return false;
        }
        
        String path = fileList()[info.position];

        switch (item.getItemId()) {
        // case R.id.list_item_open:
        //     doOpen( info.position );
        //     handled = true;
        //     break;
        case R.id.list_item_config:
            doConfig( path );
            break;
        case R.id.list_item_delete:
            if ( ! deleteFile( path ) ) {
                Utils.logf( "unable to delete " + path );
            }
            break;

        case R.id.list_item_copy:
            stream = Utils.savedGame( this, path );
            Utils.saveGame( this, stream );
            break;

        case R.id.list_item_new_from:
            stream = Utils.savedGame( this, path );
            CurGameInfo gi = new CurGameInfo( this );
            XwJNI.gi_from_stream( gi, stream );
            stream = XwJNI.gi_to_stream( gi );
            Utils.saveGame( this, stream );
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

        if ( handled ) {
            onContentChanged();
        }

        return handled;
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

        switch (item.getItemId()) {
        case R.id.gamel_menu_delete_all:
            String[] files = fileList();
            for( String file : files ) {
                if ( deleteFile( file  ) ) {
                    Utils.logf( "deleted " + file );
                } else {
                    Utils.logf( "unable to delete " + file );
                }
            }
            m_adapter = new GameListAdapter( this );
            setListAdapter( m_adapter );
            handled = true;
            break;

        case R.id.gamel_menu_prefs:
            Intent intent = new Intent( this, PrefsActivity.class );
            startActivity( intent );
            break;

        case R.id.gamel_menu_about:
            Utils.about(this);
            break;

        case R.id.gamel_menu_view_hidden:
            Utils.notImpl( this );
            break;
        default:
            handled = false;
        }

        return handled;
    }

    public void onClick( View v ) {
        saveNew( new CurGameInfo( this ) );
        onContentChanged();
    }

    @Override
    protected void onListItemClick(ListView l, View v, int position, long id) {
        doOpen( position );
    }

    private void doOpen( int indx ) {
        String path = fileList()[indx];
        File file = new File( path );
        Uri uri = Uri.fromFile( file );
        Intent intent = new Intent( Intent.ACTION_EDIT, uri,
                                    GamesList.this, BoardActivity.class );
        startActivity( intent );
    }

    private void doConfig( String path )
    {
        Uri uri = Uri.fromFile( new File(path) );
        
        Intent intent = new Intent( Intent.ACTION_EDIT, uri,
                                    this, GameConfig.class );
        startActivity( intent );
    }

    private void saveNew( CurGameInfo gi )
    {
        byte[] bytes = XwJNI.gi_to_stream( gi );
        if ( null != bytes ) {
            Utils.saveGame( this, bytes );
        } else {
            Utils.logf( "gi_to_stream=>null" );
        }
    }

    static {
        System.loadLibrary("xwjni");
    }
}
