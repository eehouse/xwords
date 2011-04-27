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
import android.os.Bundle;
import android.widget.Button;
import android.widget.TextView;
import android.widget.AdapterView;
import android.content.Context;
import android.content.DialogInterface;
import android.view.View;
import android.view.ViewGroup;
import android.view.ContextMenu;
import android.view.MenuItem;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.preference.PreferenceManager;
import android.content.SharedPreferences;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.jni.JNIUtilsImpl;

public class DictsActivity extends XWListActivity 
    implements View.OnClickListener,
               XWListItem.DeleteCallback {
    String[] m_dicts;

    private class DictListAdapter extends XWListAdapter {
        private Context m_context;

        public DictListAdapter( Context context ) {
            super( context, m_dicts.length );
            m_context = context;
        }

        public Object getItem( int position) { return m_dicts[position]; }
        public View getView( final int position, View convertView, 
                             ViewGroup parent ) {
            LayoutInflater factory = LayoutInflater.from( DictsActivity.this );
            final XWListItem view
                = (XWListItem)factory.inflate( R.layout.list_item, null );
            view.setPosition( position );

            // append language name
            view.setText( DictLangCache.
                          annotatedDictName( DictsActivity.this, 
                                             m_dicts[position] ) );
            if ( !GameUtils.dictIsBuiltin( DictsActivity.this,
                                           m_dicts[position] ) ) {
                view.setDeleteCallback( DictsActivity.this );
            }

            return view;
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate( savedInstanceState );

        setContentView( R.layout.dict_browse );
        registerForContextMenu( getListView() );

        Button download = (Button)findViewById( R.id.download );
        download.setOnClickListener( this );

        mkListAdapter();

        showNotAgainDlg( R.string.not_again_dicts, 
                         R.string.key_notagain_dicts );
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        mkListAdapter();
    }

    public void onClick( View v ) {
        startActivity( Utils.mkDownloadActivity( this ) );
    }

    @Override
    public void onCreateContextMenu( ContextMenu menu, View view, 
                                     ContextMenuInfo menuInfo ) 
    {
        super.onCreateContextMenu( menu, view, menuInfo );

        MenuInflater inflater = getMenuInflater();
        inflater.inflate( R.menu.dicts_item_menu, menu );

        AdapterView.AdapterContextMenuInfo info
            = (AdapterView.AdapterContextMenuInfo)menuInfo;
    }
   
    @Override
    public boolean onContextItemSelected( MenuItem item ) 
    {
        boolean handled = false;
        AdapterView.AdapterContextMenuInfo info;
        try {
            info = (AdapterView.AdapterContextMenuInfo) item.getMenuInfo();
        } catch (ClassCastException e) {
            Utils.logf( "bad menuInfo:" + e.toString() );
            return false;
        }
        
        int id = item.getItemId();
        int position = info.position;
        switch( id ) {
        case R.id.dicts_item_select_human:
            setDefault( R.string.key_default_dict, position );
            break;
        case R.id.dicts_item_select_robot:
            setDefault( R.string.key_default_robodict, position );
            break;
        case R.id.dicts_item_details:
            Utils.notImpl( this );
            break;
        }

        return handled;
    }

    private void setDefault( int keyId, int position )
    {
        SharedPreferences sp
            = PreferenceManager.getDefaultSharedPreferences( this );
        SharedPreferences.Editor editor = sp.edit();
        String key = getString( keyId );
        editor.putString( key, m_dicts[position] );
        editor.commit();
    }

    // DeleteCallback interface
    public void deleteCalled( final int myPosition )
    {
        final String dict = m_dicts[myPosition];
        int lang = DictLangCache.getDictLangCode( this, dict );
        int nGames = DBUtils.countGamesUsing( this, lang );
        String msg = String.format( getString( R.string.confirm_delete_dictf ),
                                    dict );
        DialogInterface.OnClickListener action = 
            new DialogInterface.OnClickListener() {
                public void onClick( DialogInterface dlg, int item ) {
                    deleteDict( dict );
                }
            };

        if ( nGames > 0 ) {
            int fmt;
            if ( 1 == DictLangCache.getHaveLang( this, lang ).length ) {
                fmt = R.string.confirm_deleteonly_dictf;
            } else {
                fmt = R.string.confirm_deletemore_dictf;
            }
            String langName = DictLangCache.getLangName( this, lang );
            msg += String.format( getString(fmt), langName );
        }

        showConfirmThen( msg, action );
    }

    private void deleteDict( String dict )
    {
        GameUtils.deleteDict( this, dict );
        DictLangCache.inval( this, dict, false );
        mkListAdapter();
    }

    private void mkListAdapter()
    {
        m_dicts = GameUtils.dictList( this );
        setListAdapter( new DictListAdapter( this ) );
    }
}