/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
/*
 * Copyright 2009 - 2011 by Eric House (xwords@eehouse.org).  All
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
import android.app.ExpandableListActivity;
import android.database.DataSetObserver;
import android.os.Bundle;
import android.os.Handler;
import android.widget.Button;
import android.widget.TextView;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
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
import android.widget.Toast;
import android.preference.PreferenceManager;
import android.net.Uri;
import java.util.Arrays;
import java.util.HashMap;
import junit.framework.Assert;

import org.eehouse.android.xw4.DictUtils.DictAndLoc;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.jni.JNIUtilsImpl;
import org.eehouse.android.xw4.jni.CommonPrefs;

public class DictsActivity extends ExpandableListActivity 
    implements View.OnClickListener, XWListItem.DeleteCallback,
               MountEventReceiver.SDCardNotifiee, DlgDelegate.DlgClickNotify {

    private static final String DICT_DOLAUNCH = "do_launch";
    private static final String DICT_LANG_EXTRA = "use_lang";
    private static final String DICT_NAME_EXTRA = "use_dict";
    private static final String PACKED_POSITION = "packed_position";
    private static final String DELETE_DICT = "delete_dict";
    private static final String NAME = "name";
    private static final String LANG = "lang";
    private static final String MOVEFROMLOC = "movefromloc";

    private static HashMap<String,Boolean> s_openStates =
        new HashMap<String,Boolean>();

    // For new callback alternative
    private static final int DELETE_DICT_ACTION = 1;

    private static final int PICK_STORAGE = DlgDelegate.DIALOG_LAST + 1;
    private static final int MOVE_DICT = DlgDelegate.DIALOG_LAST + 2;
    private static final int SET_DEFAULT = DlgDelegate.DIALOG_LAST + 3;
    private int m_lang = 0;
    private String[] m_langs;
    private String m_name = null;
    private String m_deleteDict = null;
    private String m_download;
    private ExpandableListView m_expView;
    private DlgDelegate m_delegate;
    private String[] m_locNames;
    private DictListAdapter m_adapter;

    private long m_packedPosition;
    private DictUtils.DictLoc m_moveFromLoc;
    private int m_moveFromItem;
    private int m_moveToItm;

    private LayoutInflater m_factory;

    private class DictListAdapter implements ExpandableListAdapter {
        private Context m_context;
        private XWListItem[][] m_cache;

        public DictListAdapter( Context context ) {
            m_context = context;
        }

        public boolean areAllItemsEnabled() { return false; }

        public Object getChild( int groupPosition, int childPosition )
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
            return getChildView( groupPosition, childPosition );
        }

        private View getChildView( int groupPosition, int childPosition )
        {
            XWListItem view = null;
            if ( null != m_cache && null != m_cache[groupPosition] ) {
                view = m_cache[groupPosition][childPosition];
            }

            if ( null == view ) {
                view = (XWListItem)
                    m_factory.inflate( R.layout.list_item, null );

                int lang = (int)getGroupId( groupPosition );
                DictAndLoc[] dals = DictLangCache.getDALsHaveLang( m_context, 
                                                                   lang );

                if ( null != dals && childPosition < dals.length ) {
                    DictAndLoc dal;
                    dal = dals[childPosition];
                    view.setText( dal.name );

                    DictUtils.DictLoc loc = dal.loc;
                    view.setComment( m_locNames[loc.ordinal()] );
                    view.cache( loc );
                    if ( DictUtils.DictLoc.BUILT_IN != loc ) {
                        view.setDeleteCallback( DictsActivity.this );
                    }
                } else {
                    view.setText( m_download );
                }

                addToCache( groupPosition, childPosition, view );
                view.setOnClickListener( DictsActivity.this );
            }
            return view;
        }

        public int getChildrenCount( int groupPosition )
        {
            int lang = (int)getGroupId( groupPosition );
            DictAndLoc[] dals = DictLangCache.getDALsHaveLang( m_context, lang );
            int result = 0; // 1;     // 1 for the download option
            if ( null != dals ) {
                result += dals.length;
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
            View row = 
                Utils.inflate(DictsActivity.this, 
                              android.R.layout.simple_expandable_list_item_1 );
            TextView view = (TextView)row.findViewById( android.R.id.text1 );
            view.setText( m_langs[groupPosition] );
            return view;
        }

        public boolean hasStableIds() { return false; }
        public boolean isChildSelectable( int groupPosition, 
                                          int childPosition ) { return true; }
        public boolean isEmpty() { return false; }
        public void onGroupCollapsed(int groupPosition)
        {
            s_openStates.put( m_langs[groupPosition], false );
        }
        public void onGroupExpanded(int groupPosition){
            s_openStates.put( m_langs[groupPosition], true );
        }
        public void registerDataSetObserver( DataSetObserver obs ){}
        public void unregisterDataSetObserver( DataSetObserver obs ){}

        protected XWListItem getSelChildView()
        {
            int groupPosition = 
                ExpandableListView.getPackedPositionGroup( m_packedPosition );
            int childPosition = 
                ExpandableListView.getPackedPositionChild( m_packedPosition );
            return (XWListItem)getChildView( groupPosition, childPosition );
        }

        private void addToCache( int group, int child, XWListItem view )
        {
            if ( null == m_cache ) {
                m_cache = new XWListItem[getGroupCount()][];
            }
            if ( null == m_cache[group] ) {
                m_cache[group] = new XWListItem[getChildrenCount(group)];
            }
            Assert.assertTrue( null == m_cache[group][child] );
            m_cache[group][child] = view;
        }
    }

    @Override
    protected Dialog onCreateDialog( int id )
    {
        OnClickListener lstnr;
        Dialog dialog;
        String format;
        String message;
        boolean doRemove = true;

        switch( id ) {
        case PICK_STORAGE:
            lstnr = new OnClickListener() {
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
            message = Utils.format( this, R.string.move_dictf,
                                    m_adapter.getSelChildView().getText() );

            String[] items = new String[3];
            for ( int ii = 0; ii < 3; ++ii ) {
                DictUtils.DictLoc loc = itemToRealLoc(ii);
                if ( loc.equals( m_moveFromLoc ) ) {
                    m_moveFromItem = ii;
                }
                items[ii] = m_locNames[loc.ordinal()];
            }

            OnClickListener newSelLstnr =
                new OnClickListener() {
                    public void onClick( DialogInterface dlgi, int item ) {
                        m_moveToItm = item;
                        AlertDialog dlg = (AlertDialog)dlgi;
                        Button btn = 
                            dlg.getButton( AlertDialog.BUTTON_POSITIVE ); 
                        btn.setEnabled( m_moveToItm != m_moveFromItem );
                    }
                };

            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        XWListItem rowView = m_adapter.getSelChildView();
                        Assert.assertTrue( m_moveToItm != m_moveFromItem );
                        DictUtils.DictLoc toLoc = itemToRealLoc( m_moveToItm );
                        if ( DictUtils.moveDict( DictsActivity.this,
                                                 rowView.getText(),
                                                 m_moveFromLoc,
                                                 toLoc ) ) {
                            rowView.setComment( m_locNames[toLoc.ordinal()] );
                            rowView.cache( toLoc );
                            rowView.invalidate();
                        } else {
                            Utils.logf( "moveDict(%s) failed", 
                                        rowView.getText() );
                        }
                    }
                };

            dialog = new AlertDialog.Builder( this )
                .setTitle( message )
                .setSingleChoiceItems( items, m_moveFromItem, newSelLstnr )
                .setPositiveButton( R.string.button_move, lstnr )
                .setNegativeButton( R.string.button_cancel, null )
                .create();
            break;

        case SET_DEFAULT:
            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        if ( DialogInterface.BUTTON_NEGATIVE == item
                             || DialogInterface.BUTTON_POSITIVE == item ) {
                            setDefault( R.string.key_default_dict );
                        }
                        if ( DialogInterface.BUTTON_NEGATIVE == item 
                             || DialogInterface.BUTTON_NEUTRAL == item ) {
                            setDefault( R.string.key_default_robodict );
                        }
                    }
                };
            XWListItem rowView = m_adapter.getSelChildView();
            String lang = 
                DictLangCache.getLangName( this, rowView.getText() );
            format = getString( R.string.set_default_messagef );
            message = String.format( format, lang );
            dialog = new AlertDialog.Builder( this )
                .setTitle( R.string.query_title )
                .setMessage( message )
                .setPositiveButton( R.string.button_default_human, lstnr )
                .setNeutralButton( R.string.button_default_robot, lstnr )
                .setNegativeButton( R.string.button_default_both, lstnr )
                .create();
            break;
        default:
            dialog = m_delegate.onCreateDialog( id );
            doRemove = false;
            break;
        }

        if ( doRemove && null != dialog ) {
            Utils.setRemoveOnDismiss( this, dialog, id );
        }

        return dialog;
    } // onCreateDialog

    @Override
    protected void onPrepareDialog( int id, Dialog dialog )
    {
        super.onPrepareDialog( id, dialog );
        m_delegate.onPrepareDialog( id, dialog );

        if ( MOVE_DICT == id ) {
            // The move button should always start out disabled
            // because the selected location should be where it
            // currently is.
            ((AlertDialog)dialog).getButton( AlertDialog.BUTTON_POSITIVE )
                .setEnabled( false );
        }
    }

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );
        getBundledData( savedInstanceState );

        Resources res = getResources();
        m_locNames = res.getStringArray( R.array.loc_names );

        m_delegate = new DlgDelegate( this, this, savedInstanceState );
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
        MountEventReceiver.register( this );

        mkListAdapter();
        expandGroups();
    }

    @Override
    protected void onSaveInstanceState( Bundle outState ) 
    {
        super.onSaveInstanceState( outState );
        m_delegate.onSaveInstanceState( outState );

        outState.putLong( PACKED_POSITION, m_packedPosition );
        outState.putString( NAME, m_name );
        outState.putInt( LANG, m_lang );
        outState.putString( DELETE_DICT, m_deleteDict );
        if ( null != m_moveFromLoc ) {
            outState.putInt( MOVEFROMLOC, m_moveFromLoc.ordinal() );
        }
    }

    private void getBundledData( Bundle savedInstanceState )
    {
        if ( null != savedInstanceState ) {
            m_packedPosition = savedInstanceState.getLong( PACKED_POSITION );
            m_name = savedInstanceState.getString( NAME );
            m_lang = savedInstanceState.getInt( LANG );
            m_deleteDict = savedInstanceState.getString( DELETE_DICT );

            int tmp = savedInstanceState.getInt( MOVEFROMLOC, -1 );
            if ( -1 != tmp ) {
                m_moveFromLoc = DictUtils.DictLoc.values()[tmp];
            }
        }
    }

    @Override
    protected void onStop() {
        MountEventReceiver.unregister( this );
        super.onStop();
    }

    public void onClick( View view ) 
    {
        if ( view instanceof Button ) {
            askStartDownload( 0, null );
        } else {
            XWListItem item = (XWListItem)view;
            DictBrowseActivity.launch( this, item.getText() );
        }
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
            DictUtils.DictLoc loc = (DictUtils.DictLoc)row.getCached();
            if ( loc == DictUtils.DictLoc.BUILT_IN
                 || ! DictUtils.haveWriteableSD() ) {
                menu.removeItem( R.id.dicts_item_move );
            }

            String fmt = getString(R.string.game_item_menu_titlef );
            menu.setHeaderTitle( String.format( fmt, row.getText() ) );
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
            Utils.logf( "bad menuInfo: %s", e.toString() );
            return false;
        }
        
        m_packedPosition = info.packedPosition;

        int id = item.getItemId();
        switch( id ) {
        case R.id.dicts_item_move:
            askMoveDict( (XWListItem)info.targetView );
            break;
        case R.id.dicts_item_select:
            showDialog( SET_DEFAULT );
            break;
        }

        return handled;
    }

    private void setDefault( int keyId )
    {
        XWListItem view = m_adapter.getSelChildView();
        SharedPreferences sp
            = PreferenceManager.getDefaultSharedPreferences( this );
        SharedPreferences.Editor editor = sp.edit();
        String key = getString( keyId );
        String name = view.getText();
        editor.putString( key, name );
        editor.commit();
    }

    // Move dict.  Put up dialog asking user to confirm move from XX
    // to YY.  So we need both XX and YY.  There may be several
    // options for YY?
    private void askMoveDict( XWListItem item )
    {
        m_moveFromLoc = (DictUtils.DictLoc)item.getCached();
        showDialog( MOVE_DICT );
    }

    // XWListItem.DeleteCallback interface
    public void deleteCalled( XWListItem item )
    {
        String dict = item.getText();
        String msg = String.format( getString( R.string.confirm_delete_dictf ),
                                    dict );

        m_deleteDict = dict;
        m_moveFromLoc = (DictUtils.DictLoc)item.getCached();

        // When and what to warn about.  First off, if there's another
        // identical dict, simply confirm.  Or if nobody's using this
        // dict *and* it's not the last of a language that somebody's
        // using, simply confirm.  If somebody is using it, then we
        // want different warnings depending on whether it's the last
        // available dict in its language.

        if ( 1 < DictLangCache.getDictCount( this, dict ) ) {
            // there's another; do nothing
        } else {
            int fmtid = 0;
            int langcode = DictLangCache.getDictLangCode( this, dict );
            DictAndLoc[] langDals = DictLangCache.getDALsHaveLang( this, 
                                                                   langcode );
            int nUsingLang = DBUtils.countGamesUsingLang( this, langcode );

            if ( 1 == langDals.length ) { // last in this language?
                if ( 0 < nUsingLang ) {
                    fmtid = R.string.confirm_deleteonly_dictf;
                }
            } else if ( 0 < DBUtils.countGamesUsingDict( this, dict ) ) {
                fmtid = R.string.confirm_deletemore_dictf;
            }
            if ( 0 != fmtid ) {
                String fmt = getString( fmtid );
                msg += String.format( fmt, DictLangCache.
                                       getLangName( this, langcode ) );
            }
        }

        m_delegate.showConfirmThen( msg, R.string.button_delete, 
                                    DELETE_DICT_ACTION );
    }

    // MountEventReceiver.SDCardNotifiee interface
    public void cardMounted( boolean nowMounted )
    {
        Utils.logf( "DictsActivity.cardMounted(%b)", nowMounted );
        // post so other SDCardNotifiee implementations get a chance
        // to process first: avoid race conditions
        new Handler().post( new Runnable() {
                public void run() {
                    mkListAdapter();
                    expandGroups();
                }
            } );
    }

    // DlgDelegate.DlgClickNotify interface
    public void dlgButtonClicked( int id, int which )
    {
        switch( id ) {
        case DELETE_DICT_ACTION:
            if ( DialogInterface.BUTTON_POSITIVE == which ) {
                deleteDict( m_deleteDict, m_moveFromLoc );
            }
            break;
        default:
            Assert.fail();
        }
    }

    private DictUtils.DictLoc itemToRealLoc( int item )
    {
        item += DictUtils.DictLoc.INTERNAL.ordinal();
        return DictUtils.DictLoc.values()[item];
    }

    private void deleteDict( String dict, DictUtils.DictLoc loc )
    {
        DictUtils.deleteDict( this, dict, loc );
        DictLangCache.inval( this, dict, loc, false );
        mkListAdapter();
        expandGroups();
    }

    private void askStartDownload( int lang, String name )
    {
        if ( DictUtils.haveWriteableSD() ) {
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
        try {
            startActivity( mkDownloadIntent( this, lang, name ) );
        } catch ( android.content.ActivityNotFoundException anfe ) {
            Toast.makeText( this, R.string.no_download_warning, 
                            Toast.LENGTH_SHORT).show();
        }
    }

    private void mkListAdapter()
    {
        m_langs = DictLangCache.listLangs( this );
        Arrays.sort( m_langs );
        m_adapter = new DictListAdapter( this );
        setListAdapter( m_adapter );
    }

    private void expandGroups()
    {
        for ( int ii = 0; ii < m_langs.length; ++ii ) {
            boolean open = true;
            String lang = m_langs[ii];
            if ( s_openStates.containsKey( lang ) ) {
                open = s_openStates.get( lang );
            }
            if ( open ) {
                m_expView.expandGroup( ii );
            }
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