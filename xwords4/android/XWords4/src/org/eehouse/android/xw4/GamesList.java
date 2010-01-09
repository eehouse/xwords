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
    // private InputStream m_dict;
    private GameListAdapter m_adapter;

    // Menu item ids
    public static final int MENU_ITEM_DELETE = Menu.FIRST;
    public static final int MENU_ITEM_INSERT = Menu.FIRST + 1;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);


        setContentView(R.layout.game_list);

        // setDefaultKeyMode(DEFAULT_KEYS_SHORTCUT);

        // AssetManager am = getAssets();
        // try {
        //     m_dict = am.open( "BasEnglish2to8.xwd", 
        //                       android.content.res.AssetManager.ACCESS_RANDOM );
        //     Utils.logf( "opened" );
        // } catch ( java.io.IOException ee ){
        //     m_dict = null;
        //     Utils.logf( "failed to open" );
        // }

        m_adapter = new GameListAdapter( this );
        setListAdapter( m_adapter );

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

        // Inform the list we provide context menus for items
        // getListView().setOnCreateContextMenuListener(this);
        
        // Perform a managed query. The Activity will handle closing
        // and requerying the cursor when needed.
        // Cursor cursor = managedQuery(getIntent().getData(), PROJECTION, null, null,
        //         Notes.DEFAULT_SORT_ORDER);

        // Used to map notes entries from the database to views
        // SimpleCursorAdapter adapter = 
        //     new SimpleCursorAdapter(this, R.layout.noteslist_item, cursor,
        //                             new String[] { Notes.TITLE }, 
        //                             new int[] { android.R.id.text1 });
        // setListAdapter(adapter);
    }

    // @Override
    // public boolean onCreateOptionsMenu(Menu menu) {
    //     super.onCreateOptionsMenu(menu);

    //     // This is our one standard application action -- inserting a
    //     // new note into the list.
    //     menu.add(0, MENU_ITEM_INSERT, 0, R.string.menu_insert)
    //             .setShortcut('3', 'a')
    //             .setIcon(android.R.drawable.ic_menu_add);

    //     // Generate any additional actions that can be performed on the
    //     // overall list.  In a normal install, there are no additional
    //     // actions found here, but this allows other applications to extend
    //     // our menu with their own actions.
    //     Intent intent = new Intent(null, getIntent().getData());
    //     intent.addCategory(Intent.CATEGORY_ALTERNATIVE);
    //     menu.addIntentOptions(Menu.CATEGORY_ALTERNATIVE, 0, 0,
    //             new ComponentName(this, GamesList.class), null, intent, 0, null);

    //     return true;
    // }

    // @Override
    // public boolean onPrepareOptionsMenu(Menu menu) {
    //     super.onPrepareOptionsMenu(menu);
    //     final boolean haveItems = getListAdapter().getCount() > 0;

    //     // If there are any notes in the list (which implies that one of
    //     // them is selected), then we need to generate the actions that
    //     // can be performed on the current selection.  This will be a combination
    //     // of our own specific actions along with any extensions that can be
    //     // found.
    //     if (haveItems) {
    //         // This is the selected item.
    //         Uri uri = ContentUris.withAppendedId(getIntent().getData(), getSelectedItemId());

    //         // Build menu...  always starts with the EDIT action...
    //         Intent[] specifics = new Intent[1];
    //         specifics[0] = new Intent(Intent.ACTION_EDIT, uri);
    //         MenuItem[] items = new MenuItem[1];

    //         // ... is followed by whatever other actions are available...
    //         Intent intent = new Intent(null, uri);
    //         intent.addCategory(Intent.CATEGORY_ALTERNATIVE);
    //         menu.addIntentOptions(Menu.CATEGORY_ALTERNATIVE, 0, 0, null, specifics, intent, 0,
    //                 items);

    //         // Give a shortcut to the edit action.
    //         if (items[0] != null) {
    //             items[0].setShortcut('1', 'e');
    //         }
    //     } else {
    //         menu.removeGroup(Menu.CATEGORY_ALTERNATIVE);
    //     }

    //     return true;
    // }

    // @Override
    // public boolean onOptionsItemSelected(MenuItem item) {
    //     switch (item.getItemId()) {
    //     case MENU_ITEM_INSERT:
    //         // Launch activity to insert a new item
    //         startActivity(new Intent(Intent.ACTION_INSERT, getIntent().getData()));
    //         return true;
    //     }
    //     return super.onOptionsItemSelected(item);
    // }

    @Override
    public void onCreateContextMenu( ContextMenu menu, View view, 
                                     ContextMenuInfo menuInfo ) {
        // AdapterView.AdapterContextMenuInfo info;
        // try {
        //      info = (AdapterView.AdapterContextMenuInfo) menuInfo;
        // } catch (ClassCastException e) {
        //     Log.e(TAG, "bad menuInfo", e);
        //     return;
        // }

        Utils.logf( "onCreateContextMenu called" );
        MenuInflater inflater = getMenuInflater();
        inflater.inflate( R.menu.games_list_item_menu, menu );

        // Cursor cursor = (Cursor) getListAdapter().getItem(info.position);
        // if (cursor == null) {
        //     // For some reason the requested item isn't available, do nothing
        //     return;
        // }

        // Setup the menu header
        // menu.setHeaderTitle(cursor.getString(COLUMN_INDEX_TITLE));
    }
        
    private void doOpen() {
        Intent intent = new Intent( Intent.ACTION_EDIT );
        intent.setClassName( "org.eehouse.android.xw4",
                             "org.eehouse.android.xw4.BoardActivity");
        startActivity( intent );
    }

    @Override
    public boolean onContextItemSelected( MenuItem item ) {
        boolean handled = false;
        AdapterView.AdapterContextMenuInfo info;
        try {
             info = (AdapterView.AdapterContextMenuInfo) item.getMenuInfo();
        } catch (ClassCastException e) {
            Log.e(TAG, "bad menuInfo", e);
            return false;
        }

        switch (item.getItemId()) {
        case R.id.list_item_open:
            doOpen();
            handled = true;
            break;
        case R.id.list_item_view:
            Utils.logf( "view" );
            handled = true;
            break;
        case R.id.list_item_hide:
            Utils.logf( "hide" );
            handled = true;
            break;
        case R.id.list_item_delete:
            Utils.logf( "delete" );
            handled = true;
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
        }
        return handled;
    }

    public void onClick( View v ) {
        Intent intent = new Intent();
        intent.setClassName( "org.eehouse.android.xw4",
                             "org.eehouse.android.xw4.GameConfig");
        intent.setAction( Intent.ACTION_INSERT );
        startActivity( intent );
    }

    @Override
    protected void onListItemClick(ListView l, View v, int position, long id) {
        doOpen();
    }


    static {
        System.loadLibrary("xwjni");
    }
}
