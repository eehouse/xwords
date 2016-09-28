/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009 - 2016 by Eric House (xwords@eehouse.org).  All
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
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemLongClickListener;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;

import junit.framework.Assert;

import org.eehouse.android.xw4.DBUtils.GameChangeType;
import org.eehouse.android.xw4.DBUtils.GameGroupInfo;
import org.eehouse.android.xw4.DBUtils.SentInvitesInfo;
import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.ActionPair;
import org.eehouse.android.xw4.DlgDelegate.NAKey;
import org.eehouse.android.xw4.DwnldDelegate.DownloadFinishedListener;
import org.eehouse.android.xw4.DwnldDelegate.OnGotLcDictListener;
import org.eehouse.android.xw4.jni.CommonPrefs;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.CurGameInfo;
import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.jni.LastMoveInfo;
import org.eehouse.android.xw4.loc.LocUtils;

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
               GroupStateListener {


    private static final String SAVE_ROWID = "SAVE_ROWID";
    private static final String SAVE_ROWIDS = "SAVE_ROWIDS";
    private static final String SAVE_GROUPID = "SAVE_GROUPID";
    private static final String SAVE_DICTNAMES = "SAVE_DICTNAMES";
    private static final String SAVE_NEXTSOLO = "SAVE_NEXTSOLO";

    private static final String RELAYIDS_EXTRA = "relayids";
    private static final String ROWID_EXTRA = "rowid";
    private static final String GAMEID_EXTRA = "gameid";
    private static final String REMATCH_ROWID_EXTRA = "rm_rowid";
    private static final String REMATCH_DICT_EXTRA = "rm_dict";
    private static final String REMATCH_LANG_EXTRA = "rm_lang";
    private static final String REMATCH_PREFS_EXTRA = "rm_prefs";
    private static final String REMATCH_NEWNAME_EXTRA = "rm_nnm";
    private static final String REMATCH_ADDRS_EXTRA = "rm_addrs";
    private static final String REMATCH_BTADDR_EXTRA = "rm_btaddr";
    private static final String REMATCH_PHONE_EXTRA = "rm_phone";
    private static final String REMATCH_RELAYID_EXTRA = "rm_relayid";


    private static final String ALERT_MSG = "alert_msg";

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
            m_groupPositions = checkPositions();
        }

        protected Object[] makeListData()
        {
            final Map<Long,GameGroupInfo> gameInfo = DBUtils.getGroups( m_activity );
            ArrayList<Object> alist = new ArrayList<Object>();
            long[] positions = getGroupPositions();
            for ( int ii = 0; ii < positions.length; ++ii ) {
                long groupID = positions[ii];
                GameGroupInfo ggi = gameInfo.get( groupID );
                // m_groupIndices[ii] = alist.size();
                alist.add( new GroupRec( groupID, ii ) );

                if ( ggi.m_expanded ) {
                    List<Object> children = makeChildren( groupID );
                    alist.addAll( children );
                    Assert.assertTrue( ggi.m_count == children.size() );
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
                item.setSelected( m_selGames.contains( rec.m_rowID ) );
                result = item;
            } else {
                Assert.fail();
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
            long[] positions = getGroupPositions();
            for ( int ii = 0; ii < positions.length; ++ii ) {
                if ( positions[ii] == groupID ) {
                    posn = ii;
                    break;
                }
            }
            if ( -1 == posn ) {
                DbgUtils.logd( getClass(), "getGroupPosition: group %d not found", groupID );
            }
            return posn;
        }

        long[] getGroupPositions()
        {
            // do not modify!!!!
            final Set<Long> keys = DBUtils.getGroups( m_activity ).keySet();

            if ( null == m_groupPositions ||
                 m_groupPositions.length != keys.size() ) {

                HashSet<Long> unused = new HashSet<Long>( keys );
                long[] newArray = new long[unused.size()];

                // First copy the existing values, in order
                int nextIndx = 0;
                if ( null != m_groupPositions ) {
                    for ( long id: m_groupPositions ) {
                        if ( unused.contains( id ) ) {
                            newArray[nextIndx++] = id;
                            unused.remove( id );
                        }
                    }
                }

                // Then copy in what's left
                Iterator<Long> iter = unused.iterator();
                while ( iter.hasNext() ) {
                    newArray[nextIndx++] = iter.next();
                }
                m_groupPositions = newArray;
            }
            return m_groupPositions;
        }

        int getChildrenCount( long groupID )
        {
            GameGroupInfo ggi = DBUtils.getGroups( m_activity ).get( groupID );
            return ggi.m_count;
        }

        void moveGroup( long groupID, boolean moveUp )
        {
            int src = getGroupPosition( groupID );
            int dest = src + (moveUp ? -1 : 1);

            long[] positions = getGroupPositions();
            long tmp = positions[src];
            positions[src] = positions[dest];
            positions[dest] = tmp;
            // DbgUtils.logf( "positions now %s", DbgUtils.toString( positions ) );

            swapGroups( src, dest );
        }

        boolean setField( String newField )
        {
            boolean changed = false;
            int newID = fieldToID( newField );
            if ( -1 == newID ) {
                DbgUtils.logd( getClass(), "setField(): unable to match"
                               + " fieldName %s", newField );
            } else if ( m_fieldID != newID ) {
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
            GroupRec rec = (GroupRec)getObjectAt( indx );
            // rec.m_ggi.m_expanded = false;
            removeChildrenOf( indx );
        }

        private void addChildrenOf( long groupID )
        {
            int indx = findGroupItem( makeGroupTestFor( groupID ) );
            GroupRec rec = (GroupRec)getObjectAt( indx );
            // rec.m_ggi.m_expanded = false;
            addChildrenOf( indx, makeChildren( groupID ) );
        }

        private List<Object> makeChildren( long groupID )
        {
            long[] rows = DBUtils.getGroupGames( m_activity, groupID );
            List<Object> alist = new ArrayList<Object>( rows.length );
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
            DbgUtils.logd( getClass(), "removeRange(start=%d, len=%d)", start, len );
            ArrayList<Object> result = new ArrayList<Object>(len);
            for ( int ii = 0; ii < len; ++ii ) {
                result.add( list.remove( start ) );
            }
            return result;
        }

        private Set<GameListGroup> getGroupWithID( long groupID )
        {
            Set<Long> groupIDs = new HashSet<Long>();
            groupIDs.add( groupID );
            Set<GameListGroup> result = getGroupsWithIDs( groupIDs );
            return result;
        }

        // Yes, iterating is bad, but any hashing to get around it will mean
        // hanging onto Views that Android's list management might otherwise
        // get to page out when they scroll offscreen.
        private Set<GameListGroup> getGroupsWithIDs( Set<Long> groupIDs )
        {
            Set<GameListGroup> result = new HashSet<GameListGroup>();
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
            HashSet<Long> rowSet = new HashSet<Long>();
            rowSet.add( rowID );
            return getGamesFromElems( rowSet );
        }

        private Set<GameListItem> getGamesFromElems( Set<Long> rowIDs )
        {
            Set<GameListItem> result = new HashSet<GameListItem>();
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

        private int fieldToID( String fieldName )
        {
            int[] ids = {
                R.string.game_summary_field_empty,
                R.string.game_summary_field_language,
                R.string.game_summary_field_opponents,
                R.string.game_summary_field_state,
                R.string.game_summary_field_rowid,
                R.string.game_summary_field_gameid,
                R.string.game_summary_field_npackets,
            };
            int result = -1;
            for ( int id : ids ) {
                if ( LocUtils.getString( m_activity, id ).equals( fieldName )){
                    result = id;
                    break;
                }
            }
            return result;
        }

        private long[] checkPositions()
        {
            long[] result = XWPrefs.getGroupPositions( m_activity );

            if ( null != result ) {
                final Map<Long,GameGroupInfo> gameInfo =
                    DBUtils.getGroups( m_activity );
                Set<Long> posns = gameInfo.keySet();
                if ( result.length != posns.size() ) {
                    result = null;
                } else {
                    for ( long id : result ) {
                        if ( ! posns.contains( id ) ) {
                            result = null;
                            break;
                        }
                    }
                }
            }
            return result;
        }
    } // class GameListAdapter

    private static final int[] DEBUG_ITEMS = {
        // R.id.games_menu_loaddb,
        R.id.games_menu_storedb,
    };
    private static final int[] NOSEL_ITEMS = {
        R.id.games_menu_newgroup,
        R.id.games_menu_prefs,
        R.id.games_menu_dicts,
        R.id.games_menu_about,
        R.id.games_menu_email,
        R.id.games_menu_checkmoves,
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
    private String m_missingDictName;
    private long m_missingDictRowId = DBUtils.ROWID_NOTFOUND;
    private int m_missingDictMenuId;
    private String[] m_sameLangDicts;
    private int m_missingDictLang;
    private long m_rowid;
    private long[] m_rowids;
    private long m_groupid;
    private String m_nameField;
    private NetLaunchInfo m_netLaunchInfo;
    private GameNamer m_namer;
    private Set<Long> m_launchedGames; // prevent problems with double-taps
    private boolean m_menuPrepared;
    private boolean m_moveAfterNewGroup;
    private Set<Long> m_selGames;
    private Set<Long> m_selGroupIDs;
    private String m_origTitle;
    private boolean m_nextIsSolo;
    private Button[] m_newGameButtons;
    private boolean m_haveShownGetDict;
    private Intent m_rematchIntent;
    private Object[] m_newGameParams;

    public GamesListDelegate( Delegator delegator, Bundle sis )
    {
        super( delegator, sis, R.layout.game_list, R.menu.games_list_menu );
        m_activity = delegator.getActivity();
        m_launchedGames = new HashSet<Long>();
        s_self = this;
    }

    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = null;
        OnClickListener lstnr;
        OnClickListener lstnr2;
        LinearLayout layout;

        AlertDialog.Builder ab;
        DlgID dlgID = DlgID.values()[id];
        switch ( dlgID ) {
        case WARN_NODICT:
        case WARN_NODICT_NEW:
        case WARN_NODICT_SUBST:
            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        GamesListDelegate self = curThis();
                        // no name, so user must pick
                        if ( null == m_missingDictName ) {
                            DictsDelegate
                                .downloadForResult( getDelegator(),
                                                    RequestCode
                                                    .REQUEST_LANG_GL,
                                                    self.m_missingDictLang );
                        } else {
                            DwnldDelegate
                                .downloadDictInBack( self.m_activity,
                                                     self.m_missingDictLang,
                                                     self.m_missingDictName,
                                                     self );
                        }
                    }
                };
            String message;
            String langName =
                DictLangCache.getLangName( m_activity, m_missingDictLang );
            String locLang = xlateLang( langName );
            String gameName = GameUtils.getName( m_activity, m_missingDictRowId );
            if ( DlgID.WARN_NODICT == dlgID ) {
                message = getString( R.string.no_dict_fmt, gameName, locLang );
            } else if ( DlgID.WARN_NODICT_NEW == dlgID ) {
                message = getString( R.string.invite_dict_missing_body_noname_fmt,
                                     null, m_missingDictName, locLang );
            } else {
                // WARN_NODICT_SUBST
                message = getString( R.string.no_dict_subst_fmt, gameName,
                                     m_missingDictName, locLang );
            }

            ab = makeAlertBuilder()
                .setTitle( R.string.no_dict_title )
                .setMessage( message )
                .setPositiveButton( android.R.string.cancel, null )
                .setNegativeButton( R.string.button_download, lstnr )
                ;
            if ( DlgID.WARN_NODICT_SUBST == dlgID ) {
                lstnr = new OnClickListener() {
                        public void onClick( DialogInterface dlg, int item ) {
                            curThis().showDialog( DlgID.SHOW_SUBST );
                        }
                    };
                ab.setNeutralButton( R.string.button_substdict, lstnr );
            }
            dialog = ab.create();
            setRemoveOnDismiss( dialog, dlgID );
            break;
        case SHOW_SUBST:
            m_sameLangDicts =
                DictLangCache.getHaveLangCounts( m_activity, m_missingDictLang );
            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg,
                                         int which ) {
                        GamesListDelegate self = curThis();
                        int pos = ((AlertDialog)dlg).getListView().
                            getCheckedItemPosition();
                        String dict = self.m_sameLangDicts[pos];
                        dict = DictLangCache.stripCount( dict );
                        if ( GameUtils.replaceDicts( self.m_activity,
                                                     self.m_missingDictRowId,
                                                     self.m_missingDictName,
                                                     dict ) ) {
                            self.launchGameIf();
                        }
                    }
                };
            dialog = makeAlertBuilder()
                .setTitle( R.string.subst_dict_title )
                .setPositiveButton( R.string.button_substdict, lstnr )
                .setNegativeButton( android.R.string.cancel, null )
                .setSingleChoiceItems( m_sameLangDicts, 0, null )
                .create();
            // Force destruction so onCreateDialog() will get
            // called next time and we can insert a different
            // list.  There seems to be no way to change the list
            // inside onPrepareDialog().
            setRemoveOnDismiss( dialog, dlgID );
            break;

        case RENAME_GAME:
            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        GamesListDelegate self = curThis();
                        String name = self.m_namer.getName();
                        DBUtils.setName( self.m_activity, self.m_rowid,
                                         name );
                        self.m_adapter.invalName( self.m_rowid );
                    }
                };
            GameSummary summary = GameUtils.getSummary( m_activity, m_rowid );
            int labelID = (summary.isMultiGame() && !summary.anyMissing())
                ? R.string.rename_label_caveat : R.string.rename_label;
            dialog = buildNamerDlg( GameUtils.getName( m_activity, m_rowid ),
                                    labelID, R.string.game_rename_title,
                                    lstnr, null, DlgID.RENAME_GAME );
            break;

        case RENAME_GROUP:
            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        GamesListDelegate self = curThis();
                        String name = self.m_namer.getName();
                        DBUtils.setGroupName( self.m_activity,
                                              self.m_groupid, name );
                        self.reloadGame( self.m_rowid );
                        self.mkListAdapter();
                    }
                };
            dialog = buildNamerDlg( m_adapter.groupName( m_groupid ),
                                    R.string.rename_group_label,
                                    R.string.game_name_group_title,
                                    lstnr, null, DlgID.RENAME_GROUP );
            break;

        case NEW_GROUP:
            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        GamesListDelegate self = curThis();
                        String name = self.m_namer.getName();
                        DBUtils.addGroup( self.m_activity, name );
                        self.mkListAdapter();
                        self.showNewGroupIf();
                    }
                };
            lstnr2 = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        curThis().showNewGroupIf();
                    }
                };
            dialog = buildNamerDlg( "", R.string.newgroup_label,
                                    R.string.game_name_group_title,
                                    lstnr, lstnr2, DlgID.RENAME_GROUP );
            setRemoveOnDismiss( dialog, dlgID );
            break;

        case CHANGE_GROUP:
            final long startGroup = ( 1 == m_rowids.length )
                ? DBUtils.getGroupForGame( m_activity, m_rowids[0] ) : -1;
            final int[] selItem = {-1}; // hack!!!!
            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlgi, int item ) {
                        GamesListDelegate self = curThis();
                        selItem[0] = item;
                        AlertDialog dlg = (AlertDialog)dlgi;
                        Button btn =
                            dlg.getButton( AlertDialog.BUTTON_POSITIVE );
                        boolean enabled = startGroup == -1;
                        if ( !enabled ) {
                            long newGroup = self.m_adapter.getGroupIDFor( item );
                            enabled = newGroup != startGroup;
                        }
                        btn.setEnabled( enabled );
                    }
                };
            lstnr2 = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        GamesListDelegate self = curThis();
                        Assert.assertTrue( -1 != selItem[0] );
                        long gid = self.m_adapter.getGroupIDFor( selItem[0] );
                        for ( long rowid : self.m_rowids ) {
                            DBUtils.moveGame( self.m_activity, rowid, gid );
                        }
                        DBUtils.setGroupExpanded( self.m_activity, gid, true );
                        self.mkListAdapter();
                    }
                };
            OnClickListener lstnr3 =
                new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        GamesListDelegate self = curThis();
                        self.m_moveAfterNewGroup = true;
                        self.showDialog( DlgID.NEW_GROUP );
                    }
                };
            String[] groups = m_adapter.groupNames();
            int curGroupPos = m_adapter.getGroupPosition( startGroup );
            dialog = makeAlertBuilder()
                .setTitle( R.string.change_group )
                .setSingleChoiceItems( groups, curGroupPos, lstnr )
                .setPositiveButton( R.string.button_move, lstnr2 )
                .setNeutralButton( R.string.button_newgroup, lstnr3 )
                .setNegativeButton( android.R.string.cancel, null )
                .create();
            setRemoveOnDismiss( dialog, dlgID );
            break;

        case GET_NAME:
            layout = (LinearLayout)inflate( R.layout.dflt_name );
            final EditText etext =
                (EditText)layout.findViewById( R.id.name_edit );
            etext.setText( CommonPrefs.getDefaultPlayerName( m_activity,
                                                             0, true ) );
            dialog = makeAlertBuilder()
                .setTitle( R.string.default_name_title )
                .setMessage( R.string.default_name_message )
                .setPositiveButton( android.R.string.ok, null )
                .setView( layout )
                .create();
            dialog.setOnDismissListener(new DialogInterface.
                                        OnDismissListener() {
                    public void onDismiss( DialogInterface dlg ) {
                        String name = etext.getText().toString();
                        if ( 0 == name.length() ) {
                            name = CommonPrefs.
                                getDefaultPlayerName( m_activity, 0, true );
                        }
                        CommonPrefs.setDefaultPlayerName( m_activity, name );

                        makeThenLaunchOrConfigure();
                    }
                });
            break;

        case GAMES_LIST_NEWGAME:
            LinearLayout view = (LinearLayout)
                LocUtils.inflate( m_activity, R.layout.msg_label_and_edit );
            final EditText edit = (EditText)view.findViewById( R.id.edit );
            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        curThis().makeThenLaunchOrConfigure( edit, true, false );
                    }
                };
            lstnr2 = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        curThis().makeThenLaunchOrConfigure( edit, false, false );
                    }
                };

            dialog = makeAlertBuilder()
                .setView( view )
                .setTitle( "foo" )// ditto, but can't be empty (!)
                .setIcon( R.drawable.sologame__gen ) // same for icon
                .setPositiveButton( R.string.newgame_configure_first, lstnr )
                .setNegativeButton( R.string.use_defaults, lstnr2 )
                .create();
            break;

        case GAMES_LIST_NAME_REMATCH:
            view = (LinearLayout)
                LocUtils.inflate( m_activity, R.layout.msg_label_and_edit );
            dialog = makeAlertBuilder()
                .setView( view )
                .setTitle( R.string.button_rematch )
                .setIcon( R.drawable.sologame__gen )
                .setPositiveButton( android.R.string.ok, new OnClickListener() {
                        public void onClick( DialogInterface dlg, int item ) {
                            EditText edit = (EditText)((Dialog)dlg)
                                .findViewById( R.id.edit );
                            curThis().startRematchWithName( edit );
                        }
                    } )
                .create();
            break;

        default:
            dialog = super.onCreateDialog( id );
            break;
        }
        return dialog;
    } // onCreateDialog

    @Override
    protected void prepareDialog( DlgID dlgID, Dialog dialog )
    {
        AlertDialog ad = (AlertDialog)dialog;
        switch( dlgID ) {
        case CHANGE_GROUP:
            ad.getButton( AlertDialog.BUTTON_POSITIVE ).setEnabled( false );
            break;
        case GAMES_LIST_NEWGAME:
            boolean canDoDefaults = m_nextIsSolo
                || 0 < XWPrefs.getAddrTypes( m_activity ).size();
            ad.getButton( AlertDialog.BUTTON_NEGATIVE )
                .setVisibility( canDoDefaults ? View.VISIBLE : View.GONE );

            ad.setIcon( m_nextIsSolo ? R.drawable.sologame__gen
                        : R.drawable.multigame__gen );
            ad.setTitle( m_nextIsSolo ? R.string.new_game
                         : R.string.new_game_networked);

            String msg = getString( canDoDefaults ? R.string.new_game_message
                                    : R.string.new_game_message_nodflt );
            if ( !m_nextIsSolo ) {
                msg += "\n\n" + getString( R.string.new_game_message_net );
            }
            TextView edit = (TextView)dialog.findViewById( R.id.msg );
            edit.setText( msg );
            edit = (TextView)dialog.findViewById( R.id.edit );
            edit.setText( GameUtils.makeDefaultName( m_activity ) );
            edit.setVisibility( View.VISIBLE );
            break;

        case GAMES_LIST_NAME_REMATCH:
            edit = (TextView)dialog.findViewById( R.id.edit );
            edit.setText( m_rematchIntent
                          .getStringExtra( REMATCH_NEWNAME_EXTRA ) );
            boolean solo =
                -1 == m_rematchIntent.getIntExtra( REMATCH_ADDRS_EXTRA, -1 );
            ad.setIcon( solo ? R.drawable.sologame__gen
                        : R.drawable.multigame__gen );
            ((TextView)dialog.findViewById( R.id.msg ))
                .setVisibility( View.GONE );
            break;
        }
    }

    @Override
    protected void init( Bundle savedInstanceState )
    {
        m_origTitle = getTitle();

        m_handler = new Handler();
        // Next line useful if contents of DB are crashing app on start
        // DBUtils.saveDB( m_activity );

        CrashTrack.init( m_activity );

        Utils.cancelNotification( m_activity, R.string.post_dualpane_title );

        m_selGames = new HashSet<Long>();
        m_selGroupIDs = new HashSet<Long>();
        getBundledData( savedInstanceState );

        DBUtils.setDBChangeListener( this );

        boolean isUpgrade = Utils.firstBootThisVersion( m_activity );
        if ( isUpgrade ) {
            if ( !s_firstShown ) {
                if ( LocUtils.getCurLangCode( m_activity ).equals( "en" ) ) {
                    FirstRunDialog.show( m_activity );
                }
                s_firstShown = true;
            } else if ( !XWPrefs.getPrefsBoolean( m_activity,
                                                  R.string.key_enable_dualpane,
                                                  false )
                        && XWPrefs.getIsTablet( m_activity ) ) {
                makeConfirmThenBuilder(R.string.invite_dualpane, Action.ENABLE_DUALPANE)
                    .setNAKey(R.string.key_notagain_dualpane)
                    .setPosButton(R.string.enable_dualpane)
                    .setNegButton(R.string.button_later)
                    .show();
            }
        }

        m_newGameButtons = new Button[] {
            (Button)findViewById( R.id.button_newgame_solo ),
            (Button)findViewById( R.id.button_newgame_multi )
        };

        mkListAdapter();
        getListView().setOnItemLongClickListener( this );

        NetUtils.informOfDeaths( m_activity );

        post( new Runnable() {
                @Override
                public void run() {
                    tryStartsFromIntent( getIntent() );
                    getDictForLangIf();
                }
            } );

        updateField();
    } // init

    @Override
    protected boolean canHandleNewIntent( Intent intent )
    {
        return true;
    }

    @Override
    protected void handleNewIntent( Intent intent )
    {
        m_launchedGames.clear();
        Assert.assertNotNull( intent );
        invalRelayIDs( intent.getStringArrayExtra( RELAYIDS_EXTRA ) );
        reloadGame( intent.getLongExtra( ROWID_EXTRA, -1 ) );
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
        XWPrefs.setGroupPositions( m_activity, positions );
        super.onStop();
    }

    protected void onDestroy()
    {
        DBUtils.clearDBChangeListener( this );
        if ( s_self == this ) {
            s_self = null;
        }
    }

    protected void onSaveInstanceState( Bundle outState )
    {
        // super.onSaveInstanceState( outState );
        outState.putLong( SAVE_ROWID, m_rowid );
        outState.putLongArray( SAVE_ROWIDS, m_rowids );
        outState.putLong( SAVE_GROUPID, m_groupid );
        outState.putString( SAVE_DICTNAMES, m_missingDictName );
        outState.putBoolean( SAVE_NEXTSOLO, m_nextIsSolo );
        if ( null != m_netLaunchInfo ) {
            m_netLaunchInfo.putSelf( outState );
        }
    }

    private void getBundledData( Bundle bundle )
    {
        if ( null != bundle ) {
            m_rowid = bundle.getLong( SAVE_ROWID );
            m_rowids = bundle.getLongArray( SAVE_ROWIDS );
            m_groupid = bundle.getLong( SAVE_GROUPID );
            m_netLaunchInfo = NetLaunchInfo.makeFrom( bundle );
            m_missingDictName = bundle.getString( SAVE_DICTNAMES );
            m_nextIsSolo = bundle.getBoolean( SAVE_NEXTSOLO );
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

    public void invalidateOptionsMenuIf()
    {
        super.invalidateOptionsMenuIf();

        if ( !XWPrefs.getHideNewgameButtons( m_activity ) ) {
            boolean enabled = 0 == m_selGames.size() && 1 >= m_selGroupIDs.size();
            for ( Button button : m_newGameButtons ) {
                button.setEnabled( enabled );
            }
        }
    }

    protected void onWindowFocusChanged( boolean hasFocus )
    {
        if ( hasFocus ) {
            updateField();

            m_launchedGames.clear();
        }
    }

    @Override
    protected GamesListDelegate curThis()
    {
        return (GamesListDelegate)super.curThis();
    }

    // OnItemLongClickListener interface
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
    public void gameSaved( final long rowid, final GameChangeType change )
    {
        post( new Runnable() {
                public void run() {
                    switch( change ) {
                    case GAME_DELETED:
                        m_adapter.removeGame( rowid );
                        m_launchedGames.remove( rowid );
                        m_selGames.remove( rowid );
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
                    default:
                        Assert.fail();
                        break;
                    }
                }
            } );
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
            if ( ! m_launchedGames.contains( rowid ) ) {
                makeNotAgainBuilder( R.string.not_again_newselect,
                                     R.string.key_notagain_newselect,
                                     Action.OPEN_GAME )
                    .setParams( rowid, summary )
                    .show();
            }
        }
    }

    public void itemToggled( SelectableItem.LongClickHandler toggled,
                             boolean selected )
    {
        if ( toggled instanceof GameListItem ) {
            long rowid = ((GameListItem)toggled).getRowID();
            if ( selected ) {
                m_selGames.add( rowid );
                clearSelectedGroups();
            } else {
                m_selGames.remove( rowid );
            }
        } else if ( toggled instanceof GameListGroup ) {
            long id = ((GameListGroup)toggled).getGroupID();
            if ( selected ) {
                m_selGroupIDs.add( id );
                clearSelectedGames();
            } else {
                m_selGroupIDs.remove( id );
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
            selected = m_selGames.contains( rowid );
        } else if ( obj instanceof GameListGroup ) {
            long groupID = ((GameListGroup)obj).getGroupID();
            selected = m_selGroupIDs.contains( groupID );
        } else {
            Assert.fail();
            selected = false;
        }
        return selected;
    }

    // BTService.MultiEventListener interface
    public void eventOccurred( MultiService.MultiEvent event,
                               final Object ... args )
    {
        switch( event ) {
        case HOST_PONGED:
            post( new Runnable() {
                    public void run() {
                        DbgUtils.showf( m_activity,
                                        "Pong from %s", args[0].toString() );
                    }
                });
            break;
        case BT_GAME_CREATED:
            post( new Runnable() {
                    public void run() {
                        long rowid = (Long)args[0];
                        if ( checkWarnNoDict( rowid ) ) {
                            launchGame( rowid, true );
                        }
                    }
                });
            break;
        default:
            super.eventOccurred( event, args );
            break;
        }
    }

    // DlgDelegate.DlgClickNotify interface
    public void dlgButtonClicked( Action action, int which, Object[] params )
    {
        if ( AlertDialog.BUTTON_POSITIVE == which ) {
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
            case SYNC_MENU:
                doSyncMenuitem();
                break;
            case NEW_FROM:
                long curID = (Long)params[0];
                long newid = GameUtils.dupeGame( m_activity, curID );
                if ( DBUtils.ROWID_NOTFOUND != newid ) {
                    m_selGames.add( newid );
                    reloadGame( newid );
                }
                break;

            case SET_HIDE_NEWGAME_BUTTONS:
                XWPrefs.setHideNewgameButtons(m_activity, true);
                setupButtons();
                // FALLTHRU
            case NEW_GAME_PRESSED:
                handleNewGame( m_nextIsSolo );
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
                deleteGames( (long[])params[0] );
                break;
            case OPEN_GAME:
                doOpenGame( params );
                break;
            case ENABLE_DUALPANE:
                makeOkOnlyBuilder( R.string.dualpane_exit_now)
                    .setAction( Action.ENABLE_DUALPANE_EXIT )
                    .show();
                break;
            case CLEAR_SELS:
                clearSelections();
                break;
            case DWNLD_LOC_DICT:
                String lang = (String)params[0];
                String name = (String)params[1];
                DownloadFinishedListener lstnr = new DownloadFinishedListener() {
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
            default:
                Assert.fail();
            }
        } else if ( AlertDialog.BUTTON_NEGATIVE == which ) {
            if ( Action.NEW_GAME_DFLT_NAME == action ) {
                m_newGameParams = params;
                makeThenLaunchOrConfigure();
            }
        } else if ( DlgDelegate.DISMISS_BUTTON == which ) {
            switch( action ) {
            case ENABLE_DUALPANE_EXIT:
                setDualpaneAndFinish( true );
                break;
            }
        }
    }

    @Override
    protected void onActivityResult( RequestCode requestCode, int resultCode,
                                     Intent data )
    {
        boolean cancelled = Activity.RESULT_CANCELED == resultCode;
        switch ( requestCode ) {
        case REQUEST_LANG_GL:
            if ( !cancelled ) {
                DbgUtils.logd( getClass(), "lang need met" );
                if ( checkWarnNoDict( m_missingDictRowId ) ) {
                    launchGameIf();
                }
            }
            break;
        case CONFIG_GAME:
            if ( !cancelled ) {
                long rowID = data.getLongExtra( GameUtils.INTENT_KEY_ROWID,
                                                DBUtils.ROWID_NOTFOUND );
                launchGame( rowID );
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
        boolean handled = 0 < m_selGames.size() || 0 < m_selGroupIDs.size();
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
        int nGamesSelected = m_selGames.size();
        int nGroupsSelected = m_selGroupIDs.size();
        int groupCount = m_adapter.getGroupCount();
        m_menuPrepared = 0 == nGamesSelected || 0 == nGroupsSelected;
        if ( m_menuPrepared ) {
            boolean nothingSelected = 0 == (nGroupsSelected + nGamesSelected);

            final boolean showDbg = BuildConfig.DEBUG
                || XWPrefs.getDebugEnabled( m_activity );
            showItemsIf( DEBUG_ITEMS, menu, nothingSelected && showDbg );
            Utils.setItemVisible( menu, R.id.games_menu_loaddb,
                                  showDbg && nothingSelected &&
                                  DBUtils.gameDBExists( m_activity ) );

            showItemsIf( NOSEL_ITEMS, menu, nothingSelected );
            showItemsIf( ONEGAME_ITEMS, menu, 1 == nGamesSelected );
            showItemsIf( ONEGROUP_ITEMS, menu, 1 == nGroupsSelected );

            // check for updates only serves release builds, so don't offer in
            // DEBUG case
            boolean enable = showDbg && nothingSelected
                && UpdateCheckReceiver.haveToCheck( m_activity );
            Utils.setItemVisible( menu, R.id.games_menu_checkupdates, enable );

            int selGroupPos = -1;
            if ( 1 == nGroupsSelected ) {
                long id = m_selGroupIDs.iterator().next();
                selGroupPos = m_adapter.getGroupPosition( id );
            }

            // You can't delete the default group, nor make it the default.
            // But we enable delete so a warning message later can explain.
            Utils.setItemVisible( menu, R.id.games_group_delete,
                                  1 <= nGroupsSelected );
            enable = (1 == nGroupsSelected) && ! m_selGroupIDs
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
            Utils.setItemVisible( menu, R.id.games_group_moveup,
                                  enable && 0 < selGroupPos );
            Utils.setItemVisible( menu, R.id.games_group_movedown, enable
                                  && (selGroupPos + 1) < groupCount );

            // New game available when nothing selected or one group
            Utils.setItemVisible( menu, R.id.games_menu_newgame_solo,
                                  nothingSelected || 1 == nGroupsSelected );
            Utils.setItemVisible( menu, R.id.games_menu_newgame_net,
                                  nothingSelected || 1 == nGroupsSelected );

            // Multiples can be deleted, but disable if any selected game is
            // currently open
            enable = 0 < nGamesSelected;
            for ( long row : m_selGames ) {
                enable = enable && !m_launchedGames.contains( row );
            }
            Utils.setItemVisible( menu, R.id.games_game_delete, enable );
            Utils.setItemVisible( menu, R.id.games_game_reset, enable );

            // multiple games can be regrouped/reset.
            Utils.setItemVisible( menu, R.id.games_game_move,
                                  0 < nGamesSelected );

            // Hide rate-me if not a google play app
            enable = nothingSelected && Utils.isGooglePlayApp( m_activity );
            Utils.setItemVisible( menu, R.id.games_menu_rateme, enable );

            enable = nothingSelected && XWPrefs.getStudyEnabled( m_activity );
            Utils.setItemVisible( menu, R.id.games_menu_study, enable );

            enable = nothingSelected &&
                0 < DBUtils.getGamesWithSendsPending( m_activity ).size();
            Utils.setItemVisible( menu, R.id.games_menu_resend, enable );

            enable = XWPrefs.getPrefsBoolean( m_activity, R.string.key_enable_dualpane,
                                              false );
            Utils.setItemVisible( menu, R.id.games_menu_disable_dualpane, enable );

            Assert.assertTrue( m_menuPrepared );
        } else {
            DbgUtils.logd( getClass(), "onPrepareOptionsMenu: incomplete so bailing" );
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
        boolean changeContent = false;
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

        case R.id.games_menu_disable_dualpane:
            setDualpaneAndFinish( false );
            break;
        case R.id.games_menu_resend:
            GameUtils.resendAllIf( m_activity, null, true );
            break;
        case R.id.games_menu_newgame_solo:
            handleNewGame( true );
            break;
        case R.id.games_menu_newgame_net:
            handleNewGame( false );
            break;

        case R.id.games_menu_newgroup:
            m_moveAfterNewGroup = false;
            showDialog( DlgID.NEW_GROUP );
            break;

        case R.id.games_menu_dicts:
            DictsDelegate.start( getDelegator() );
            break;

        case R.id.games_menu_checkmoves:
            makeNotAgainBuilder( R.string.not_again_sync,
                                 R.string.key_notagain_sync,
                                 Action.SYNC_MENU )
                .show();
            break;

        case R.id.games_menu_checkupdates:
            UpdateCheckReceiver.checkVersions( m_activity, true );
            break;

        case R.id.games_menu_prefs:
            Utils.launchSettings( m_activity );
            break;

        case R.id.games_menu_rateme:
            String str = String.format( "market://details?id=%s",
                                        m_activity.getPackageName() );
            try {
                startActivity( new Intent( Intent.ACTION_VIEW, Uri.parse( str ) ) );
            } catch ( android.content.ActivityNotFoundException anf ) {
                makeOkOnlyBuilder( R.string.no_market ).show();
            }
            break;

        case R.id.games_menu_study:
            StudyListDelegate.launchOrAlert( getDelegator(), StudyListDelegate.NO_LANG, this );
            break;

        case R.id.games_menu_about:
            showAboutDialog();
            break;

        case R.id.games_menu_email:
            Utils.emailAuthor( m_activity );
            break;

        case R.id.games_menu_loaddb:
            DBUtils.loadDB( m_activity );
            XWPrefs.clearGroupPositions( m_activity );
            mkListAdapter();
            changeContent = true;
            break;
        case R.id.games_menu_storedb:
            DBUtils.saveDB( m_activity );
            showToast( R.string.db_store_done );
            break;

        default:
            handled = handleSelGamesItem( itemID, selRowIDs )
                || handleSelGroupsItem( itemID, getSelGroupIDs() );
        }

        if ( changeContent ) {
            mkListAdapter();
        }

        return handled;// || super.onOptionsItemSelected( item );
    }

    public void onCreateContextMenu( ContextMenu menu, View view,
                                     ContextMenuInfo menuInfo )
    {
        boolean enable;
        super.onCreateContextMenu( menu, view, menuInfo );

        int id = 0;
        boolean selected = false;
        GameListItem item = null;
        AdapterView.AdapterContextMenuInfo info
            = (AdapterView.AdapterContextMenuInfo)menuInfo;
        View targetView = info.targetView;
        DbgUtils.logd( getClass(), "onCreateContextMenu(t=%s)",
                       targetView.getClass().getSimpleName() );
        if ( targetView instanceof GameListItem ) {
            item = (GameListItem)targetView;
            id = R.menu.games_list_game_menu;

            selected = m_selGames.contains( item.getRowID() );
        } else if ( targetView instanceof GameListGroup ) {
            id = R.menu.games_list_group_menu;

            long groupID = ((GameListGroup)targetView).getGroupID();
            selected = m_selGroupIDs.contains( groupID );
        } else {
            Assert.fail();
        }

        if ( 0 != id ) {
            m_activity.getMenuInflater().inflate( id, menu );

            int hideId = selected
                ? R.id.games_game_select : R.id.games_game_deselect;
            Utils.setItemVisible( menu, hideId, false );

            if ( null != item ) {
                long rowID = item.getRowID();
                enable = BoardDelegate.rematchSupported( m_activity, rowID );
                Utils.setItemVisible( menu, R.id.games_game_rematch, enable );

                enable = item.getSummary().isMultiGame()
                    && (BuildConfig.DEBUG || XWPrefs.getDebugEnabled( m_activity ));
                Utils.setItemVisible( menu, R.id.games_game_invites, enable );

                enable = !m_launchedGames.contains( rowID );
                Utils.setItemVisible( menu, R.id.games_game_delete, enable );
                Utils.setItemVisible( menu, R.id.games_game_reset, enable );
            }
        }
    }

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
                Assert.fail();
            }
        }

        return handled || super.onContextItemSelected( item );
    }

    //////////////////////////////////////////////////////////////////////
    // DwnldActivity.DownloadFinishedListener interface
    //////////////////////////////////////////////////////////////////////
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
                m_selGames.remove( row );
            }
            invalidateOptionsMenuIf();
            setTitle();
        }
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

    private boolean handleSelGamesItem( int itemID, final long[] selRowIDs )
    {
        boolean handled = true;
        boolean dropSels = false;

        switch( itemID ) {
        case R.id.games_game_delete:
            String msg = getQuantityString( R.plurals.confirm_seldeletes_fmt,
                                            selRowIDs.length, selRowIDs.length );
            makeConfirmThenBuilder( msg, Action.DELETE_GAMES )
                .setPosButton( R.string.button_delete )
                .setParams( selRowIDs )
                .show();
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
            m_rowids = selRowIDs;
            showDialog( DlgID.CHANGE_GROUP );
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
            final GameSummary smry = GameUtils.getSummary( m_activity,
                                                           selRowIDs[0] );
            if ( smry.inRelayGame() ) {
                makeOkOnlyBuilder( R.string.no_copy_network ).show();
            } else {
                dropSels = true;    // will select the new game instead
                post( new Runnable() {
                        public void run() {
                            Activity self = m_activity;
                            byte[] stream =
                                GameUtils.savedGame( self, selRowIDs[0] );
                            long groupID = XWPrefs
                                .getDefaultNewGameGroup( self );
                            GameLock lock =
                                GameUtils.saveNewGame( self, stream, groupID );
                            DBUtils.saveSummary( self, lock, smry );
                            m_selGames.add( lock.getRowid() );
                            lock.unlock();
                            mkListAdapter();
                        }
                    });
            }
            break;

        case R.id.games_game_reset:
            doConfirmReset( selRowIDs );
            break;

        case R.id.games_game_rename:
            m_rowid = selRowIDs[0];
            showDialog( DlgID.RENAME_GAME );
            break;

            // DEBUG only
        case R.id.games_game_invites:
            msg = GameUtils.getSummary( m_activity, selRowIDs[0] )
                .conTypes.toString( m_activity );
            msg = getString( R.string.invites_net_fmt, msg );

            SentInvitesInfo info = DBUtils.getInvitesFor( m_activity,
                                                          selRowIDs[0] );
            if ( null != info ) {
                msg += "\n\n" + info.getAsText( m_activity );
            }
            makeOkOnlyBuilder( msg ).show();
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
                if ( m_selGroupIDs.contains( dftGroup ) ) {
                    msg = getString( R.string.cannot_delete_default_group_fmt,
                                     m_adapter.groupName( dftGroup ) );
                    makeOkOnlyBuilder( msg ).show();
                } else {
                    Assert.assertTrue( 0 < groupIDs.length );
                    msg = getQuantityString( R.plurals.groups_confirm_del_fmt,
                                             groupIDs.length, groupIDs.length );

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
                m_groupid = groupID;
                showDialog( DlgID.RENAME_GROUP );
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

    private void handleNewGame( boolean solo )
    {
        m_nextIsSolo = solo;
        showDialog( DlgID.GAMES_LIST_NEWGAME );
    }

    private void handleNewGameButton( boolean solo )
    {
        m_nextIsSolo = solo;

        int count = m_adapter.getCount();
        boolean skipOffer = 6 > count || XWPrefs.getHideNewgameButtons( m_activity );
        if ( skipOffer ) {
            handleNewGame( solo );
        } else {
            ActionPair pair = new ActionPair( Action.SET_HIDE_NEWGAME_BUTTONS,
                                              R.string.set_pref );
            makeNotAgainBuilder( R.string.not_again_hidenewgamebuttons,
                                 R.string.key_notagain_hidenewgamebuttons,
                                 Action.NEW_GAME_PRESSED )
                .setParams(pair)
                .show();
        }
    }

    @Override
    protected void setTitle()
    {
        int fmt = 0;
        int nSels = m_selGames.size();
        if ( 0 < nSels ) {
            fmt = R.string.sel_games_fmt;
        } else {
            nSels = m_selGroupIDs.size();
            if ( 0 < nSels ) {
                fmt = R.string.sel_groups_fmt;
            }
        }

        setTitle( 0 == fmt ? m_origTitle : getString( fmt, nSels ) );
    }

    private boolean checkWarnNoDict( NetLaunchInfo nli )
    {
        // check that we have the dict required
        boolean haveDict;
        if ( null == nli.dict ) { // can only test for language support
            String[] dicts = DictLangCache.getHaveLang( m_activity, nli.lang );
            haveDict = 0 < dicts.length;
            if ( haveDict ) {
                // Just pick one -- good enough for the period when
                // users aren't using new clients that include the
                // dict name.
                nli.dict = dicts[0];
            }
        } else {
            haveDict =
                DictLangCache.haveDict( m_activity, nli.lang, nli.dict );
        }
        if ( !haveDict ) {
            m_netLaunchInfo = nli;
            m_missingDictLang = nli.lang;
            m_missingDictName = nli.dict;
            showDialog( DlgID.WARN_NODICT_NEW );
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
        int[] missingLang = new int[1];
        boolean hasDicts;
        try {
            hasDicts = GameUtils.gameDictsHere( m_activity, rowid, missingNames,
                                                missingLang );
        } catch ( GameUtils.NoSuchGameException nsge ) {
            hasDicts = true;    // irrelevant question
        } catch ( GameLock.GameLockedException gle ) {
            hasDicts = true;    // irrelevant question
        }

        if ( !hasDicts ) {
            m_missingDictLang = missingLang[0];
            if ( 0 < missingNames[0].length ) {
                m_missingDictName = missingNames[0][0];
            } else {
                m_missingDictName = null;
            }
            m_missingDictRowId = rowid;
            m_missingDictMenuId = forMenu;
            if ( 0 == DictLangCache.getLangCount( m_activity, m_missingDictLang ) ) {
                showDialog( DlgID.WARN_NODICT );
            } else if ( null != m_missingDictName ) {
                showDialog( DlgID.WARN_NODICT_SUBST );
            } else {
                String dict =
                    DictLangCache.getHaveLang( m_activity, m_missingDictLang)[0];
                if ( GameUtils.replaceDicts( m_activity, m_missingDictRowId,
                                             null, dict ) ) {
                    launchGameIf();
                }
            }
        }
        return hasDicts;
    }

    private void invalRelayIDs( String[] relayIDs )
    {
        if ( null != relayIDs ) {
            for ( String relayID : relayIDs ) {
                long[] rowids = DBUtils.getRowIDsFor( m_activity, relayID );
                if ( null != rowids ) {
                    for ( long rowid : rowids ) {
                        reloadGame( rowid );
                    }
                }
            }
        }
    }

    // Launch the first of these for which there's a dictionary
    // present.
    private boolean startFirstHasDict( String[] relayIDs )
    {
        boolean launched = false;
        if ( null != relayIDs ) {
            outer:
            for ( String relayID : relayIDs ) {
                long[] rowids = DBUtils.getRowIDsFor( m_activity, relayID );
                if ( null != rowids ) {
                    for ( long rowid : rowids ) {
                        if ( GameUtils.gameDictsHere( m_activity, rowid ) ) {
                            launchGame( rowid );
                            launched = true;
                            break outer;
                        }
                    }
                }
            }
        }
        return launched;
    }

    private void startFirstHasDict( long rowid )
    {
        if ( -1 != rowid && DBUtils.haveGame( m_activity, rowid ) ) {
            boolean haveDict;
            try {
                haveDict = GameUtils.gameDictsHere( m_activity, rowid );
            } catch ( GameLock.GameLockedException gle ) {
                haveDict = true;
            }
            if ( haveDict ) {
                launchGame( rowid );
            }
        }
    }

    private void startFirstHasDict( Intent intent )
    {
        if ( null != intent ) {
            String[] relayIDs = intent.getStringArrayExtra( RELAYIDS_EXTRA );
            if ( !startFirstHasDict( relayIDs ) ) {
                long rowid = intent.getLongExtra( ROWID_EXTRA, -1 );
                startFirstHasDict( rowid );
            }
        }
    }

    private void startNewNetGame( NetLaunchInfo nli )
    {
        Assert.assertTrue( nli.isValid() );

        Date create = null;
        create = DBUtils.getMostRecentCreate( m_activity, nli.gameID() );

        if ( null == create ) {
            if ( checkWarnNoDict( nli ) ) {
                makeNewNetGame( nli );
            }
        } else if ( XWPrefs.getSecondInviteAllowed( m_activity ) ) {
            String msg = getString( R.string.dup_game_query_fmt,
                                    create.toString() );
            m_netLaunchInfo = nli;
            makeConfirmThenBuilder( msg, Action.NEW_NET_GAME )
                .setParams( nli )
                .show();
        } else {
            makeOkOnlyBuilder( R.string.dropped_dupe ).show();
        }
    } // startNewNetGame

    private void startNewNetGame( Intent intent )
    {
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
            startNewNetGame( nli );
        }
    } // startNewNetGame

    private void startHasGameID( int gameID )
    {
        long[] rowids = DBUtils.getRowIDsFor( m_activity, gameID );
        if ( null != rowids && 0 < rowids.length ) {
            launchGame( rowids[0] );
        }
    }

    private void startHasGameID( Intent intent )
    {
        int gameID = intent.getIntExtra( GAMEID_EXTRA, 0 );
        if ( 0 != gameID ) {
            startHasGameID( gameID );
        }
    }

    // Create a new game that's a copy, sending invitations via the means it
    // used to connect.
    private void startRematch( Intent intent )
    {
        if ( -1 != intent.getLongExtra( REMATCH_ROWID_EXTRA, -1 ) ) {
            m_rematchIntent = intent;
            showDialog( DlgID.GAMES_LIST_NAME_REMATCH );
        }
    }

    private void startRematchWithName( EditText edit )
    {
        String gameName = edit.getText().toString();
        if ( null != gameName && 0 < gameName.length() ) {
            Intent intent = m_rematchIntent;
            long srcRowID = intent.getLongExtra( REMATCH_ROWID_EXTRA, -1 );
            String btAddr = intent.getStringExtra( REMATCH_BTADDR_EXTRA );
            String phone = intent.getStringExtra( REMATCH_PHONE_EXTRA );
            String relayID = intent.getStringExtra( REMATCH_RELAYID_EXTRA );
            String dict = intent.getStringExtra( REMATCH_DICT_EXTRA );
            int lang = intent.getIntExtra( REMATCH_LANG_EXTRA, -1 );
            String json = intent.getStringExtra( REMATCH_PREFS_EXTRA );
            int bits = intent.getIntExtra( REMATCH_ADDRS_EXTRA, -1 );
            CommsConnTypeSet addrs = new CommsConnTypeSet( bits );

            long newid;
            if ( null == btAddr && null == phone && null == relayID ) {
                newid = GameUtils.dupeGame( m_activity, srcRowID );
                if ( DBUtils.ROWID_NOTFOUND != newid ) {
                    DBUtils.setName( m_activity, newid, gameName );
                }
            } else {
                long groupID = DBUtils.getGroupForGame( m_activity, srcRowID );
                newid = GameUtils.makeNewMultiGame( m_activity, groupID, dict,
                                                    lang, json, addrs, gameName );
                DBUtils.addRematchInfo( m_activity, newid, btAddr, phone,
                                        relayID );
            }
            launchGame( newid );
        }
        m_rematchIntent = null;
    }

    private void tryAlert( Intent intent )
    {
        String msg = intent.getStringExtra( ALERT_MSG );
        if ( null != msg ) {
            makeOkOnlyBuilder( msg ).show();
        }
    }

    private void tryNFCIntent( Intent intent )
    {
        String data = NFCUtils.getFromIntent( intent );
        if ( null != data ) {
            NetLaunchInfo nli = new NetLaunchInfo( m_activity, data );
            if ( nli.isValid() ) {
                startNewNetGame( nli );
            }
        }
    }

    private void askDefaultName()
    {
        String name = CommonPrefs.getDefaultPlayerName( m_activity, 0, true );
        CommonPrefs.setDefaultPlayerName( m_activity, name );
        showDialog( DlgID.GET_NAME );
    }

    private void getDictForLangIf()
    {
        if ( ! m_haveShownGetDict &&
             ! XWPrefs.getPrefsBoolean( m_activity, R.string.key_got_langdict,
                                        false ) ) {
            m_haveShownGetDict = true;

            String lc = LocUtils.getCurLangCode( m_activity );
            if ( !lc.equals("en") ) {
                int code = LocUtils.codeForLangCode( m_activity, lc );
                if ( 0 < code ) {
                    String[] names = DictLangCache.getHaveLang( m_activity, code );
                    if ( 0 == names.length ) {
                        final Runnable onNA = new Runnable() {
                                public void run() {
                                    XWPrefs.setPrefsBoolean( m_activity, R.string
                                                             .key_got_langdict,
                                                             true );
                                }
                            };

                        OnGotLcDictListener lstnr = new OnGotLcDictListener() {
                                public void gotDictInfo( boolean success, String lang,
                                                         String name ) {
                                    stopProgress();
                                    if ( success ) {
                                        String msg =
                                            getString( R.string.confirm_get_locdict_fmt,
                                                       xlateLang( lang ) );
                                        makeConfirmThenBuilder( msg, Action.DWNLD_LOC_DICT )
                                            .setPosButton( R.string.button_download )
                                            .setOnNA( onNA )
                                            .setParams( lang, name )
                                            .show();
                                    }
                                }
                            };

                        String langName = DictLangCache.getLangName( m_activity, code );
                        String locLang = xlateLang( langName );
                        String msg = getString( R.string.checking_for_fmt, locLang );
                        startProgress( R.string.checking_title, msg );
                        DictsDelegate.downloadDefaultDict( m_activity, lc, lstnr );
                    }
                }
            }
        }
    }

    private void updateField()
    {
        String newField = CommonPrefs.getSummaryField( m_activity );
        if ( m_adapter.setField( newField ) ) {
            // The adapter should be able to decide whether full
            // content change is required.  PENDING
            mkListAdapter();
        }
    }

    private Dialog buildNamerDlg( String curname, int labelID, int titleID,
                                  OnClickListener lstnr1, OnClickListener lstnr2,
                                  DlgID dlgID )
    {
        m_namer = (GameNamer)inflate( R.layout.rename_game );
        m_namer.setName( curname );
        m_namer.setLabel( labelID );

        Dialog dialog = makeAlertBuilder()
            .setTitle( titleID )
            .setPositiveButton( android.R.string.ok, lstnr1 )
            .setNegativeButton( android.R.string.cancel, lstnr2 )
            .setView( m_namer )
            .create();
        setRemoveOnDismiss( dialog, dlgID );
        return dialog;
    }

    private void showNewGroupIf()
    {
        if ( m_moveAfterNewGroup ) {
            m_moveAfterNewGroup = false;
            showDialog( DlgID.CHANGE_GROUP );
        }
    }

    private void deleteGames( long[] rowids )
    {
        for ( long rowid : rowids ) {
            GameUtils.deleteGame( m_activity, rowid, false );
            m_selGames.remove( rowid );
        }
        invalidateOptionsMenuIf();
        setTitle();

        NetUtils.informOfDeaths( m_activity );
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

        m_selGames.add( rowid );
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
        boolean needsClear = 0 < m_selGames.size();
        if ( needsClear ) {
            // long[] rowIDs = getSelRowIDs();
            Set<Long> selGames = new HashSet<Long>( m_selGames );
            m_selGames.clear();
            m_adapter.clearSelectedGames( selGames );
        }
        return needsClear;
    }

    private boolean clearSelectedGroups()
    {
        // clear any selection
        boolean needsClear = 0 < m_selGroupIDs.size();
        if ( needsClear ) {
            m_adapter.clearSelectedGroups( m_selGroupIDs );
            m_selGroupIDs.clear();
        }
        return needsClear;
    }

    private boolean launchGameIf()
    {
        boolean madeGame = DBUtils.ROWID_NOTFOUND != m_missingDictRowId;
        if ( madeGame ) {
            // save in case checkWarnNoDict needs to set them
            long rowID = m_missingDictRowId;
            int menuID = m_missingDictMenuId;
            m_missingDictRowId = DBUtils.ROWID_NOTFOUND;
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

    private void launchGame( long rowid, boolean invited )
    {
        if ( DBUtils.ROWID_NOTFOUND == rowid ) {
            DbgUtils.logd( getClass(), "launchGame(): dropping bad rowid" );
        } else if ( ! m_launchedGames.contains( rowid ) ) {
            m_launchedGames.add( rowid );
            if ( m_adapter.inExpandedGroup( rowid ) ) {
                setSelGame( rowid );
            }
            GameUtils.launchGame( getDelegator(), rowid, invited );
        }
    }

    private void launchGame( long rowid )
    {
        launchGame( rowid, false );
    }

    private void makeNewNetGame( NetLaunchInfo nli )
    {
        long rowid = DBUtils.ROWID_NOTFOUND;
        rowid = GameUtils.makeNewMultiGame( m_activity, nli );
        launchGame( rowid, true );
    }

    private void tryStartsFromIntent( Intent intent )
    {
        startFirstHasDict( intent );
        startNewNetGame( intent );
        startHasGameID( intent );
        startRematch( intent );
        tryAlert( intent );
        tryNFCIntent( intent );
    }

    private void doOpenGame( Object[] params )
    {
        GameSummary summary = (GameSummary)params[1];
        long rowid = (Long)params[0];

        if ( summary.conTypes.contains( CommsAddrRec.CommsConnType.COMMS_CONN_RELAY )
             && summary.roomName.length() == 0 ) {
            Assert.fail();
        } else {
            try {
                if ( checkWarnNoDict( rowid ) ) {
                    launchGame( rowid );
                }
            } catch ( GameLock.GameLockedException gle ) {
                DbgUtils.loge( gle );
                finish();
            }
        }
    }

    private long[] getSelRowIDs()
    {
        long[] result = new long[m_selGames.size()];
        int ii = 0;
        for ( Iterator<Long> iter = m_selGames.iterator();
              iter.hasNext(); ) {
            result[ii++] = iter.next();
        }
        return result;
    }

    private int getSelGroupPos()
    {
        int result = -1;
        if ( 1 == m_selGroupIDs.size() ) {
            long id = m_selGroupIDs.iterator().next();
            result = m_adapter.getGroupPosition( id );
        }
        return result;
    }

    private long[] getSelGroupIDs()
    {
        long[] result = new long[m_selGroupIDs.size()];
        int ii = 0;
        for ( Iterator<Long> iter = m_selGroupIDs.iterator();
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
            .setPosButton(R.string.button_reset)
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
    private boolean askingChangeName( EditText edit, boolean doConfigure )
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

                Runnable onChecked = new Runnable() {
                        public void run() {
                            XWPrefs
                                .setPrefsBoolean( m_activity,
                                                  R.string.key_notagain_dfltname,
                                                  true );
                        }
                    };
                makeConfirmThenBuilder( msg, Action.NEW_GAME_DFLT_NAME )
                    .setOnNA( onChecked )
                    .setNegButton( R.string.button_later )
                    .setParams( edit, doConfigure )
                    .show();
            }
        }
        return asking;
    }

    private boolean makeThenLaunchOrConfigure()
    {
        boolean handled = null != m_newGameParams;
        if ( handled ) {
            EditText edit = (EditText)m_newGameParams[0];
            boolean doConfigure = (Boolean)m_newGameParams[1];
            m_newGameParams = null;
            makeThenLaunchOrConfigure( edit, doConfigure, true );
        }
        return handled;
    }

    private void makeThenLaunchOrConfigure( EditText edit, boolean doConfigure,
                                            boolean skipAsk )
    {
        if ( skipAsk || !askingChangeName( edit, doConfigure ) ) {
            String name = edit.getText().toString();
            long rowID;
            long groupID = 1 == m_selGroupIDs.size()
                ? m_selGroupIDs.iterator().next() : DBUtils.GROUPID_UNSPEC;

            // Ideally we'd check here whether user has set player name.

            if ( m_nextIsSolo ) {
                rowID = GameUtils.saveNew( m_activity,
                                           new CurGameInfo( m_activity ),
                                           groupID, name );
            } else {
                rowID = GameUtils.makeNewMultiGame( m_activity, groupID, name );
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

    private void setDualpaneAndFinish( boolean enable )
    {
        XWPrefs.setPrefsBoolean( m_activity, R.string.key_enable_dualpane,
                                 enable );
        Intent intent = makeSelfIntent( m_activity );
        int bodyID = enable ? R.string.post_dualpane_on_body
            : R.string.post_dualpane_off_body;
        Utils.postNotification( m_activity, intent,
                                R.string.post_dualpane_title,
                                bodyID, R.string.post_dualpane_title );
        Utils.showToast( m_activity, R.string.dualpane_restart );
        m_activity.finish();
    }

    public static void boardDestroyed( long rowid )
    {
        if ( null != s_self ) {
            s_self.m_launchedGames.remove( rowid );
        }
    }

    public static void onGameDictDownload( Context context, Intent intent )
    {
        intent.setClass( context, MainActivity.class );
        context.startActivity( intent );
    }

    private static Intent makeSelfIntent( Context context )
    {
        Intent intent = new Intent( context, MainActivity.class );
        intent.setFlags( Intent.FLAG_ACTIVITY_CLEAR_TOP
                         | Intent.FLAG_ACTIVITY_SINGLE_TOP );
        return intent;
    }

    public static Intent makeRowidIntent( Context context, long rowid )
    {
        Intent intent = makeSelfIntent( context );
        intent.putExtra( ROWID_EXTRA, rowid );
        return intent;
    }

    public static Intent makeGameIDIntent( Context context, int gameID )
    {
        Intent intent = makeSelfIntent( context );
        intent.putExtra( GAMEID_EXTRA, gameID );
        return intent;
    }

    public static Intent makeRematchIntent( Context context, long rowid,
                                            CurGameInfo gi,
                                            CommsConnTypeSet addrTypes,
                                            String btAddr, String phone,
                                            String relayID, String newName )
    {
        Intent intent = null;
        intent = makeSelfIntent( context );
        intent.putExtra( REMATCH_ROWID_EXTRA, rowid );
        intent.putExtra( REMATCH_DICT_EXTRA, gi.dictName );
        intent.putExtra( REMATCH_LANG_EXTRA, gi.dictLang );
        intent.putExtra( REMATCH_PREFS_EXTRA, gi.getJSONData() );
        intent.putExtra( REMATCH_NEWNAME_EXTRA, newName );

        if ( null != addrTypes ) {
            intent.putExtra( REMATCH_ADDRS_EXTRA, addrTypes.toInt() ); // here
            if ( null != btAddr ) {
                intent.putExtra( REMATCH_BTADDR_EXTRA, btAddr );
            }
            if ( null != phone ) {
                intent.putExtra( REMATCH_PHONE_EXTRA, phone );
            }
            if ( null != relayID ) {
                intent.putExtra( REMATCH_RELAYID_EXTRA, relayID );
            }
        }
        return intent;
    }

    public static Intent makeAlertIntent( Context context, String msg )
    {
        Intent intent = makeSelfIntent( context );
        intent.putExtra( ALERT_MSG, msg );
        return intent;
    }

    public static void sendNFCToSelf( Context context, String data )
    {
        Intent intent = makeSelfIntent( context );
        NFCUtils.populateIntent( intent, data );
        context.startActivity( intent );
    }

    public static void openGame( Context context, Uri data )
    {
        Intent intent = makeSelfIntent( context );
        intent.setData( data );
        context.startActivity( intent );
    }
}
