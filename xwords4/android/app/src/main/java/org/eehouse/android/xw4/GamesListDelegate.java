/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2021 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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
import android.content.Context;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.text.TextUtils;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AbsListView.OnScrollListener;
import android.widget.AbsListView;
import android.widget.AdapterView.OnItemLongClickListener;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;

import com.jakewharton.processphoenix.ProcessPhoenix;

import org.eehouse.android.xw4.DBUtils.GameChangeType;
import org.eehouse.android.xw4.DBUtils.GameGroupInfo;
import org.eehouse.android.xw4.DBUtils.SentInvitesInfo;
import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.ActionPair;
import org.eehouse.android.xw4.DwnldDelegate.DownloadFinishedListener;
import org.eehouse.android.xw4.DwnldDelegate.OnGotLcDictListener;
import org.eehouse.android.xw4.Perms23.Perm;
import org.eehouse.android.xw4.ZipUtils.SaveWhat;
import org.eehouse.android.xw4.jni.CommonPrefs;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.CurGameInfo;
import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.jni.LastMoveInfo;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;
import static org.eehouse.android.xw4.DBUtils.ROWID_NOTFOUND;

import java.io.File;
import java.io.Serializable;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

public class GamesListDelegate extends ListDelegateBase
    implements OnItemLongClickListener,
               DBUtils.DBChangeListener, SelectableItem,
               DownloadFinishedListener, DlgDelegate.HasDlgDelegate,
               GroupStateListener, Log.ResultProcs {
    private static final String TAG = GamesListDelegate.class.getSimpleName();

    private static final String SAVE_NEXTSOLO = "SAVE_NEXTSOLO";
    private static final String SAVE_REMATCHEXTRAS = "SAVE_REMATCHEXTRAS";
    private static final String SAVE_MYSIS = TAG + "/MYSIS";

    private static final String ROWID_EXTRA = "rowid";
    private static final String GAMEID_EXTRA = "gameid";
    private static final String INVITEE_REC_EXTRA = "invitee_rec";
    private static final String REMATCH_ROWID_EXTRA = "rm_rowid";
    private static final String REMATCH_GROUPID_EXTRA = "rm_groupid";
    private static final String REMATCH_DICT_EXTRA = "rm_dict";
    private static final String REMATCH_LANG_EXTRA = "rm_lang";
    private static final String REMATCH_PREFS_EXTRA = "rm_prefs";
    private static final String REMATCH_NEWNAME_EXTRA = "rm_nnm";
    private static final String REMATCH_IS_SOLO = "rm_solo";
    private static final String REMATCH_ADDRS_EXTRA = "rm_addrs";
    private static final String REMATCH_BTADDR_EXTRA = "rm_btaddr";
    private static final String REMATCH_PHONE_EXTRA = "rm_phone";
    private static final String REMATCH_P2PADDR_EXTRA = "rm_p2pma";

    private static final String INVITE_ACTION = "org.eehouse.action_invite";
    private static final String INVITE_DATA = "data_invite";

    private static final String ALERT_MSG = "alert_msg";
    private static final String WITH_EMAIL = "with_email";

    private static class MySIS implements Serializable {
        public MySIS(){
            selGames = new HashSet<>();
            selGroupIDs = new HashSet<>();
        }
        int groupSelItem;
        boolean nextIsSolo;
        long[] moveAfterNewGroup;
        Set<Long> selGames;
        Set<Long> selGroupIDs;
    }
    private MySIS m_mySIS;

    private class GameListAdapter extends XWExpListAdapter {
        private long[] m_groupPositions;

        private class GroupRec {
            public GroupRec( long groupID, int position )
            {
                m_groupID = groupID;
                m_position = position;
            }
            long m_groupID;
            int m_position;
        }

        private class GameRec {
            public GameRec( long rowID ) {
                m_rowID = rowID;
            }
            long m_rowID;
        }

        GameListAdapter()
        {
            super( new Class[] { GroupRec.class, GameRec.class } );
            m_groupPositions = null;
        }

        protected Object[] makeListData()
        {
            final Map<Long,GameGroupInfo> gameInfo = DBUtils.getGroups( m_activity );
            ArrayList<Object> alist = new ArrayList<>();
            long[] positions = getGroupPositions();
            for ( int ii = 0; ii < positions.length; ++ii ) {
                long groupID = positions[ii];
                GameGroupInfo ggi = gameInfo.get( groupID );
                // m_groupIndices[ii] = alist.size();
                alist.add( new GroupRec( groupID, ii ) );

                if ( ggi.m_expanded ) {
                    List<Object> children = makeChildren( groupID );
                    alist.addAll( children );

                    if ( BuildConfig.DEBUG && ggi.m_count != children.size() ) {
                        Log.e( TAG, "m_count: %d != size: %d",
                               ggi.m_count, children.size() );
                        Assert.failDbg();
                    }
                }
            }

            return alist.toArray( new Object[alist.size()] );
        }

        @Override
        public View getView( Object dataObj, View convertView )
        {
            View result = null;
            if ( dataObj instanceof GroupRec ) {
                GroupRec rec = (GroupRec)dataObj;
                GameGroupInfo ggi = DBUtils.getGroups( m_activity )
                    .get( rec.m_groupID );
                GameListGroup group =
                    GameListGroup.makeForPosition( m_activity, convertView,
                                                   rec.m_groupID, ggi.m_count,
                                                   ggi.m_expanded,
                                                   GamesListDelegate.this,
                                                   GamesListDelegate.this );
                updateGroupPct( group, ggi );

                String name =
                    LocUtils.getQuantityString( m_activity,
                                                R.plurals.group_name_fmt,
                                                ggi.m_count, ggi.m_name,
                                                ggi.m_count );
                group.setText( name );
                group.setSelected( getSelected( group ) );
                result = group;
            } else if ( dataObj instanceof GameRec ) {
                GameRec rec = (GameRec)dataObj;
                GameListItem item =
                    GameListItem.makeForRow( m_activity, convertView,
                                             rec.m_rowID, m_handler,
                                             m_fieldID, GamesListDelegate.this );
                item.setSelected( m_mySIS.selGames.contains( rec.m_rowID ) );
                result = item;
            } else {
                Assert.failDbg();
            }
            return result;
        }

        void setSelected( long rowID, boolean selected )
        {
            Set<GameListItem> games = getGamesFromElems( rowID );
            if ( 1 == games.size() ) {
                games.iterator().next().setSelected( selected );
            }
        }

        void invalName( long rowID )
        {
            Set<GameListItem> games = getGamesFromElems( rowID );
            if ( 1 == games.size() ) {
                games.iterator().next().invalName();
            }
        }

        protected void removeGame( long rowID )
        {
            removeChildren( makeChildTestFor( rowID  ) );
        }

        protected boolean inExpandedGroup( long rowID )
        {
            boolean expanded = false;
            GroupRec rec = (GroupRec)
                findParent( makeChildTestFor( rowID  ) );
            if ( null != rec ) {
                GameGroupInfo ggi =
                    DBUtils.getGroups( m_activity ).get( rec.m_groupID );
                expanded = ggi.m_expanded;
            }
            return expanded;
        }

        protected GameListItem reloadGame( long rowID )
        {
            GameListItem item = null;
            Set<GameListItem> games = getGamesFromElems( rowID );
            if ( 0 < games.size() ) {
                item = games.iterator().next();
                item.forceReload();
            } else {
                // If the game's not visible, update the parent group in case
                // the game's changed in a way that makes it draw differently
                long parent = DBUtils.getGroupForGame( m_activity, rowID );
                Iterator<GameListGroup> iter = getGroupWithID( parent ).iterator();
                if ( iter.hasNext() ) {
                    GameListGroup group = iter.next();
                    GameGroupInfo ggi = DBUtils.getGroups( m_activity ).get( parent );
                    updateGroupPct( group, ggi );
                }
            }
            return item;
        }

        String groupName( long groupID )
        {
            final Map<Long,GameGroupInfo> gameInfo =
                DBUtils.getGroups( m_activity );
            return gameInfo.get(groupID).m_name;
        }

        long getGroupIDFor( int groupPos )
        {
            return getGroupPositions()[groupPos];
        }

        String[] groupNames()
        {
            long[] positions = getGroupPositions();
            final Map<Long,GameGroupInfo> gameInfo =
                DBUtils.getGroups( m_activity );
            Assert.assertTrue( positions.length == gameInfo.size() );
            String[] names = new String[positions.length];
            for ( int ii = 0; ii < positions.length; ++ii ) {
                names[ii] = gameInfo.get( positions[ii] ).m_name;
            }
            return names;
        }

        int getGroupPosition( long groupID )
        {
            int posn = -1;
            if ( -1 != groupID ) {
                long[] positions = getGroupPositions();
                for ( int ii = 0; ii < positions.length; ++ii ) {
                    if ( positions[ii] == groupID ) {
                        posn = ii;
                        break;
                    }
                }
                if ( -1 == posn ) {
                    Log.d( TAG, "getGroupPosition: group %d not found", groupID );
                }
            }
            return posn;
        }

        long[] getGroupPositions()
        {
            // do not modify!!!!
            final Set<Long> dbGroups = DBUtils.getGroups( m_activity ).keySet();

            if ( null == m_groupPositions || m_groupPositions.length != dbGroups.size() ) {

                // If the stored order is out-of-sync with the DB, e.g. if
                // there have been additions or deletions, keep the ordering
                // of groups that we have ordering for. Then add the rest.
                m_groupPositions = new long[dbGroups.size()];
                Set<Long> added = new HashSet<>();

                long[] groupPositions = loadGroupPositions();
                int nextIndx = 0;
                for ( long posn : groupPositions ) {
                    if ( dbGroups.contains(posn) ) {
                        m_groupPositions[nextIndx++] = posn;
                        added.add(posn);
                    }
                }

                // Now add at the end the ones we're missing
                for ( long posn : dbGroups ) {
                    if ( !added.contains(posn) ) {
                        m_groupPositions[nextIndx++] = posn;
                    }
                }

            } else if ( BuildConfig.DEBUG ) {
                for ( long posn : m_groupPositions ) {
                    Assert.assertTrueNR( dbGroups.contains(posn) );
                }
            }
            return m_groupPositions;
        }

        String formatGroupNames( long[] groupIDs )
        {
            List<String> names = new ArrayList<>();
            // Iterate in-order to produce strings in order
            long[] inOrder = getGroupPositions();
            for ( long id : inOrder ) {
                for ( long inSet : groupIDs ) {
                    if ( id == inSet ) {
                        names.add( groupName( id ) );
                        break;
                    }
                }
            }
            return TextUtils.join( ", ", names );
        }

        int getChildrenCount( long groupID )
        {
            GameGroupInfo ggi = DBUtils.getGroups( m_activity ).get( groupID );
            return ggi.m_count;
        }

        void moveGroup( long groupID, boolean moveUp )
        {
            try {
                int src = getGroupPosition( groupID );
                int dest = src + (moveUp ? -1 : 1);

                long[] positions = getGroupPositions();
                long tmp = positions[src];
                positions[src] = positions[dest];
                positions[dest] = tmp;
                storeGroupPositions( positions );

                swapGroups( src, dest );
            } catch ( ArrayIndexOutOfBoundsException ioob ) {
                Log.ex( TAG, ioob );
            }
        }

        boolean setField( int newID )
        {
            boolean changed = false;
            if ( 0 != newID && m_fieldID != newID ) {
                m_fieldID = newID;
                // return true so caller will do onContentChanged.
                // There's no other way to signal GameListItem instances
                // since we don't maintain a list of them.
                changed = true;
            }
            return changed;
        }

        void clearSelectedGames( Set<Long> rowIDs )
        {
            Set<GameListItem> games = getGamesFromElems( rowIDs );
            for ( Iterator<GameListItem> iter = games.iterator();
                  iter.hasNext(); ) {
                iter.next().setSelected( false );
            }
        }

        void clearSelectedGroups( Set<Long> groupIDs )
        {
            Set<GameListGroup> groups = getGroupsWithIDs( groupIDs );
            for ( GameListGroup group : groups ) {
                group.setSelected( false );
            }
        }

        void setExpanded( long groupID, boolean expanded )
        {
            if ( expanded ) {
                addChildrenOf( groupID );
            } else {
                removeChildrenOf( groupID );
            }
        }

        private void updateGroupPct( GameListGroup group, GameGroupInfo ggi )
        {
            if ( !ggi.m_expanded ) {
                group.setPct( m_handler, ggi.m_hasTurn, ggi.m_turnLocal,
                              ggi.m_lastMoveTime );
            }
        }

        private void removeChildrenOf( long groupID )
        {
            int indx = findGroupItem( makeGroupTestFor( groupID ) );
            removeChildrenOf( indx );
        }

        private void addChildrenOf( long groupID )
        {
            int indx = findGroupItem( makeGroupTestFor( groupID ) );
            addChildrenOf( indx, makeChildren( groupID ) );
        }

        private List<Object> makeChildren( long groupID )
        {
            long[] rows = DBUtils.getGroupGames( m_activity, groupID );
            List<Object> alist = new ArrayList<>( rows.length );
            for ( long row : rows ) {
                alist.add( new GameRec( row ) );
            }
            // DbgUtils.logf( "GamesListDelegate.makeChildren(%d) => %d kids", groupID, alist.size() );
            return alist;
        }

        private XWExpListAdapter.GroupTest makeGroupTestFor( final long groupID  )
        {
            return new XWExpListAdapter.GroupTest() {
                public boolean isTheGroup( Object item ) {
                    GroupRec rec = (GroupRec)item;
                    return rec.m_groupID == groupID;
                }
            };
        }

        private XWExpListAdapter.ChildTest makeChildTestFor( final long rowID  )
        {
            return new XWExpListAdapter.ChildTest() {
                public boolean isTheChild( Object item ) {
                    GameRec rec = (GameRec)item;
                    return rec.m_rowID == rowID;
                }
            };
        }

        private ArrayList<Object> removeRange( ArrayList<Object> list,
                                               int start, int len )
        {
            Log.d( TAG, "removeRange(start=%d, len=%d)", start, len );
            ArrayList<Object> result = new ArrayList<>(len);
            for ( int ii = 0; ii < len; ++ii ) {
                result.add( list.remove( start ) );
            }
            return result;
        }

        private Set<GameListGroup> getGroupWithID( long groupID )
        {
            Set<Long> groupIDs = new HashSet<>();
            groupIDs.add( groupID );
            Set<GameListGroup> result = getGroupsWithIDs( groupIDs );
            return result;
        }

        // Yes, iterating is bad, but any hashing to get around it will mean
        // hanging onto Views that Android's list management might otherwise
        // get to page out when they scroll offscreen.
        private Set<GameListGroup> getGroupsWithIDs( Set<Long> groupIDs )
        {
            Set<GameListGroup> result = new HashSet<>();
            ListView listView = getListView();
            int count = listView.getChildCount();
            for ( int ii = 0; ii < count; ++ii ) {
                View view = listView.getChildAt( ii );
                if ( view instanceof GameListGroup ) {
                    GameListGroup tryme = (GameListGroup)view;
                    if ( groupIDs.contains( tryme.getGroupID() ) ) {
                        result.add( tryme );
                    }
                }
            }
            return result;
        }

        private Set<GameListItem> getGamesFromElems( long rowID )
        {
            HashSet<Long> rowSet = new HashSet<>();
            rowSet.add( rowID );
            return getGamesFromElems( rowSet );
        }

        private Set<GameListItem> getGamesFromElems( Set<Long> rowIDs )
        {
            Set<GameListItem> result = new HashSet<>();
            ListView listView = getListView();
            int count = listView.getChildCount();
            for ( int ii = 0; ii < count; ++ii ) {
                View view = listView.getChildAt( ii );
                if ( view instanceof GameListItem ) {
                    GameListItem tryme = (GameListItem)view;
                    long rowID = tryme.getRowID();
                    if ( rowIDs.contains( rowID ) ) {
                        result.add( tryme );
                    }
                }
            }
            return result;
        }
    } // class GameListAdapter

    private static final int[] DEBUG_ITEMS = {
        R.id.games_menu_writegit,
        R.id.games_submenu_logs,
    };
    private static final int[] NOSEL_ITEMS = {
        R.id.games_menu_newgroup,
        R.id.games_menu_prefs,
        R.id.games_menu_dicts,
        R.id.games_menu_about,
        R.id.games_menu_email,
    };
    private static final int[] ONEGAME_ITEMS = {
        R.id.games_game_config,
        R.id.games_game_rename,
        R.id.games_game_new_from,
        R.id.games_game_copy,
    };

    private static final int[] ONEGROUP_ITEMS = {
        R.id.games_group_rename,
    };

    private static boolean s_firstShown = false;
    private int m_fieldID;

    private Activity m_activity;
    private static GamesListDelegate s_self;
    private GameListAdapter m_adapter;
    private Handler m_handler;
    private String m_missingDict;
    private long m_missingDictRowId = ROWID_NOTFOUND;
    private int m_missingDictMenuId;
    private int m_missingDictLang;
    private String m_nameField;
    private NetLaunchInfo m_netLaunchInfo;
    private boolean m_menuPrepared;
    private String m_origTitle;
    private Button[] m_newGameButtons;
    private boolean m_haveShownGetDict;
    private Bundle m_rematchExtras;
    private Object[] m_newGameParams;
    private int mCurScrollState;

    public GamesListDelegate( Delegator delegator, Bundle sis )
    {
        super( delegator, sis, R.layout.game_list, R.menu.games_list_menu );
        m_activity = delegator.getActivity();
        s_self = this;
    }

    @Override
    protected Dialog makeDialog( DBAlert alert, Object[] params )
    {
        Dialog dialog = null;
        OnClickListener lstnr, lstnr2;
        AlertDialog.Builder ab;

        DlgID dlgID = alert.getDlgID();
        switch ( dlgID ) {
        case WARN_NODICT_GENERIC:
        case WARN_NODICT_INVITED:
        case WARN_NODICT_SUBST: {
            final long rowid = (Long)params[0];
            final String missingDictName = (String)params[1];
            final String missingDictLang = (String)params[2];

            lstnr = new OnClickListener() {
                    @Override
                    public void onClick( DialogInterface dlg, int item ) {
                        if ( null == missingDictName ) {
                            DictsDelegate
                                .downloadForResult( getDelegator(),
                                                    RequestCode
                                                    .REQUEST_LANG_GL,
                                                    missingDictLang );
                        } else {
                            DwnldDelegate
                                .downloadDictInBack( m_activity,
                                                     missingDictLang,
                                                     missingDictName,
                                                     GamesListDelegate.this );
                        }
                    }
                };
            String message;
            String langName =
                DictLangCache.getLangNameForISOCode( m_activity, missingDictLang );
            String locLang = xlateLang( langName );
            String gameName = GameUtils.getName( m_activity, rowid );
            if ( DlgID.WARN_NODICT_GENERIC == dlgID ) {
                message = getString( R.string.no_dict_fmt, gameName, locLang );
            } else if ( DlgID.WARN_NODICT_INVITED == dlgID ) {
                message = getString( R.string.invite_dict_missing_body_noname_fmt,
                                     null, missingDictName, locLang );
            } else {
                // WARN_NODICT_SUBST
                message = getString( R.string.no_dict_subst_fmt, gameName,
                                     missingDictName, locLang );
            }

            ab = makeAlertBuilder()
                .setTitle( R.string.no_dict_title )
                .setMessage( message )
                .setPositiveButton( android.R.string.cancel, null )
                .setNegativeButton( R.string.button_download, lstnr )
                ;
            if ( DlgID.WARN_NODICT_SUBST == dlgID ) {
                OnClickListener neuLstnr = new OnClickListener() {
                        @Override
                        public void onClick( DialogInterface dlg, int item ) {
                            showDialogFragment( DlgID.SHOW_SUBST, rowid,
                                                missingDictName, missingDictLang );
                        }
                    };
                ab.setNeutralButton( R.string.button_substdict, neuLstnr );
            } else if ( DlgID.WARN_NODICT_GENERIC == dlgID ) {
                OnClickListener neuLstnr = new OnClickListener() {
                        @Override
                        public void onClick( DialogInterface dlg, int item ) {
                            long[] rowids = {rowid};
                            deleteNamedIfConfirmed( rowids, false );
                        }
                    };
                ab.setNeutralButton( R.string.button_delete, neuLstnr );
            }
            dialog = ab.create();
        }
            break;
        case SHOW_SUBST: {
            final long rowid = (Long)params[0];
            final String missingDict = (String)params[1];
            final String isoCode = (String)params[2];

            final String[] sameLangDicts =
                DictLangCache.getHaveLangCounts( m_activity, isoCode );
            lstnr = new OnClickListener() {
                    @Override
                    public void onClick( DialogInterface dlg,
                                         int which ) {
                        int pos = ((AlertDialog)dlg).getListView().
                            getCheckedItemPosition();
                        String newDict = sameLangDicts[pos];
                        newDict = DictLangCache.stripCount( newDict );
                        if ( GameUtils.replaceDicts( m_activity, rowid,
                                                     missingDict, newDict ) ) {
                            launchGameIf();
                        }
                    }
                };
            dialog = makeAlertBuilder()
                .setTitle( R.string.subst_dict_title )
                .setPositiveButton( R.string.button_substdict, lstnr )
                .setNegativeButton( android.R.string.cancel, null )
                .setSingleChoiceItems( sameLangDicts, 0, null )
                .create();
        }
            break;

        case RENAME_GAME: {
            final long rowid = (Long)params[0];
            GameSummary summary = GameUtils.getSummary( m_activity, rowid );
            int labelID = (summary.isMultiGame() && !summary.anyMissing())
                ? R.string.rename_label_caveat : R.string.rename_label;
            final Renamer namer =
                buildRenamer( GameUtils.getName( m_activity, rowid ), labelID );
            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        String name = namer.getName();
                        DBUtils.setName( m_activity, rowid,
                                         name );
                        m_adapter.invalName( rowid );
                    }
                };
            dialog = buildNamerDlg( namer, R.string.game_rename_title,
                                    lstnr, null, DlgID.RENAME_GAME );
        }
            break;

        case RENAME_GROUP: {
            final long groupID = (Long)params[0];
            final Renamer namer = buildRenamer( m_adapter.groupName(groupID),
                                                R.string.rename_group_label );
            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        GamesListDelegate self = curThis();
                        String name = namer.getName();
                        DBUtils.setGroupName( m_activity,
                                              groupID, name );
                        // Don't have m_rowid any more. But what's this doing again?
                        // reloadGame( m_rowid );
                        self.mkListAdapter();
                    }
                };
            dialog = buildNamerDlg( namer, R.string.game_name_group_title,
                                    lstnr, null, DlgID.RENAME_GROUP );
        }
            break;

        case BACKUP_LOADSTORE:
            Uri uri = 0 == params.length ? null : Uri.parse((String)params[0]);
            dialog = mkLoadStoreDlg( uri );
            break;

        case NEW_GROUP: {
            final Renamer namer = buildRenamer( "", R.string.newgroup_label );
            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        String name = namer.getName();
                        long hasName = DBUtils.getGroup( m_activity, name );
                        if ( DBUtils.GROUPID_UNSPEC == hasName ) {
                            DBUtils.addGroup( m_activity, name );
                            mkListAdapter();
                            showNewGroupIf();
                        } else {
                            String msg = LocUtils
                                .getString( m_activity,
                                            R.string.duplicate_group_name_fmt,
                                            name );
                            makeOkOnlyBuilder( msg ).show();
                        }
                    }
                };
            lstnr2 = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        curThis().showNewGroupIf();
                    }
                };
            dialog = buildNamerDlg( namer,
                                    R.string.game_name_group_title,
                                    lstnr, lstnr2, DlgID.RENAME_GROUP );
        }
            break;

        case CHANGE_GROUP:
            final long[] games = (long[])params[0];
            dialog = makeAlertBuilder()
                .setTitle( R.string.change_group )
                .setSingleChoiceItems( m_adapter.groupNames(),
                                       m_mySIS.groupSelItem,
                                       new OnClickListener() {
                                           public void onClick(DialogInterface
                                                               dlgi,
                                                               int item ) {
                                               m_mySIS.groupSelItem = item;
                                               enableMoveGroupButton( dlgi );
                                           }
                                       } )
                .setPositiveButton( R.string.button_move,
                                    new OnClickListener() {
                                        public void onClick( DialogInterface dlg,
                                                             int item ) {
                                            long gid = m_adapter
                                                .getGroupIDFor( m_mySIS
                                                                .groupSelItem );
                                            moveSelGamesTo( games, gid );
                                        }
                                    } )
                .setNeutralButton( R.string.button_newgroup,
                                   new OnClickListener() {
                                       @Override
                                       public void onClick( DialogInterface dlg,
                                                            int item ) {
                                           m_mySIS.moveAfterNewGroup = games;
                                           showDialogFragment( DlgID.NEW_GROUP );
                                       }
                                   } )
                .setNegativeButton( android.R.string.cancel, null )
                .create();
            dialog.setOnShowListener(new DialogInterface.OnShowListener() {
                    @Override
                    public void onShow(DialogInterface dlg) {
                        enableMoveGroupButton( dlg );
                    }
                });
            break;

        case GET_NAME: {
            LinearLayout layout = (LinearLayout)inflate( R.layout.dflt_name );
            final EditText etext =
                (EditText)layout.findViewById( R.id.name_edit );
            etext.setText( CommonPrefs.getDefaultPlayerName( m_activity,
                                                             0, true ) );
            alert.setOnDismissListener( new DBAlert.OnDismissListener() {
                    @Override
                    public void onDismissed( XWDialogFragment frag ) {
                        String name = etext.getText().toString();
                        if ( 0 == name.length() ) {
                            name = CommonPrefs.
                                getDefaultPlayerName( m_activity, 0, true );
                        } else {
                            CommonPrefs.setDefaultPlayerName( m_activity, name );
                        }
                        makeThenLaunchOrConfigure();
                    }
                } );
            dialog = makeAlertBuilder()
                .setTitle( R.string.default_name_title )
                .setMessage( R.string.default_name_message )
                .setPositiveButton( android.R.string.ok, null )
                .setView( layout )
                .create();
        }
            break;

        case GAMES_LIST_NEWGAME:
            boolean solo = (Boolean)params[0];
            dialog = mkNewNetGameDialog( solo );
            if ( !solo && XwJNI.hasKnownPlayers() ) {
                makeNotAgainBuilder( R.string.not_again_quicknetgame,
                                     R.string.key_na_quicknetgame )
                    .setTitle( R.string.new_feature_title )
                    .show();
            }
            break;

        case GAMES_LIST_NAME_REMATCH: {
            final LinearLayout view = (LinearLayout)
                LocUtils.inflate( m_activity, R.layout.msg_label_and_edit );
            int iconResID = R.drawable.ic_sologame;
            if ( null != m_rematchExtras ) {
                EditWClear edit = (EditWClear)view.findViewById( R.id.edit );
                edit.setText( m_rematchExtras.getString( REMATCH_NEWNAME_EXTRA ));
                solo = m_rematchExtras.getBoolean( REMATCH_IS_SOLO, true );
                if ( !solo ) {
                    iconResID = R.drawable.ic_multigame;
                }
                view.findViewById( R.id.msg ).setVisibility( View.GONE );
            }

            dialog = makeAlertBuilder()
                .setView( view )
                .setTitle( R.string.button_rematch )
                .setIcon( iconResID )
                .setPositiveButton( android.R.string.ok, new OnClickListener() {
                        @Override
                        public void onClick( DialogInterface dlg, int item ) {
                            EditWClear edit = (EditWClear)((Dialog)dlg)
                                .findViewById( R.id.edit );
                            String gameName = edit.getText().toString();
                            startRematchWithName( gameName, true );
                        }
                    } )
                .create();
        }
            break;

        default:
            dialog = super.makeDialog( alert, params );
            break;
        }
        return dialog;
    } // makeDialog

    private Dialog mkNewNetGameDialog( final boolean standalone )
    {
        // String[] names = XwJNI.kplr_getPlayers();
        final NewWithKnowns view = (NewWithKnowns)
            LocUtils.inflate( m_activity, R.layout.new_game_with_knowns );
        AlertDialog.Builder ab = makeAlertBuilder()
            .setView( view )
            .setTitle( standalone ? R.string.new_game : R.string.new_game_networked )
            .setIcon( standalone ? R.drawable.ic_sologame: R.drawable.ic_multigame )
            .setPositiveButton( android.R.string.cancel, new OnClickListener() {
                    @Override
                    public void onClick( DialogInterface dlg, int item ) {
                        view.onButtonPressed(new NewWithKnowns.ButtonCallbacks() {
                                @Override
                                public void onUseKnown( String knownName, String gameName )
                                {
                                    Assert.assertTrueNR( !standalone );
                                    CommsAddrRec addr = XwJNI.kplr_getAddr( knownName );
                                    if ( null != addr ) {
                                        launchLikeRematch( addr, gameName );
                                    }
                                }
                                @Override
                                public void onStartGame( String gameName, boolean solo,
                                                         boolean configFirst )
                                {
                                    Assert.assertTrueNR( solo == standalone );
                                    Assert.assertTrueNR( solo == m_mySIS.nextIsSolo );
                                    curThis().makeThenLaunchOrConfigure( gameName,
                                                                         configFirst,
                                                                         false );
                                }
                            } );
                    }
                } );
        if ( !standalone && XwJNI.hasKnownPlayers() ) {
            ab.setNegativeButton( R.string.gamel_menu_knownplyrs,
                                  new OnClickListener() {
                                      @Override
                                      public void onClick( DialogInterface dlg,
                                                           int item ) {
                                          KnownPlayersDelegate
                                              .launchOrAlert( getDelegator(),
                                                              GamesListDelegate.this );
                                      }
                                  } );
        }

        final AlertDialog dialog = ab.create();

        dialog.setOnShowListener(new DialogInterface.OnShowListener() {
            @Override
            public void onShow( DialogInterface diface ) {
                view.setCallback( new NewWithKnowns.ButtonChangeListener() {
                        @Override
                        public void onNewButtonText( String txt ) {
                            Button button = dialog.getButton( DialogInterface.BUTTON_POSITIVE );
                            if ( null != button ) {
                                button.setText( txt );
                            } else {
                                Assert.failDbg();
                            }
                        }
                    } );
                view.configure( standalone, GameUtils.makeDefaultName( m_activity ) );
            }
        });

        return dialog;
    }

    private void enableMoveGroupButton( DialogInterface dlgi )
    {
        Utils.enableAlertButton( (AlertDialog)dlgi, AlertDialog.BUTTON_POSITIVE,
                                 0 <= m_mySIS.groupSelItem );
    }

    @Override
    protected void init( Bundle savedInstanceState )
    {
        boolean isFirstLaunch = null == savedInstanceState;
        m_origTitle = getTitle();

        m_handler = new Handler();
        // Next line useful if contents of DB are crashing app on start
        // DBUtils.saveDB( m_activity );

        getBundledData( savedInstanceState );

        DBUtils.setDBChangeListener( this );

        boolean isUpgrade = Utils.firstBootThisVersion( m_activity );
        if ( isUpgrade ) {
            if ( !s_firstShown ) {
                if ( LocUtils.getCurLangCode( m_activity ).equals( "en" ) ) {
                    show( FirstRunDialog.newInstance() );
                }
                s_firstShown = true;
            }
        }

        m_newGameButtons = new Button[] {
            (Button)findViewById( R.id.button_newgame_solo ),
            (Button)findViewById( R.id.button_newgame_multi )
        };

        mkListAdapter();

        final ListView lv = getListView();
        lv.setOnItemLongClickListener( this );

        // Can't just enable fast scrolling because the scroller's wide touch
        // area disables taps on what's underneath. The expander arrows in
        // this case. So these two listener callbacks enable fast scrolling
        // only after the user's started scrolling and disable it when [s]he's
        // done
        //
        // See https://stackoverflow.com/questions/33619453/scrollbar-touch-area-in-android-6
        mCurScrollState = OnScrollListener.SCROLL_STATE_IDLE;
        lv.setOnScrollListener( new OnScrollListener() {
                @Override
                public void onScroll( AbsListView absListView, int firstVis,
                                      int visCount, int totalCount )
                {
                    if ( 0 < visCount && visCount < totalCount ) {
                        checkOfferHideButtons();
                    }
                    if ( mCurScrollState == OnScrollListener.SCROLL_STATE_TOUCH_SCROLL ) {
                        lv.setFastScrollEnabled( true );
                    }
                }
                @Override
                public void onScrollStateChanged( AbsListView absListView, int state )
                {
                    if ( state == OnScrollListener.SCROLL_STATE_IDLE
                         && mCurScrollState != state ) {
                        lv.postDelayed( new Runnable() {
                                @Override
                                public void run() {
                                    lv.setFastScrollEnabled( false );
                                }
                            }, 500 );
                    }
                    mCurScrollState = state;
                }
            } );

        post( new Runnable() {
                @Override
                public void run() {
                    tryStartsFromIntent( getIntent() );
                    getDictForLangIf();
                }
            } );

        updateField();

        // RECEIVE_SMS is required now (Oreo/SDK_26) but wasn't
        // before. There's logic elsewhere to ask for it AND SEND_SMS, but if
        // the user's already granted SEND_SMS we can get RECEIVE_SMS just by
        // asking (OS will grant without user interaction) since they're in
        // the same group. So just do it now.  This code can be removed
        // later...
        if ( !Perm.RECEIVE_SMS.isBanned(m_activity) ) {
            if ( Perms23.havePermissions( m_activity, Perm.SEND_SMS ) ) {
                Perms23.tryGetPerms( this, Perm.RECEIVE_SMS, 0, Action.SKIP_CALLBACK );
            }
        } else if ( isFirstLaunch ) {
            warnSMSBannedIf();
        }

        if ( false ) {
            Set<Long> dupModeGames = DBUtils.getDupModeGames( m_activity ).keySet();
            long[] asArray = new long[dupModeGames.size()];
            int ii = 0;
            for ( long rowid : dupModeGames ) {
                Log.d( TAG, "row %d is dup-mode", rowid );
                asArray[ii++] = rowid;
            }
            deleteGames( asArray, true );
        }
    } // init

    @Override
    protected boolean canHandleNewIntent( Intent intent )
    {
        return true;
    }

    @Override
    protected void handleNewIntent( Intent intent )
    {
        Log.d( TAG, "handleNewIntent(extras={%s})", DbgUtils.extrasToString( intent ) );
        Assert.assertNotNull( intent );
        tryStartsFromIntent( intent );
    }

    @Override
    protected void onStop()
    {
        // TelephonyManager mgr =
        //     (TelephonyManager)getSystemService( Context.TELEPHONY_SERVICE );
        // mgr.listen( m_phoneStateListener, PhoneStateListener.LISTEN_NONE );
        // m_phoneStateListener = null;
        long[] positions = m_adapter.getGroupPositions();
        storeGroupPositions( positions );
        super.onStop();
    }

    protected void onDestroy()
    {
        DBUtils.clearDBChangeListener( this );
        if ( s_self == this ) {
            s_self = null;
        }
    }

    @Override
    protected void onSaveInstanceState( Bundle outState )
    {
        outState.putSerializable( SAVE_MYSIS, m_mySIS );
        if ( null != m_netLaunchInfo ) {
            m_netLaunchInfo.putSelf( outState );
        }
        if ( null != m_rematchExtras ) {
            outState.putBundle( SAVE_REMATCHEXTRAS, m_rematchExtras );
        }
        super.onSaveInstanceState( outState );
    }

    // Only called when scrolling's necessary
    private boolean m_offeredHideButtons = false;
    private void checkOfferHideButtons()
    {
        if ( !m_offeredHideButtons ) {
            if ( XWPrefs.getHideNewgameButtons( m_activity ) ) {
                m_offeredHideButtons = true; // don't do expensive check again
            } else {
                m_offeredHideButtons = true;
                makeNotAgainBuilder( R.string.not_again_hidenewgamebuttons,
                                     R.string.key_notagain_hidenewgamebuttons )
                    .setActionPair( Action.SET_HIDE_NEWGAME_BUTTONS,
                                    R.string.set_pref )
                    .show();
            }
        }
    }

    private void getBundledData( Bundle bundle )
    {
        if ( null != bundle ) {
            m_netLaunchInfo = NetLaunchInfo.makeFrom( bundle );
            m_rematchExtras = bundle.getBundle( SAVE_REMATCHEXTRAS );
            m_mySIS = (MySIS)bundle.getSerializable( SAVE_MYSIS );
        } else {
            m_mySIS = new MySIS();
        }
    }

    private void warnSMSBannedIf()
    {
        if ( !Perms23.havePermissions( m_activity, Perm.SEND_SMS, Perm.RECEIVE_SMS )
             && Perm.SEND_SMS.isBanned(m_activity) ) {
            int smsGameCount = DBUtils.countOpenGamesUsingNBS( m_activity );
            if ( 0 < smsGameCount ) {
                String msg = LocUtils.getString( m_activity,
                                                 R.string.not_again_nbsGamesOnUpgrade,
                                                 smsGameCount );
                makeNotAgainBuilder( msg, R.string.key_notagain_nbsGamesOnUpgrade )
                    .setActionPair( Action.PERMS_BANNED_INFO,
                                    R.string.button_more_info )
                    .show();
            }
        }
    }

    private void moveGroup( long groupID, boolean moveUp )
    {
        m_adapter.moveGroup( groupID, moveUp );
        //     long[] positions = m_adapter.getGroupPositions();
        //     XWPrefs.setGroupPositions( m_activity, positions );

        //     m_adapter.notifyDataSetChanged();
        //     // mkListAdapter();
        // }
    }

    private void moveSelGamesTo( long[] games, long gid )
    {
        boolean destOpen = DBUtils.getGroups( m_activity ).get( gid ).m_expanded;
        for ( long rowid : games ) {
            DBUtils.moveGame( m_activity, rowid, gid );
            unselIfHidden( rowid, gid );
        }
    }

    private void unselIfHidden( long rowid, long gid )
    {
        boolean groupOpen = DBUtils.getGroups( m_activity )
            .get( gid ).m_expanded;
        if ( !groupOpen ) {
            m_mySIS.selGames.remove( rowid );
            // Invalidate if there could have been change
            invalidateOptionsMenuIf();
            setTitle();
        }
    }

    private void unselIfHidden( long rowid )
    {
        long gid = DBUtils.getGroupForGame( m_activity, rowid );
        unselIfHidden( rowid, gid );
    }

    public void invalidateOptionsMenuIf()
    {
        super.invalidateOptionsMenuIf();

        if ( !XWPrefs.getHideNewgameButtons( m_activity ) ) {
            boolean enabled = 0 == m_mySIS.selGames.size()
                && 1 >= m_mySIS.selGroupIDs.size();
            for ( Button button : m_newGameButtons ) {
                button.setEnabled( enabled );
            }
        }
    }

    protected void onWindowFocusChanged( boolean hasFocus )
    {
        if ( hasFocus ) {
            updateField();
        }
    }

    @Override
    protected GamesListDelegate curThis()
    {
        return (GamesListDelegate)super.curThis();
    }

    // OnItemLongClickListener interface
    @Override
    public boolean onItemLongClick( AdapterView<?> parent, View view,
                                    int position, long id ) {
        boolean success = ! XWApp.CONTEXT_MENUS_ENABLED
            && view instanceof SelectableItem.LongClickHandler;
        if ( success ) {
            ((SelectableItem.LongClickHandler)view).longClicked();
        }
        return success;
    }

    //////////////////////////////////////////////////////////////////////
    // DBUtils.DBChangeListener interface
    //////////////////////////////////////////////////////////////////////
    @Override
    public void gameSaved( Context context, final long rowid,
                           final GameChangeType change )
    {
        post( new Runnable() {
                public void run() {
                    switch( change ) {
                    case GAME_DELETED:
                        m_adapter.removeGame( rowid );
                        m_mySIS.selGames.remove( rowid );
                        invalidateOptionsMenuIf();
                        break;
                    case GAME_CHANGED:
                        if ( DBUtils.ROWIDS_ALL == rowid ) { // all changed
                            mkListAdapter();
                        } else {
                            reloadGame( rowid );
                            if ( m_adapter.inExpandedGroup( rowid ) ) {
                                long groupID = DBUtils.getGroupForGame( m_activity, rowid );
                                m_adapter.setExpanded( groupID, false );
                                m_adapter.setExpanded( groupID, true );
                            }
                        }
                        break;
                    case GAME_CREATED:
                        mkListAdapter();
                        setSelGame( rowid );
                        break;
                    case GAME_MOVED:
                        unselIfHidden( rowid );
                        mkListAdapter();
                        break;
                    default:
                        Assert.failDbg();
                        break;
                    }
                }
            } );
    }

    private void openWithChecks( long rowid, GameSummary summary )
    {
        if ( ! BoardDelegate.gameIsOpen( rowid ) ) {
            if ( Quarantine.safeToOpen( rowid ) ) {
                makeNotAgainBuilder( R.string.not_again_newselect,
                                     R.string.key_notagain_newselect,
                                     Action.OPEN_GAME )
                    .setParams( rowid, summary )
                    .show();
            } else {
                makeConfirmThenBuilder( R.string.unsafe_open_warning,
                                        Action.QUARANTINE_CLEAR )
                    .setPosButton( R.string.unsafe_open_disregard )
                    .setNegButton( 0 )
                    .setActionPair( Action.QUARANTINE_DELETE,
                                    R.string.button_delete )
                    .setParams( rowid, summary )
                    .show();
            }
        }
    }

    //////////////////////////////////////////////////////////////////////
    // SelectableItem interface
    //////////////////////////////////////////////////////////////////////
    public void itemClicked( SelectableItem.LongClickHandler clicked,
                             GameSummary summary )
    {
        // We need a way to let the user get back to the basic-config
        // dialog in case it was dismissed.  That way it to check for
        // an empty room name.
        if ( clicked instanceof GameListItem ) {
            long rowid = ((GameListItem)clicked).getRowID();
            openWithChecks( rowid, summary );
        }
    }

    public void itemToggled( SelectableItem.LongClickHandler toggled,
                             boolean selected )
    {
        if ( toggled instanceof GameListItem ) {
            long rowid = ((GameListItem)toggled).getRowID();
            if ( selected ) {
                m_mySIS.selGames.add( rowid );
                clearSelectedGroups();
            } else {
                m_mySIS.selGames.remove( rowid );
            }
        } else if ( toggled instanceof GameListGroup ) {
            long id = ((GameListGroup)toggled).getGroupID();
            if ( selected ) {
                m_mySIS.selGroupIDs.add( id );
                clearSelectedGames();
            } else {
                m_mySIS.selGroupIDs.remove( id );
            }
        }
        invalidateOptionsMenuIf();
        setTitle();
        // mkListAdapter();
    }

    public boolean getSelected( SelectableItem.LongClickHandler obj )
    {
        boolean selected;
        if ( obj instanceof GameListItem ) {
            long rowid = ((GameListItem)obj).getRowID();
            selected = m_mySIS.selGames.contains( rowid );
        } else if ( obj instanceof GameListGroup ) {
            long groupID = ((GameListGroup)obj).getGroupID();
            selected = m_mySIS.selGroupIDs.contains( groupID );
        } else {
            Assert.failDbg();
            selected = false;
        }
        return selected;
    }

    // Log.ResultProcs interface
    @Override
    public void onDumping( final int nRecords )
    {
        runOnUiThread( new Runnable() {
                @Override
                public void run() {
                    Utils.showToast( m_activity, R.string.logstore_dumping_fmt, nRecords );
                }
            } );
    }

    @Override
    public void onDumped( final File logLoc )
    {
        runOnUiThread( new Runnable() {
                @Override
                public void run() {
                    String dumpMsg;
                    if ( null == logLoc ) {
                        dumpMsg = LocUtils.getString( m_activity,
                                                      R.string.logstore_notdumped );
                    } else {
                        dumpMsg = LocUtils
                            .getString( m_activity, R.string.logstore_dumped_fmt,
                                        logLoc.getPath() );
                    }
                    makeOkOnlyBuilder( dumpMsg )
                        .setParams( logLoc )
                        .setPosButton( android.R.string.cancel )
                        .setActionPair( Action.SEND_LOGS, R.string.button_send_logs )
                        .show();
                }
            } );
    }

    @Override
    public void onCleared( final int nDumped )
    {
        runOnUiThread( new Runnable() {
                @Override
                public void run() {
                    Utils.showToast( m_activity, R.string.logstore_cleared_fmt, nDumped );
                }
            } );
    }

    // DlgDelegate.DlgClickNotify interface
    @Override
    public boolean onPosButton( Action action, Object[] params )
    {
        boolean handled = true;
        switch( action ) {
        case NEW_NET_GAME:
            m_netLaunchInfo = (NetLaunchInfo)params[0];
            if ( checkWarnNoDict( m_netLaunchInfo ) ) {
                makeNewNetGameIf();
            }
            break;
        case RESET_GAMES:
            long[] rowids = (long[])params[0];
            boolean changed = false;
            for ( long rowid : rowids ) {
                changed = GameUtils.resetGame( m_activity, rowid ) || changed;
            }
            if ( changed ) {
                mkListAdapter(); // required because position may change
            }
            break;
        case NEW_FROM:
            long curID = (Long)params[0];
            long newid = GameUtils.dupeGame( m_activity, curID );
            if ( ROWID_NOTFOUND != newid ) {
                m_mySIS.selGames.add( newid );
                reloadGame( newid );
            }
            break;

        case SET_HIDE_NEWGAME_BUTTONS:
            XWPrefs.setHideNewgameButtons( m_activity, true );
            setupButtons();
            break;

        case DELETE_GROUPS:
            long[] groupIDs = (long[])params[0];
            for ( long groupID : groupIDs ) {
                GameUtils.deleteGroup( m_activity, groupID );
            }
            clearSelections();
            mkListAdapter();
            break;
        case DELETE_GAMES:
            deleteGames( (long[])params[0], (Boolean)params[1] );
            break;
        case OPEN_GAME:
            doOpenGame( params );
            break;
        case QUARANTINE_CLEAR:
            long rowid = (long)params[0];
            Quarantine.clear( rowid );
            GameSummary summary = (GameSummary)params[1];
            openWithChecks( rowid, summary );
            break;

        case BACKUP_OVERWRITE:
            ArrayList<SaveWhat> whats = (ArrayList<SaveWhat>)params[0];
            Uri uri = Uri.parse((String)params[1]);
            if ( ZipUtils.load( m_activity, uri, whats ) ) {
                ProcessPhoenix.triggerRebirth( m_activity );
            }
            break;

        case BACKUP_RETRY:
            startFileChooser( null );
            break;

        case QUARANTINE_DELETE:
            deleteIfConfirmed( new long[] {(long)params[0]}, true );
            break;

        case CLEAR_SELS:
            clearSelections();
            break;
        case DWNLD_LOC_DICT:
            String lang = (String)params[0];
            String name = (String)params[1];
            DownloadFinishedListener lstnr = new DownloadFinishedListener() {
                    @Override
                    public void downloadFinished( String lang, String name, boolean success )
                    {
                        if ( success ) {
                            XWPrefs.setPrefsString( m_activity,
                                                    R.string.key_default_language,
                                                    lang );
                            name = DictUtils.removeDictExtn( name );
                            int[] ids = { R.string.key_default_dict,
                                          R.string.key_default_robodict };
                            for ( int id : ids ) {
                                XWPrefs.setPrefsString( m_activity, id, name );
                            }

                            XWPrefs.setPrefsBoolean( m_activity,
                                                     R.string.key_got_langdict,
                                                     true );
                        }
                    }
                };
            DwnldDelegate.downloadDictInBack( m_activity, lang, name, lstnr );
            break;
        case NEW_GAME_DFLT_NAME:
            m_newGameParams = params;
            askDefaultName();
            break;

        case SEND_EMAIL:
            Utils.emailAuthor( m_activity );
            break;

        case WRITE_LOG_DB:
            Log.dumpStored( this );
            break;

        case CLEAR_LOG_DB:
            Log.clearStored( this );
            break;

        case ASKED_PHONE_STATE:
            rematchWithNameAndPerm( true, params );
            break;

        case APPLY_CONFIG:
            Uri data = Uri.parse( (String)params[0] );
            CommonPrefs.loadColorPrefs( m_activity, data );
            break;

        case SEND_LOGS:
            File logLoc = (File)params[0];
            Utils.emailLogFile( m_activity, logLoc );
            break;

        case CLEAR_INT_STATS:
            TimerReceiver.clearStats( m_activity );
            break;

        case OPEN_BYOD_DICT:
            DictBrowseDelegate.launch( getDelegator(), (String)params[0] );
            break;

        default:
            handled = super.onPosButton( action, params );
        }
        return handled;
    }

    private Dialog mkLoadStoreDlg( final Uri uri )
    {
        final BackupConfigView view = (BackupConfigView)
            LocUtils.inflate( m_activity, R.layout.backup_config_view );
        view.init( uri );

        AlertDialog.Builder ab = makeAlertBuilder()
            .setTitle( view.getAlertTitle() )
            .setView( view )
            .setPositiveButton( view.getPosButtonTxt(), new OnClickListener() {
                    @Override
                    public void onClick( DialogInterface dlg, int item ) {
                        if ( null == uri ) { // store case
                            startFileChooser( view.getSaveWhat() );
                        } else {
                            ArrayList<SaveWhat> what = view.getSaveWhat();
                            String name = ZipUtils.getFileName( m_activity, uri );
                            String msg = getString( R.string.backup_overwrite_confirm_fmt,
                                                    name );
                            makeConfirmThenBuilder( msg, Action.BACKUP_OVERWRITE )
                                .setParams(what, uri.toString())
                                .show();
                        }
                    }
                } )
            .setNegativeButton( android.R.string.cancel, null )
            ;
        return view.setDialog( ab.create() );
    }

    // This is in liu of passing through the startActivityForResult call,
    // which apparently isn't supported.
    private List<SaveWhat> mSaveWhat;

    private void startFileChooser( List<SaveWhat> what )
    {
        mSaveWhat = what;       // will be null in load case

        String intentAction = null;
        RequestCode rq = null;
        boolean isStore = null != what;
        if ( isStore ) {
            intentAction = Intent.ACTION_CREATE_DOCUMENT;
            rq = RequestCode.STORE_DATA_FILE;
        } else {
            intentAction = Intent.ACTION_OPEN_DOCUMENT;
            rq = RequestCode.LOAD_DATA_FILE;
        }
        Intent intent = new Intent( intentAction );
        intent.addCategory( Intent.CATEGORY_OPENABLE );
        intent.setType( ZipUtils.getMimeType( isStore ) );
        if ( isStore ) {
            intent.putExtra( Intent.EXTRA_TITLE, ZipUtils.getFileName(m_activity) );
        }
        startActivityForResult( intent, rq );
    }

    @Override
    public boolean onNegButton( Action action, Object[] params )
    {
        boolean handled = true;
        switch ( action ) {
        case NEW_GAME_DFLT_NAME:
            m_newGameParams = params;
            makeThenLaunchOrConfigure();
            break;

        case ASKED_PHONE_STATE:
            rematchWithNameAndPerm( false, params );
            break;

        default:
            handled = super.onNegButton( action, params );
        }
        return handled;
    }

    @Override
    protected void onActivityResult( RequestCode requestCode, int resultCode,
                                     Intent data )
    {
        boolean cancelled = Activity.RESULT_CANCELED == resultCode;
        switch ( requestCode ) {
        case REQUEST_LANG_GL:
            if ( !cancelled ) {
                Log.d( TAG, "lang need met" );
                if ( checkWarnNoDict( m_missingDictRowId ) ) {
                    launchGameIf();
                }
            }
            break;
        case CONFIG_GAME:
            if ( !cancelled ) {
                long rowID = data.getLongExtra( GameUtils.INTENT_KEY_ROWID,
                                                ROWID_NOTFOUND );
                launchGame( rowID );
            }
            break;
        case STORE_DATA_FILE:
        case LOAD_DATA_FILE:
            if ( Activity.RESULT_OK == resultCode && data != null ) {
                Uri uri = data.getData();
                boolean isStore = RequestCode.STORE_DATA_FILE == requestCode;
                if ( isStore ) {
                    boolean saved =
                        ZipUtils.save( m_activity, uri, mSaveWhat );
                    int msgID = saved ? R.string.db_store_done
                        : R.string.db_store_failed;
                    showToast( msgID );
                } else {
                    post( new Runnable() {
                            @Override
                            public void run() {
                                if ( ZipUtils.hasWhats( m_activity, uri ) ) {
                                    String uriStr = uri.toString();
                                    showDialogFragment( DlgID.BACKUP_LOADSTORE, uriStr );
                                } else {
                                    String name = ZipUtils.getFileName( m_activity, uri );
                                    if ( null != name ) {
                                        String msg = getString( R.string.backup_bad_file_fmt,
                                                                name );
                                        makeOkOnlyBuilder( msg )
                                            .setActionPair( Action.BACKUP_RETRY,
                                                            R.string.button_pick_again )
                                            .show();
                                    }
                                }
                            }
                        } );
                }
            }
            break;
        }
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        setupButtons();
    }

    @Override
    protected boolean handleBackPressed()
    {
        boolean handled = 0 < m_mySIS.selGames.size()
            || 0 < m_mySIS.selGroupIDs.size();
        if ( handled ) {
            makeNotAgainBuilder( R.string.not_again_backclears,
                                 R.string.key_notagain_backclears,
                                 Action.CLEAR_SELS )
                .show();
        }
        return handled;
    }

    @Override
    public boolean onPrepareOptionsMenu( Menu menu )
    {
        m_menuPrepared = null != m_mySIS;
        if ( m_menuPrepared ) {
            int nGamesSelected = m_mySIS.selGames.size(); // NPE
            int nGroupsSelected = m_mySIS.selGroupIDs.size();
            int groupCount = m_adapter.getGroupCount();
            m_menuPrepared = 0 == nGamesSelected || 0 == nGroupsSelected;

            if ( m_menuPrepared ) {
                boolean nothingSelected = 0 == (nGroupsSelected + nGamesSelected);

                final boolean showDbg = BuildConfig.NON_RELEASE
                    || XWPrefs.getDebugEnabled( m_activity );
                showItemsIf( DEBUG_ITEMS, menu, nothingSelected && showDbg );
                showItemsIf( NOSEL_ITEMS, menu, nothingSelected );
                showItemsIf( ONEGAME_ITEMS, menu, 1 == nGamesSelected );
                showItemsIf( ONEGROUP_ITEMS, menu, 1 == nGroupsSelected );

                final boolean showDataItems =
                    Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT
                    && nothingSelected;
                for ( int item : new int[] { R.id.games_menu_storedb,
                                             R.id.games_menu_loaddb } ) {
                    Utils.setItemVisible( menu, item, showDataItems );
                }

                boolean enable = showDbg && nothingSelected;
                Utils.setItemVisible( menu, R.id.games_menu_checkupdates, enable );

                int selGroupPos = -1;
                if ( 1 == nGroupsSelected ) {
                    long id = m_mySIS.selGroupIDs.iterator().next();
                    selGroupPos = m_adapter.getGroupPosition( id );
                }

                // You can't delete the default group, nor make it the default.
                // But we enable delete so a warning message later can explain.
                Utils.setItemVisible( menu, R.id.games_group_delete,
                                      1 <= nGroupsSelected );
                enable = (1 == nGroupsSelected) && ! m_mySIS.selGroupIDs
                    .contains( XWPrefs.getDefaultNewGameGroup( m_activity ) );
                Utils.setItemVisible( menu, R.id.games_group_default, enable );

                // Rematch supported if there's one game selected
                enable = 1 == nGamesSelected;
                if ( enable ) {
                    enable = BoardDelegate.rematchSupported( m_activity,
                                                             getSelRowIDs()[0] );
                }
                Utils.setItemVisible( menu, R.id.games_game_rematch, enable );

                // Move up/down enabled for groups if not the top-most or bottommost
                // selected
                enable = 1 == nGroupsSelected;
                enableGroupUpDown( menu, selGroupPos, enable );

                // New game available when nothing selected or one group
                Utils.setItemVisible( menu, R.id.games_menu_newgame_solo,
                                      nothingSelected || 1 == nGroupsSelected );
                Utils.setItemVisible( menu, R.id.games_menu_newgame_net,
                                      nothingSelected || 1 == nGroupsSelected );

                // Multiples can be deleted, but disable if any selected game is
                // currently open
                enable = 0 < nGamesSelected;
                for ( long row : m_mySIS.selGames ) {
                    enable = enable && !BoardDelegate.gameIsOpen( row );
                }
                Utils.setItemVisible( menu, R.id.games_game_delete, enable );
                Utils.setItemVisible( menu, R.id.games_game_reset, enable );

                Utils.setItemVisible( menu, R.id.games_game_hide,
                                      enable && BuildConfig.NON_RELEASE );

                // multiple games can be regrouped/reset.
                Utils.setItemVisible( menu, R.id.games_game_move,
                                      0 < nGamesSelected );

                // Hide rate-me if not a google play app
                enable = nothingSelected && Utils.isGooglePlayApp( m_activity );
                Utils.setItemVisible( menu, R.id.games_menu_rateme, enable );

                enable = nothingSelected && XWPrefs.getStudyEnabled( m_activity );
                Utils.setItemVisible( menu, R.id.games_menu_study, enable );

                enable = nothingSelected && XwJNI.hasKnownPlayers();
                Utils.setItemVisible( menu, R.id.games_menu_knownplyrs, enable );

                enable = nothingSelected &&
                    0 < DBUtils.getGamesWithSendsPending( m_activity ).size();
                Utils.setItemVisible( menu, R.id.games_menu_resend, enable );

                Utils.setItemVisible( menu, R.id.games_menu_timerStats, showDbg );

                enable = Log.getStoreLogs();
                Utils.setItemVisible( menu, R.id.games_menu_enableLogStorage, !enable );
                Utils.setItemVisible( menu, R.id.games_menu_disableLogStorage, enable );
                Utils.setItemVisible( menu, R.id.games_menu_emailLogs, enable );

                Assert.assertTrue( m_menuPrepared );
            }
        }

        if ( !m_menuPrepared ) {
            Log.d( TAG, "onPrepareOptionsMenu: incomplete so bailing" );
        }
        return m_menuPrepared;
    } // onPrepareOptionsMenu

    @Override
    public boolean onOptionsItemSelected( MenuItem item )
    {
        Assert.assertTrue( m_menuPrepared );

        String msg;
        int itemID = item.getItemId();
        boolean handled = true;
        int groupPos = getSelGroupPos();
        long groupID = DBUtils.GROUPID_UNSPEC;
        if ( 0 <= groupPos ) {
            groupID = m_adapter.getGroupIDFor( groupPos );
        }
        final long[] selRowIDs = getSelRowIDs();

        if ( 1 == selRowIDs.length
             && R.id.games_game_delete != itemID
             && R.id.games_game_move != itemID
             && !checkWarnNoDict( selRowIDs[0], itemID ) ) {
            return true;        // FIXME: RETURN FROM MIDDLE!!!
        }

        switch ( itemID ) {
            // There's no selection for these items, so nothing to clear

        case R.id.games_menu_resend:
            GameUtils.resendAllIf( m_activity, null, true, true );
            break;
        case R.id.games_menu_newgame_solo:
            handleNewGameButton( true );
            break;
        case R.id.games_menu_newgame_net:
            handleNewGameButton( false );
            break;

        case R.id.games_menu_newgroup:
            m_mySIS.moveAfterNewGroup = null;
            showDialogFragment( DlgID.NEW_GROUP );
            break;

        case R.id.games_menu_dicts:
            DictsDelegate.start( getDelegator() );
            break;

        case R.id.games_menu_checkupdates:
            UpdateCheckReceiver.checkVersions( m_activity, true );
            break;

        case R.id.games_menu_prefs:
            PrefsDelegate.launch( m_activity );
            break;

        case R.id.games_menu_rateme:
            String str = String.format( "market://details?id=%s",
                                        BuildConfig.APPLICATION_ID );
            try {
                startActivity( new Intent( Intent.ACTION_VIEW, Uri.parse( str ) ) );
            } catch ( android.content.ActivityNotFoundException anf ) {
                makeOkOnlyBuilder( R.string.no_market ).show();
            }
            break;

        case R.id.games_menu_study:
            StudyListDelegate.launchOrAlert( getDelegator(), this );
            break;

        case R.id.games_menu_knownplyrs:
            KnownPlayersDelegate.launchOrAlert( getDelegator(), this );
            break;

        case R.id.games_menu_about:
            show( AboutAlert.newInstance() );
            break;

        case R.id.games_menu_email:
            Utils.emailAuthor( m_activity );
            break;

            // Load and store both use the same dialog, and both use the
            // ContentResolver/startActivityForResult grossness, but in
            // different orders. Store needs to know *what* to store before
            // asking where, but load needs to know what's being loaded before
            // asking what subset of that the user wants to use. So we start
            // with the choose-what alert in the store case, and with the
            // choose-where (OS) grossness in the load case
        case R.id.games_menu_storedb:
            showDialogFragment( DlgID.BACKUP_LOADSTORE );
            break;
        case R.id.games_menu_loaddb:
            startFileChooser( null );
            break;

        case R.id.games_menu_writegit:
            Utils.gitInfoToClip( m_activity );
            break;

        case R.id.games_menu_timerStats:
            makeOkOnlyBuilder( TimerReceiver.statsStr( m_activity ) )
                .setActionPair( Action.CLEAR_INT_STATS, R.string.button_clear_stats )
                .show();
            break;

        case R.id.games_menu_enableLogStorage:
            Log.setStoreLogs( true );
            break;
        case R.id.games_menu_disableLogStorage:
            Log.setStoreLogs( false );
            break;
        case R.id.games_menu_clearLogStorage:
            makeConfirmThenBuilder( R.string.logstore_clear_confirm,
                                    Action.CLEAR_LOG_DB )
                .setPosButton( R.string.loc_item_clear )
                .show();
            break;
        case R.id.games_menu_emailLogs:
            Perms23.tryGetPerms( this, Perm.STORAGE, null,
                                 Action.WRITE_LOG_DB );
            break;

        default:
            handled = handleSelGamesItem( itemID, selRowIDs )
                || handleSelGroupsItem( itemID, getSelGroupIDs() );
        }

        return handled;// || super.onOptionsItemSelected( item );
    }

    @Override
    public void onCreateContextMenu( ContextMenu menu, View view,
                                     ContextMenuInfo menuInfo )
    {
        boolean enable;
        super.onCreateContextMenu( menu, view, menuInfo );

        int id = 0;
        boolean selected = false;
        GameListItem gameItem = null;
        int selGroupPos = -1;
        AdapterView.AdapterContextMenuInfo info
            = (AdapterView.AdapterContextMenuInfo)menuInfo;
        View targetView = info.targetView;
        Log.d( TAG, "onCreateContextMenu(t=%s)",
               targetView.getClass().getSimpleName() );
        if ( targetView instanceof GameListItem ) {
            gameItem = (GameListItem)targetView;
            id = R.menu.games_list_game_menu;

            selected = m_mySIS.selGames.contains( gameItem.getRowID() );
        } else if ( targetView instanceof GameListGroup ) {
            id = R.menu.games_list_group_menu;

            long groupID = ((GameListGroup)targetView).getGroupID();
            selected = m_mySIS.selGroupIDs.contains( groupID );
            selGroupPos = m_adapter.getGroupPosition( groupID );
        } else {
            Assert.failDbg();
        }

        if ( 0 != id ) {
            m_activity.getMenuInflater().inflate( id, menu );

            int hideId = selected
                ? R.id.games_game_select : R.id.games_game_deselect;
            Utils.setItemVisible( menu, hideId, false );

            if ( null != gameItem ) {
                long rowID = gameItem.getRowID();
                enable = BoardDelegate.rematchSupported( m_activity, rowID );
                Utils.setItemVisible( menu, R.id.games_game_rematch, enable );

                // Deal with possibility summary's temporarily null....
                GameSummary summary = gameItem.getSummary();
                enable = false;
                boolean isMultiGame = false;
                if ( null != summary ) {
                    isMultiGame = summary.isMultiGame();
                    enable = isMultiGame
                        && (BuildConfig.DEBUG || XWPrefs.getDebugEnabled( m_activity ));
                }
                Utils.setItemVisible( menu, R.id.games_game_invites, enable );
                Utils.setItemVisible( menu, R.id.games_game_netstats, isMultiGame );
                Utils.setItemVisible( menu, R.id.games_game_relaypage,
                                      BuildConfig.NON_RELEASE && isMultiGame );

                enable = BuildConfig.DEBUG || XWPrefs.getDebugEnabled( m_activity );
                Utils.setItemVisible( menu, R.id.games_game_markbad, enable );

                enable = !BoardDelegate.gameIsOpen( rowID );
                Utils.setItemVisible( menu, R.id.games_game_delete, enable );
                Utils.setItemVisible( menu, R.id.games_game_reset, enable );
            } else {
                enableGroupUpDown( menu, selGroupPos, true );
            }
        }
    } // onCreateContextMenu

    @Override
    public boolean onContextItemSelected( MenuItem item )
    {
        boolean handled = true;
        AdapterView.AdapterContextMenuInfo info
            = (AdapterView.AdapterContextMenuInfo)item.getMenuInfo();
        View targetView = info.targetView;

        int itemID = item.getItemId();
        if ( ! handleToggleItem( itemID, targetView ) ) {
            long[] selIds = new long[1];
            if ( targetView instanceof GameListItem ) {
                selIds[0] = ((GameListItem)targetView).getRowID();
                handled = handleSelGamesItem( itemID, selIds );
            } else if ( targetView instanceof GameListGroup ) {
                selIds[0] = ((GameListGroup)targetView).getGroupID();
                handled = handleSelGroupsItem( itemID, selIds );
            } else {
                Assert.failDbg();
            }
        }

        return handled || super.onContextItemSelected( item );
    }

    //////////////////////////////////////////////////////////////////////
    // DwnldActivity.DownloadFinishedListener interface
    //////////////////////////////////////////////////////////////////////
    @Override
    public void downloadFinished( String lang, String name,
                                  final boolean success )
    {
        runWhenActive( new Runnable() {
                @Override
                public void run() {
                    boolean madeGame = false;
                    if ( success ) {
                        madeGame = makeNewNetGameIf() || launchGameIf();
                    }
                    if ( ! madeGame ) {
                        int id = success ? R.string.download_done
                            : R.string.download_failed;
                        showToast( id );
                    }
                }
            } );
    }

    //////////////////////////////////////////////////////////////////////
    // GroupStateListener interface
    //////////////////////////////////////////////////////////////////////
    public void onGroupExpandedChanged( Object obj, boolean expanded )
    {
        GameListGroup glg = (GameListGroup)obj;
        long groupID = glg.getGroupID();
        // DbgUtils.logf( "onGroupExpandedChanged(expanded=%b); groupID = %d",
        //                expanded , groupID );
        DBUtils.setGroupExpanded( m_activity, groupID, expanded );

        m_adapter.setExpanded( groupID, expanded );

        // Deselect any games that are being hidden.
        if ( !expanded ) {
            long[] rows = DBUtils.getGroupGames( m_activity, groupID );
            for ( long row : rows ) {
                m_mySIS.selGames.remove( row );
            }
            invalidateOptionsMenuIf();
            setTitle();
        }
    }

    private void enableGroupUpDown( Menu menu, int selGroupPos, final boolean enable )
    {
        Utils.setItemVisible( menu, R.id.games_group_moveup,
                              enable && 0 < selGroupPos );
        Utils.setItemVisible( menu, R.id.games_group_movedown,
                              enable && (selGroupPos + 1) < m_adapter.getGroupCount() );
    }

    private static final String GROUP_POSNS_KEY = TAG + "/group_posns";
    private void storeGroupPositions( long[] posns )
    {
        // Log.d( TAG, "storeGroupPositions(%s)", DbgUtils.toString(posns) );
        DBUtils.setSerializableFor( m_activity, GROUP_POSNS_KEY, posns );
    }

    private long[] loadGroupPositions()
    {
        long[] result = {};
        Serializable obj = DBUtils.getSerializableFor( m_activity, GROUP_POSNS_KEY );
        if ( null != obj && obj instanceof long[] ) {
            result = (long[])obj;
        }
        // Log.d( TAG, "loadGroupPositions() => %s", DbgUtils.toString(result) );
        return result;
    }

    private void reloadGame( long rowID )
    {
        if ( null != m_adapter ) {
            m_adapter.reloadGame( rowID );
        }
    }

    private boolean handleToggleItem( int itemID, View target )
    {
        boolean handled;
        switch( itemID ) {
        case R.id.games_game_select:
        case R.id.games_game_deselect:
            SelectableItem.LongClickHandler toggled
                = (SelectableItem.LongClickHandler)target;
            toggled.longClicked();
            handled = true;
            break;
        default:
            handled = false;
        }

        return handled;
    }

    private boolean handleSelGamesItem( int itemID, long[] selRowIDs )
    {
        boolean handled = true;
        boolean dropSels = false;

        switch( itemID ) {
        case R.id.games_game_hide:
            DBUtils.hideGames( m_activity, selRowIDs[0] );
            break;

        case R.id.games_game_delete:
            deleteIfConfirmed( selRowIDs, false );
            break;

        case R.id.games_game_rematch:
            Assert.assertTrue( 1 == selRowIDs.length );
            BoardDelegate.setupRematchFor( m_activity, selRowIDs[0] );
            break;

        case R.id.games_game_config:
            GameConfigDelegate.editForResult( getDelegator(),
                                              RequestCode.CONFIG_GAME,
                                              selRowIDs[0], false );
            break;

        case R.id.games_game_move:
            m_mySIS.groupSelItem = -1;
            showDialogFragment( DlgID.CHANGE_GROUP, selRowIDs );
            break;
        case R.id.games_game_new_from:
            dropSels = true;    // will select the new game instead
            makeNotAgainBuilder( R.string.not_again_newfrom,
                                 R.string.key_notagain_newfrom,
                                 Action.NEW_FROM )
                .setParams(selRowIDs[0])
                .show();
            break;
        case R.id.games_game_copy:
            final long selRowID = selRowIDs[0];
            final GameSummary smry = GameUtils.getSummary( m_activity,
                                                           selRowID );
            if ( smry.inRelayGame() ) {
                makeOkOnlyBuilder( R.string.no_copy_network ).show();
            } else {
                dropSels = true;    // will select the new game instead
                post( new Runnable() {
                        @Override
                        public void run() {
                            Activity self = m_activity;
                            byte[] stream =
                                GameUtils.savedGame( self, selRowID );
                            long groupID = XWPrefs
                                .getDefaultNewGameGroup( self );
                            try ( GameLock lock =
                                  GameUtils.saveNewGame( self, stream, groupID ) ) {
                                DBUtils.saveSummary( self, lock, smry );
                                m_mySIS.selGames.add( lock.getRowid() );
                            }
                            mkListAdapter();
                        }
                    });
            }
            break;

        case R.id.games_game_reset:
            doConfirmReset( selRowIDs );
            break;

        case R.id.games_game_rename:
            showDialogFragment( DlgID.RENAME_GAME, selRowIDs[0] );
            break;

        case R.id.games_game_netstats:
            onStatusClicked( selRowIDs[0] );
            break;

        case R.id.games_game_relaypage:
            GameSummary summary = GameUtils.getSummary( m_activity, selRowIDs[0] );
            NetUtils.copyAndLaunchGamePage( m_activity, summary.gameID );
            break;

            // DEBUG only
        case R.id.games_game_invites:
            String msg = GameUtils.getSummary( m_activity, selRowIDs[0] )
                .conTypes.toString( m_activity, true );
            msg = getString( R.string.invites_net_fmt, msg );

            SentInvitesInfo info = DBUtils.getInvitesFor( m_activity,
                                                          selRowIDs[0] );
            if ( null != info ) {
                msg += "\n\n" + info.getAsText( m_activity );
            }
            makeOkOnlyBuilder( msg ).show();
            break;

            // DEBUG only
        case R.id.games_game_markbad:
            Quarantine.markBad( selRowIDs[0] );
            break;

        default:
            handled = false;
        }

        if ( dropSels ) {
            clearSelections();
        }

        return handled;
    }

    private boolean handleSelGroupsItem( int itemID, long[] groupIDs )
    {
        boolean handled = 0 < groupIDs.length;
        if ( handled ) {
            String msg;
            long groupID = groupIDs[0];
            switch( itemID ) {
            case R.id.games_group_delete:
                long dftGroup = XWPrefs.getDefaultNewGameGroup( m_activity );
                if ( groupID == dftGroup ) {
                    msg = getString( R.string.cannot_delete_default_group_fmt,
                                     m_adapter.groupName( dftGroup ) );
                    makeOkOnlyBuilder( msg ).show();
                } else {
                    Assert.assertTrue( 0 < groupIDs.length );
                    String names = m_adapter.formatGroupNames( groupIDs );
                    msg = getQuantityString( R.plurals.groups_confirm_del_fmt,
                                             groupIDs.length, names );

                    int nGames = 0;
                    for ( long tmp : groupIDs ) {
                        nGames += m_adapter.getChildrenCount( tmp );
                    }
                    if ( 0 < nGames ) {
                        msg += getQuantityString( R.plurals.groups_confirm_del_games_fmt,
                                                  nGames, nGames );
                    }
                    makeConfirmThenBuilder( msg, Action.DELETE_GROUPS )
                        .setParams( groupIDs )
                        .show();
                }
                break;
            case R.id.games_group_default:
                XWPrefs.setDefaultNewGameGroup( m_activity, groupID );
                break;
            case R.id.games_group_rename:
                showDialogFragment( DlgID.RENAME_GROUP, groupID );
                break;
            case R.id.games_group_moveup:
                moveGroup( groupID, true );
                break;
            case R.id.games_group_movedown:
                moveGroup( groupID, false );
                break;
            default:
                handled = false;
            }
        }
        return handled;
    }

    private void setupButtons()
    {
        boolean hidden = XWPrefs.getHideNewgameButtons( m_activity );
        boolean[] isSolos = { true, false };
        for ( int ii = 0; ii < m_newGameButtons.length; ++ii ) {
            Button button = m_newGameButtons[ii];
            if ( hidden ) {
                button.setVisibility( View.GONE );
            } else {
                button.setVisibility( View.VISIBLE );
                final boolean solo = isSolos[ii];
                button.setOnClickListener( new View.OnClickListener() {
                        public void onClick( View view ) {
                            curThis().handleNewGameButton( solo );
                        }
                    } );
            }
        }
    }

    private void handleNewGameButton( boolean solo )
    {
        m_mySIS.nextIsSolo = solo;
        showDialogFragment( DlgID.GAMES_LIST_NEWGAME, solo );
    }

    @Override
    protected void setTitle()
    {
        int fmt = 0;
        int nSels = m_mySIS.selGames.size();
        if ( 0 < nSels ) {
            fmt = R.plurals.sel_games_fmt;
        } else {
            nSels = m_mySIS.selGroupIDs.size();
            if ( 0 < nSels ) {
                fmt = R.plurals.sel_groups_fmt;
            }
        }

        setTitle( 0 == fmt ? m_origTitle : getQuantityString( fmt, nSels, nSels ) );
    }

    private boolean checkWarnNoDict( NetLaunchInfo nli )
    {
        // check that we have the dict required
        boolean haveDict;
        if ( null == nli.dict ) { // can only test for language support

            String[] dicts = DictLangCache.getHaveLang( m_activity, nli.isoCode );
            haveDict = 0 < dicts.length;
            if ( haveDict ) {
                // Just pick one -- good enough for the period when
                // users aren't using new clients that include the
                // dict name.
                nli.dict = dicts[0];
            }
        } else {
            haveDict =
                DictLangCache.haveDict( m_activity, nli.isoCode, nli.dict );
        }
        if ( !haveDict ) {
            m_netLaunchInfo = nli;
            showDialogFragment( DlgID.WARN_NODICT_INVITED, 0L, nli.dict, nli.isoCode );
        }
        return haveDict;
    }

    private boolean checkWarnNoDict( long rowid )
    {
        return checkWarnNoDict( rowid, -1 );
    }

    private boolean checkWarnNoDict( long rowid, int forMenu )
    {
        String[][] missingNames = new String[1][];
        String[] missingLang = { "en" };
        boolean hasDicts;
        try {
            hasDicts = GameUtils.gameDictsHere( m_activity, rowid, missingNames,
                                                missingLang );
        } catch ( GameLock.GameLockedException | GameUtils.NoSuchGameException ex ) {
            hasDicts = true;    // irrelevant question
        }

        if ( !hasDicts ) {
            String missingDictName = null;
            String missingDictLang = missingLang[0];
            if ( 0 < missingNames[0].length ) {
                missingDictName = missingNames[0][0];
            }
            m_missingDictRowId = rowid;
            m_missingDictMenuId = forMenu;
            if ( 0 == DictLangCache.getLangCount( m_activity, missingDictLang ) ) {
                showDialogFragment( DlgID.WARN_NODICT_GENERIC, rowid,
                                    missingDictName, missingDictLang );
            } else if ( null != missingDictName ) {
                showDialogFragment( DlgID.WARN_NODICT_SUBST, rowid, missingDictName,
                                    missingDictLang );
            } else {
                String dict =
                    DictLangCache.getHaveLang( m_activity, missingDictLang)[0];
                if ( GameUtils.replaceDicts( m_activity, rowid, null, dict ) ) {
                    launchGameIf();
                }
            }
        }
        return hasDicts;
    }

    private boolean startFirstHasDict( final long rowid, final Bundle extras )
    {
        Assert.assertTrueNR( ROWID_NOTFOUND != rowid );
        boolean handled = DBUtils.haveGame( m_activity, rowid );
        if ( handled ) {
            GameLock.getLockThen( rowid, 100L, m_handler,
                                  new GameLock.GotLockProc() {
                                      @Override
                                      public void gotLock( GameLock lock ) {
                                          Log.d( TAG, "startFirstHasDict.gotLock(%s)", lock );
                                          if ( lock != null ) {
                                              boolean haveDict = GameUtils
                                                  .gameDictsHere( m_activity, lock );
                                              lock.release();
                                              if ( haveDict ) {
                                                  launchGame( rowid, extras );
                                              }
                                          }
                                      }
                                  } );
        }
        Log.d( TAG, "startFirstHasDict(rowid=%d) => %b", rowid, handled );
        return handled;
    }

    private boolean startFirstHasDict( Intent intent )
    {
        boolean result = false;
        if ( null != intent ) {
            long rowid = intent.getLongExtra( ROWID_EXTRA, ROWID_NOTFOUND );
            if ( ROWID_NOTFOUND != rowid ) {
                result = startFirstHasDict( rowid, intent.getExtras() );
            }
        }
        return result;
    }

    private boolean startWithInvitee( Intent intent )
    {
        boolean result = false;
        try {
            CommsAddrRec addr = (CommsAddrRec)intent.getSerializableExtra( INVITEE_REC_EXTRA );
            if ( null != addr ) {
                String name = intent.getStringExtra( REMATCH_NEWNAME_EXTRA );
                makeThenLaunchOrConfigure( name, false, false, addr );
            }
        } catch ( Exception ex ) {
            Log.ex( TAG, ex );
        }

        return result;
    }

    private boolean postWordlistURL( Intent intent )
    {
        Uri data = intent.getData();
        boolean result = null != data;
        if ( result ) {
            // dfl: In case it's a BYOD download
            DwnldDelegate.DownloadFinishedListener dfl =
                new DwnldDelegate.DownloadFinishedListener() {
                    @Override
                    public void downloadFinished( String ignore,
                                                  String name,
                                                  boolean success ) {
                        int resid = success ? R.string.byod_success
                            : R.string.byod_failure;
                        DlgDelegate.Builder builder =
                            makeOkOnlyBuilder( getString( resid, name ) );
                        if ( success ) {
                            builder.setActionPair( Action.OPEN_BYOD_DICT,
                                                   R.string.button_open_dict )
                                .setParams( name );
                        }
                        builder.show();
                    }
                };
            result = UpdateCheckReceiver.postedForDictDownload( m_activity,
                                                                data, dfl );
        }
        return result;
    }

    private boolean startNewNetGame( NetLaunchInfo nli )
    {
        boolean handled = false;
        Assert.assertTrue( nli.isValid() );

        Map<Long, Integer> rowids = DBUtils
            .getRowIDsAndChannels( m_activity, nli.gameID() );

        if ( 0 < rowids.size() ) {
            // There's already a game? Better not have same channel as invite
            // creates
            for ( long rowid : rowids.keySet() ) {
                if ( nli.forceChannel == rowids.get(rowid) ) {
                    DbgUtils.printStack( TAG );
                    if ( BuildConfig.NON_RELEASE ) {
                        Utils.showToast( m_activity, R.string.dropped_dupe );
                    }

                    final Object[] params = { rowid, null };
                    post( new Runnable() {
                            @Override
                            public void run() {
                                doOpenGame( params );
                            }
                        } );
                    handled = true;
                    break;
                }
            }
        }

        if ( !handled && checkWarnNoDict( nli ) ) {
            makeNewNetGame( nli );
            handled = true;
        }

        return handled;
    } // startNewNetGame

    private boolean startNewNetGame( Intent intent )
    {
        boolean handled = false;
        NetLaunchInfo nli = null;
        if ( MultiService.isMissingDictIntent( intent ) ) {
            nli = MultiService.getMissingDictData( m_activity, intent );
        } else {
            Uri data = intent.getData();
            if ( null != data ) {
                nli = new NetLaunchInfo( m_activity, data );
            }
        }
        if ( null != nli && nli.isValid() ) {
            handled = startNewNetGame( nli );
        }
        return handled;
    } // startNewNetGame

    private boolean loadConfig( Intent intent )
    {
        boolean success = false;
        Uri data = intent.getData();
        if ( null != data ) {
            try {
                String path = data.getPath();
                String prefix = LocUtils.getString( m_activity, R.string.conf_prefix );
                Log.d( TAG, "loadConfig(): path: %s; prefix: %s", path, prefix );
                if ( path.startsWith( prefix ) ) {
                    makeConfirmThenBuilder( R.string.apply_config, Action.APPLY_CONFIG )
                        .setPosButton( R.string.button_apply_config )
                        .setNegButton( android.R.string.cancel )
                        .setParams( data.toString() )
                        .show();
                    success = true;
                }
            } catch ( Exception ex ) {
                Log.ex( TAG, ex );
            }
        }

        return success;
    }

    private boolean startHasGameID( int gameID )
    {
        boolean handled = false;
        long[] rowids = DBUtils.getRowIDsFor( m_activity, gameID );
        if ( 0 < rowids.length ) {
            long rowid = rowids[0];
            if ( checkWarnNoDict( rowid ) ) {
                launchGame( rowid );
            }
            handled = true;
        }
        return handled;
    }

    private boolean startHasGameID( Intent intent )
    {
        boolean handled = false;
        int gameID = intent.getIntExtra( GAMEID_EXTRA, 0 );
        if ( 0 != gameID ) {
            handled = startHasGameID( gameID );
        }
        return handled;
    }

    // Create a new game that's a copy, sending invitations via the means it
    // used to connect.
    private boolean startRematch( Intent intent )
    {
        boolean handled = false;
        if ( -1 != intent.getLongExtra( REMATCH_ROWID_EXTRA, -1 ) ) {
            m_rematchExtras = intent.getExtras();
            showDialogFragment( DlgID.GAMES_LIST_NAME_REMATCH );
            handled = true;
        }
        return handled;
    }

    private void startRematchWithName( final String gameName,
                                       boolean showRationale )
    {
        if ( null != gameName && 0 < gameName.length() ) {
            Bundle extras = m_rematchExtras;
            // should default be 0 not -1, which is all bits set ?? PENDING
            int bits = extras.getInt( REMATCH_ADDRS_EXTRA, -1 );
            final CommsConnTypeSet addrs = new CommsConnTypeSet( bits );
            boolean hasSMS = addrs.contains( CommsConnType.COMMS_CONN_SMS );
            if ( !hasSMS || null != SMSPhoneInfo.get( m_activity ) ) {
                rematchWithNameAndPerm( gameName, addrs );
            } else {
                int id = (1 == addrs.size())
                    ? R.string.phone_lookup_rationale_drop
                    : R.string.phone_lookup_rationale_others;
                String msg = getString( R.string.phone_lookup_rationale )
                    + "\n\n" + getString( id );
                Perms23.tryGetPerms( this, Perm.READ_PHONE_STATE, msg,
                                     Action.ASKED_PHONE_STATE, gameName, addrs );
            }
        }
    }

    private void rematchWithNameAndPerm( boolean granted, Object[] params )
    {
        CommsConnTypeSet addrs = (CommsConnTypeSet)params[1];
        if ( !granted ) {
            addrs.remove( CommsConnType.COMMS_CONN_SMS );
            m_rematchExtras.remove( REMATCH_PHONE_EXTRA );
        }
        if ( 0 < addrs.size() ) {
            rematchWithNameAndPerm( (String)params[0], addrs );
        }
    }

    private void rematchWithNameAndPerm( String gameName, CommsConnTypeSet addrs )
    {
        if ( null != gameName && 0 < gameName.length() ) {
            Bundle extras = m_rematchExtras;
            long srcRowID = extras.getLong( REMATCH_ROWID_EXTRA,
                                            ROWID_NOTFOUND );
            long groupID = extras.getLong( REMATCH_GROUPID_EXTRA,
                                           DBUtils.GROUPID_UNSPEC );
            if ( DBUtils.GROUPID_UNSPEC == groupID ) {
                groupID = DBUtils.getGroupForGame( m_activity, srcRowID );
            }
            // Don't save rematch in Archive group
            if ( groupID == DBUtils.getArchiveGroup( m_activity ) ) {
                groupID = XWPrefs.getDefaultNewGameGroup( m_activity );
            }
            boolean solo = extras.getBoolean( REMATCH_IS_SOLO, true );

            long newid;
            if ( solo ) {
                newid = GameUtils.dupeGame( m_activity, srcRowID, groupID );
                if ( ROWID_NOTFOUND != newid ) {
                    DBUtils.setName( m_activity, newid, gameName );
                }
            } else {
                String btAddr = extras.getString( REMATCH_BTADDR_EXTRA );
                String phone = extras.getString( REMATCH_PHONE_EXTRA );
                String p2pMacAddress = extras.getString( REMATCH_P2PADDR_EXTRA );
                String dict = extras.getString( REMATCH_DICT_EXTRA );
                String isoCode = extras.getString( REMATCH_LANG_EXTRA );
                String mqttDevID = extras.getString( GameSummary.EXTRA_REMATCH_MQTT );
                String json = extras.getString( REMATCH_PREFS_EXTRA );

                newid = GameUtils.makeNewMultiGame( m_activity, groupID, dict,
                                                    isoCode, json, addrs, gameName );
                DBUtils.addRematchInfo( m_activity, newid, btAddr, phone,
                                        p2pMacAddress, mqttDevID );
            }
            launchGame( newid );
        }
        m_rematchExtras = null;
    }

    private boolean tryAlert( Intent intent )
    {
        boolean handled = false;
        String msg = intent.getStringExtra( ALERT_MSG );
        if ( null != msg ) {
            DlgDelegate.Builder builder =
                makeOkOnlyBuilder( msg );
            if ( intent.getBooleanExtra( WITH_EMAIL, false ) ) {
                builder.setActionPair( Action.SEND_EMAIL,
                                       R.string.board_menu_file_email );
            }
            builder.show();
            handled = true;
        }
        return handled;
    }

    private boolean tryInviteIntent( Intent intent )
    {
        boolean result = false;
        byte[] data = getFromIntent( intent );
        if ( null != data ) {
            NetLaunchInfo nli = NetLaunchInfo.makeFrom( m_activity, data );
            if ( null != nli && nli.isValid() ) {
                startNewNetGame( nli );
                result = true;
            } else {
                Assert.failDbg();
            }
        }
        return result;
    }

    private void askDefaultName()
    {
        String name = CommonPrefs.getDefaultPlayerName( m_activity, 0, true );
        CommonPrefs.setDefaultPlayerName( m_activity, name );
        showDialogFragment( DlgID.GET_NAME );
    }

    private void getDictForLangIf()
    {
        if ( ! m_haveShownGetDict &&
             ! XWPrefs.getPrefsBoolean( m_activity, R.string.key_got_langdict,
                                        false ) ) {
            m_haveShownGetDict = true;

            String isoCode = LocUtils.getCurLangCode( m_activity );
            if ( !isoCode.equals("en") ) {
                String[] names = DictLangCache.getHaveLang( m_activity, isoCode );
                if ( 0 == names.length ) {

                    OnGotLcDictListener lstnr = new OnGotLcDictListener() {
                            @Override
                            public void gotDictInfo( boolean success, String lang,
                                                     String name ) {
                                stopProgress();
                                if ( success ) {
                                    String msg =
                                        getString( R.string.confirm_get_locdict_fmt,
                                                   xlateLang( lang ) );
                                    makeConfirmThenBuilder( msg, Action.DWNLD_LOC_DICT )
                                        .setPosButton( R.string.button_download )
                                        .setNegButton( R.string.button_no )
                                        .setNAKey( R.string.key_got_langdict )
                                        .setParams( lang, name )
                                        .show();
                                }
                            }
                        };

                    String langName = DictLangCache.getLangNameForISOCode( m_activity, isoCode );
                    String locLang = xlateLang( langName );
                    String msg = getString( R.string.checking_for_fmt, locLang );
                    startProgress( R.string.checking_title, msg );
                    DictsDelegate.downloadDefaultDict( m_activity, isoCode, lstnr );
                }
            }
        }
    }

    private void updateField()
    {
        int newField = CommonPrefs.getSummaryFieldId( m_activity );
        if ( m_adapter.setField( newField ) ) {
            // The adapter should be able to decide whether full
            // content change is required.  PENDING
            mkListAdapter();
        }
    }

    private Renamer buildRenamer( String name, int labelID )
    {
        Renamer renamer = ((Renamer)inflate( R.layout.renamer ))
            .setName( name )
            .setLabel( labelID )
            ;
        return renamer;
    }

    private void showNewGroupIf()
    {
        long[] games = m_mySIS.moveAfterNewGroup;
        if ( null != games ) {
            m_mySIS.moveAfterNewGroup = null;
            showDialogFragment( DlgID.CHANGE_GROUP, games );
        }
    }

    private void mkDeleteAlert( String msg, long[] rowids, boolean skipTell )
    {
        makeConfirmThenBuilder( msg, Action.DELETE_GAMES )
            .setPosButton( R.string.button_delete )
            .setParams( rowids, skipTell )
            .show();
    }

    private void deleteIfConfirmed( long[] rowids, boolean skipTell )
    {
        String msg = getQuantityString( R.plurals.confirm_seldeletes_fmt,
                                        rowids.length, rowids.length );
        mkDeleteAlert( msg, rowids, skipTell );
    }

    private void deleteNamedIfConfirmed( long[] rowids, boolean skipTell )
    {
        String[] names = new String[rowids.length];
        for ( int ii = 0; ii < rowids.length; ++ii ) {
            names[ii] = DBUtils.getName( m_activity, rowids[ii] );
        }
        String namesStr = TextUtils.join( ", ", names );

        String msg = getQuantityString( R.plurals.confirm_nameddeletes_fmt,
                                        rowids.length, namesStr );
        mkDeleteAlert( msg, rowids, skipTell );
    }

    private void deleteGames( long[] rowids, boolean skipTell )
    {
        for ( long rowid : rowids ) {
            GameUtils.deleteGame( m_activity, rowid, false, skipTell );
            m_mySIS.selGames.remove( rowid );
        }
        invalidateOptionsMenuIf();
        setTitle();
    }

    private boolean makeNewNetGameIf()
    {
        boolean madeGame = null != m_netLaunchInfo;
        if ( madeGame ) {
            makeNewNetGame( m_netLaunchInfo );
            m_netLaunchInfo = null;
        }
        return madeGame;
    }

    private void setSelGame( long rowid )
    {
        clearSelections( false );

        m_mySIS.selGames.add( rowid );
        m_adapter.setSelected( rowid, true );

        invalidateOptionsMenuIf();
        setTitle();
    }

    private void clearSelections()
    {
        clearSelections( true );
    }

    private void clearSelections( boolean updateStuff )
    {
        boolean inval = clearSelectedGames();
        inval = clearSelectedGroups() || inval;
        if ( updateStuff && inval ) {
            invalidateOptionsMenuIf();
            setTitle();
        }
    }

    private boolean clearSelectedGames()
    {
        // clear any selection
        boolean needsClear = 0 < m_mySIS.selGames.size();
        if ( needsClear ) {
            // long[] rowIDs = getSelRowIDs();
            Set<Long> selGames = new HashSet<>( m_mySIS.selGames );
            m_mySIS.selGames.clear();
            m_adapter.clearSelectedGames( selGames );
        }
        return needsClear;
    }

    private boolean clearSelectedGroups()
    {
        // clear any selection
        boolean needsClear = 0 < m_mySIS.selGroupIDs.size();
        if ( needsClear ) {
            m_adapter.clearSelectedGroups( m_mySIS.selGroupIDs );
            m_mySIS.selGroupIDs.clear();
        }
        return needsClear;
    }

    private boolean launchGameIf()
    {
        boolean madeGame = ROWID_NOTFOUND != m_missingDictRowId;
        if ( madeGame ) {
            // save in case checkWarnNoDict needs to set them
            long rowID = m_missingDictRowId;
            int menuID = m_missingDictMenuId;
            m_missingDictRowId = ROWID_NOTFOUND;
            m_missingDictMenuId = -1;

            if ( R.id.games_game_reset == menuID ) {
                long[] rowIDs = { rowID };
                doConfirmReset( rowIDs );
            } else if ( checkWarnNoDict( rowID ) ) {
                GameUtils.launchGame( getDelegator(), rowID );
            }
        }
        return madeGame;
    }

    private void launchGame( long rowid, Bundle extras )
    {
        if ( ROWID_NOTFOUND == rowid ) {
            Log.d( TAG, "launchGame(): dropping bad rowid" );
        } else if ( ! BoardDelegate.gameIsOpen( rowid ) ) {
            if ( m_adapter.inExpandedGroup( rowid ) ) {
                setSelGame( rowid );
            }
            GameUtils.launchGame( getDelegator(), rowid, extras );
        }
    }

    private void launchGame( long rowid )
    {
        launchGame( rowid, null );
    }

    private void makeNewNetGame( NetLaunchInfo nli )
    {
        long rowid = ROWID_NOTFOUND;
        rowid = GameUtils.makeNewMultiGame( m_activity, nli );
        launchGame( rowid, null );
    }

    private void tryStartsFromIntent( Intent intent )
    {
        Log.d( TAG, "tryStartsFromIntent(extras={%s})", DbgUtils.extrasToString( intent ) );
        boolean handled = startFirstHasDict( intent )
            || startWithInvitee( intent )
            || postWordlistURL( intent )
            || loadConfig( intent )
            || startNewNetGame( intent )
            || startHasGameID( intent )
            || startRematch( intent )
            || tryAlert( intent )
            || tryInviteIntent( intent )
            ;
        Log.d( TAG, "tryStartsFromIntent() => handled: %b", handled );
    }

    private void doOpenGame( Object[] params )
    {
        final long rowid = (Long)params[0];
        try {
            if ( checkWarnNoDict( rowid ) ) {
                launchGame( rowid );
            }
        } catch ( GameLock.GameLockedException gle ) {
            Log.ex( TAG, gle );
            finish();
        }
    }

    private long[] getSelRowIDs()
    {
        long[] result = new long[m_mySIS.selGames.size()];
        int ii = 0;
        for ( Iterator<Long> iter = m_mySIS.selGames.iterator();
              iter.hasNext(); ) {
            result[ii++] = iter.next();
        }
        return result;
    }

    private int getSelGroupPos()
    {
        int result = -1;
        if ( 1 == m_mySIS.selGroupIDs.size() ) {
            long id = m_mySIS.selGroupIDs.iterator().next();
            result = m_adapter.getGroupPosition( id );
        }
        return result;
    }

    private long[] getSelGroupIDs()
    {
        long[] result = new long[m_mySIS.selGroupIDs.size()];
        int ii = 0;
        for ( Iterator<Long> iter = m_mySIS.selGroupIDs.iterator();
              iter.hasNext(); ) {
            result[ii++] = iter.next();
        }
        return result;
    }

    private void showItemsIf( int[] items, Menu menu, boolean select )
    {
        for ( int item : items ) {
            Utils.setItemVisible( menu, item, select );
        }
    }

    private void doConfirmReset( long[] rowIDs )
    {
        String msg = getQuantityString( R.plurals.confirm_reset_fmt,
                                        rowIDs.length, rowIDs.length );
        makeConfirmThenBuilder( msg, Action.RESET_GAMES )
            .setPosButton( R.string.button_reset )
            .setParams( rowIDs )
            .show();
    }

    private void mkListAdapter()
    {
        // DbgUtils.logf( "GamesListDelegate.mkListAdapter()" );
        m_adapter = new GameListAdapter();
        setListAdapterKeepScroll( m_adapter );

        ListView listView = getListView();
        m_activity.registerForContextMenu( listView );

        // String field = CommonPrefs.getSummaryField( m_activity );
        // long[] positions = XWPrefs.getGroupPositions( m_activity );
        // GameListAdapter adapter =
        //     new GameListAdapter( m_activity, listview, new Handler(),
        //                          this, positions, field );
        // setListAdapter( adapter );
        // adapter.expandGroups( listview );
        // return adapter;
    }

    // Returns true if user has what looks like a default name and has not
    // said he wants us to stop bugging him about it.
    private boolean askingChangeName( String name, boolean doConfigure )
    {
        boolean asking = false;
        boolean skipAsk = XWPrefs
            .getPrefsBoolean( m_activity, R.string.key_notagain_dfltname,
                              false );
        if ( ! skipAsk ) {
            String name1 = CommonPrefs.getDefaultPlayerName( m_activity, 0,
                                                             false );
            String name2 = CommonPrefs.getDefaultOriginalPlayerName( m_activity, 0 );
            if ( null == name1 || name1.equals( name2 ) ) {
                asking = true;

                String msg = LocUtils
                    .getString( m_activity, R.string.not_again_dfltname_fmt,
                                name2 );

                makeConfirmThenBuilder( msg, Action.NEW_GAME_DFLT_NAME )
                    .setNAKey( R.string.key_notagain_dfltname )
                    .setNegButton( R.string.button_later )
                    .setParams( name, doConfigure )
                    .show();
            }
        }
        return asking;
    }

    private boolean makeThenLaunchOrConfigure()
    {
        boolean handled = null != m_newGameParams;
        if ( handled ) {
            String name = (String)m_newGameParams[0];
            boolean doConfigure = (Boolean)m_newGameParams[1];
            m_newGameParams = null;
            makeThenLaunchOrConfigure( name, doConfigure, true );
        }
        return handled;
    }

    private void makeThenLaunchOrConfigure( String name, boolean doConfigure,
                                            boolean skipAsk )
    {
        makeThenLaunchOrConfigure( name, doConfigure, skipAsk, null );
    }

    private void makeThenLaunchOrConfigure( String name, boolean doConfigure,
                                            boolean skipAsk, CommsAddrRec addr )
    {
        if ( skipAsk || !askingChangeName( name, doConfigure ) ) {
            long rowID;
            long groupID = 1 == m_mySIS.selGroupIDs.size()
                ? m_mySIS.selGroupIDs.iterator().next() : DBUtils.GROUPID_UNSPEC;

            // Ideally we'd check here whether user has set player name.

            if ( m_mySIS.nextIsSolo ) {
                rowID = GameUtils.saveNew( m_activity,
                                           new CurGameInfo( m_activity ),
                                           groupID, name );
            } else {
                rowID = GameUtils.makeNewMultiGame( m_activity, groupID, name );
            }

            if ( null != addr ) {
                DBUtils.addRematchInfo( m_activity, rowID, addr );
            }

            if ( doConfigure ) {
                // configure it
                GameConfigDelegate.editForResult( getDelegator(),
                                                  RequestCode.CONFIG_GAME,
                                                  rowID, true );
            } else {
                // launch it
                GameUtils.launchGame( getDelegator(), rowID );
            }
        }
    }

    public static void boardDestroyed( long rowid )
    {
        if ( null != s_self ) {
            s_self.invalidateOptionsMenuIf();
        }
    }

    public static void onGameDictDownload( Context context, Intent intent )
    {
        intent.setClass( context, MainActivity.class );
        addLaunchFlags( intent );
        context.startActivity( intent );
    }

    private static Intent makeSelfIntent( Context context )
    {
        Intent intent = new Intent( context, MainActivity.class );
        addLaunchFlags( intent );
        return intent;
    }

    private static void addLaunchFlags( Intent intent )
    {
        intent.setFlags( Intent.FLAG_ACTIVITY_CLEAR_TOP
                          | Intent.FLAG_ACTIVITY_SINGLE_TOP )
            // FLAG_ACTIVITY_CLEAR_TASK -- don't think so
            ;
    }

    public static Intent makeRowidIntent( Context context, long rowid )
    {
        Intent intent = makeSelfIntent( context )
            .putExtra( ROWID_EXTRA, rowid );
        return intent;
    }

    public static Intent makeGameIDIntent( Context context, int gameID )
    {
        Intent intent = makeSelfIntent( context )
            .putExtra( GAMEID_EXTRA, gameID );
        return intent;
    }

    private void launchLikeRematch( CommsAddrRec addr, String name )
    {
        Intent intent = makeSelfIntent( m_activity )
            .putExtra( INVITEE_REC_EXTRA, (Serializable)addr )
            .putExtra( REMATCH_NEWNAME_EXTRA, name )
            ;
        startActivity( intent );
    }

    public static Intent makeRematchIntent( Context context, long rowid,
                                            long groupID, CurGameInfo gi,
                                            CommsConnTypeSet addrTypes,
                                            String btAddr, String phone,
                                            String p2pMacAddress, String mqttDevID,
                                            String newName )
    {
        Intent intent = null;
        boolean isSolo = gi.serverRole == CurGameInfo.DeviceRole.SERVER_STANDALONE;
        intent = makeSelfIntent( context )
            .putExtra( REMATCH_ROWID_EXTRA, rowid )
            .putExtra( REMATCH_GROUPID_EXTRA, groupID )
            .putExtra( REMATCH_DICT_EXTRA, gi.dictName )
            .putExtra( REMATCH_IS_SOLO, isSolo )
            .putExtra( REMATCH_LANG_EXTRA, gi.isoCode )
            .putExtra( REMATCH_PREFS_EXTRA, gi.getJSONData() )
            .putExtra( REMATCH_NEWNAME_EXTRA, newName );

        if ( null != addrTypes ) {
            Assert.assertTrueNR( !addrTypes.contains( CommsConnType.COMMS_CONN_RELAY ) );
            intent.putExtra( REMATCH_ADDRS_EXTRA, addrTypes.toInt() );
            if ( null != btAddr ) {
                Assert.assertTrue( addrTypes.contains( CommsConnType.COMMS_CONN_BT ) );
                intent.putExtra( REMATCH_BTADDR_EXTRA, btAddr );
            }
            if ( null != phone ) {
                Assert.assertTrue( addrTypes.contains( CommsConnType.COMMS_CONN_SMS ) );
                intent.putExtra( REMATCH_PHONE_EXTRA, phone );
            }
            if ( null != p2pMacAddress ) {
                Assert.assertTrue( addrTypes.contains( CommsConnType.COMMS_CONN_P2P ) );
                intent.putExtra( REMATCH_P2PADDR_EXTRA, p2pMacAddress );
            }
            if ( null != mqttDevID ) {
                intent.putExtra( GameSummary.EXTRA_REMATCH_MQTT, mqttDevID );
            }
        }
        return intent;
    }

    public static Intent makeAlertIntent( Context context, String msg )
    {
        Intent intent = makeSelfIntent( context )
            .putExtra( ALERT_MSG, msg );
        return intent;
    }

    public static Intent makeAlertWithEmailIntent( Context context, String msg )
    {
        return makeAlertIntent( context, msg )
            .putExtra( WITH_EMAIL, true )
            ;
    }

    public static void postReceivedInvite( Context context, byte[] data )
    {
        Intent intent = makeSelfIntent( context )
            .addFlags( Intent.FLAG_ACTIVITY_NEW_TASK )
            ;
        populateInviteIntent( context, intent, data );
        context.startActivity( intent );
    }

    private static void populateInviteIntent( Context context, Intent intent,
                                              byte[] data )
    {
        NetLaunchInfo nli = NetLaunchInfo.makeFrom( context, data );
        if ( null != nli ) {
            intent.setAction( INVITE_ACTION )
                .putExtra( INVITE_DATA, data );
        } else {
            Assert.failDbg();
        }
    }

    private byte[] getFromIntent( Intent intent )
    {
        byte[] result = null;

        String action = intent.getAction();
        if ( INVITE_ACTION.equals( action ) ) {
            result = intent.getByteArrayExtra( INVITE_DATA );
        }

        // Log.d( TAG, "getFromIntent() => %s", result );
        return result;
    }

    public static void openGame( Context context, Uri data )
    {
        Intent intent = makeSelfIntent( context )
            .setData( data );
        context.startActivity( intent );
    }
}
