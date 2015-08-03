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
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
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
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;

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

import org.eehouse.android.xw4.DictUtils.DictAndLoc;
import org.eehouse.android.xw4.DictUtils.DictLoc;
import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.loc.LocUtils;
import org.eehouse.android.xw4.DwnldDelegate.DownloadFinishedListener;
import org.eehouse.android.xw4.DwnldDelegate.OnGotLcDictListener;

public class DictsDelegate extends ListDelegateBase
    implements View.OnClickListener, AdapterView.OnItemLongClickListener,
               SelectableItem, MountEventReceiver.SDCardNotifiee, 
               DlgDelegate.DlgClickNotify, GroupStateListener,
               DownloadFinishedListener, XWListItem.ExpandedListener {

    protected static final String DICT_SHOWREMOTE = "do_launch";
    protected static final String DICT_LANG_EXTRA = "use_lang";
    protected static final String DICT_NAME_EXTRA = "use_dict";
    protected static final String RESULT_LAST_LANG = "last_lang";
    protected static final String RESULT_LAST_DICT = "last_dict";

    private static final int SEL_LOCAL = 0;
    private static final int SEL_REMOTE = 1;

    private Activity m_activity;
    private Set<String> m_closedLangs;
    private Set<DictInfo> m_expandedItems;
    private DictListAdapter m_adapter;

    private boolean m_quickFetchMode;
    private String[] m_langs;
    private ListView m_listView;
    private CheckBox m_checkbox;
    private String[] m_locNames;
    private String m_finishOnName;
    private Map<String, XWListItem> m_selDicts;
    private String m_origTitle;
    private boolean m_showRemote = false;
    private String m_filterLang;
    private Map<String, Uri> m_needUpdates;
    private String m_onServerStr;
    private String m_lastLang;
    private String m_lastDict;
    private String m_noteNone;

    private static class DictInfo implements Comparable {
        public String m_name;
        public String m_lang;
        public int m_nWords;
        public long m_nBytes;
        public String m_note;
        public DictInfo( String name, String lang, int nWords, 
                         long nBytes, String note )
        {
            m_name = name;
            m_lang = lang;
            m_nWords = nWords;
            m_nBytes = nBytes;
            m_note = note;
        }
        public int compareTo( Object obj ) {
            DictInfo other = (DictInfo)obj;
            return m_name.compareTo( other.m_name );
        }
    }
    private static class LangInfo {
        int m_numDicts;
        int m_posn;
        public LangInfo( int posn, int numDicts ) 
        {
            m_posn = posn;
            m_numDicts = numDicts;
        }
    }
    private HashMap<String, DictInfo[]> m_remoteInfo;

    private boolean m_launchedForMissing = false;

    private class DictListAdapter extends XWExpListAdapter {
        private Context m_context;

        public DictListAdapter( Context context ) 
        {
            super( new Class[] { LangInfo.class,
                                 DictAndLoc.class,
                                 DictInfo.class
                } );
            m_context = context;
        }

        @Override
        public Object[] makeListData()
        {
            ArrayList<Object> alist = new ArrayList<Object>();
            int nLangs = m_langs.length;
            for ( int ii = 0; ii < nLangs; ++ii ) {
                String langName = m_langs[ii];
                if ( null != m_filterLang && 
                     ! m_filterLang.equals(langName) ) {
                    continue;
                }

                ArrayList<Object> items = makeLangItems( langName );

                alist.add( new LangInfo( ii, items.size() ) );
                if ( ! m_closedLangs.contains( langName ) ) {
                    alist.addAll( items );
                }
            }
            return alist.toArray( new Object[alist.size()] );
        } // makeListData

        @Override
        public View getView( Object dataObj, View convertView )
        {
            View result = null;

            if ( dataObj instanceof LangInfo ) {
                LangInfo info = (LangInfo)dataObj;
                int groupPos = info.m_posn;
                String langName = m_langs[groupPos];
                int langCode = DictLangCache.getLangLangCode( m_context,
                                                              langName );
                boolean expanded = ! m_closedLangs.contains( langName );
                String locLangName = xlateLang( langName );
                String name = getQuantityString( R.plurals.lang_name_fmt, 
                                                 info.m_numDicts,
                                                 locLangName, info.m_numDicts );
                name = Utils.capitalize( name );
                result = ListGroup.make( m_context, convertView, 
                                         DictsDelegate.this, groupPos, name, 
                                         expanded );
            } else {
                XWListItem item;
                if ( null != convertView && convertView instanceof XWListItem ) {
                    item = (XWListItem)convertView;
                } else {
                    item = XWListItem.inflate( m_activity, DictsDelegate.this );
                }
                result = item;

                if ( dataObj instanceof DictAndLoc ) {
                    DictAndLoc dal = (DictAndLoc)dataObj;

                    String name = dal.name;
                    item.setText( name );

                    DictLoc loc = dal.loc;
                    item.setComment( m_locNames[loc.ordinal()] );
                    item.setCached( loc );

                    item.setOnClickListener( DictsDelegate.this );
                    item.setExpandedListener( null ); // item might be reused

                    // Replace sel entry if present
                    if ( m_selDicts.containsKey( name ) ) {
                        m_selDicts.put( name, item );
                        item.setSelected( true );
                    }
                } else if ( dataObj instanceof DictInfo ) {
                    DictInfo info = (DictInfo)dataObj;
                    String name = info.m_name;
                    item.setText( name );
                    item.setCached( info );

                    item.setExpandedListener( DictsDelegate.this );
                    item.setExpanded( m_expandedItems.contains( info ) );
                    item.setComment( m_onServerStr );

                    if ( m_selDicts.containsKey( name ) ) {
                        m_selDicts.put( name, item );
                        item.setSelected( true );
                    }
                } else {
                    Assert.fail();
                }
            } 
            return result;
        }

        private XWExpListAdapter.GroupTest makeTestFor( final String langName )
        {
            return new XWExpListAdapter.GroupTest() {
                public boolean isTheGroup( Object item ) {
                    LangInfo info = (LangInfo)item;
                    return m_langs[info.m_posn].equals( langName );
                }
            };
        }

        protected void removeLangItems( String langName )
        {
            int indx = findGroupItem( makeTestFor( langName ) );
            removeChildrenOf( indx );
        }

        protected void addLangItems( String langName )
        {
            int indx = findGroupItem( makeTestFor( langName ) );
            addChildrenOf( indx, makeLangItems( langName ) );
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

            if ( m_showRemote && null != m_remoteInfo ) {
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
    }

    protected DictsDelegate( ListDelegator delegator, Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState, R.layout.dict_browse, 
               R.menu.dicts_menu );
        m_activity = delegator.getActivity();
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
                                selItem.setCached( toLoc );
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
                .setNegativeButton( android.R.string.cancel, null )
                .create();
            break;

        case SET_DEFAULT:
            final XWListItem row = m_selDicts.values().iterator().next();
            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        if ( DialogInterface.BUTTON_NEGATIVE == item
                             || DialogInterface.BUTTON_POSITIVE == item ) {
                            setDefault( row, R.string.key_default_dict, 
                                        R.string.key_default_robodict );
                        }
                        if ( DialogInterface.BUTTON_NEGATIVE == item 
                             || DialogInterface.BUTTON_NEUTRAL == item ) {
                            setDefault( row, R.string.key_default_robodict, 
                                        R.string.key_default_dict );
                        }
                    }
                };
            String name = row.getText();
            String lang = DictLangCache.getLangName( m_activity, name);
            lang = xlateLang( lang );
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

        m_expandedItems = new HashSet<DictInfo>();

        m_locNames = getStringArray( R.array.loc_names );
        m_noteNone = getString( R.string.note_none );

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
                boolean showRemote = intent.getBooleanExtra( DICT_SHOWREMOTE, 
                                                             false );
                if ( showRemote ) {
                    m_quickFetchMode = true;
                    m_showRemote = true;
                    m_checkbox.setVisibility( View.GONE );

                    int lang = intent.getIntExtra( DICT_LANG_EXTRA, 0 );
                    if ( 0 < lang ) {
                        m_filterLang = DictLangCache.getLangNames( m_activity )[lang];
                        m_closedLangs.remove( m_filterLang );
                    }
                    String name = intent.getStringExtra( DICT_NAME_EXTRA );
                    if ( null == name ) {
                        new FetchListTask( m_activity ).execute();
                    } else {
                        m_finishOnName = name;
                        startDownload( lang, name );
                    }
                }

                downloadNewDict( intent );
            }
        }

        m_origTitle = getTitle();

        showNotAgainDlg( R.string.not_again_dicts, R.string.key_na_dicts );
    } // init

    @Override
    protected void onResume()
    {
        super.onResume();

        MountEventReceiver.register( this );

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
            DictBrowseDelegate.launch( m_activity, item.getText(), 
                                       (DictLoc)item.getCached() );
        }
    }

    protected boolean onBackPressed() 
    {
        boolean handled = 0 < m_selDicts.size();
        if ( handled ) {
            clearSelections();
        } else {
            Intent intent = new Intent();
            if ( null != m_lastLang ) {
                intent.putExtra( RESULT_LAST_LANG, m_lastLang );
            }
            if ( null != m_lastDict ) {
                intent.putExtra( RESULT_LAST_DICT, m_lastDict );
            }
            setResult( Activity.RESULT_OK, intent );
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
            Uri[] uris = new Uri[countNeedDownload()];
            String[] names = new String[uris.length];
            int count = 0;
            for ( Iterator<XWListItem> iter = m_selDicts.values().iterator(); 
                  iter.hasNext(); ) {
                XWListItem litm = iter.next();
                Object cached = litm.getCached();
                if ( cached instanceof DictInfo ) {
                    DictInfo info = (DictInfo)cached;
                    String name = litm.getText();
                    Uri uri = Utils.makeDictUri( m_activity, info.m_lang, 
                                                 name );
                    uris[count] = uri;
                    names[count] = name;
                    ++count;
                }
            }
            DwnldDelegate.downloadDictsInBack( m_activity, uris, names, this );
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
            String name = 
                intent.getStringExtra( UpdateCheckReceiver.NEW_DICT_NAME );
            String url = 
                intent.getStringExtra( UpdateCheckReceiver.NEW_DICT_URL );
            Uri uri = Uri.parse( url );
            DwnldDelegate.downloadDictInBack( m_activity, uri, name, null );
            finish();
        }
    }

    private void setDefault( XWListItem view, int keyId, int otherKey )
    {
        String name = view.getText();
        int langCode = DictLangCache.getDictLangCode( m_activity, name );
        String curLangName = XWPrefs.getPrefsString( m_activity, R.string.key_default_language );
        int curLangCode = DictLangCache.getLangLangCode( m_activity, curLangName );
        boolean changeLang = langCode != curLangCode;

        SharedPreferences sp
            = PreferenceManager.getDefaultSharedPreferences( m_activity );
        SharedPreferences.Editor editor = sp.edit();
        String key = getString( keyId );
        editor.putString( key, name );

        if ( changeLang ) {
            // change other dict too
            key = getString( otherKey );
            editor.putString( key, name );

            // and change language
            String langName = DictLangCache.getLangName( m_activity, langCode );
            key = getString( R.string.key_default_language );
            editor.putString( key, langName );
        }

        editor.commit();
    }

    //////////////////////////////////////////////////////////////////////
    // GroupStateListener interface
    //////////////////////////////////////////////////////////////////////
    public void onGroupExpandedChanged( Object groupObj, boolean expanded )
    {
        ListGroup lg = (ListGroup)groupObj;
        String langName = m_langs[lg.getPosition()];
        if ( expanded ) {
            m_closedLangs.remove( langName );
            m_adapter.addLangItems( langName );
        } else {
            m_closedLangs.add( langName );
            m_adapter.removeLangItems( langName );
        }
        saveClosed();
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
        String msg = getQuantityString( R.plurals.confirm_delete_dict_fmt, 
                                        items.length, getJoinedNames( items ) );

        // Confirm.  And for each dict, warn if (after ALL are deleted) any
        // game will no longer be openable without downloading.  For now
        // anyway skip warning for the case where user will have to switch to
        // a different same-lang wordlist to open a game.

        class LangDelData {
            public LangDelData( int langCode ) {
                delDicts = new HashSet<String>();
                langName = DictLangCache.getLangName( m_activity, langCode );
                nDicts = DictLangCache.getDALsHaveLang( m_activity, langCode ).length;
            }
            public String dictsStr()
            {
                if ( null == m_asArray ) {
                    String[] arr = delDicts.toArray(new String[delDicts.size()]); 
                    m_asArray = TextUtils.join( ", ", arr );
                }
                return m_asArray;
            }
            Set<String> delDicts;
            private String m_asArray;
            String langName;
            int nDicts;
        }

        Map<Integer, LangDelData> dels = new HashMap<Integer, LangDelData>();
        Set<Integer> skipLangs = new HashSet<Integer>();
        for ( XWListItem item : items ) {
            String dict = item.getText();
            int langCode = DictLangCache.getDictLangCode( m_activity, dict );
            if ( skipLangs.contains( langCode ) ) {
                continue;
            }
            int nUsingLang = DBUtils.countGamesUsingLang( m_activity, langCode );
            if ( 0 == nUsingLang ) {
                // remember, since countGamesUsingLang is expensive
                skipLangs.add( langCode );
            } else {
                LangDelData data = dels.get( langCode );
                if ( null == data ) {
                    data = new LangDelData( langCode );
                    dels.put( langCode, data );
                }
                data.delDicts.add( dict );
            }
        }

        for ( Iterator<LangDelData> iter = dels.values().iterator(); iter.hasNext(); ) {
            LangDelData data = iter.next();
            int nLeftAfter = data.nDicts - data.delDicts.size();

            if ( 0 == nLeftAfter ) { // last in this language?
                String locName = xlateLang( data.langName );
                String newMsg = getString( R.string.confirm_deleteonly_dicts_fmt,
                                           data.dictsStr(), locName );
                msg += "\n\n" + newMsg;
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
        if ( DialogInterface.BUTTON_POSITIVE == which ) {
            switch( action ) {
            case DELETE_DICT_ACTION:
                XWListItem[] items = (XWListItem[])params[0];
                for ( XWListItem item : items ) {
                    String name = item.getText();
                    DictLoc loc = (DictLoc)item.getCached();
                    deleteDict( name, loc );
                }
                clearSelections();
                mkListAdapter();
                break;
            case UPDATE_DICTS_ACTION:
                Uri[] uris = new Uri[m_needUpdates.size()];
                String[] names = new String[uris.length];
                int count = 0;
                for ( Iterator<String> iter = m_needUpdates.keySet().iterator();
                      iter.hasNext();  ) {
                    String name = iter.next();
                    names[count] = name;
                    uris[count] = m_needUpdates.get( name );
                    ++count;
                }
                DwnldDelegate.downloadDictsInBack( m_activity, uris, names, this );
                break;
            default:
                Assert.fail();
            }
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
    }

    private void startDownload( int lang, String name )
    {
        DwnldDelegate.downloadDictInBack( m_activity, lang, name, this );
    }

    private void mkListAdapter()
    {
        Set<String> langs = new HashSet<String>();
        langs.addAll( Arrays.asList(DictLangCache.listLangs( m_activity )) );
        if ( m_showRemote && null != m_remoteInfo ) {
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
        // String dict_url = Utils.makeDictUri( context, lang, dict );
        // return mkDownloadIntent( context, dict_url );
    }

    public static void downloadForResult( Activity activity, int requestCode, 
                                          int lang, String name )
    {
        Intent intent = new Intent( activity, DictsActivity.class );
        intent.putExtra( DICT_SHOWREMOTE, true );
        if ( lang > 0 ) {
            intent.putExtra( DICT_LANG_EXTRA, lang );
        }
        if ( null != name ) {
            Assert.assertTrue( lang != 0 );
            intent.putExtra( DICT_NAME_EXTRA, name );
        }

        activity.startActivityForResult( intent, requestCode );
    }

    public static void downloadForResult( Activity activity, int requestCode,
                                          int lang )
    {
        downloadForResult( activity, requestCode, lang, null );
    }

    public static void downloadForResult( Activity activity, int requestCode )
    {
        downloadForResult( activity, requestCode, 0, null );
    }

    public static void downloadDefaultDict( Context context, String lc,
                                            OnGotLcDictListener lstnr )
    {
        new GetDefaultDictTask( context, lc, lstnr ).execute();
    }

    //////////////////////////////////////////////////////////////////////
    // XWListItem.ExpandedListener interface
    //////////////////////////////////////////////////////////////////////
    public void expanded( XWListItem me, boolean expanded )
    {
        final DictInfo info = (DictInfo)me.getCached();
        if ( expanded ) {
            m_expandedItems.add( info ); // may already be there
            LinearLayout view = 
                (LinearLayout)inflate( R.layout.remote_dict_details );
            Button button = (Button)view.findViewById( R.id.download_button );
            button.setOnClickListener( new View.OnClickListener() {
                    public void onClick( View view ) {
                        DwnldDelegate.
                            downloadDictInBack( m_activity, info.m_lang, 
                                                info.m_name, 
                                                DictsDelegate.this );
                    }
                } );
            
            long kBytes = (info.m_nBytes + 999) / 1000;
            String note = null == info.m_note ? m_noteNone : info.m_note;
            String msg = getString( R.string.dict_info_fmt, info.m_nWords, 
                                    kBytes, note );
            TextView summary = (TextView)view.findViewById( R.id.details );
            summary.setText( msg );
            
            me.addExpandedView( view );
        } else {
            me.removeExpandedView();
            m_expandedItems.remove( info );
        }
    }

    //////////////////////////////////////////////////////////////////////
    // DwnldActivity.DownloadFinishedListener interface
    //////////////////////////////////////////////////////////////////////
    public void downloadFinished( String lang, final String name, 
                                  final boolean success )
    {
        if ( success && m_showRemote ) {
            m_lastLang = lang;
            m_lastDict = name;
        }

        if ( m_launchedForMissing ) {
            post( new Runnable() {
                    public void run() {
                        if ( success ) {
                            Intent intent = getIntent();
                            if ( MultiService.returnOnDownload( m_activity,
                                                                intent ) ) {
                                finish();
                            } else if ( null != m_finishOnName
                                        && m_finishOnName.equals( name ) ) {
                                finish();
                            }
                        } else {
                            showToast( R.string.download_failed );
                        }
                    }
                } );
        } else {
            mkListAdapter();
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

    private static class GetDefaultDictTask extends AsyncTask<Void, Void, String> {
        private Context m_context;
        private String m_lc;
        private String m_langName;
        private OnGotLcDictListener m_lstnr;

        public GetDefaultDictTask( Context context, String lc, 
                                   OnGotLcDictListener lnr ) {
            m_context = context;
            m_lc = lc;
            m_lstnr = lnr;
        }

        @Override 
        public String doInBackground( Void... unused )
        {
            // FIXME: this should pass up the language code to retrieve and
            // parse less data
            String name = null;
            String proc = String.format( "listDicts?lc=%s", m_lc );
            HttpPost post = NetUtils.makePost( m_context, proc );
            if ( null != post ) {
                JSONObject theOne = null;
                String langName = null;
                String json = NetUtils.runPost( post, new JSONObject() );
                if ( null != json ) {
                    try {
                        JSONObject obj = new JSONObject( json );
                        JSONArray langs = obj.optJSONArray( "langs" );
                        int nLangs = langs.length();
                        for ( int ii = 0; ii < nLangs; ++ii ) {
                            JSONObject langObj = langs.getJSONObject( ii );
                            String langCode = langObj.getString( "lc" );
                            if ( ! langCode.equals( m_lc ) ) {
                                continue;
                            }
                            // we have our language; look for one marked default;
                            // otherwise take the largest.
                            m_langName = langObj.getString( "lang" );
                            JSONArray dicts = langObj.getJSONArray( "dicts" );
                            int nDicts = dicts.length();
                            int theOneNWords = 0;
                            for ( int jj = 0; jj < nDicts; ++jj ) {
                                JSONObject dict = dicts.getJSONObject( jj );
                                if ( dict.optBoolean( "isDflt", false ) ) {
                                    theOne = dict;
                                    break;
                                } else {
                                    int nWords = dict.getInt( "nWords" );
                                    if ( null == theOne 
                                         || nWords > theOneNWords ) {
                                        theOne = dict;
                                        theOneNWords = nWords;
                                    }
                                }
                            }
                        }

                        // If we got here and theOne isn't set, there is
                        // no wordlist available for this language. Set
                        // the flag so we don't try again, even though
                        // we've failed.
                        if ( null == theOne ) {
                            XWPrefs.setPrefsBoolean( m_context, 
                                                     R.string.key_got_langdict, 
                                                     true );
                        }

                    } catch ( JSONException ex ) {
                        DbgUtils.loge( ex );
                        theOne = null;
                    }
                }

                if ( null != theOne ) {
                    name = theOne.optString( "xwd" );
                }
            }
            return name;
        }

        @Override
        protected void onPostExecute( String name )
        {
            m_lstnr.gotDictInfo( null != name, m_langName, name );
        }
    }

    private class FetchListTask extends AsyncTask<Void, Void, Boolean> 
        implements OnCancelListener {
        private Context m_context;

        public FetchListTask( Context context )
        {
            m_context = context;
            startProgress( R.string.progress_title, R.string.remote_empty, this );
        }

        @Override 
        public Boolean doInBackground( Void... unused )
        {
            boolean success = false;
            HttpPost post = NetUtils.makePost( m_context, "listDicts" );
            if ( null != post ) {
                String json = NetUtils.runPost( post, new JSONObject() );
                if ( !isCancelled() ) {
                    if ( null != json ) {
                        post( new Runnable() {
                                public void run() {
                                    setProgressMsg( R.string.remote_digesting );
                                }
                            } );
                    }
                    success = digestData( json );
                }
            }
            return new Boolean( success );
        }
            
        @Override 
        protected void onCancelled()
        {
            m_remoteInfo = null;
            m_showRemote = false;
        }

        @Override 
        protected void onCancelled( Boolean success )
        {
            onCancelled();
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

        private boolean digestData( String jsonData )
        {
            boolean success = false;
            JSONArray langs = null;

            m_needUpdates = new HashMap<String, Uri>();
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
                    for ( int ii = 0; !isCancelled() && ii < nLangs; ++ii ) {
                        JSONObject langObj = langs.getJSONObject( ii );
                        String langName = langObj.getString( "lang" );
                    
                        if ( null != m_filterLang && 
                             ! m_filterLang.equals( langName ) ) {
                            continue;
                        }

                        if ( ! curLangs.contains( langName ) ) {
                            closedLangs.add( langName );
                        }

                        JSONArray dicts = langObj.getJSONArray( "dicts" );
                        int nDicts = dicts.length();
                        ArrayList<DictInfo> dictNames = 
                            new ArrayList<DictInfo>();
                        for ( int jj = 0; !isCancelled() && jj < nDicts; 
                              ++jj ) {
                            JSONObject dict = dicts.getJSONObject( jj );
                            String name = dict.getString( "xwd" );
                            name = DictUtils.removeDictExtn( name );
                            long nBytes = dict.optLong( "nBytes", -1 );
                            int nWords = dict.optInt( "nWords", -1 );
                            String note = dict.optString( "note" );
                            if ( 0 == note.length() ) {
                                note = null;
                            }
                            DictInfo info = new DictInfo( name, langName, nWords,
                                                          nBytes, note );

                            if ( !m_quickFetchMode ) {
                                // Check if we have it and it needs an update
                                if ( DictLangCache.haveDict( m_activity, 
                                                             langName, name )){
                                    boolean matches = true;
                                    String curSum = DictLangCache
                                        .getDictMD5Sum( m_activity, name );
                                    if ( null != curSum ) {
                                        JSONArray sums = 
                                            dict.getJSONArray("md5sums");
                                        if ( null != sums ) {
                                            matches = false;
                                            for ( int kk = 0; 
                                                  !matches && kk < sums.length(); 
                                                  ++kk ) {
                                                String sum = sums.getString( kk );
                                                matches = sum.equals( curSum );
                                            }
                                        }
                                    }
                                    if ( !matches ) {
                                        Uri uri = 
                                            Utils.makeDictUri( m_activity, 
                                                               langName, name );
                                        m_needUpdates.put( name, uri );
                                    }
                                }
                            }
                            dictNames.add( info );
                        }
                        if ( 0 < dictNames.size() ) {
                            DictInfo[] asArray = new DictInfo[dictNames.size()];
                            asArray = dictNames.toArray( asArray );
                            Arrays.sort( asArray );
                            m_remoteInfo.put( langName, asArray );
                        }
                    }

                    closedLangs.remove( m_filterLang );
                    m_closedLangs.addAll( closedLangs );

                    success = true;
                } catch ( JSONException ex ) {
                    DbgUtils.loge( ex );
                }
            }

            return success;
        } // digestData

        /////////////////////////////////////////////////////////////////
        // DialogInterface.OnCancelListener interface
        /////////////////////////////////////////////////////////////////
        public void onCancel( DialogInterface dialog )
        {
            m_checkbox.setChecked( false );
            cancel( true );
        }
    } // class FetchListTask
}
