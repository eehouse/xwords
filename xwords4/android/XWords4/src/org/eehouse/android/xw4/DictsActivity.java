/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2009 - 2012 by Eric House (xwords@eehouse.org).  All
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

import android.text.TextUtils;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.database.DataSetObserver;
import android.net.Uri;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ExpandableListAdapter;
import android.widget.ExpandableListView;
import android.widget.AdapterView;
import android.widget.PopupMenu;
import android.widget.TextView;

import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;

import junit.framework.Assert;

import org.eehouse.android.xw4.DictUtils.DictAndLoc;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.jni.JNIUtilsImpl;
import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.DictUtils.DictLoc;

public class DictsActivity extends XWExpandableListActivity 
    implements View.OnClickListener, AdapterView.OnItemLongClickListener,
               SelectableItem, MountEventReceiver.SDCardNotifiee, 
               DlgDelegate.DlgClickNotify,
               DictImportActivity.DownloadFinishedListener {

    private static interface SafePopup {
        public void doPopup( Context context, View button, String curDict );
    }
    private static SafePopup s_safePopup = null;

    private static final String DICT_DOLAUNCH = "do_launch";
    private static final String DICT_LANG_EXTRA = "use_lang";
    private static final String DICT_NAME_EXTRA = "use_dict";

    private HashSet<String> m_closedLangs;

    // For new callback alternative
    private static final int DELETE_DICT_ACTION = 1;

    private static final int MOVE_DICT = DlgDelegate.DIALOG_LAST + 1;
    private static final int SET_DEFAULT = DlgDelegate.DIALOG_LAST + 2;
    private static final int DICT_OR_DECLINE = DlgDelegate.DIALOG_LAST + 3;

    // I can't provide a subclass of MenuItem to hold DictAndLoc, so
    // settle for a hash on the side.
    private static HashMap<MenuItem, DictAndLoc> s_itemData;

    private String[] m_langs;
    private String m_download;
    private ExpandableListView m_expView;
    private String[] m_locNames;
    private DictListAdapter m_adapter;
    private HashSet<XWListItem> m_selDicts;
    private CharSequence m_origTitle;

    private boolean m_launchedForMissing = false;

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
                view.setSelCB( DictsActivity.this );

                int lang = (int)getGroupId( groupPosition );
                DictAndLoc[] dals = DictLangCache.getDALsHaveLang( m_context, 
                                                                   lang );

                if ( null != dals && childPosition < dals.length ) {
                    DictAndLoc dal;
                    dal = dals[childPosition];
                    view.setText( dal.name );

                    DictLoc loc = dal.loc;
                    view.setComment( m_locNames[loc.ordinal()] );
                    view.cache( loc );
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
            m_closedLangs.add( m_langs[groupPosition] );
            saveClosed();
        }
        public void onGroupExpanded(int groupPosition){
            m_closedLangs.remove( m_langs[groupPosition] );
            saveClosed();
        }
        public void registerDataSetObserver( DataSetObserver obs ){}
        public void unregisterDataSetObserver( DataSetObserver obs ){}

        protected XWListItem getSelChildView()
        {
            Assert.assertTrue( 1 == m_selDicts.size() );
            return m_selDicts.iterator().next();
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
        OnClickListener lstnr, lstnr2;
        Dialog dialog;
        String format;
        String message;
        boolean doRemove = true;

        switch( id ) {
        case MOVE_DICT:
            final XWListItem[] selItems = getSelItems();
            final int[] moveTo = { -1 };
            message = Utils.format( this, R.string.move_dictf,
                                    getJoinedNames( selItems ) );

            OnClickListener newSelLstnr =
                new OnClickListener() {
                    public void onClick( DialogInterface dlgi, int item ) {
                        moveTo[0] = item;
                        AlertDialog dlg = (AlertDialog)dlgi;
                        Button btn = 
                            dlg.getButton( AlertDialog.BUTTON_POSITIVE ); 
                        btn.setEnabled( true );
                    }
                };

            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        DictLoc toLoc = itemToRealLoc( moveTo[0] );
                        for ( XWListItem selItem : selItems ) {
                            DictLoc fromLoc = (DictLoc)selItem.getCached();
                            String name = selItem.getText();
                            if ( fromLoc == toLoc ) {
                                DbgUtils.logf( "not moving %s: same loc", name );
                            } else if ( DictUtils.moveDict( DictsActivity.this,
                                                            name, fromLoc, 
                                                            toLoc ) ) {
                                selItem.setComment( m_locNames[toLoc.ordinal()] );
                                selItem.cache( toLoc );
                                selItem.invalidate();
                                DBUtils.dictsMoveInfo( DictsActivity.this,
                                                       name, fromLoc, toLoc );
                            } else {
                                DbgUtils.logf( "moveDict(%s) failed", name );
                            }
                        }
                    }
                };

            dialog = new AlertDialog.Builder( this )
                .setTitle( message )
                .setSingleChoiceItems( makeDictDirItems(), moveTo[0],
                                       newSelLstnr )
                .setPositiveButton( R.string.button_move, lstnr )
                .setNegativeButton( R.string.button_cancel, null )
                .create();
            break;

        case SET_DEFAULT:
            final XWListItem row = m_selDicts.iterator().next();
            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        if ( DialogInterface.BUTTON_NEGATIVE == item
                             || DialogInterface.BUTTON_POSITIVE == item ) {
                            setDefault( row, R.string.key_default_dict );
                        }
                        if ( DialogInterface.BUTTON_NEGATIVE == item 
                             || DialogInterface.BUTTON_NEUTRAL == item ) {
                            setDefault( row, R.string.key_default_robodict );
                        }
                    }
                };
            String name = row.getText();
            String lang = DictLangCache.getLangName( this, name);
            message = getString( R.string.set_default_messagef, name, lang );
            dialog = new AlertDialog.Builder( this )
                .setTitle( R.string.query_title )
                .setMessage( message )
                .setPositiveButton( R.string.button_default_human, lstnr )
                .setNeutralButton( R.string.button_default_robot, lstnr )
                .setNegativeButton( R.string.button_default_both, lstnr )
                .create();
            break;

        case DICT_OR_DECLINE:
            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        Intent intent = getIntent();
                        int lang = intent.getIntExtra( MultiService.LANG, -1 );
                        String name = intent.getStringExtra( MultiService.DICT );
                        m_launchedForMissing = true;
                        DictImportActivity
                            .downloadDictInBack( DictsActivity.this, lang, 
                                                 name, DictsActivity.this );
                    }
                };
            lstnr2 = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        finish();
                    }
                };

            dialog = MultiService.missingDictDialog( this, getIntent(), 
                                                     lstnr, lstnr2 );
            break;

        default:
            dialog = super.onCreateDialog( id );
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

        m_closedLangs = new HashSet<String>();
        String[] closed = XWPrefs.getClosedLangs( this );
        if ( null != closed ) {
            for ( String str : closed ) {
                m_closedLangs.add( str );
            }
        }

        Resources res = getResources();
        m_locNames = res.getStringArray( R.array.loc_names );

        m_factory = LayoutInflater.from( this );

        m_download = getString( R.string.download_dicts );
            
        setContentView( R.layout.dict_browse );
        m_expView = getExpandableListView();
        m_expView.setOnItemLongClickListener( this );
        
        Button download = (Button)findViewById( R.id.download );
        download.setOnClickListener( this );

        mkListAdapter();
        m_selDicts = new HashSet<XWListItem>();

        Intent intent = getIntent();
        if ( null != intent ) {
            if ( MultiService.isMissingDictIntent( intent ) ) {
                showDialog( DICT_OR_DECLINE );
            } else {
                boolean downloadNow = intent.getBooleanExtra( DICT_DOLAUNCH, false );
                if ( downloadNow ) {
                    int lang = intent.getIntExtra( DICT_LANG_EXTRA, 0 );
                    String name = intent.getStringExtra( DICT_NAME_EXTRA );
                    startDownload( lang, name );
                }

                downloadNewDict( intent );
            }
        }

        m_origTitle = getTitle();
    } // onCreate

    @Override
    protected void onResume()
    {
        super.onResume();
        MountEventReceiver.register( this );

        mkListAdapter();
        expandGroups();
    }

    @Override
    protected void onStop() {
        MountEventReceiver.unregister( this );
        super.onStop();
    }

    public void onClick( View view ) 
    {
        if ( view instanceof Button ) {
            startDownload( 0, null );
        } else {
            XWListItem item = (XWListItem)view;
            DictBrowseActivity.launch( this, item.getText(), 
                                       (DictLoc)item.getCached() );
        }
    }

    @Override
    public void onBackPressed() {
        if ( 0 == m_selDicts.size() ) {
            super.onBackPressed();
        } else {
            clearSelections();
        }
    }

    @Override
    public boolean onCreateOptionsMenu( Menu menu )
    {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate( R.menu.dicts_menu, menu );

        return true;
    }

    @Override
    public boolean onPrepareOptionsMenu( Menu menu ) 
    {
        int nSel = m_selDicts.size();
        Utils.setItemVisible( menu, R.id.dicts_download, 0 == nSel );
        Utils.setItemVisible( menu, R.id.dicts_select, 1 == nSel );

        boolean allVolatile = selItemsVolatile();
        Utils.setItemVisible( menu, R.id.dicts_move, 
                              allVolatile && DictUtils.haveWriteableSD() );
        Utils.setItemVisible( menu, R.id.dicts_delete, allVolatile );

        return super.onPrepareOptionsMenu( menu );
    }

    public boolean onOptionsItemSelected( MenuItem item )
    {
        boolean handled = true;

        switch ( item.getItemId() ) {
        case R.id.dicts_download:
            startDownload( 0, null );
            break;
        case R.id.dicts_delete:
            deleteSelected();
            break;
        case R.id.dicts_move:
            askMoveSelDicts();
            break;
        case R.id.dicts_select:
            showDialog( SET_DEFAULT );
            break;
        default:
            handled = false;
        }

        return handled || super.onOptionsItemSelected( item );
    }

    private void downloadNewDict( Intent intent )
    {
        int loci = intent.getIntExtra( UpdateCheckReceiver.NEW_DICT_LOC, 0 );
        if ( 0 < loci ) {
            String url = 
                intent.getStringExtra( UpdateCheckReceiver.NEW_DICT_URL );
            DictImportActivity.downloadDictInBack( this, url );
            finish();
        }
    }

    private void setDefault( XWListItem view, int keyId )
    {
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
    private void askMoveSelDicts()
    {
        showDialog( MOVE_DICT );
    }

    // OnItemLongClickListener interface
    public boolean onItemLongClick( AdapterView<?> parent, View view, 
                                    int position, long id ) {
        boolean success = view instanceof SelectableItem.LongClickHandler;
        if ( success ) {
            ((SelectableItem.LongClickHandler)view).longClicked();
        }
        return success;
    }

    private boolean selItemsVolatile() 
    {
        boolean result = 0 < m_selDicts.size();
        for ( Iterator<XWListItem> iter = m_selDicts.iterator(); 
              result && iter.hasNext(); ) {
            DictLoc loc = (DictLoc)iter.next().getCached();
            if ( loc == DictLoc.BUILT_IN ) {
                result = false;
            }
        }
        return result;
    }

    private void deleteSelected()
    {
        XWListItem[] items = getSelItems();
        String msg = getString( R.string.confirm_delete_dictf, 
                                getJoinedNames( items ) );

        // When and what to warn about.  First off, if there's another
        // identical dict, simply confirm.  Or if nobody's using this
        // dict *and* it's not the last of a language that somebody's
        // using, simply confirm.  If somebody is using it, then we
        // want different warnings depending on whether it's the last
        // available dict in its language.

        for ( XWListItem item : items ) {
            String dict = item.getText();
            if ( 1 < DictLangCache.getDictCount( this, dict ) ) {
                // there's another; do nothing
            } else {
                String newMsg = null;
                int langcode = DictLangCache.getDictLangCode( this, dict );
                String langName = DictLangCache.getLangName( this, langcode );
                DictAndLoc[] langDals = DictLangCache.getDALsHaveLang( this, 
                                                                       langcode );
                int nUsingLang = DBUtils.countGamesUsingLang( this, langcode );

                if ( 1 == langDals.length ) { // last in this language?
                    if ( 0 < nUsingLang ) {
                        newMsg = getString( R.string.confirm_deleteonly_dictf,
                                            dict, langName );
                    }
                } else if ( 0 < DBUtils.countGamesUsingDict( this, dict ) ) {
                    newMsg = getString( R.string.confirm_deletemore_dictf,
                                        langName );
                }
                if ( null != newMsg ) {
                    msg += "\n\n" + newMsg;
                }
            }
        }

        showConfirmThen( msg, R.string.button_delete, DELETE_DICT_ACTION,
                         (Object)items );
    } // deleteSelected

    // MountEventReceiver.SDCardNotifiee interface
    public void cardMounted( boolean nowMounted )
    {
        DbgUtils.logf( "DictsActivity.cardMounted(%b)", nowMounted );
        // post so other SDCardNotifiee implementations get a chance
        // to process first: avoid race conditions
        post( new Runnable() {
                public void run() {
                    mkListAdapter();
                    expandGroups();
                }
            } );
    }

    // DlgDelegate.DlgClickNotify interface
    public void dlgButtonClicked( int id, int which, Object[] params )
    {
        switch( id ) {
        case DELETE_DICT_ACTION:
            if ( DialogInterface.BUTTON_POSITIVE == which ) {
                XWListItem[] items = (XWListItem[])params[0];
                for ( XWListItem item : items ) {
                    String name = item.getText();
                    DictLoc loc = (DictLoc)item.getCached();
                    deleteDict( name, loc );
                }
                clearSelections();
            }
            break;
        default:
            Assert.fail();
        }
    }

    private DictLoc itemToRealLoc( int item )
    {
        item += DictLoc.INTERNAL.ordinal();
        return DictLoc.values()[item];
    }

    private void deleteDict( String dict, DictLoc loc )
    {
        DictUtils.deleteDict( this, dict, loc );
        DictLangCache.inval( this, dict, loc, false );
        mkListAdapter();
        expandGroups();
    }

    private void startDownload( int lang, String name )
    {
        Intent intent = mkDownloadIntent( this, lang, name );
        startDownload( intent );
    }

    private void startDownload( Intent downloadIntent )
    {
        try {
            startActivity( downloadIntent );
        } catch ( android.content.ActivityNotFoundException anfe ) {
            Utils.showToast( this, R.string.no_download_warning );
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
            if ( ! m_closedLangs.contains( lang ) ) {
                m_expView.expandGroup( ii );
            }
        }
    }

    private void saveClosed()
    {
        String[] asArray = m_closedLangs.toArray( new String[m_closedLangs.size()] );
        XWPrefs.setClosedLangs( this, asArray );
    }

    private void clearSelections()
    {
        if ( 0 < m_selDicts.size() ) {
            XWListItem[] items = getSelItems();

            m_selDicts.clear();

            for ( XWListItem item : items ) {
                item.setSelected( false );
            }
        }
    }

    private String getJoinedNames( XWListItem[] items )
    {
        String[] names = new String[items.length];
        int ii = 0;
        for ( XWListItem item : items ) {
            names[ii++] = item.getText();
        }
        return TextUtils.join( ", ", names );
    }

    private XWListItem[] getSelItems()
    {
        XWListItem[] items = new XWListItem[m_selDicts.size()];
        int indx = 0;
        for ( Iterator<XWListItem> iter = m_selDicts.iterator(); 
              iter.hasNext(); ) {
            items[indx++] = iter.next();
        }
        return items;
    }

    private void setTitleBar()
    {
        int nSels = m_selDicts.size();
        if ( 0 < nSels ) {
            setTitle( getString( R.string.sel_dictsf, nSels ) );
        } else {
            setTitle( m_origTitle );
        }
    }

    private String[] makeDictDirItems() 
    {
        boolean showDownload = DictUtils.haveDownloadDir( this );
        int nItems = showDownload ? 3 : 2;
        int nextI = 0;
        String[] items = new String[nItems];
        for ( int ii = 0; ii < 3; ++ii ) {
            DictLoc loc = itemToRealLoc(ii);
            if ( !showDownload && DictLoc.DOWNLOAD == loc ) {
                continue;
            }
            items[nextI++] = m_locNames[loc.ordinal()];
        }
        return items;
    }

    private static Intent mkDownloadIntent( Context context, String dict_url )
    {
        Uri uri = Uri.parse( dict_url );
        Intent intent = new Intent( Intent.ACTION_VIEW, uri );
        intent.setFlags( Intent.FLAG_ACTIVITY_NEW_TASK );
        return intent;
    }

    private static Intent mkDownloadIntent( Context context,
                                            int lang, String dict )
    {
        String dict_url = Utils.makeDictUrl( context, lang, dict );
        return mkDownloadIntent( context, dict_url );
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

    public static void launchAndDownload( Activity activity )
    {
        launchAndDownload( activity, 0, null );
    }

    // DictImportActivity.DownloadFinishedListener interface
    public void downloadFinished( String name, final boolean success )
    {
        if ( m_launchedForMissing ) {
            post( new Runnable() {
                    public void run() {
                        if ( success ) {
                            Intent intent = getIntent();
                            if ( MultiService.returnOnDownload( DictsActivity.this,
                                                                intent ) ) {
                                finish();
                            }
                        } else {
                            Utils.showToast( DictsActivity.this, 
                                             R.string.download_failed );
                        }
                    }
                } );
        }
    }

    // SelectableItem interface
    public void itemClicked( SelectableItem.LongClickHandler clicked,
                             GameSummary summary )
    {
        DbgUtils.logf( "itemClicked not implemented" );
    }

    public void itemToggled( SelectableItem.LongClickHandler toggled, 
                             boolean selected )
    {
        XWListItem dictView = (XWListItem)toggled;
        if ( selected ) {
            m_selDicts.add( dictView );
        } else {
            m_selDicts.remove( dictView );
        }
        Utils.invalidateOptionsMenuIf( this );
        setTitleBar();
    }

    public boolean getSelected( SelectableItem.LongClickHandler obj )
    {
        XWListItem dictView = (XWListItem)obj;
        return m_selDicts.contains( dictView );
    }

    private static class SafePopupImpl implements SafePopup {
        public void doPopup( final Context context, View button, 
                             String curDict ) {

            MenuItem.OnMenuItemClickListener listener = 
                new MenuItem.OnMenuItemClickListener() {
                    public boolean onMenuItemClick( MenuItem item )
                    {
                        DictAndLoc dal = s_itemData.get( item );
                        s_itemData = null;

                        if ( null == dal ) {
                            DictsActivity.start( context );
                        } else {
                            DictBrowseActivity.launch( context, dal.name, 
                                                       dal.loc );
                        }
                        return true;
                    }
                };

            s_itemData = new HashMap<MenuItem, DictAndLoc>();
            PopupMenu popup = new PopupMenu( context, button );
            Menu menu = popup.getMenu();
            menu.add( R.string.show_wordlist_browser )
                .setOnMenuItemClickListener( listener );

            // Add at top but save until have dal info
            MenuItem curItem = menu.add( curDict );

            DictAndLoc[] dals = DictUtils.dictList( context );
            for ( DictAndLoc dal : dals ) {
                MenuItem item = dal.name.equals(curDict)
                    ? curItem : menu.add( dal.name );
                item.setOnMenuItemClickListener( listener );
                s_itemData.put( item, dal );
            }
            popup.show();
        }
    }

    public static boolean handleDictsPopup( Context context, View button,
                                            String curDict )
    {
        if ( null == s_safePopup ) {
            int sdkVersion = Integer.valueOf( android.os.Build.VERSION.SDK );
            if ( 11 <= sdkVersion ) {
                s_safePopup = new SafePopupImpl();
            }
        }

        boolean canHandle = null != s_safePopup;
        if ( canHandle ) {
            s_safePopup.doPopup( context, button, curDict );
        }
        return canHandle;
    }

    public static void start( Context context )
    {
        Intent intent = new Intent( context, DictsActivity.class );
        context.startActivity( intent );
    }

}
