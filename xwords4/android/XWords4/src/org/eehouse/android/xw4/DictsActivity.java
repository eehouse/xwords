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
import android.app.ExpandableListActivity;
import android.database.DataSetObserver;
import android.os.Bundle;
import android.widget.Button;
import android.widget.TextView;
import android.widget.AdapterView;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.Resources;
import android.content.SharedPreferences;
import android.view.View;
import android.view.ViewGroup;
import android.view.ContextMenu;
import android.view.MenuItem;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.widget.ExpandableListAdapter;
import android.widget.ExpandableListView;
import android.widget.ExpandableListView.ExpandableListContextMenuInfo;
import android.preference.PreferenceManager;
import android.net.Uri;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.jni.JNIUtilsImpl;
import org.eehouse.android.xw4.jni.CommonPrefs;

public class DictsActivity extends ExpandableListActivity 
    implements View.OnClickListener, XWListItem.DeleteCallback {

    private static final String DICT_DOLAUNCH = "do_launch";
    private static final String DICT_LANG_EXTRA = "use_lang";
    private static final String DICT_NAME_EXTRA = "use_dict";

    private static final int PICK_STORAGE = DlgDelegate.DIALOG_LAST + 1;
    private static final int MOVE_DICT = DlgDelegate.DIALOG_LAST + 2;
    private static final int SET_DEFAULT = DlgDelegate.DIALOG_LAST + 3;
    private int m_lang = 0;
    private String[] m_langs;
    private String m_name = null;
    private String m_download;
    private ExpandableListView m_expView;
    private DlgDelegate m_delegate;
    private String[] m_locNames;

    private XWListItem m_rowView;
    GameUtils.DictLoc m_moveFromLoc;
    GameUtils.DictLoc m_moveToLoc;
    String m_moveName;

    LayoutInflater m_factory;

    private class DictListAdapter implements ExpandableListAdapter {
        private Context m_context;

        public DictListAdapter( Context context ) {
            //super( context, m_dicts.length );
            m_context = context;
        }

        public boolean areAllItemsEnabled() { return false; }

        public Object getChild(int groupPosition, int childPosition)
        {
            return null;
        }

        public long getChildId( int groupPosition, int childPosition )
        {
            return childPosition;
        }

        public View getChildView( int groupPosition, int childPosition, 
                                  boolean isLastChild, View convertView, 
                                  ViewGroup parent)
        {
            int lang = (int)getGroupId( groupPosition );
            String[] dicts = DictLangCache.getHaveLang( m_context, lang );
            String text;
            boolean canDelete = false;
            if ( null != dicts && childPosition < dicts.length ) {
                text = dicts[childPosition];
                canDelete = !GameUtils.dictIsBuiltin( DictsActivity.this,
                                                      text );
            } else {
                text = m_download;
            }
            XWListItem view =
                (XWListItem)m_factory.inflate( R.layout.list_item, null );
            view.setText( text );
            if ( canDelete ) {
                view.setDeleteCallback( DictsActivity.this );
            }

            GameUtils.DictLoc loc = 
                GameUtils.getDictLoc( DictsActivity.this, text );
            view.setComment( m_locNames[loc.ordinal()] );
            view.cache( loc );

            return view;
        }

        public int getChildrenCount( int groupPosition )
        {
            int lang = (int)getGroupId( groupPosition );
            String[] dicts = DictLangCache.getHaveLang( m_context, lang );
            int result = 0; // 1;     // 1 for the download option
            if ( null != dicts ) {
                result += dicts.length;
            }
            return result;
        }

        public long getCombinedChildId( long groupId, long childId )
        {
            return groupId << 16 | childId;
        }

        public long getCombinedGroupId( long groupId )
        {
            return groupId;
        }

        public Object getGroup( int groupPosition )
        {
            return null;
        }

        public int getGroupCount()
        {
            return m_langs.length;
        }

        public long getGroupId( int groupPosition )
        {
            int lang = DictLangCache.getLangLangCode( m_context, 
                                                      m_langs[groupPosition] );
            return lang;
        }

        public View getGroupView( int groupPosition, boolean isExpanded, 
                                  View convertView, ViewGroup parent )
        {
            View row = LayoutInflater.from(DictsActivity.this).
                inflate(android.R.layout.simple_expandable_list_item_1, null );
            TextView view = (TextView)row.findViewById( android.R.id.text1 );
            view.setText( m_langs[groupPosition] );
            return view;
        }

        public boolean hasStableIds() { return false; }
        public boolean isChildSelectable( int groupPosition, int childPosition ) { return true; }
        public boolean isEmpty() { return false; }
        public void onGroupCollapsed(int groupPosition){}
        public void onGroupExpanded(int groupPosition){}
        public void registerDataSetObserver( DataSetObserver obs ){}
        public void unregisterDataSetObserver( DataSetObserver obs ){}

        // public Object getItem( int position) { return m_dicts[position]; }
        // public View getView( final int position, View convertView, 
        //                      ViewGroup parent ) {
        //     LayoutInflater factory = LayoutInflater.from( DictsActivity.this );
        //     final XWListItem view
        //         = (XWListItem)factory.inflate( R.layout.list_item, null );
        //     view.setPosition( position );

        //     // append language name
        //     view.setText( DictLangCache.
        //                   annotatedDictName( DictsActivity.this, 
        //                                      m_dicts[position] ) );
        //     if ( !GameUtils.dictIsBuiltin( DictsActivity.this,
        //                                    m_dicts[position] ) ) {
        //         view.setDeleteCallback( DictsActivity.this );
        //     }

        //     return view;
        // }
    }

    @Override
    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog;
        DialogInterface.OnClickListener lstnr;

        switch( id ) {
        case PICK_STORAGE:

            lstnr = new DialogInterface.OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        startDownload( m_lang, m_name, item != 
                                       DialogInterface.BUTTON_POSITIVE );
                    }
                };

            dialog = new AlertDialog.Builder( this )
                .setTitle( R.string.storeWhereTitle )
                .setMessage( R.string.storeWhereMsg )
                .setPositiveButton( R.string.button_internal, lstnr )
                .setNegativeButton( R.string.button_sd, lstnr )
                .create();
            break;
        case MOVE_DICT:
            lstnr = new DialogInterface.OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        Utils.logf( "CALLING moveDict" );
                        if ( GameUtils.moveDict( DictsActivity.this,
                                                 m_moveName,
                                                 m_moveFromLoc,
                                                 m_moveToLoc ) ) {
                            m_rowView.
                                setComment( m_locNames[m_moveToLoc.ordinal()]);
                            m_rowView.invalidate();
                        }
                        Utils.logf( "moveDict RETURNED" );
                    }
                };
            dialog = new AlertDialog.Builder( this )
                .setMessage( "" ) // will set later
                .setPositiveButton( R.string.button_ok, lstnr )
                .setNegativeButton( R.string.button_cancel, null )
                .create();
            break;
        case SET_DEFAULT:
            lstnr = new DialogInterface.OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        if ( DialogInterface.BUTTON_NEGATIVE == item
                             || DialogInterface.BUTTON_POSITIVE == item ) {
                            setDefault( R.string.key_default_dict, m_rowView );
                        }
                        if ( DialogInterface.BUTTON_NEGATIVE == item 
                             || DialogInterface.BUTTON_NEUTRAL == item ) {
                            setDefault( R.string.key_default_robodict, 
                                        m_rowView );
                        }
                    }
                };
            dialog = new AlertDialog.Builder( this )
                .setTitle( R.string.query_title )
                .setMessage( "" ) // or can't change it later!
                .setPositiveButton( R.string.button_default_human, lstnr )
                .setNeutralButton( R.string.button_default_robot, lstnr )
                .setNegativeButton( R.string.button_default_both, lstnr )
                .create();
            break;
        default:
            dialog = m_delegate.onCreateDialog( id );
            break;
        }
        return dialog;
    }

    @Override
    public void onPrepareDialog( int id, Dialog dialog )
    {
        AlertDialog ad = (AlertDialog)dialog;
        String format;
        String message;

        switch( id ) {
        case PICK_STORAGE:
            break;
        case MOVE_DICT:
            format = getString( R.string.move_dictf );
            message = String.format( format, m_moveName,
                                     m_locNames[ m_moveFromLoc.ordinal() ],
                                     m_locNames[ m_moveToLoc.ordinal() ] );
            ad.setMessage( message );
            break;
        case SET_DEFAULT:
            String lang = 
                DictLangCache.getLangName( this, m_rowView.getText() );
            format = getString( R.string.set_default_messagef );
            message = String.format( format, lang );
            ad.setMessage( message );
            break;
        default:
            m_delegate.onPrepareDialog( id, dialog );
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) 
    {
        super.onCreate( savedInstanceState );

        Resources res = getResources();
        m_locNames = res.getStringArray( R.array.loc_names );

        m_delegate = new DlgDelegate( this );
        m_factory = LayoutInflater.from( this );

        m_download = getString( R.string.download_dicts );
            
        setContentView( R.layout.dict_browse );
        m_expView = getExpandableListView();
        registerForContextMenu( m_expView );

        Button download = (Button)findViewById( R.id.download );
        download.setOnClickListener( this );

        mkListAdapter();

        // showNotAgainDlg( R.string.not_again_dicts, 
        //                  R.string.key_notagain_dicts );

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
        expandGroups();
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

        ExpandableListView.ExpandableListContextMenuInfo info
            = (ExpandableListView.ExpandableListContextMenuInfo)menuInfo;
        long packedPosition = info.packedPosition;
        int childPosition = ExpandableListView.
            getPackedPositionChild( packedPosition );
        // int groupPosition = ExpandableListView.
        //     getPackedPositionGroup( packedPosition );
        // Utils.logf( "onCreateContextMenu: group: %d; child: %d", 
        //             groupPosition, childPosition );

        // We don't have a menu yet for languages, just for their dict
        // children
        if ( childPosition >= 0 ) {
            MenuInflater inflater = getMenuInflater();
            inflater.inflate( R.menu.dicts_item_menu, menu );
            
            XWListItem row = (XWListItem)info.targetView;
            GameUtils.DictLoc loc = (GameUtils.DictLoc)row.getCached();
            if ( loc == GameUtils.DictLoc.BUILT_IN
                 || ! GameUtils.haveWriteableSD() ) {
                menu.removeItem( R.id.dicts_item_move );
            }
        }
    }
   
    @Override
    public boolean onContextItemSelected( MenuItem item ) 
    {
        boolean handled = false;
        ExpandableListContextMenuInfo info = null;
        try {
            info = (ExpandableListContextMenuInfo)item.getMenuInfo();
        } catch (ClassCastException e) {
            Utils.logf( "bad menuInfo:" + e.toString() );
            return false;
        }
        
        XWListItem row = (XWListItem)info.targetView;
        int id = item.getItemId();
        switch( id ) {
        case R.id.dicts_item_move:
            askMoveDict( row );
            break;
        case R.id.dicts_item_select:
            m_rowView = row;
            showDialog( SET_DEFAULT );
            break;
        case R.id.dicts_item_details:
            Utils.notImpl( this );
            break;
        }

        return handled;
    }

    private void setDefault( int keyId, final XWListItem text )
    {
        SharedPreferences sp
            = PreferenceManager.getDefaultSharedPreferences( this );
        SharedPreferences.Editor editor = sp.edit();
        String key = getString( keyId );
        String name = text.getText();
        editor.putString( key, name );
        editor.commit();
    }

    // Move dict.  Put up dialog asking user to confirm move from XX
    // to YY.  So we need both XX and YY.  There may be several
    // options for YY?
    private void askMoveDict( XWListItem item )
    {
        m_rowView = item;
        m_moveFromLoc = (GameUtils.DictLoc)item.getCached();
        if ( m_moveFromLoc == GameUtils.DictLoc.INTERNAL ) {
            m_moveToLoc = GameUtils.DictLoc.EXTERNAL;
        } else {
            m_moveToLoc = GameUtils.DictLoc.INTERNAL;
        }
        m_moveName = item.getText();

        showDialog( MOVE_DICT );
    }

    // DeleteCallback interface
    public void deleteCalled( int myPosition, final String dict )
    {
        int code = DictLangCache.getDictLangCode( this, dict );
        String lang = DictLangCache.getLangName( this, code );
        int nGames = DBUtils.countGamesUsing( this, code );
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
            if ( 1 == DictLangCache.getHaveLang( this, code ).length ) {
                fmt = R.string.confirm_deleteonly_dictf;
            } else {
                fmt = R.string.confirm_deletemore_dictf;
            }
            msg += String.format( getString(fmt), lang );
        }

        m_delegate.showConfirmThen( msg, action );
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
        m_langs = DictLangCache.listLangs( this );
        //m_langs = DictLangCache.getLangNames( this );
        ExpandableListAdapter adapter = new DictListAdapter( this );
        setListAdapter( adapter );
    }

    private void expandGroups()
    {
        for ( int ii = 0; ii < m_langs.length; ++ii ) {
            m_expView.expandGroup( ii );
        }
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

    public static void launchAndDownload( Activity activity, int lang, 
                                          String name )
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