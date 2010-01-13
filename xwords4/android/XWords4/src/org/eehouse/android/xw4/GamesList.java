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

import org.eehouse.android.xw4.XWords4.Games; // for constants

public class GamesList extends ListActivity implements View.OnClickListener {
    private static final String TAG = "GamesList";
    private GameListAdapter m_adapter;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.game_list);

        // setDefaultKeyMode(DEFAULT_KEYS_SHORTCUT);

        registerForContextMenu( getListView() );

        Button newGameB = (Button)findViewById(R.id.new_game);
        newGameB.setOnClickListener( this );
        Utils.logf( "got button" );

        // If no data was given in the intent (because we were started
        // as a MAIN activity), then use our default content provider.
        Intent intent = getIntent();
        Utils.logf( intent.toString() );
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
        boolean handled = false;

        switch (item.getItemId()) {
        case R.id.list_item_open:
            doOpen();
            handled = true;
            break;
        case R.id.list_item_view:
        case R.id.list_item_hide:
        case R.id.list_item_delete:
        case R.id.list_item_copy:
        case R.id.list_item_new_from:
        case R.id.list_item_move_up:
        case R.id.list_item_move_down:
        case R.id.list_item_move_to_top:
        case R.id.list_item_move_to_bottom:
            handled = true;
            Utils.notImpl( this );
            break;
        }
        return handled;
    }

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
            for( String file : fileList() ) {
                deleteFile( file  );
            }
            m_adapter = new GameListAdapter( this );
            setListAdapter( m_adapter );
            handled = true;
            break;
        case R.id.gamel_menu_view_hidden:
            Utils.notImpl( this );
            break;
        }
        return handled;
    }

    public void onClick( View v ) {
        Intent intent = new Intent( GamesList.this, GameConfig.class );
        intent.setAction( Intent.ACTION_INSERT );
        startActivity( intent );
    }

    @Override
    protected void onListItemClick(ListView l, View v, int position, long id) {
        doOpen();
    }

    private void doOpen() {
        Intent intent = new Intent( Intent.ACTION_EDIT );
        intent.setClassName( "org.eehouse.android.xw4",
                             "org.eehouse.android.xw4.BoardActivity");
        startActivity( intent );
    }

    static {
        System.loadLibrary("xwjni");
    }
}
