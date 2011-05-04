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

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.ListActivity;
import android.os.Bundle;
import android.widget.Button;
import android.widget.TextView;
import android.widget.AdapterView;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.view.View;
import android.view.ViewGroup;
import android.view.ContextMenu;
import android.view.MenuItem;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.preference.PreferenceManager;
import android.net.Uri;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.jni.JNIUtilsImpl;
import org.eehouse.android.xw4.jni.CommonPrefs;

public class DictsActivity extends XWListActivity 
    implements View.OnClickListener,
               XWListItem.DeleteCallback {

    private static final String DICT_DOLAUNCH = "do_launch";
    private static final String DICT_LANG_EXTRA = "use_lang";
    private static final String DICT_NAME_EXTRA = "use_dict";

    private String[] m_dicts;
    private static final int PICK_STORAGE = DlgDelegate.DIALOG_LAST + 1;
    private int m_lang = 0;
    private String m_name = null;

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
    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = super.onCreateDialog( id );
        switch( id ) {
        case PICK_STORAGE:
            DialogInterface.OnClickListener lstnrSD;

            lstnrSD = new DialogInterface.OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        startDownload( m_lang, m_name, item != 
                                       DialogInterface.BUTTON_POSITIVE );
                    }
                };

            dialog = new AlertDialog.Builder( this )
                .setTitle( R.string.storeWhereTitle )
                .setMessage( R.string.storeWhereMsg )
                .setPositiveButton( R.string.button_internal, lstnrSD )
                .setNegativeButton( R.string.button_sd, lstnrSD )
                .create();
            break;
        }
        return dialog;
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

        Intent intent = getIntent();
        if ( null != intent ) {
            boolean downloadNow = intent.getBooleanExtra( DICT_DOLAUNCH, false );
            if ( downloadNow ) {
                int lang = intent.getIntExtra( DICT_LANG_EXTRA, 0 );
                String name = intent.getStringExtra( DICT_NAME_EXTRA );
                askStartDownload( lang, name );
            }
        }
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        mkListAdapter();
    }

    public void onClick( View v ) 
    {
        askStartDownload( 0, null );
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

    private void askStartDownload( int lang, String name )
    {
        if ( GameUtils.haveWriteableSD() ) {
            m_lang = lang;
            m_name = name;
            showDialog( PICK_STORAGE );
        } else {
            startDownload( lang, name, false );
        }
    }

    private void startDownload( int lang, String name, boolean toSD )
    {
        DictImportActivity.setUseSD( toSD );
        startActivity( mkDownloadIntent( this, lang, name ) );
    }

    private void mkListAdapter()
    {
        m_dicts = GameUtils.dictList( this );
        setListAdapter( new DictListAdapter( this ) );
    }

    private static Intent mkDownloadIntent( Context context,
                                            int lang, String dict )
    {
        String dict_url = CommonPrefs.getDefaultDictURL( context );
        if ( 0 != lang ) {
            dict_url += "/" + DictLangCache.getLangName( context, lang );
        }
        if ( null != dict ) {
            dict_url += "/" + dict + XWConstants.DICT_EXTN;
        }
        Uri uri = Uri.parse( dict_url );
        Intent intent = new Intent( Intent.ACTION_VIEW, uri );
        intent.setFlags( Intent.FLAG_ACTIVITY_NEW_TASK );
        return intent;
    }

    public static void launchAndDownload( Activity activity, int lang, String name )
    {
        Intent intent = new Intent( activity, DictsActivity.class );
        intent.putExtra( DICT_DOLAUNCH, true );
        if ( lang > 0 ) {
            intent.putExtra( DICT_LANG_EXTRA, lang );
        }
        if ( null != name ) {
            Assert.assertTrue( lang != 0 );
            intent.putExtra( DICT_NAME_EXTRA, name );
        }

        activity.startActivity( intent );
    }

    public static void launchAndDownload( Activity activity, int lang )
    {
        launchAndDownload( activity, lang, null );
    }

}