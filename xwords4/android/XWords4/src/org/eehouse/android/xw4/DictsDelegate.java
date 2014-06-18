/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
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

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.ListActivity;
import android.content.Context;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.ListView;

import org.apache.http.client.methods.HttpPost;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.Collections;
import java.util.ArrayList;
import java.util.List;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.HashSet;
import java.util.Iterator;

import junit.framework.Assert;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DictUtils.DictAndLoc;
import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.DictUtils.DictLoc;

public class DictsDelegate extends ListDelegateBase
    implements View.OnClickListener, AdapterView.OnItemLongClickListener,
               SelectableItem, MountEventReceiver.SDCardNotifiee, 
               DlgDelegate.DlgClickNotify, GroupStateListener,
               DwnldDelegate.DownloadFinishedListener {

    protected static final String DICT_DOLAUNCH = "do_launch";
    protected static final String DICT_LANG_EXTRA = "use_lang";
    protected static final String DICT_NAME_EXTRA = "use_dict";
    private static final int SEL_LOCAL = 0;
    private static final int SEL_REMOTE = 1;

    private ListActivity m_activity;
    private Set<String> m_closedLangs;
    private DictListAdapter m_adapter;

    private String[] m_langs;
    private ListView m_listView;
    private CheckBox m_checkbox;
    private String[] m_locNames;
    private Map<String, XWListItem> m_selDicts;
    private String m_origTitle;
    private boolean m_showRemote = false;
    private Map<String, String> m_needUpdates;
    private HashMap<String, XWListItem> m_curDownloads;
    private String m_onServerStr;

    private static class DictInfo implements Comparable {
        public String m_name;
        // public boolean m_needsUpdate;
        public String m_lang;
        public int m_nWords, m_nBytes;
        public String m_note;
        public DictInfo( String name, String lang, int nWords, int nBytes ) { 
            m_name = name;
            m_lang = lang;
            m_nWords = nWords;
            m_nBytes = nBytes;
            m_note = "This is the note";
        }
        public int compareTo( Object obj ) {
            DictInfo other = (DictInfo)obj;
            return m_name.compareTo( other.m_name );
        }
    }
    private HashMap<String, DictAndLoc[]> m_localInfo;
    private HashMap<String, DictInfo[]> m_remoteInfo;

    private boolean m_launchedForMissing = false;

    private class DictListAdapter extends XWListAdapter {
        private Context m_context;
        private Object[] m_listInfo;

        public DictListAdapter( Context context ) 
        {
            super( 0 );
            m_context = context;
        }
        
        @Override
        public int getCount() 
        {
            if ( null == m_listInfo ) {
                ArrayList<Object> alist = new ArrayList<Object>();
                int nLangs = m_langs.length;
                for ( int ii = 0; ii < nLangs; ++ii ) {
                    alist.add( new Integer(ii) );

                    String langName = m_langs[ii];
                    if ( m_closedLangs.contains( langName ) ) {
                        continue;
                    }

                    ArrayList<Object> items = makeLangItems( langName );
                    alist.addAll( items );
                }
                m_listInfo = alist.toArray( new Object[alist.size()] );
            }
            return m_listInfo.length;
        } // getCount

        @Override
        public int getViewTypeCount() { return 2; }

        @Override
        public View getView( final int position, View convertView, ViewGroup parent )
        {
            View result = null;

            Object obj = m_listInfo[position];
            if ( obj instanceof Integer ) {
                int groupPos = (Integer)obj;
                String langName = m_langs[groupPos];
                int langCode = DictLangCache.getLangLangCode( m_context,
                                                              langName );
                boolean expanded = ! m_closedLangs.contains( langName );
                result = ListGroup.make( m_context, DictsDelegate.this, groupPos,
                                         langName, expanded );
            } else if ( obj instanceof DictAndLoc ) {
                DictAndLoc dal = (DictAndLoc)obj;
                XWListItem item = 
                    XWListItem.inflate( m_activity, DictsDelegate.this );
                result = item;

                String name = dal.name;
                item.setText( name );

                DictLoc loc = dal.loc;
                item.setComment( m_locNames[loc.ordinal()] );
                item.cache( loc );

                item.setOnClickListener( DictsDelegate.this );

                // Replace sel entry if present
                if ( m_selDicts.containsKey( name ) ) {
                    m_selDicts.put( name, item );
                    item.setSelected( true );
                }
            } else if ( obj instanceof DictInfo ) {
                DictInfo info = (DictInfo)obj;
                XWListItem item = 
                    XWListItem.inflate( m_activity, DictsDelegate.this );
                result = item;

                String name = info.m_name;
                item.setText( name );

                item.setOnClickListener( DictsDelegate.this );
                item.setComment( m_onServerStr );

                item.cache( info );

                if ( m_selDicts.containsKey( name ) ) {
                    m_selDicts.put( name, item );
                    item.setSelected( true );
                }
            } else {
                Assert.fail();
            } 
            return result;
        }

        protected void removeLangItems( String langName )
        {
            ArrayList<Object> asList = new ArrayList<Object>();
            asList.addAll( Arrays.asList( m_listInfo ) );

            int indx = findLangItem( langName ) + 1;
            while ( indx < asList.size() && ! (asList.get(indx) instanceof Integer) ) {
                asList.remove( indx );
            }

            m_listInfo = asList.toArray( new Object[asList.size()] );
        }

        protected void addLangItems( String langName )
        {
            ArrayList<Object> asList = new ArrayList<Object>();
            asList.addAll( Arrays.asList( m_listInfo ) );

            ArrayList<Object> items = makeLangItems( langName );
            int indx = findLangItem( langName );
            asList.addAll( 1 + indx, items );

            m_listInfo = asList.toArray( new Object[asList.size()] );
        }

        private ArrayList<Object> makeLangItems( String langName )
        {
            ArrayList<Object> result = new ArrayList<Object>();

            HashSet<String> locals = new HashSet<String>();
            int lang = DictLangCache.getLangLangCode( m_context, langName );
            DictAndLoc[] dals = DictLangCache.getDALsHaveLang( m_context, lang );
            if ( null != dals ) {
                for ( DictAndLoc dal : dals ) {
                    locals.add( dal.name );
                }
            }

            if ( m_showRemote ) {
                DictInfo[] infos = m_remoteInfo.get( langName );
                if ( null != infos ) {
                    for ( DictInfo info : infos ) {
                        if ( ! locals.contains( info.m_name ) ) {
                            result.add( info );
                        }
                    }
                } else {
                    DbgUtils.logf( "No remote info for lang %s", langName );
                }
            }

            // Now append locals
            if ( null != dals ) {
                result.addAll( Arrays.asList( dals ) );
            }

            return result;
        }

        private int findLangItem( String langName )
        {
            int result = -1;
            int nLangs = m_langs.length;
            for ( int ii = 0; ii < m_listInfo.length; ++ii ) {
                Object obj = m_listInfo[ii];
                if ( obj instanceof Integer ) {
                    if ( m_langs[(Integer)obj].equals( langName ) ) {
                        result = ii;
                        break;
                    }
                }
            }
            Assert.assertTrue( -1 != result );
            DbgUtils.logf( "findLangItem(%s) => %d", langName, result );
            return result;
        }
    }

    protected DictsDelegate( ListActivity activity, Bundle savedInstanceState )
    {
        super( activity, savedInstanceState, R.menu.dicts_menu );
        m_activity = activity;
    }

    protected Dialog onCreateDialog( int id )
    {
        OnClickListener lstnr, lstnr2;
        Dialog dialog;
        String message;
        boolean doRemove = true;

        DlgID dlgID = DlgID.values()[id];
        switch( dlgID ) {
        case MOVE_DICT:
            final XWListItem[] selItems = getSelItems();
            final int[] moveTo = { -1 };
            message = getString( R.string.move_dict_fmt,
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
                            } else if ( DictUtils.moveDict( m_activity,
                                                            name, fromLoc, 
                                                            toLoc ) ) {
                                selItem.setComment( m_locNames[toLoc.ordinal()] );
                                selItem.cache( toLoc );
                                selItem.invalidate();
                                DBUtils.dictsMoveInfo( m_activity, name, 
                                                       fromLoc, toLoc );
                            } else {
                                DbgUtils.logf( "moveDict(%s) failed", name );
                            }
                        }
                    }
                };

            dialog = new AlertDialog.Builder( m_activity )
                .setTitle( message )
                .setSingleChoiceItems( makeDictDirItems(), moveTo[0],
                                       newSelLstnr )
                .setPositiveButton( R.string.button_move, lstnr )
                .setNegativeButton( R.string.button_cancel, null )
                .create();
            break;

        case SET_DEFAULT:
            final XWListItem row = m_selDicts.values().iterator().next();
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
            String lang = DictLangCache.getLangName( m_activity, name);
            message = getString( R.string.set_default_message_fmt, name, lang );
            dialog = makeAlertBuilder()
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
                        DwnldDelegate
                            .downloadDictInBack( m_activity, lang, 
                                                 name, DictsDelegate.this );
                    }
                };
            lstnr2 = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        finish();
                    }
                };

            dialog = MultiService.missingDictDialog( m_activity, getIntent(), 
                                                     lstnr, lstnr2 );
            break;

        default:
            dialog = super.onCreateDialog( id );
            doRemove = false;
            break;
        }

        if ( doRemove && null != dialog ) {
            setRemoveOnDismiss( dialog, dlgID );
        }

        return dialog;
    } // onCreateDialog

    @Override
    protected void prepareDialog( DlgID dlgID, Dialog dialog )
    {
        if ( DlgID.MOVE_DICT == dlgID ) {
            // The move button should always start out disabled
            // because the selected location should be where it
            // currently is.
            ((AlertDialog)dialog).getButton( AlertDialog.BUTTON_POSITIVE )
                .setEnabled( false );
        }
    }

    protected void init( Bundle savedInstanceState ) 
    {
        m_onServerStr = getString( R.string.dict_on_server );
        m_closedLangs = new HashSet<String>();
        String[] closed = XWPrefs.getClosedLangs( m_activity );
        if ( null != closed ) {
            m_closedLangs.addAll( Arrays.asList( closed ) );
        }

        m_locNames = getStringArray( R.array.loc_names );

        setContentView( R.layout.dict_browse );
        m_listView = getListView();
        m_listView.setOnItemLongClickListener( this );
        
        m_checkbox = (CheckBox)findViewById( R.id.show_remote );
        m_checkbox.setOnClickListener( this );

        mkListAdapter();

        Intent intent = getIntent();
        if ( null != intent ) {
            if ( MultiService.isMissingDictIntent( intent ) ) {
                showDialog( DlgID.DICT_OR_DECLINE );
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
    } // init

    @Override
    protected void onResume()
    {
        super.onResume();

        MountEventReceiver.register( this );

        mkListAdapter();
        setTitleBar();
    }

    protected void onStop() 
    {
        MountEventReceiver.unregister( this );
    }

    public void onClick( View view ) 
    {
        if ( view == m_checkbox ) {
            switchShowingRemote( m_checkbox.isChecked() );
        } else {
            XWListItem item = (XWListItem)view;
            Object obj = item.getCached();
            if ( obj instanceof DictLoc ) {
                DictBrowseDelegate.launch( m_activity, item.getText(), 
                                           (DictLoc)obj );
            } else {
                DictInfo info = (DictInfo)obj;
                int kBytes = (info.m_nBytes + 999) / 1000;
                String msg = getString( R.string.dict_info_fmt, info.m_name, 
                                        info.m_nWords, kBytes, info.m_note );
                int langCode = DictLangCache.getLangLangCode( m_activity, info.m_lang );
                showConfirmThen( msg, R.string.button_download, 
                                 Action.DOWNLOAD_DICT_ACTION, 
                                 langCode, info.m_name );
            }
        }
    }

    protected boolean onBackPressed() 
    {
        boolean handled = 0 < m_selDicts.size();
        if ( handled ) {
            clearSelections();
        }
        return handled;
    }

    @Override
    public boolean onPrepareOptionsMenu( Menu menu ) 
    {
        // int nSel = m_selDicts.size();
        int[] nSels = countSelDicts();
        Utils.setItemVisible( menu, R.id.dicts_select, 
                              1 == nSels[SEL_LOCAL] && 0 == nSels[SEL_REMOTE] );

        // NO -- test if any downloadable selected
        Utils.setItemVisible( menu, R.id.dicts_download, 
                              0 == nSels[SEL_LOCAL] && 0 < nSels[SEL_REMOTE] );

        Utils.setItemVisible( menu, R.id.dicts_deselect_all, 
                              0 < nSels[SEL_LOCAL] || 0 < nSels[SEL_REMOTE] );

        boolean allVolatile = 0 == nSels[SEL_REMOTE] && selItemsVolatile();
        Utils.setItemVisible( menu, R.id.dicts_move, 
                              allVolatile && DictUtils.haveWriteableSD() );
        Utils.setItemVisible( menu, R.id.dicts_delete, allVolatile );

        return true;
    }

    @Override
    public boolean onOptionsItemSelected( MenuItem item )
    {
        boolean handled = true;

        switch ( item.getItemId() ) {
        case R.id.dicts_delete:
            deleteSelected();
            break;
        case R.id.dicts_move:
            showDialog( DlgID.MOVE_DICT );
            break;
        case R.id.dicts_select:
            showDialog( DlgID.SET_DEFAULT );
            break;
        case R.id.dicts_deselect_all:
            clearSelections();
            break;
        case R.id.dicts_download:
            String[] urls = new String[countNeedDownload()];
            int count = 0;
            m_curDownloads = new HashMap<String, XWListItem>();
            for ( Iterator<XWListItem> iter = m_selDicts.values().iterator(); 
                  iter.hasNext(); ) {
                XWListItem litm = iter.next();
                Object cached = litm.getCached();
                if ( cached instanceof DictInfo ) {
                    DictInfo info = (DictInfo)cached;
                    String url = Utils.makeDictUrl( m_activity, info.m_lang, 
                                                    litm.getText() );
                    urls[count++] = url;
                    m_curDownloads.put( url, litm );
                }
            }
            DwnldDelegate.downloadDictsInBack( m_activity, urls, this );
            break;
        default:
            handled = false;
        }

        return handled;
    }

    private void switchShowingRemote( boolean showRemote )
    {
        // if showing for the first time, download remote info and let the
        // completion routine finish (or clear the checkbox if cancelled.)
        // Otherwise just toggle boolean and redraw.
        if ( m_showRemote != showRemote ) {
            m_showRemote = showRemote;
            if ( showRemote && null == m_remoteInfo ) {
                new FetchListTask( m_activity ).execute();
            } else {
                mkListAdapter();
            }
        }
    }

    private int countNeedDownload()
    {
        int result = 0;
        for ( Iterator<XWListItem> iter = m_selDicts.values().iterator(); 
              iter.hasNext(); ) {
            XWListItem litm = iter.next();
            Object obj = litm.getCached();
            if ( obj instanceof DictInfo ) {
                ++result;
            }
        }
        return result;
    }

    private void downloadNewDict( Intent intent )
    {
        int loci = intent.getIntExtra( UpdateCheckReceiver.NEW_DICT_LOC, 0 );
        if ( 0 < loci ) {
            String url = 
                intent.getStringExtra( UpdateCheckReceiver.NEW_DICT_URL );
            DwnldDelegate.downloadDictInBack( m_activity, url, null );
            finish();
        }
    }

    private void setDefault( XWListItem view, int keyId )
    {
        SharedPreferences sp
            = PreferenceManager.getDefaultSharedPreferences( m_activity );
        SharedPreferences.Editor editor = sp.edit();
        String key = getString( keyId );
        String name = view.getText();
        editor.putString( key, name );
        editor.commit();
    }

    //////////////////////////////////////////////////////////////////////
    // GroupStateListener interface
    //////////////////////////////////////////////////////////////////////
    public void onGroupExpandedChanged( int groupPosition, boolean expanded )
    {
        String langName = m_langs[groupPosition];
        if ( expanded ) {
            m_closedLangs.remove( langName );
            m_adapter.addLangItems( langName );
        } else {
            m_closedLangs.add( langName );
            m_adapter.removeLangItems( langName );
        }
        saveClosed();
        // mkListAdapter();
        m_adapter.notifyDataSetChanged();
    }
    
    //////////////////////////////////////////////////////////////////////
    // OnItemLongClickListener interface
    //////////////////////////////////////////////////////////////////////
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
        for ( Iterator<XWListItem> iter = m_selDicts.values().iterator(); 
              result && iter.hasNext(); ) {
            Object obj = iter.next().getCached();
            if ( obj instanceof DictLoc ) {
                DictLoc loc = (DictLoc)obj;
                if ( loc == DictLoc.BUILT_IN ) {
                    result = false;
                }
            } else {
                result = false;
            }
        }
        return result;
    }

    private void deleteSelected()
    {
        XWListItem[] items = getSelItems();
        String msg = getString( R.string.confirm_delete_dict_fmt, 
                                getJoinedNames( items ) );

        // When and what to warn about.  First off, if there's another
        // identical dict, simply confirm.  Or if nobody's using this
        // dict *and* it's not the last of a language that somebody's
        // using, simply confirm.  If somebody is using it, then we
        // want different warnings depending on whether it's the last
        // available dict in its language.

        for ( XWListItem item : items ) {
            String dict = item.getText();
            if ( 1 < DictLangCache.getDictCount( m_activity, dict ) ) {
                // there's another; do nothing
            } else {
                String newMsg = null;
                int langcode = DictLangCache.getDictLangCode( m_activity, dict );
                String langName = DictLangCache.getLangName( m_activity, langcode );
                DictAndLoc[] langDals = DictLangCache.getDALsHaveLang( m_activity, 
                                                                       langcode );
                int nUsingLang = DBUtils.countGamesUsingLang( m_activity, langcode );

                if ( 1 == langDals.length ) { // last in this language?
                    if ( 0 < nUsingLang ) {
                        newMsg = getString( R.string.confirm_deleteonly_dict_fmt,
                                            dict, langName );
                    }
                } else if ( 0 < DBUtils.countGamesUsingDict( m_activity, dict ) ) {
                    newMsg = getString( R.string.confirm_deletemore_dict_fmt,
                                        langName );
                }
                if ( null != newMsg ) {
                    msg += "\n\n" + newMsg;
                }
            }
        }

        showConfirmThen( msg, R.string.button_delete, Action.DELETE_DICT_ACTION,
                         (Object)items );
    } // deleteSelected

    //////////////////////////////////////////////////////////////////////
    // MountEventReceiver.SDCardNotifiee interface
    //////////////////////////////////////////////////////////////////////
    public void cardMounted( boolean nowMounted )
    {
        DbgUtils.logf( "DictsActivity.cardMounted(%b)", nowMounted );
        // post so other SDCardNotifiee implementations get a chance
        // to process first: avoid race conditions
        post( new Runnable() {
                public void run() {
                    mkListAdapter();
                }
            } );
    }

    //////////////////////////////////////////////////////////////////////
    // DlgDelegate.DlgClickNotify interface
    //////////////////////////////////////////////////////////////////////
    public void dlgButtonClicked( Action action, int which, Object[] params )
    {
        switch( action ) {
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
        case DOWNLOAD_DICT_ACTION:
            if ( DialogInterface.BUTTON_POSITIVE == which ) {
                int lang = (Integer)params[0];
                String name = (String)params[1];
                DwnldDelegate.downloadDictInBack( m_activity, lang, name, this );
            }
            break;
        case UPDATE_DICTS_ACTION:
            if ( DialogInterface.BUTTON_POSITIVE == which ) {
                String[] urls = m_needUpdates.values().
                    toArray( new String[m_needUpdates.size()] );
                DwnldDelegate.downloadDictsInBack( m_activity, urls, this );
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
        DictUtils.deleteDict( m_activity, dict, loc );
        DictLangCache.inval( m_activity, dict, loc, false );
        mkListAdapter();
    }

    private void startDownload( int lang, String name )
    {
        DwnldDelegate.downloadDictInBack( m_activity, lang, name, this );
        // Intent intent = mkDownloadIntent( m_activity, lang, name );
        // startActivity( intent );
    }

    private void mkListAdapter()
    {
        Set<String> langs = new HashSet<String>();
        langs.addAll( Arrays.asList(DictLangCache.listLangs( m_activity )) );
        if ( m_showRemote ) {
            langs.addAll( m_remoteInfo.keySet() );
        }
        m_langs = langs.toArray( new String[langs.size()] );
        Arrays.sort( m_langs );

        m_adapter = new DictListAdapter( m_activity );
        setListAdapterKeepScroll( m_adapter );

        m_selDicts = new HashMap<String, XWListItem>();
    }

    private void saveClosed()
    {
        String[] asArray = m_closedLangs.toArray( new String[m_closedLangs.size()] );
        XWPrefs.setClosedLangs( m_activity, asArray );
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
        for ( Iterator<XWListItem> iter = m_selDicts.values().iterator(); 
              iter.hasNext(); ) {
            items[indx++] = iter.next();
        }
        return items;
    }


    private int[] countSelDicts()
    {
        int[] results = new int[2];
        Assert.assertTrue( 0 == results[0] && 0 == results[1] );
        for ( Iterator<XWListItem> iter = m_selDicts.values().iterator(); 
              iter.hasNext(); ) {
            Object obj = iter.next().getCached();
            if ( obj instanceof DictLoc ) {
                ++results[SEL_LOCAL];
            } else if ( obj instanceof DictInfo ) {
                ++results[SEL_REMOTE];
            } else {
                Assert.fail();
            }
        }
        DbgUtils.logf( "countSelDicts() => {loc: %d; remote: %d}",
                       results[SEL_LOCAL], results[SEL_REMOTE] );
        return results;
    }

    private void setTitleBar()
    {
        int nSels = m_selDicts.size();
        if ( 0 < nSels ) {
            setTitle( getString( R.string.sel_items_fmt, nSels ) );
        } else {
            setTitle( m_origTitle );
        }
    }

    private String[] makeDictDirItems() 
    {
        boolean showDownload = DictUtils.haveDownloadDir( m_activity );
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

    // private static Intent mkDownloadIntent( Context context, String dict_url )
    // {
        // Uri uri = Uri.parse( dict_url );
        // Intent intent = new Intent( Intent.ACTION_VIEW, uri );
        // intent.setFlags( Intent.FLAG_ACTIVITY_NEW_TASK );
        // return intent;
    // }

    private static Intent mkDownloadIntent( Context context,
                                            int lang, String dict )
    {
        Assert.fail();
        return null;
        // String dict_url = Utils.makeDictUrl( context, lang, dict );
        // return mkDownloadIntent( context, dict_url );
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

    // DwnldActivity.DownloadFinishedListener interface
    public void downloadFinished( String name, final boolean success )
    {
        if ( m_launchedForMissing ) {
            post( new Runnable() {
                    public void run() {
                        if ( success ) {
                            Intent intent = getIntent();
                            if ( MultiService.returnOnDownload( m_activity,
                                                                intent ) ) {
                                finish();
                            }
                        } else {
                            showToast( R.string.download_failed );
                        }
                    }
                } );
        }
    }

    //////////////////////////////////////////////////////////////////////
    // SelectableItem interface
    //////////////////////////////////////////////////////////////////////
    public void itemClicked( SelectableItem.LongClickHandler clicked,
                             GameSummary summary )
    {
        DbgUtils.logf( "itemClicked not implemented" );
    }

    public void itemToggled( SelectableItem.LongClickHandler toggled, 
                             boolean selected )
    {
        XWListItem dictView = (XWListItem)toggled;
        String lang = dictView.getText();
        if ( selected ) {
            m_selDicts.put( lang, dictView );
        } else {
            m_selDicts.remove( lang );
        }
        invalidateOptionsMenuIf();
        setTitleBar();
    }

    public boolean getSelected( SelectableItem.LongClickHandler obj )
    {
        XWListItem dictView = (XWListItem)obj;
        boolean result = m_selDicts.containsKey( dictView.getText() );
        return result;
    }

    private boolean digestData( String jsonData )
    {
        boolean success = false;
        JSONArray langs = null;
        m_needUpdates = new HashMap<String, String>();
        if ( null != jsonData ) {
            Set<String> closedLangs = new HashSet<String>();
            final Set<String> curLangs =
                new HashSet<String>( Arrays.asList( m_langs ) );

            // DictLangCache hits the DB hundreds of times below. Fix!
            DbgUtils.logf( "Fix me I'm stupid" );
            try {
                // DbgUtils.logf( "data: %s", jsonData );
                JSONObject obj = new JSONObject( jsonData );
                langs = obj.optJSONArray( "langs" );

                int nLangs = langs.length();
                m_remoteInfo = new HashMap<String, DictInfo[]>();
                for ( int ii = 0; ii < nLangs; ++ii ) {
                    JSONObject langObj = langs.getJSONObject( ii );
                    String langName = langObj.getString( "lang" );

                    if ( ! curLangs.contains( langName ) ) {
                        closedLangs.add( langName );
                    }

                    JSONArray dicts = langObj.getJSONArray( "dicts" );
                    int nDicts = dicts.length();
                    ArrayList<DictInfo> dictNames = new ArrayList<DictInfo>();
                    for ( int jj = 0; jj < nDicts; ++jj ) {
                        JSONObject dict = dicts.getJSONObject( jj );
                        String name = dict.getString( "xwd" );
                        name = DictUtils.removeDictExtn( name );
                        int nBytes = dict.optInt( "nBytes", -1 );
                        int nWords = dict.optInt( "nWords", -1 );
                        DictInfo info = new DictInfo( name, langName, nWords, nBytes );
                        if ( DictLangCache.haveDict( m_activity, langName, name ) ) {
                            boolean matches = true;
                            String curSum = DictLangCache.getDictMD5Sum( m_activity, name );
                            if ( null != curSum ) {
                                JSONArray sums = dict.getJSONArray("md5sums");
                                if ( null != sums ) {
                                    matches = false;
                                    for ( int kk = 0; !matches && kk < sums.length(); ++kk ) {
                                        String sum = sums.getString( kk );
                                        matches = sum.equals( curSum );
                                    }
                                }
                            }
                            if ( !matches ) {
                                DbgUtils.logf( "adding %s to set needing update", name );
                                String url = Utils.makeDictUrl( m_activity, langName, name );
                                m_needUpdates.put( name, url );
                            }
                        }
                        dictNames.add( info );
                    }
                    if ( 0 < dictNames.size() ) {
                        DictInfo[] asArray = dictNames.toArray( new DictInfo[dictNames.size()] );
                        Arrays.sort( asArray );
                        m_remoteInfo.put( langName, asArray );
                    }
                }

                m_closedLangs.addAll( closedLangs );

                success = true;
            } catch ( JSONException ex ) {
                DbgUtils.loge( ex );
            }
        }

        return success;
    }

    private class FetchListTask extends AsyncTask<Void, Void, Boolean> {
        private Context m_context;

        public FetchListTask( Context context )
        {
            m_context = context;
            startProgress( R.string.remote_empty );
        }

        @Override 
        public Boolean doInBackground( Void... unused )
        {
            boolean success = false;
            HttpPost post = UpdateCheckReceiver.makePost( m_context, "listDicts" );
            if ( null != post ) {
                String json = UpdateCheckReceiver.runPost( post, new JSONObject() );
                if ( null != json ) {
                    post( new Runnable() {
                            public void run() {
                                setProgressMsg( R.string.remote_digesting );
                            }
                        } );
                }
                success = digestData( json );
            }
            return new Boolean( success );
        }
            
        @Override 
        protected void onPostExecute( Boolean success )
        {
            if ( success ) {
                mkListAdapter();

                if ( 0 < m_needUpdates.size() ) {
                    String[] names = m_needUpdates.keySet()
                        .toArray(new String[m_needUpdates.size()]);
                    String joined = TextUtils.join( ", ", names );
                    showConfirmThen( getString( R.string.update_dicts_fmt,
                                                joined ),
                                     R.string.button_download, 
                                     Action.UPDATE_DICTS_ACTION );
                }
            } else {
                showOKOnlyDialog( R.string.remote_no_net );
                m_checkbox.setChecked( false );
            }
            stopProgress();
        }
    }
}
