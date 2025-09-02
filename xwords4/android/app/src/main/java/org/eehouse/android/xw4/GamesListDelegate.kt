/*
 * Copyright 2009 - 2024 by Eric House (xwords@eehouse.org).  All rights
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
package org.eehouse.android.xw4

import android.app.Activity
import android.app.AlertDialog
import android.app.Dialog
import android.content.ActivityNotFoundException
import android.content.ClipboardManager
import android.content.Context
import android.content.DialogInterface
import android.content.DialogInterface.OnShowListener
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.text.TextUtils
import android.view.ContextMenu
import android.view.ContextMenu.ContextMenuInfo
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.widget.AbsListView
import android.widget.AdapterView
import android.widget.AdapterView.AdapterContextMenuInfo
import android.widget.AdapterView.OnItemLongClickListener
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.TextView
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView

import com.jakewharton.processphoenix.ProcessPhoenix
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

import java.io.File
import java.io.Serializable
import java.lang.ref.WeakReference
import java.util.Date
import org.json.JSONObject

import org.eehouse.android.xw4.DBUtils.DBChangeListener
import org.eehouse.android.xw4.DBUtils.GameChangeType
import org.eehouse.android.xw4.DBUtils.GameGroupInfo
import org.eehouse.android.xw4.DBUtils.ROWID_NOTFOUND
import org.eehouse.android.xw4.DlgDelegate.HasDlgDelegate
import org.eehouse.android.xw4.DlgDelegate.Action
import org.eehouse.android.xw4.DwnldDelegate.DownloadFinishedListener
import org.eehouse.android.xw4.DwnldDelegate.OnGotLcDictListener
import org.eehouse.android.xw4.GameLock.GameLockedException
import org.eehouse.android.xw4.GameLock.GotLockProc
import org.eehouse.android.xw4.GameUtils.NoSuchGameException
import org.eehouse.android.xw4.Log.ResultProcs
import org.eehouse.android.xw4.MQTTUtils.PingResult
import org.eehouse.android.xw4.NewWithKnowns.ButtonCallbacks
import org.eehouse.android.xw4.Perms23.Perm
import org.eehouse.android.xw4.SelectableItem.LongClickHandler
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.XWDialogFragment.OnDismissListener
import org.eehouse.android.xw4.ZipUtils.SaveWhat
import org.eehouse.android.xw4.jni.CommonPrefs
import org.eehouse.android.xw4.jni.CommsAddrRec
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet
import org.eehouse.android.xw4.jni.CurGameInfo
import org.eehouse.android.xw4.jni.DUtilCtxt
import org.eehouse.android.xw4.jni.Device
import org.eehouse.android.xw4.jni.GameMgr
import org.eehouse.android.xw4.jni.GameMgr.GroupRef
import org.eehouse.android.xw4.jni.GameRef
import org.eehouse.android.xw4.jni.GameRef.RematchOrder
import org.eehouse.android.xw4.jni.Knowns
import org.eehouse.android.xw4.jni.Stats
import org.eehouse.android.xw4.jni.Stats.STAT
import org.eehouse.android.xw4.loc.LocUtils

class GamesListDelegate(delegator: Delegator) :
    DelegateBase(delegator, R.layout.game_list, R.menu.games_list_menu),
    OnItemLongClickListener, SelectableItem, DownloadFinishedListener,
    HasDlgDelegate, GroupStateListener, ResultProcs, DUtilCtxt.Listeners {
    private class MySIS : Serializable {
        var groupSelItem: Int = 0
        var nextIsSolo: Boolean = false
        var moveAfterNewGroup: Array<GameRef>? = null
        var selGames: MutableSet<GameRef> = HashSet()
        var selGroupIDs: MutableSet<Int> = HashSet()
    }
    private var mHaveKnowns = false;

    private var m_mySIS: MySIS? = null

    // private inner class GameListAdapter : XWExpListAdapter(
    //     arrayOf<Class<*>>(
    //         GroupRec::class.java, GameRec::class.java
    //     )
    // ) {
    //     private var m_groupPositions: LongArray? = null

    //     private inner class GroupRec(var m_groupID: Long, var m_position: Int)

    //     private inner class GameRec(var mGR: GameRef)

    //     override fun makeListData(): Array<Any> {

    //         val alist = ArrayList<Any>()
    //         // val gameInfo = DBUtils.getGroups(mActivity)
    //         // val positions: LongArray = getGroupPositions()
    //         // for (ii in positions.indices) {
    //         //     val groupID = positions[ii]
    //         //     val ggi = gameInfo[groupID]
    //         //     // m_groupIndices[ii] = alist.size();
    //         //     alist.add(GroupRec(groupID, ii))

    //         //     if (ggi!!.m_expanded) {
    //         //         val children = makeChildren(groupID)
    //         //         alist.addAll(children)

    //         //         if (BuildConfig.DEBUG && ggi.m_count != children.size) {
    //         //             Log.e(
    //         //                 TAG, "m_count: %d != size: %d",
    //         //                 ggi.m_count, children.size
    //         //             )
    //         //             Assert.failDbg()
    //         //         }
    //         //     }
    //         // }

    //         return alist.toTypedArray<Any>()
    //     }

    //     override fun getView(dataObj: Any, convertView: View?): View {
    //         val result =
    //             if (dataObj is GroupRec) {
    //                 val rec = dataObj
    //                 val ggi = DBUtils.getGroups(mActivity)[rec.m_groupID]
    //                 val group =
    //                     GameListGroup.makeForPosition(
    //                         mActivity, convertView,
    //                         rec.m_groupID, ggi!!.m_count,
    //                         ggi.m_expanded,
    //                         this@GamesListDelegate,
    //                         this@GamesListDelegate
    //                     )
    //                 updateGroupPct(group, ggi)

    //                 val name =
    //                     LocUtils.getQuantityString(
    //                         mActivity,
    //                         R.plurals.group_name_fmt,
    //                         ggi.m_count, ggi.m_name,
    //                         ggi.m_count
    //                     )
    //                 group!!.setText(name)
    //                 group.isSelected = getSelected(group)
    //                 group as View
    //             } else if (dataObj is GameRec) {
    //                 val rec = dataObj
    //                 val item =
    //                     GameListItem.makeForRow(
    //                         mActivity, convertView,
    //                         0, mHandler!!,
    //                         m_fieldID, this@GamesListDelegate
    //                     )
    //                 item.isSelected = m_mySIS!!.selGames.contains(rec.mGR)
    //                 askNotifyPermsOnce()
    //                 item as View
    //             } else {
    //                 Assert.failDbg()
    //                 null
    //             }
    //         return result!!
    //     }

    //     fun setSelected(gr: GameRef, selected: Boolean) {
    //         Assert.failDbg()
    //         // val games = getGamesFromElems(rowID)
    //         // if (1 == games.size) {
    //         //     games.iterator().next().isSelected = selected
    //         // }
    //     }

    //     fun inExpandedGroup(gr: GameRef): Boolean {
    //         var expanded = false
    //         // val rec = findParent(makeChildTestFor(rowID)) as GroupRec?
    //         // if (null != rec) {
    //         //     val ggi =
    //         //         DBUtils.getGroups(mActivity)[rec.m_groupID]
    //         //     expanded = ggi!!.m_expanded
    //         // }
    //         return expanded
    //     }

    //     fun reloadGame(rowID: Long): GameListItem? {
    //         var item: GameListItem? = null
    //         val games = getGamesFromElems(rowID)
    //         if (0 < games.size) {
    //             item = games.iterator().next()
    //             item.forceReload()
    //         } else {
    //             // If the game's not visible, update the parent group in case
    //             // the game's changed in a way that makes it draw differently
    //             val parent = DBUtils.getGroupForGame(mActivity, rowID)
    //             val iter = getGroupWithID(parent).iterator()
    //             if (iter.hasNext()) {
    //                 val group = iter.next()
    //                 val ggi = DBUtils.getGroups(mActivity)[parent]
    //                 updateGroupPct(group, ggi)
    //             }
    //         }
    //         return item
    //     }

    //     fun groupName(groupID: Long): String {
    //         val gameInfo =
    //             DBUtils.getGroups(mActivity)
    //         return gameInfo[groupID]!!.m_name
    //     }

    //     fun getGroupIDFor(groupPos: Int): Long {
    //         return getGroupPositions().get(groupPos)
    //     }

    //     fun groupNames(): Array<String?> {
    //         val positions: LongArray = getGroupPositions()
    //         val gameInfo = DBUtils.getGroups(mActivity)
    //         Assert.assertTrue(positions.size == gameInfo.size)
    //         val names = arrayOfNulls<String>(positions.size)
    //         for (ii in positions.indices) {
    //             names[ii] = gameInfo[positions[ii]]!!.m_name
    //         }
    //         return names
    //     }

    //     fun getGroupPosition(groupID: Long): Int {
    //         var posn = -1
    //         if (-1L != groupID) {
    //             val positions: LongArray = getGroupPositions()
    //             for (ii in positions.indices) {
    //                 if (positions[ii] == groupID) {
    //                     posn = ii
    //                     break
    //                 }
    //             }
    //             if (-1 == posn) {
    //                 Log.d(TAG, "getGroupPosition: group %d not found", groupID)
    //             }
    //         }
    //         return posn
    //     }

    //     fun getGroupPositions(): LongArray
    //     {                // do not modify!!!!
    //         val dbGroups = DBUtils.getGroups(mActivity).keys

    //         if (null == m_groupPositions || m_groupPositions!!.size != dbGroups.size) {
    //             // If the stored order is out-of-sync with the DB, e.g. if
    //             // there have been additions or deletions, keep the ordering
    //             // of groups that we have ordering for. Then add the rest.

    //             m_groupPositions = LongArray(dbGroups.size)
    //             val added: MutableSet<Long> = HashSet()

    //             val groupPositions = loadGroupPositions()
    //             var nextIndx = 0
    //             for (posn in groupPositions) {
    //                 if (dbGroups.contains(posn)) {
    //                     m_groupPositions!![nextIndx++] = posn
    //                     added.add(posn)
    //                 }
    //             }

    //             // Now add at the end the ones we're missing
    //             for (posn in dbGroups) {
    //                 if (!added.contains(posn)) {
    //                     m_groupPositions!![nextIndx++] = posn
    //                 }
    //             }
    //         } else if (BuildConfig.DEBUG) {
    //             for (posn in m_groupPositions!!) {
    //                 Assert.assertTrueNR(dbGroups.contains(posn))
    //             }
    //         }
    //         return m_groupPositions!!
    //     }

    //     fun getChildrenCount(groupID: Long): Int {
    //         val ggi = DBUtils.getGroups(mActivity)[groupID]
    //         return ggi!!.m_count
    //     }

    //     fun moveGroup(groupID: Long, moveUp: Boolean) {
    //         try {
    //             val src = getGroupPosition(groupID)
    //             val dest = src + (if (moveUp) -1 else 1)

    //             val positions: LongArray = getGroupPositions()
    //             val tmp = positions[src]
    //             positions[src] = positions[dest]
    //             positions[dest] = tmp
    //             storeGroupPositions(positions)

    //             swapGroups(src, dest)
    //         } catch (ioob: ArrayIndexOutOfBoundsException) {
    //             Log.ex(TAG, ioob)
    //         }
    //     }

    //     fun setField(newID: Int): Boolean {
    //         var changed = false
    //         if (0 != newID && m_fieldID != newID) {
    //             m_fieldID = newID
    //             // return true so caller will do onContentChanged.
    //             // There's no other way to signal GameListItem instances
    //             // since we don't maintain a list of them.
    //             changed = true
    //         }
    //         return changed
    //     }

    //     fun clearSelectedGames(grs: Set<GameRef>) {
    //         Log.d(TAG, "clearSelectedGames()")
    //         for ( gr in grs ) {
    //             findHolderFor(gr)?.mView?.setSelected(false)
    //         }
    //     }

    //     fun clearSelectedGroups(groupIDs: Set<Long>) {
    //         val groups = getGroupsWithIDs(groupIDs)
    //         for (group in groups) {
    //             group.isSelected = false
    //         }
    //     }

    //     fun setExpanded(groupID: Long, expanded: Boolean) {
    //         if (expanded) {
    //             // addChildrenOf(groupID)
    //         } else {
    //             // removeChildrenOf(groupID)
    //         }
    //     }

    //     private fun updateGroupPct(group: GameListGroup?, ggi: GameGroupInfo?) {
    //         if (!ggi!!.m_expanded) {
    //             group!!.setPct(
    //                 mHandler!!, ggi.m_hasTurn, ggi.m_turnLocal,
    //                 ggi.m_lastMoveTime
    //             )
    //         }
    //     }

    //     // private fun removeChildrenOf(groupID: Long) {
    //     //     val indx = findGroupItem(makeGroupTestFor(groupID))
    //     //     removeChildrenOf(indx)
    //     // }

    //     // private fun addChildrenOf(groupID: Long) {
    //     //     val indx = findGroupItem(makeGroupTestFor(groupID))
    //     //     addChildrenOf(indx, makeChildren(groupID))
    //     // }

    //     // private fun makeChildren(groupID: Long): List<Any> {
    //     //     // val rows = DBUtils.getGroupGames(mActivity, groupID)
    //     //     // val alist = rows.map{GameRec(it)}
    //     //     return alist
    //     // }

    //     private fun makeGroupTestFor(groupID: Long): GroupTest {
    //         return object : GroupTest {
    //             override fun isTheGroup(item: Any): Boolean {
    //                 val rec = item as GroupRec
    //                 return rec.m_groupID == groupID
    //             }
    //         }
    //     }

    //     private fun makeChildTestFor(rowID: Long): ChildTest {
    //         return object : ChildTest {
    //             override fun isTheChild(item: Any?): Boolean {
    //                 val rec = item as GameRec?
    //                 return false // rec!!.mGR == rowID
    //             }
    //         }
    //     }

    //     private fun removeRange(
    //         list: ArrayList<Any>,
    //         start: Int, len: Int
    //     ): ArrayList<Any> {
    //         Log.d(TAG, "removeRange(start=%d, len=%d)", start, len)
    //         val result = ArrayList<Any>(len)
    //         for (ii in 0 until len) {
    //             result.add(list.removeAt(start))
    //         }
    //         return result
    //     }

    //     private fun getGroupWithID(groupID: Long): Set<GameListGroup> {
    //         val groupIDs: MutableSet<Long> = HashSet()
    //         groupIDs.add(groupID)
    //         val result = getGroupsWithIDs(groupIDs)
    //         return result
    //     }

    //     // Yes, iterating is bad, but any hashing to get around it will mean
    //     // hanging onto Views that Android's list management might otherwise
    //     // get to page out when they scroll offscreen.
    //     private fun getGroupsWithIDs(groupIDs: Set<Long>): Set<GameListGroup> {
    //         val result: MutableSet<GameListGroup> = HashSet()
    //         // val listView = listView!!
    //         // val count = listView.childCount
    //         // for (ii in 0 until count) {
    //         //     val view = listView.getChildAt(ii)
    //         //     if (view is GameListGroup) {
    //         //         val tryme = view
    //         //         if (groupIDs.contains(tryme.groupID)) {
    //         //             result.add(tryme)
    //         //         }
    //         //     }
    //         // }
    //         return result
    //     }

    //     private fun getGamesFromElems(rowID: Long): Set<GameListItem> {
    //         val rowSet = HashSet<Long>()
    //         rowSet.add(rowID)
    //         return getGamesFromElems(rowSet)
    //     }

    //     private fun getGamesFromElems(rowIDs: Set<Long>): Set<GameListItem> {
    //         val result: MutableSet<GameListItem> = HashSet()
    //         // val listView = listView!!
    //         // val count = listView.childCount
    //         // for (ii in 0 until count) {
    //         //     val view = listView.getChildAt(ii)
    //         //     if (view is GameListItem) {
    //         //         val tryme = view
    //         //         val rowID = tryme.rowID
    //         //         if (rowIDs.contains(rowID)) {
    //         //             result.add(tryme)
    //         //         }
    //         //     }
    //         // }
    //         return result
    //     }
    // } // class GameListAdapter


    private var m_fieldID = 0

    private val mActivity = delegator.getActivity()!!
    private var mHandler = Handler(Looper.getMainLooper())
    private val m_missingDict: String? = null
    private var m_missingDictRowId: Long = ROWID_NOTFOUND
    private var m_missingDictGR: GameRef? = null
    private var m_missingDictMenuId = 0
    private val m_nameField: String? = null
    private var m_netLaunchInfo: NetLaunchInfo? = null
    private var m_menuPrepared = false
    private var m_origTitle: String? = null
    private var m_newGameButtons: Array<Button>? = null
    private var m_haveShownGetDict = false
    private var m_newGameParams: Array<Any?>? = null
    private var mCurScrollState = 0
    private var mGamesList: RecyclerView? = null
    private var mAdapter: GamesViewAdapter? = null

    override fun makeDialog(alert: DBAlert, vararg params: Any?): Dialog? {
        var dialog: Dialog? = null
        val lstnr: DialogInterface.OnClickListener
        val lstnr2: DialogInterface.OnClickListener

        val dlgID = alert.dlgID
        Log.d(TAG, "makeDialog($dlgID)")
        when (dlgID) {
            DlgID.WARN_NODICT_GENERIC,
            DlgID.WARN_NODICT_INVITED,
            DlgID.WARN_NODICT_SUBST -> {
                val gr = params[0] as GameRef
                val gi = params[1] as CurGameInfo
                val missingDictName = params[2] as String
                val missingDictLang = gi.isoCode()!!

                lstnr = DialogInterface.OnClickListener { dlg, item ->
                    DwnldDelegate.downloadDictInBack(
                        mActivity, missingDictLang, missingDictName,
                        this@GamesListDelegate)
                }
                val langName =
                    DictLangCache.getLangNameForISOCode(mActivity, missingDictLang)
                val locLang = langName
                val gameName = gi.gameName
                val message =
                    if (DlgID.WARN_NODICT_GENERIC == dlgID) {
                        getString(R.string.no_dict_fmt, gameName, locLang)
                    } else if (DlgID.WARN_NODICT_INVITED == dlgID) {
                        getString(
                            R.string.invite_dict_missing_body_noname_fmt,
                            null, missingDictName, locLang
                        )
                    } else {
                        // WARN_NODICT_SUBST
                        getString(
                            R.string.no_dict_subst_fmt, gameName,
                            missingDictName, locLang
                        )
                    }

                val ab = makeAlertBuilder()
                    .setTitle(R.string.no_dict_title)
                    .setMessage(message)
                    .setPositiveButton(android.R.string.cancel, null)
                    .setNegativeButton(R.string.button_download, lstnr)

                if (DlgID.WARN_NODICT_SUBST == dlgID) {
                    val neuLstnr = DialogInterface.OnClickListener { dlg, item ->
                        showDialogFragment(
                            DlgID.SHOW_SUBST, gr,
                            missingDictName, missingDictLang
                        )
                    }
                    ab.setNeutralButton(R.string.button_substdict, neuLstnr)
                } else if (DlgID.WARN_NODICT_GENERIC == dlgID) {
                    val neuLstnr = DialogInterface.OnClickListener { dlg, item ->
                        val grs = Array<GameRef>(1){gr}
                        deleteNamedIfConfirmed(grs, false)
                    }
                    ab.setNeutralButton(R.string.button_delete_game, neuLstnr)
                }
                dialog = ab.create()
            }

            DlgID.SHOW_SUBST -> {
                val gr = params[0] as GameRef
                val missingDict = params[1] as String
                val isoCode = params[2] as ISOCode

                val sameLangDicts =
                    DictLangCache.getHaveLangCounts(mActivity, isoCode)
                lstnr = DialogInterface.OnClickListener { dlg, which ->
                    val pos = (dlg as AlertDialog)
                        .listView
                        .getCheckedItemPosition()
                    var newDict = sameLangDicts[pos]
                    newDict = DictLangCache.stripCount(newDict!!)
                    launch {
                        if (gr.replaceDicts(missingDict, newDict)) {
                            launchGameIf()
                        }
                    }
                }
                dialog = makeAlertBuilder()
                    .setTitle(R.string.subst_dict_title)
                    .setPositiveButton(R.string.button_substdict, lstnr)
                    .setNegativeButton(android.R.string.cancel, null)
                    .setSingleChoiceItems(sameLangDicts, 0, null)
                    .create()
            }

            DlgID.RENAME_GAME -> {
                val gr = params[0] as GameRef
                val namer = buildRenamer(gr)
                lstnr = DialogInterface.OnClickListener { dlg, item ->
                    gr.setGameName(namer.name)
                }
                dialog = buildNamerDlg(namer, lstnr, null, DlgID.RENAME_GAME)
            }

            DlgID.SET_MQTTID -> {
                val view = buildRenamer(null, R.string.set_devid_title)
                dialog = makeAlertBuilder()
                    .setView(view)
                    .setNegativeButton(android.R.string.cancel, null)
                    .setPositiveButton(android.R.string.ok) { dlg, item ->
                        val newID = view.name
                        view.launch {
                            withContext(Dispatchers.Main) {
                                val madeIt = Device.setMQTTDevID(newID)
                                if (madeIt) {
                                    makeOkOnlyBuilder(R.string.reboot_after_setFmt, newID)
                                        .setAction(Action.RESTART)
                                        .show()
                                } else {
                                    makeOkOnlyBuilder(R.string.badMQTTDevIDFmt, newID)
                                        .show()
                                }
                            }
                        }
                    }
                    .create()
            }

            DlgID.KACONFIG -> {
                val view = inflate(R.layout.kaconfig_view)
                dialog = makeAlertBuilder()
                    .setTitle(R.string.kaservice_title)
                    .setView(view)
                    .setIcon(R.drawable.kanotify)
                    .setPositiveButton(android.R.string.ok, null)
                    .create()
            }

            DlgID.RENAME_GROUP -> {
                val grp = params[0] as GroupRef
                val name = params[1] as String
                val namer = buildRenamer(name, R.string.rename_group_label)
                lstnr = DialogInterface.OnClickListener { dlg, item ->
                    val self = curThis()
                    val name = namer.name
                    grp.setGroupName(name)
                }
                dialog = buildNamerDlg(namer, lstnr, null, DlgID.RENAME_GROUP)
            }

            DlgID.BACKUP_LOADSTORE -> {
                val uri = if (0 == params.size) null else Uri.parse(params[0] as String)
                dialog = mkLoadStoreDlg(uri)
            }

            DlgID.NEW_GROUP -> {
                val namer = buildRenamer("", R.string.newgroup_label)
                lstnr = DialogInterface.OnClickListener { dlg, item ->
                    val name = namer.name
                    val hasName = DBUtils.getGroup(mActivity, name)
                    if (DBUtils.GROUPID_UNSPEC == hasName) {
                        GameMgr.addGroup(name)

                        // DBUtils.addGroup(mActivity, name)
                        // mkListAdapter()
                        // showNewGroupIf()
                    } else {
                        makeOkOnlyBuilder(
                            R.string.duplicate_group_name_fmt,
                            name
                        ).show()
                    }
                }
                lstnr2 = DialogInterface.OnClickListener { dlg, item -> curThis().showNewGroupIf() }
                dialog = buildNamerDlg(namer, lstnr, lstnr2, DlgID.RENAME_GROUP)
            }

            DlgID.CHANGE_GROUP -> {
                val games = params[0] as Array<GameRef>
                val namesMap = params[1] as GameMgr.GroupsNames
                var indx = 0
                for ( ii in 0..< namesMap.names.size) {
                    val one = namesMap.refs[ii]
                    if (one == m_mySIS!!.groupSelItem) {
                        indx = ii
                        break
                    }
                }
                dialog = makeAlertBuilder()
                    .setTitle(R.string.change_group)
                    .setSingleChoiceItems(
                        namesMap.names, indx
                    ) { dlgi, item ->
                        m_mySIS!!.groupSelItem = item
                        enableMoveGroupButton(dlgi)
                    }
                    .setPositiveButton(
                        R.string.button_move
                    ) { dlg, item ->
                        val gid = namesMap.refs[m_mySIS!!.groupSelItem]
                        GameMgr.moveGames(GroupRef(gid), games)
                    }
                    .setNeutralButton(
                        R.string.button_newgroup
                    ) { dlg, item ->
                        m_mySIS!!.moveAfterNewGroup = games
                        showDialogFragment(DlgID.NEW_GROUP)
                    }
                    .setNegativeButton(android.R.string.cancel, null)
                    .create()
                dialog.setOnShowListener(OnShowListener { dlg -> enableMoveGroupButton(dlg) })
            }

            DlgID.GET_NAME -> {
                val layout = inflate(R.layout.dflt_name) as LinearLayout
                val etext = layout.findViewById<EditText>(R.id.name_edit)
                etext.setText(CommonPrefs.getDefaultPlayerName(
                                  mActivity, 0, true))
                alert.setOnDismissListener(
                    object:OnDismissListener {
                        override fun onDismissed(frag: XWDialogFragment) {
                            var name = etext.text.toString()
                            if (0 == name.length) {
                                name = CommonPrefs.getDefaultPlayerName(mActivity, 0, true)
                            } else {
                                CommonPrefs.setDefaultPlayerName(mActivity, name)
                            }
                            makeThenLaunchOrConfigure()
                        }
                    })
                dialog = makeAlertBuilder()
                    .setTitle(R.string.default_name_title)
                    .setMessage(R.string.default_name_message)
                    .setPositiveButton(android.R.string.ok, null)
                    .setView(layout)
                    .create()
            }

            DlgID.GAMES_LIST_NEWGAME -> {
                val solo = params[0] as Boolean
                dialog = mkNewGameDialog(solo)
                if (!solo && mHaveKnowns) {
                    makeNotAgainBuilder(
                        R.string.key_na_quicknetgame,
                        R.string.not_again_quicknetgame
                    )
                        .setTitle(R.string.new_feature_title)
                        .show()
                }
            }

            DlgID.GAMES_LIST_NAME_REMATCH -> {
                val extras = JSONObject(params[0] as String)
                val view = LocUtils.inflate(mActivity, R.layout.rematch_config)
                    as RematchConfigView

                var iconResID = R.drawable.ic_sologame
                val gr = GameRef(extras.optLong(REMATCH_GAMEREF_EXTRA, 0))
                val archiveAfter = extras.optBoolean(REMATCH_ARCHIVEAFTER_EXTRA, false)
                val deleteAfter = extras.optBoolean(REMATCH_DELAFTER_EXTRA, false)

                view.configure(gr, this)
                val solo = extras.optBoolean(REMATCH_IS_SOLO, true)
                if (!solo) {
                    iconResID = R.drawable.ic_multigame
                }

                dialog = makeAlertBuilder()
                    .setView(view)
                    .setTitle(R.string.button_rematch)
                    .setIcon(iconResID)
                    .setPositiveButton(android.R.string.ok) { dlg, item ->
                        startRematchWithName(
                            extras,
                            view.getGR(),
                            view.getName(),
                            view.getRematchOrder(), // here?
                            true, archiveAfter, deleteAfter
                        )
                    }
                    .setNegativeButton(android.R.string.cancel, null)
                    .create()
            }

            else -> dialog = super.makeDialog(alert, *params)
        }
        return dialog
    } // makeDialog

    private fun mkNewGameDialog(standalone: Boolean): Dialog {
        val view = LocUtils.inflate(mActivity, R.layout.new_game_with_knowns)
            as NewWithKnowns
        val ab = makeAlertBuilder()
            .setView(view)
            .setTitle(if (standalone) R.string.new_game else R.string.new_game_networked)
            .setIcon(if (standalone) R.drawable.ic_sologame else R.drawable.ic_multigame)
            .setPositiveButton(android.R.string.cancel) { dlg, item ->
                view.onButtonPressed(object : ButtonCallbacks {
                    override fun onUseKnown(knownName: String, gameName: String) {
                        Assert.assertTrueNR(!standalone)
                        view.launch {
                            Knowns.getAddr(knownName)?.let { addr ->
                                launchLikeRematch(addr, gameName)
                            }
                        }
                    }

                    override fun onStartGame(
                        gameName: String, solo: Boolean,
                        configFirst: Boolean
                    ) {
                        Assert.assertTrueNR(solo == standalone)
                        Assert.assertTrueNR(solo == m_mySIS!!.nextIsSolo)
                        curThis().makeThenLaunchOrConfigure(
                            gameName,
                            configFirst,
                            false
                        )
                    }
                })
            }
        if (!standalone && mHaveKnowns) {
            ab.setNegativeButton(
                R.string.gamel_menu_knownplyrs
            ) { dlg, item ->
                KnownPlayersDelegate.launchOrAlert(
                    getDelegator(),
                    this@GamesListDelegate
                )
            }
        }

        val dialog = ab.create()

        dialog.setOnShowListener {
            view.setCallback(
                object:NewWithKnowns.ButtonChangeListener {
                    override fun onNewButtonText(txt: String) {
                        val button = dialog
                            .getButton(DialogInterface.BUTTON_POSITIVE)
                        button?.text = txt
                    }
                }
            )
                .configure(standalone, GameUtils.makeDefaultName(mActivity))
        }

        return dialog
    }

    private fun enableMoveGroupButton(dlgi: DialogInterface) {
        Utils.enableAlertButton(
            (dlgi as AlertDialog), AlertDialog.BUTTON_POSITIVE,
            0 <= m_mySIS!!.groupSelItem
        )
    }

    override fun init(savedInstanceState: Bundle?) {
        DUtilCtxt.registerListener(this)
        // hack to get static set that requires Activity
        GameUtils.getThumbSize(mActivity, 15)

        val isFirstLaunch = null == savedInstanceState
        m_origTitle = getTitle()

        // Next line useful if contents of DB are crashing app on start
        // DBUtils.saveDB( m_activity );
        getBundledData(savedInstanceState)

        val isUpgrade = Utils.firstBootThisVersion(mActivity)
        if (isUpgrade) {
            if (!s_firstShown) {
                if (LocUtils.getCurLangCode(mActivity)!!.equals(Utils.ISO_EN)) {
                    show(FirstRunDialog.newInstance())
                }
                s_firstShown = true
            }
        }

        m_newGameButtons = arrayOf(
            findViewById(R.id.button_newgame_solo) as Button,
            findViewById(R.id.button_newgame_multi) as Button
        )

        mkListAdapter()

        // val lv = listView!!
        // lv.onItemLongClickListener = this

        // Can't just enable fast scrolling because the scroller's wide touch
        // area disables taps on what's underneath. The expander arrows in
        // this case. So these two listener callbacks enable fast scrolling
        // only after the user's started scrolling and disable it when [s]he's
        // done
        //
        // See https://stackoverflow.com/questions/33619453/scrollbar-touch-area-in-android-6
        mCurScrollState = AbsListView.OnScrollListener.SCROLL_STATE_IDLE
        // lv.setOnScrollListener(object : AbsListView.OnScrollListener {
        //     override fun onScroll(
        //         absListView: AbsListView, firstVis: Int,
        //         visCount: Int, totalCount: Int
        //     ) {
        //         if (0 < visCount && visCount < totalCount) {
        //             checkOfferHideButtons()
        //         }
        //         if (mCurScrollState == AbsListView.OnScrollListener.SCROLL_STATE_TOUCH_SCROLL) {
        //             lv.isFastScrollEnabled = true
        //         }
        //     }

        //     override fun onScrollStateChanged(absListView: AbsListView, state: Int) {
        //         if (state == AbsListView.OnScrollListener.SCROLL_STATE_IDLE
        //             && mCurScrollState != state
        //         ) {
        //             lv.postDelayed({ lv.isFastScrollEnabled = false }, 500)
        //         }
        //         mCurScrollState = state
        //     }
        // })

        post(object : Runnable {
            override fun run() {
                tryStartsFromIntent(intent)
                getDictForLangIf()
            }
        })

        updateField()

        if (false) {
            // val dupModeGames = DBUtils.getDupModeGames(mActivity).keys
            // val asArray = Array<GameRef>(dupModeGames.size)
            // var ii = 0
            // for (rowid in dupModeGames) {
            //     Log.d(TAG, "row %d is dup-mode", rowid)
            //     asArray[ii++] = rowid
            // }
            // deleteGames(asArray, true)
        }

        mGamesList = findViewById(R.id.games) as RecyclerView
        initGamesView()
    } // init

    private fun initGamesView() {
        Log.d(TAG, "initGamesView()")
        onKnownPlayersChange();
        if ( true ) {           // Disable this when data corrupted....
            launch {
                val nGames = GameMgr.countItems()
                mGamesList!!.let {
                    it.setLayoutManager(GridLayoutManager(mActivity, 1/*3*/))
                    mAdapter = GamesViewAdapter(nGames)
                    it.setAdapter(mAdapter)
                }
            }
        }
    }

    private fun askNotifyPermsOnce() {
        if (!sAsked) {
            sAsked = true
            Perms23.tryGetPerms(
                this, Perm.POST_NOTIFICATIONS,
                R.string.notify_perms_rationale,  // R.string.key_na_perms_notifications,
                Action.NOTIFY_PERMS
            )
        }
    }

    override fun canHandleNewIntent(intent: Intent): Boolean {
        return true
    }

    override fun handleNewIntent(intent: Intent) {
        Log.d(TAG, "handleNewIntent(extras={%s})", DbgUtils.extrasToString(intent))
        Assert.assertNotNull(intent)
        tryStartsFromIntent(intent)
    }

    // override fun onStop() {
    //     // TelephonyManager mgr =
    //     //     (TelephonyManager)getSystemService( Context.TELEPHONY_SERVICE );
    //     // mgr.listen( m_phoneStateListener, PhoneStateListener.LISTEN_NONE );
    //     // m_phoneStateListener = null;
    //     val positions: LongArray = m_adapter!!.getGroupPositions()
    //     storeGroupPositions(positions)
    //     super.onStop()
    // }

    override fun onDestroy() {
        if (s_self?.get() === this) {
            s_self = null
        }
    }

    override fun onSaveInstanceState(outState: Bundle) {
        outState.putSerializable(SAVE_MYSIS, m_mySIS)
        if (null != m_netLaunchInfo) {
            m_netLaunchInfo!!.putSelf(outState)
        }
        super.onSaveInstanceState(outState)
    }

    // Only called when scrolling's necessary
    private var m_offeredHideButtons = false
    private fun checkOfferHideButtons() {
        if (!m_offeredHideButtons) {
            if (XWPrefs.getHideNewgameButtons(mActivity)) {
                m_offeredHideButtons = true // don't do expensive check again
            } else {
                m_offeredHideButtons = true
                makeNotAgainBuilder(
                    R.string.key_notagain_hidenewgamebuttons,
                    R.string.not_again_hidenewgamebuttons
                )
                    .setActionPair(
                        Action.SET_HIDE_NEWGAME_BUTTONS,
                        R.string.set_pref
                    )
                    .show()
            }
        }
    }

    private fun getBundledData(bundle: Bundle?) {
        if (null != bundle) {
            m_netLaunchInfo = NetLaunchInfo.makeFrom(bundle)
            m_mySIS = bundle.getSerializable(SAVE_MYSIS) as MySIS?
        } else {
            m_mySIS = MySIS()
        }
    }

    private fun moveGroup(groupID: Long, moveUp: Boolean) {
        // m_adapter!!.moveGroup(groupID, moveUp)

        //     long[] positions = m_adapter.getGroupPositions();
        //     XWPrefs.setGroupPositions( m_activity, positions );

        //     m_adapter.notifyDataSetChanged();
        //     // mkListAdapter();
        // }
    }

    private fun unselIfHidden(gr: GameRef, gid: Long) {
        // val groupOpen = DBUtils.getGroups(mActivity)[gid]!!.m_expanded
        // if (!groupOpen) {
        //     m_mySIS!!.selGames.remove(rowid)
        //     // Invalidate if there could have been change
        //     invalidateOptionsMenuIf()
        //     setTitle()
        // }
    }

    private fun unselIfHidden(gr: GameRef) {
        // val gid = DBUtils.getGroupForGame(mActivity, rowid)
        // unselIfHidden(rowid, gid)
    }

    override fun invalidateOptionsMenuIf() {
        super.invalidateOptionsMenuIf()

        if (!XWPrefs.getHideNewgameButtons(mActivity)) {
            val enabled = (0 == m_mySIS!!.selGames.size
                               && 1 >= m_mySIS!!.selGroupIDs.size)
            m_newGameButtons!!.map{it.isEnabled = enabled}
        }
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        if (hasFocus) {
            updateField()
        }
    }

    override fun curThis(): GamesListDelegate {
        return super.curThis() as GamesListDelegate
    }

    // OnItemLongClickListener interface
    override fun onItemLongClick(
        parent: AdapterView<*>?, view: View,
        position: Int, id: Long
    ): Boolean {
        val success = (!XWApp.CONTEXT_MENUS_ENABLED
                && view is LongClickHandler)
        if (success) {
            (view as LongClickHandler).longClicked()
        }
        return success
    }

    //////////////////////////////////////////////////////////////////////
    // DBUtils.DBChangeListener interface
    //////////////////////////////////////////////////////////////////////
    // override fun gameSaved(
    //     context: Context, rowid: Long,
    //     change: GameChangeType
    // ) {
    //     Assert.fail()
        // post {
        //     when (change) {
        //         GameChangeType.GAME_DELETED -> {
        //             m_adapter!!.removeGame(gr)
        //             m_mySIS!!.selGames.remove(gr)
        //             invalidateOptionsMenuIf()
        //         }

        //         GameChangeType.GAME_CHANGED -> {
        //             if (DBUtils.ROWIDS_ALL.toLong() == gr) { // all changed
        //                 mkListAdapter()
        //             } else {
        //                 reloadGame(gr)
        //                 if (m_adapter!!.inExpandedGroup(gr)) {
        //                     val groupID = DBUtils.getGroupForGame(mActivity, gr)
        //                     m_adapter!!.setExpanded(groupID, false)
        //                     m_adapter!!.setExpanded(groupID, true)
        //                 }
        //             }
        //             KAService.startIf(mActivity)
        //         }

        //         GameChangeType.GAME_CREATED -> {
        //             mkListAdapter()
        //             setSelGame(gr)
        //             showKAHintIf(gr)
        //         }

        //         GameChangeType.GAME_MOVED -> {
        //             unselIfHidden(gr)
        //             mkListAdapter()
        //         }
        //     }
        // }
    // }

    private fun openWithChecks(gr: GameRef) {
        Log.d(TAG, "openWithChecks()")
        if (!BoardDelegate.gameIsOpen(gr)) {
            launch {
                if (gr.safeToOpen()) {
                    Log.d(TAG, "openWithChecks(): safe")
                    makeNotAgainBuilder(R.string.key_notagain_newselect,
                                        Action.OPEN_GAME,
                                        R.string.not_again_newselect
                    )
                        .setParams(gr)
                        .show()
                } else {
                    Log.d(TAG, "openWithChecks(): unsafe!!!")
                    makeConfirmThenBuilder(
                        Action.QUARANTINE_CLEAR,
                        R.string.unsafe_open_warning
                    )
                        .setPosButton(R.string.unsafe_open_disregard)
                        .setNegButton(0)
                        .setActionPair(
                            Action.QUARANTINE_DELETE,
                            R.string.button_delete
                        )
                        .setParams(gr)
                        .show()
                }
            }
        }
    }

    //////////////////////////////////////////////////////////////////////
    // SelectableItem interface
    //////////////////////////////////////////////////////////////////////
    override fun itemClicked(clicked: LongClickHandler)
    {
        Log.d(TAG, "itemClicked()")
        // We need a way to let the user get back to the basic-config
        // dialog in case it was dismissed.  That way it to check for
        // an empty room name.
        if (clicked is GameListItem) {
            openWithChecks(clicked.gr())
        }
    }

    override fun itemToggled(
        toggled: LongClickHandler,
        selected: Boolean
    ) {
        if (toggled is GameListItem) {
            val gr = toggled.gr()
            if (selected) {
                m_mySIS!!.selGames.add(gr)
                clearSelectedGroups()
            } else {
                m_mySIS!!.selGames.remove(gr)
            }
        } else if (toggled is GameListGroup) {
            val id = toggled.getGrp()!!.grp
            if (selected) {
                m_mySIS!!.selGroupIDs.add(id)
                clearSelectedGames()
            } else {
                m_mySIS!!.selGroupIDs.remove(id)
            }
        }
        invalidateOptionsMenuIf()
        setTitle()
        // mkListAdapter();
    }

    override fun getSelected(obj: LongClickHandler): Boolean {
        val selected: Boolean
        if (obj is GameListItem) {
            val gr = obj.gr()
            selected = m_mySIS!!.selGames.contains(gr)
        } else if (obj is GameListGroup) {
            val groupID = obj.getGrp()!!.grp
            selected = m_mySIS!!.selGroupIDs. contains(groupID)
        } else {
            Assert.failDbg()
            selected = false
        }
        return selected
    }

    // Log.ResultProcs interface
    override fun onDumping(nRecords: Int) {
        runOnUiThread { Utils.showToast(mActivity,
                                        R.string.logstore_dumping_fmt,
                                        nRecords)
        }
    }

    override fun onDumped(logLoc: File) {
        runOnUiThread {
            val dumpMsg = LocUtils.getString(
                mActivity, R.string.logstore_dumped_fmt,
                logLoc.path
            )
            makeOkOnlyBuilder(dumpMsg)
                .setParams(logLoc)
                .setPosButton(android.R.string.cancel)
                .setActionPair(Action.SEND_LOGS,
                               R.string.button_send_logs)
                .show()
        }
    }

    override fun onCleared(nDumped: Int) {
        runOnUiThread {
            Utils.showToast(mActivity, R.string.logstore_cleared_fmt,
                            nDumped)
        }
    }

    // DlgDelegate.DlgClickNotify interface
    override fun onPosButton(action: Action,
                             vararg params: Any?): Boolean
    {
        var handled = true
        when (action) {
            Action.NEW_NET_GAME -> {
                m_netLaunchInfo = params[0] as NetLaunchInfo
                if (checkWarnNoDict(m_netLaunchInfo!!)) {
                    makeNewNetGameIf()
                }
            }

            Action.RESET_GAMES -> {
                val grs = params[0] as Array<GameRef>
                var changed = false
                for (gr in grs) {
                    changed = false
                    // GameUtils.resetGame(mActivity, rowid)|| changed
                }
                if (changed) {
                    mkListAdapter() // required because position may change
                }
            }

            Action.SET_HIDE_NEWGAME_BUTTONS -> {
                XWPrefs.setHideNewgameButtons(mActivity, true)
                setupButtons()
            }

            Action.DELETE_GROUPS -> {
                val groupIDs = params[0] as IntArray
                groupIDs.map{GameMgr.deleteGroup(GroupRef(it))}
            }

            Action.DELETE_GAMES -> deleteGames(
                params[0] as Array<GameRef>,
                params[1] as Boolean
            )

            Action.OPEN_GAME -> doOpenGame(params[0] as GameRef)
            Action.QUARANTINE_CLEAR -> {
                val gr = params[0] as GameRef
                gr.setOpenCount(0)
                openWithChecks(gr)
            }

            Action.BACKUP_DO -> showDialogFragment(DlgID.BACKUP_LOADSTORE)
            Action.BACKUP_LOADDB -> startFileChooser(null)
            Action.BACKUP_OVERWRITE -> {
                val whats = params[0] as ArrayList<SaveWhat?>
                val uri = Uri.parse(params[1] as String)
                if (ZipUtils.load(mActivity, uri, whats)) {
                    ProcessPhoenix.triggerRebirth(mActivity)
                }
            }

            Action.BACKUP_RETRY -> startFileChooser(null)
            Action.QUARANTINE_DELETE -> deleteIfConfirmed(
                Array<GameRef>(1, {params[0] as GameRef}),
                true
            )

            Action.CLEAR_SELS -> clearSelections()
            Action.DWNLD_LOC_DICT -> {
                val isoCode = params[0] as ISOCode
                val name = params[1] as String
                val lstnr: DownloadFinishedListener = object : DownloadFinishedListener {
                    override fun downloadFinished(
                        isoCode: ISOCode,
                        name: String,
                        success: Boolean
                    ) {
                        var name: String? = name
                        if (success) {
                            XWPrefs.setPrefsString(
                                mActivity,
                                R.string.key_default_language,
                                isoCode.toString()
                            )
                            name = DictUtils.removeDictExtn(name!!)
                            val ids = intArrayOf(
                                R.string.key_default_dict,
                                R.string.key_default_robodict
                            )
                            for (id in ids) {
                                XWPrefs.setPrefsString(mActivity, id, name)
                            }

                            XWPrefs.setPrefsBoolean(
                                mActivity, R.string.key_got_langdict, true)
                        }
                    }
                }
                DwnldDelegate.downloadDictInBack(mActivity, isoCode, name, lstnr)
            }

            Action.NEW_GAME_DFLT_NAME -> {
                m_newGameParams = arrayOf(*params)
                askDefaultName()
            }

            Action.SEND_EMAIL -> Utils.emailAuthor(mActivity)
            Action.CLEAR_LOG_DB -> Log.clearStored(this)
            Action.ASKED_PHONE_STATE ->
                rematchWithNameAndPerm(true, arrayOf(*params))
            Action.APPLY_CONFIG -> {
                val data = Uri.parse(params[0] as String)
                CommonPrefs.loadColorPrefs(mActivity, data)
            }

            Action.SEND_LOGS -> {
                val logLoc = params[0] as File
                Utils.emailLogFile(mActivity, logLoc)
            }

            Action.OPEN_BYOD_DICT ->
                DictBrowseDelegate.launch(getDelegator(), (params[0] as String))
            Action.LAUNCH_AFTER_DEL ->
                deleteGames(Array<GameRef>(1, {params[1] as GameRef}), false)
            Action.CLEAR_STATS -> Stats.clearAll()

            Action.SHOW_KA -> launchKAConfigOnce()

            else -> handled = super.onPosButton(action, *params)
        }
        return handled
    }

    override fun onDismissed(action: Action,
                             vararg params: Any?): Boolean
    {
        var handled = true
        when (action) {
            Action.LAUNCH_AFTER_DEL -> launchGame(params[0] as GameRef)
            Action.RESTART -> ProcessPhoenix.triggerRebirth(mActivity)
            else -> handled = false
        }
        return handled || super.onDismissed(action, *params)
    }

    private fun mkLoadStoreDlg(uri: Uri?): Dialog {
        val view = LocUtils.inflate(mActivity, R.layout.backup_config_view) as BackupConfigView
        view.init(uri)

        val ab = makeAlertBuilder()
            .setTitle(view.alertTitle)
            .setView(view)
            .setPositiveButton(view.posButtonTxt) { dlg, item ->
                if (null == uri) { // store case
                    startFileChooser(view.saveWhat)
                } else {
                    val what = view.saveWhat
                    val name = ZipUtils.getFileName(mActivity, uri)
                    makeConfirmThenBuilder(
                        Action.BACKUP_OVERWRITE,
                        R.string.backup_overwrite_confirm_fmt,
                        name
                    )
                        .setParams(what, uri.toString())
                        .show()
                }
            }
            .setNegativeButton(android.R.string.cancel, null)

        return view.setDialog(ab.create())
    }

    // This is in liu of passing through the startActivityForResult call,
    // which apparently isn't supported.
    private var mSaveWhat: List<SaveWhat>? = null

    private fun startFileChooser(what: List<SaveWhat>?) {
        mSaveWhat = what // will be null in load case

        var intentAction: String? = null
        var rq: RequestCode? = null
        val isStore = null != what
        if (isStore) {
            intentAction = Intent.ACTION_CREATE_DOCUMENT
            rq = RequestCode.STORE_DATA_FILE
        } else {
            intentAction = Intent.ACTION_OPEN_DOCUMENT
            rq = RequestCode.LOAD_DATA_FILE
        }
        val intent = Intent(intentAction)
        intent.addCategory(Intent.CATEGORY_OPENABLE)
        intent.setType(ZipUtils.getMimeType(isStore))
        if (isStore) {
            intent.putExtra(
                Intent.EXTRA_TITLE, ZipUtils.getFileName(mActivity)
            )
        }
        startActivityForResult(intent, rq)
    }

    override fun onNegButton(action: Action,
                             vararg params: Any?): Boolean
    {
        var handled = true
        when (action) {
            Action.NEW_GAME_DFLT_NAME -> {
                m_newGameParams = arrayOf(*params)
                makeThenLaunchOrConfigure()
            }

            Action.ASKED_PHONE_STATE ->
                rematchWithNameAndPerm(false, arrayOf(*params))
            Action.NOTIFY_PERMS ->
                Log.d(TAG, "said NO for notify perms")
            else -> handled = super.onNegButton(action, *params)
        }
        return handled
    }

    override fun onActivityResult(
        requestCode: RequestCode, resultCode: Int,
        data: Intent
    ) {
        val cancelled = Activity.RESULT_CANCELED == resultCode
        when (requestCode) {
            RequestCode.REQUEST_LANG_GL ->
                if (!cancelled) {
                    Log.d(TAG, "lang need met")
                    if (checkWarnNoDict(m_missingDictGR!!)) {
                        launchGameIf()
                    }
                }

            RequestCode.CONFIG_GAME ->
                if (!cancelled) {
                    val gr = data.getLongExtra(
                        GameUtils.INTENT_KEY_GAMEREF, 0
                    )
                    if (0L != gr) {
                        launchGame(GameRef(gr))
                    } else {        // new game case?
                        (data.getSerializableExtra(GameConfigDelegate.INTENT_KEY_GI) as CurGameInfo?)
                            ?.let { makeAndLaunch(it) }
                        // val selfAddr = data
                        //     .getSerializableExtra(GameConfigDelegate.INTENT_KEY_SADDR) as CommsAddrRec?
                        // val selfTypes = selfAddr!!.conTypes
                        // val gr = GameUtils.makeNewMultiGame7(
                        //     mActivity, gi!!,
                        //     selfTypes, name!!
                        // )
                        // launchGame(gr)
                    }
                }

            RequestCode.STORE_DATA_FILE, RequestCode.LOAD_DATA_FILE ->
                if (Activity.RESULT_OK == resultCode) {
                    val uri = data.data
                        val isStore = RequestCode.STORE_DATA_FILE == requestCode
                    if (isStore) {
                        val saved =
                            ZipUtils.save(mActivity, uri!!, mSaveWhat!!)
                        val msgID = if (saved) R.string.db_store_done
                        else R.string.db_store_failed
                        showToast(msgID)
                    } else {
                        post {
                            if (ZipUtils.hasWhats(mActivity, uri)) {
                                val uriStr = uri.toString()
                                showDialogFragment(DlgID.BACKUP_LOADSTORE, uriStr)
                            } else {
                                val name = ZipUtils.getFileName(mActivity, uri)
                                if (null != name) {
                                    makeOkOnlyBuilder(R.string.backup_bad_file_fmt, name)
                                        .setActionPair(
                                            Action.BACKUP_RETRY,
                                            R.string.button_pick_again
                                        )
                                        .show()
                                }
                            }
                        }
                    }
                }
            else -> {Log.d(TAG, "unexpected requestCode $requestCode")}
        }
    }

    override fun onResume() {
        Log.d(TAG, "onResume()")
        super.onResume()
        setupButtons()
    }

    override fun handleBackPressed(): Boolean {
        val handled = (0 < m_mySIS!!.selGames.size
                || 0 < m_mySIS!!.selGroupIDs.size)
        if (handled) {
            makeNotAgainBuilder(
                R.string.key_notagain_backclears,
                Action.CLEAR_SELS, R.string.not_again_backclears
            )
                .show()
        }
        return handled
    }

    override fun onPrepareOptionsMenu(menu: Menu): Boolean {
        m_menuPrepared = null != m_mySIS
        if (m_menuPrepared) {
            val nGamesSelected = m_mySIS!!.selGames.size
            val nGroupsSelected = m_mySIS!!.selGroupIDs.size
            m_menuPrepared = 0 == nGamesSelected || 0 == nGroupsSelected

            if (m_menuPrepared) {
                val nothingSelected = 0 == (nGroupsSelected + nGamesSelected)
                val singleSummary =
                    if (1 == nGamesSelected) {
                        // GameUtils.getSummary(mActivity, m_mySIS!!.selGames.iterator().next())
                        null
                    } else null

                val showDbg = (BuildConfig.NON_RELEASE
                        || XWPrefs.getDebugEnabled(mActivity))
                showItemsIf(DEBUG_ITEMS, menu, nothingSelected && showDbg)
                showItemsIf(NOSEL_ITEMS, menu, nothingSelected)
                showItemsIf(ONEGAME_ITEMS, menu, 1 == nGamesSelected)
                showItemsIf(ONEGROUP_ITEMS, menu, 1 == nGroupsSelected)

                val showDataItems = (
                        Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT
                                && nothingSelected)
                Utils.setItemVisible(menu, R.id.games_submenu_backup, showDataItems)

                var enable = showDbg && nothingSelected
                Utils.setItemVisible(menu, R.id.games_menu_checkupdates, enable)

                // FIX ME. Every time the os thinks about putting up a menu
                // four or more strings get loaded from resources, just so we
                // don't get confused when somebody changes locales. That's
                // wrong.
                enable = nothingSelected
                    && LocUtils.getCheckPref(mActivity, R.array.ka_menuwhen,
                                             key = R.string.key_ka_menuwhen,
                                             default = R.string.ka_menuwhen_available
                    ).let {
                        when (it) {
                            LocUtils.getString(mActivity, R.string.ka_menuwhen_never)
                                -> false
                            LocUtils.getString(mActivity, R.string.ka_menuwhen_available)
                                -> 0 < DBUtils.getKAMinutesLeft(mActivity)
                            LocUtils.getString(mActivity, R.string.ka_menuwhen_running)
                                -> KAService.isRunning()
                            LocUtils.getString(mActivity, R.string.ka_menuwhen_always)
                                -> true
                            else -> {
                                Assert.failDbg()
                                false
                            }
                        }
                    }
                Utils.setItemVisible(menu, R.id.games_menu_ksconfig, enable)

                enable = showDbg && takeFromClip(false)
                Utils.setItemVisible(menu, R.id.games_menu_fromclip, enable)

                // val selGroupPos =
                //     if (1 == nGroupsSelected) {
                //         val id = m_mySIS!!.selGroupIDs.iterator().next()
                //         m_adapter!!.getGroupPosition(id)
                //     } else -1

                // You can't delete the default group, nor make it the default.
                // But we enable delete so a warning message later can explain.
                Utils.setItemVisible(
                    menu, R.id.games_group_delete,
                    1 <= nGroupsSelected
                )
                launch {
                    val dflt = GameMgr.getDefaultGroup().grp
                    val enable = (1 == nGroupsSelected) && !m_mySIS!!.selGroupIDs
                        .contains(dflt)
                    Utils.setItemVisible(menu, R.id.games_group_default, enable)
                }

                // Move up/down enabled for groups if not the top-most or bottommost
                // selected
                // enable = 1 == nGroupsSelected
                // enableGroupUpDown(menu, selGroupPos, enable)

                // New game available when nothing selected or one group
                Utils.setItemVisible(
                    menu, R.id.games_menu_newgame_solo,
                    nothingSelected || 1 == nGroupsSelected
                )
                Utils.setItemVisible(
                    menu, R.id.games_menu_newgame_net,
                    nothingSelected || 1 == nGroupsSelected
                )

                // Multiples can be deleted, but disable if any selected game is
                // currently open
                enable = 0 < nGamesSelected
                for (row in m_mySIS!!.selGames) {
                    enable = enable && !BoardDelegate.gameIsOpen(row)
                }
                Utils.setItemVisible(menu, R.id.games_game_delete, enable)
                Utils.setItemVisible(menu, R.id.games_game_reset, enable)

                Utils.setItemVisible(
                    menu, R.id.games_game_hide,
                    enable && BuildConfig.NON_RELEASE
                )

                // multiple games can be regrouped/reset.
                Utils.setItemVisible(
                    menu, R.id.games_game_move,
                    0 < nGamesSelected
                )

                enable = false // singleSummary?.let{!it.isMultiGame} ?: false
                Utils.setItemVisible(menu, R.id.games_game_copy, enable)

                // Hide rate-me if not a google play app
                enable = nothingSelected && Utils.isGooglePlayApp(mActivity)
                Utils.setItemVisible(menu, R.id.games_menu_rateme, enable)

                enable = LegalPhoniesDelegate.haveLegalPhonies(mActivity)
                Utils.setItemVisible(menu, R.id.games_menu_legalPhonies, enable)

                enable = (nothingSelected && XWPrefs.getStudyEnabled(mActivity)
                        && !DBUtils.studyListLangs(mActivity).isEmpty())
                Utils.setItemVisible(menu, R.id.games_menu_study, enable)

                enable = nothingSelected && mHaveKnowns
                Utils.setItemVisible(menu, R.id.games_menu_knownplyrs, enable)

                enable = Log.storeLogs
                Utils.setItemVisible(menu, R.id.games_menu_enableLogStorage, !enable)
                Utils.setItemVisible(menu, R.id.games_menu_disableLogStorage, enable)
                Utils.setItemVisible(menu, R.id.games_menu_emailLogs, enable)

                Assert.assertTrue(m_menuPrepared)
            }
        }

        if (!m_menuPrepared) {
            Log.d(TAG, "onPrepareOptionsMenu: incomplete so bailing")
        }
        return m_menuPrepared
    } // onPrepareOptionsMenu

    private suspend fun formatStats(): String {
        val obj = Stats.export()
        val stats = obj.getJSONObject("stats")
        val startTime = Date(obj.getLong("since")*1000)
        val pairs = ArrayList<String>()
        stats.keys().forEach { key ->
            val value = stats.getLong(key)
            pairs.add("$key: $value")
        }
        val txt = "Since: $startTime\n" +
            TextUtils.join("\n", pairs)
        return txt
    }

    private fun takeFromClip(doIt: Boolean = true): Boolean {
        var haveData = false
        val clipMgr = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        val asTxt = clipMgr.getPrimaryClip()?.getItemAt(0)?.coerceToText(mActivity)?.toString()

        val nli = asTxt?.let { txt ->
            NetLaunchInfo.makeFrom(mActivity, txt)
                ?: NetLaunchInfo.makeFrom(mActivity, Uri.parse(txt))
        }

        if (nli?.isValid == true) {
            if (doIt) {
                startNewNetGame(nli)
            }
            haveData = true
        } else if ( doIt ) {
            makeOkOnlyBuilder(R.string.no_clipboard).show()
        }
        return haveData
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        Assert.assertTrue(m_menuPrepared)

        var msg: String
        val itemID = item.itemId
        var handled = true
        val selRowIDs = selRowIDs

        // What's going on here???
        if (// && !BoardDelegate.gameIsOpen( selRowIDs[0] )
            1 == selRowIDs.size
                && R.id.games_game_delete != itemID
                && R.id.games_game_move != itemID
                && !checkWarnNoDict(selRowIDs[0], itemID)
        ) {
            return true // FIXME: RETURN FROM MIDDLE!!!
        }

        val delegator = getDelegator()
        when (itemID) {
            R.id.games_menu_newgame_solo -> handleNewGameButton(true)
            R.id.games_menu_newgame_net -> handleNewGameButton(false)
            R.id.games_menu_newgroup -> {
                m_mySIS!!.moveAfterNewGroup = null
                showDialogFragment(DlgID.NEW_GROUP)
            }

            R.id.games_menu_dicts -> DictsDelegate.start(delegator)
            R.id.games_menu_checkupdates -> UpdateCheckReceiver
                                                .checkVersions(mActivity, true)
            R.id.games_menu_prefs -> PrefsDelegate.launch(mActivity)
            R.id.games_menu_ksconfig -> launchKAConfigOnce()
            R.id.games_menu_fromclip -> takeFromClip()
            R.id.games_menu_rateme -> {
                val str = String.format(
                    "market://details?id=%s",
                    BuildConfig.APPLICATION_ID
                )
                try {
                    startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(str)))
                } catch (anf: ActivityNotFoundException) {
                    makeOkOnlyBuilder(R.string.no_market).show()
                }
            }

            R.id.games_menu_legalPhonies -> LegalPhoniesDelegate.launch(delegator)
            R.id.games_menu_study -> StudyListDelegate.launch(delegator)
            R.id.games_menu_knownplyrs -> KnownPlayersDelegate.launchOrAlert(delegator, this)
            R.id.games_menu_about -> show(AboutAlert.newInstance())
            R.id.games_menu_email -> Utils.emailAuthor(mActivity)
            R.id.games_menu_storedb -> if (Build.VERSION.SDK_INT == Build.VERSION_CODES.P) {
                makeConfirmThenBuilder(
                    Action.BACKUP_DO,
                    R.string.backup_only_on_9
                )
                    .show()
            } else {
                onPosButton(Action.BACKUP_DO)
            }

            R.id.games_menu_loaddb -> if (Build.VERSION.SDK_INT == Build.VERSION_CODES.P) {
                makeOkOnlyBuilder(R.string.no_restore_on_9)
                    .show()
            } else {
                makeNotAgainBuilder(
                    R.string.key_notagain_loaddb,
                    Action.BACKUP_LOADDB, R.string.not_again_loaddb
                )
                    .show()
            }

            R.id.games_menu_writegit -> Utils.gitInfoToClip(mActivity)
            R.id.games_menu_copyDevid -> {
                MQTTUtils.getMQTTDevID()?.let { devid ->
                    Utils.stringToClip(mActivity, devid)
                    showToast(devid)
                }
            }

            R.id.games_menu_setDevid -> showDialogFragment(DlgID.SET_MQTTID)
            R.id.games_menu_pingMqtt -> {
                MQTTUtils
                    .ping(mActivity,
                          object : PingResult {
                              override fun onSuccess(host: String,
                                                     elapsed: Long) {
                                  val txt = LocUtils
                                      .getString(mActivity,
                                                 R.string.ping_result_fmt,
                                                 host, elapsed)
                                  runOnUiThread { Utils.showToast(mActivity, txt) }
                              }
                          })
            }

            R.id.games_menu_mqttStats -> {
                val stats = MQTTUtils.getStats(mActivity).orEmpty()
                if ( !TextUtils.isEmpty(stats) ) {
                    makeOkOnlyBuilder(stats).show()
                }
            }

            R.id.games_menu_restart -> ProcessPhoenix.triggerRebirth(mActivity)
            R.id.games_menu_enableLogStorage -> Log.storeLogs = true
            R.id.games_menu_disableLogStorage -> Log.storeLogs = false
            R.id.games_menu_pruneLogStorage -> Log.pruneStored(this)
            R.id.games_menu_clearLogStorage -> makeConfirmThenBuilder(
                Action.CLEAR_LOG_DB,
                R.string.logstore_clear_confirm
            )
                .setPosButton(R.string.loc_item_clear)
                .show()

            R.id.games_menu_emailLogs -> Log.dumpStored(this)

            R.id.games_menu_statsShow -> {
                launch {
                    makeOkOnlyBuilder(formatStats()).show()
                }
            }
            R.id.games_menu_statsCopy -> {
                launch {
                    Utils.stringToClip(mActivity, formatStats())
                    showToast(R.string.statsCopiedToast)
                }
            }
            R.id.games_menu_statsClear ->
                makeConfirmThenBuilder(DlgDelegate.Action.CLEAR_STATS,
                                       R.string.statsClearConfirm)
                    .show()

            else -> handled = (handleSelGamesItem(itemID, selRowIDs)
                    || handleSelGroupsItem(itemID, selGroupIDs))
        }
        return handled // || super.onOptionsItemSelected( item );
    }

    // Because ContextMenuInfo doesn't work with RecyclerView (see lots of
    // hacks online) *my* hack is to remember what view I created the menu for
    // and then refer to it down below.
    private var mTargetView: WeakReference<View>? = null

    public override fun onCreateContextMenu(
        menu: ContextMenu, targetView: View,
        ignoreMe: ContextMenuInfo?
    ) {
        var enable: Boolean
        super.onCreateContextMenu(menu, targetView, ignoreMe)

        mTargetView = WeakReference<View>(targetView)

        var selected = false
        var gameItem: GameListItem? = null
        Log.d(TAG, "onCreateContextMenu(t=$targetView)")

        // Only one of these will work
        val tv = targetView as GameListElem
        val id =
            if (tv.isGame) {
                gameItem = tv.getGLI()!!
                selected = m_mySIS!!.selGames.contains(gameItem.gr())
                R.menu.games_list_game_menu
            } else {
                val groupID = tv.getGLG()!!.getGrp()!!.grp
                selected = m_mySIS!!.selGroupIDs.contains(groupID)
                R.menu.games_list_group_menu
            }

        mActivity.menuInflater.inflate(id, menu)

        val hideId =
            if (selected) R.id.games_game_select
            else R.id.games_game_deselect
        Utils.setItemVisible(menu, hideId, false)

        gameItem?.let { gameItem ->
            val gr = gameItem.gr()

            // Deal with possibility summary's temporarily null....
            val summary = gameItem.getSummary()!!
            val gi = gameItem.getGI()
            enable = false
            var isMultiGame = false
            summary?.let { summary ->
                Utils.setItemVisible(
                    menu, R.id.games_game_rematch,
                    summary.canRematch
                )

                isMultiGame = summary.isMultiGame
                enable = (isMultiGame
                              && (BuildConfig.DEBUG || XWPrefs.getDebugEnabled(mActivity)))
            }
            Utils.setItemVisible(menu, R.id.games_game_netstats, isMultiGame)
            Utils.setItemVisible(menu, R.id.games_game_copy, !isMultiGame)
            enable = (isMultiGame && BuildConfig.NON_RELEASE
                          && gi.conTypes!!
                          .contains(CommsConnType.COMMS_CONN_MQTT))
            Utils.setItemVisible(menu, R.id.games_game_relaypage, enable)

            enable = BuildConfig.DEBUG || XWPrefs.getDebugEnabled(mActivity)
            Utils.setItemVisible(menu, R.id.games_game_markbad, enable)

            enable = !BoardDelegate.gameIsOpen(gr)
            Utils.setItemVisible(menu, R.id.games_game_delete, enable)
            Utils.setItemVisible(menu, R.id.games_game_reset, enable)
        } ?: run {          // Group case
            // enableGroupUpDown(menu, selGroupPos, true)
        }
    } // onCreateContextMenu

    public override fun onContextItemSelected(item: MenuItem): Boolean {
        var handled = false
        mTargetView?.get()?.let { targetView ->
                         val itemID = item.itemId
                         if (!handleToggleItem(itemID, targetView)) {
                             // val selIds = LongArray(1)
                             // val selIds = Array<GameRef>(1)
                             val tv = targetView as GameListElem
                             if ( tv.isGame ) {
                                 tv.getGLI()!!.let {
                                     val gr = it.gr()
                                     val selIds = Array<GameRef>(1){gr}
                                     handled = handleSelGamesItem(itemID, selIds)
                                 }
                             } else {
                                 tv.getGLG()!!.let {
                                     val grp = it.getGrp()!!.grp
                                     val selIds = intArrayOf(grp)
                                     handled = handleSelGroupsItem(itemID, selIds)
                                 }
                             }
                         }
                     }

        return handled || super.onContextItemSelected(item)
    }

    //////////////////////////////////////////////////////////////////////
    // DwnldActivity.DownloadFinishedListener interface
    //////////////////////////////////////////////////////////////////////
    override fun downloadFinished(
        isoCode: ISOCode, name: String,
        success: Boolean
    ) {
        runWhenActive {
            var madeGame = false
            if (success) {
                madeGame = makeNewNetGameIf() || launchGameIf()
            }
            if (!madeGame) {
                val id = if (success) R.string.download_done
                else R.string.download_failed
                showToast(id)
            }
        }
    }

    //////////////////////////////////////////////////////////////////////
    // GroupStateListener interface
    //////////////////////////////////////////////////////////////////////
    override fun onGroupExpandedChanged(obj: Any, expanded: Boolean) {
        val glg = obj as GameListGroup
        glg.getGrp()!!.setGroupCollapsed(!expanded)
        initGamesView()         // force redraw -- FIXME!!

        // DbgUtils.logf( "onGroupExpandedChanged(expanded=%b); groupID = %d",
        //                expanded , groupID );
        // DBUtils.setGroupExpanded(mActivity, groupID, expanded)

        // m_adapter!!.setExpanded(groupID, expanded)

        // Deselect any games that are being hidden.
        if (!expanded) {
            // Assert.fail()
            // val rows = DBUtils.getGroupGames(mActivity, groupID)
            // for (row in rows) {
            //     // m_mySIS!!.selGames.remove(row)
            // }
            // invalidateOptionsMenuIf()
            // setTitle()
        }
    }

    // private fun enableGroupUpDown(menu: Menu, selGroupPos: Int, enable: Boolean) {
    //     Utils.setItemVisible(
    //         menu, R.id.games_group_moveup,
    //         enable && 0 < selGroupPos
    //     )
    //     Utils.setItemVisible(
    //         menu, R.id.games_group_movedown,
    //         enable && (selGroupPos + 1) < m_adapter!!.groupCount
    //     )
    // }

    init {
        s_self = WeakReference<GamesListDelegate>(this)
    }

    private fun storeGroupPositions(posns: LongArray) {
        // Log.d( TAG, "storeGroupPositions(%s)", DbgUtils.toString(posns) );
        DBUtils.setSerializableFor(mActivity, GROUP_POSNS_KEY, posns)
    }

    private fun loadGroupPositions(): LongArray {
        var result = longArrayOf()
        val obj = DBUtils.getSerializableFor(mActivity, GROUP_POSNS_KEY)
        if (null != obj && obj is LongArray) {
            result = obj
        }
        // Log.d( TAG, "loadGroupPositions() => %s", DbgUtils.toString(result) );
        return result
    }

    // private fun reloadGame(rowID: Long) {
    //     if (null != m_adapter) {
    //         m_adapter!!.reloadGame(rowID)
    //     }
    // }

    private fun handleToggleItem(itemID: Int, target: View): Boolean {
        val handled: Boolean
        when (itemID) {
            R.id.games_game_select, R.id.games_game_deselect -> {
                val toggled = target as LongClickHandler
                toggled.longClicked()
                handled = true
            }

            else -> handled = false
        }
        return handled
    }

    private fun handleSelGamesItem(itemID: Int, selRowIDs: Array<GameRef>): Boolean {
        var handled = true
        var dropSels = false

        when (itemID) {
            R.id.games_game_hide -> {}//DBUtils.hideGames(mActivity, selRowIDs[0])
            R.id.games_game_delete -> deleteIfConfirmed(selRowIDs, false)
            R.id.games_game_rematch -> BoardDelegate.setupRematchFor(mActivity, selRowIDs[0])
            R.id.games_game_config -> GameConfigDelegate.editForResult(
                getDelegator(),
                RequestCode.CONFIG_GAME,
                selRowIDs[0]
            )

            R.id.games_game_move -> launch {
                m_mySIS!!.groupSelItem = -1
                val groupsMap = GameMgr.getGroupsMap()
                showDialogFragment(DlgID.CHANGE_GROUP, selRowIDs, groupsMap)
            }

            R.id.games_game_copy -> {
                Assert.fail()
                // val selRowID = selRowIDs[0]
                // val smry = GameUtils.getSummary(mActivity, selRowID)
                // smry?.let {
                //     dropSels = true // will select the new game instead
                //     post {
                //         val stream = GameUtils.savedGame(mActivity, selRowID)
                //         val groupID = XWPrefs.getDefaultNewGameGroup(mActivity)
                //         GameUtils.saveNewGame(mActivity, stream, groupID).use { lock ->
                //             DBUtils.saveSummary(mActivity, lock!!, smry)
                //             m_mySIS!!.selGames.add(lock.rowid)
                //         }
                //         mkListAdapter()
                //     }
                // }
            }

            R.id.games_game_reset -> doConfirmReset(selRowIDs)
            R.id.games_game_rename -> showDialogFragment(DlgID.RENAME_GAME, selRowIDs[0])
            R.id.games_game_netstats -> onStatusClicked(selRowIDs[0])
            R.id.games_game_relaypage -> {
                // val summary = GameUtils.getSummary(mActivity, selRowIDs[0])!!
                // NetUtils.copyAndLaunchGamePage(mActivity, summary.gameID)
            }

            R.id.games_game_markbad -> selRowIDs[0].setOpenCount(1)
            else -> handled = false
        }
        if (dropSels) {
            clearSelections()
        }

        return handled
    }

    private fun handleSelGroupsItem(itemID: Int, groupIDs: IntArray): Boolean {
        var handled = 0 < groupIDs.size
        if (handled) {
            val groupID = GroupRef(groupIDs[0])
            when (itemID) {
                R.id.games_group_delete -> launch {
                    val dftGroup = GameMgr.getDefaultGroup()
                    if (groupID.equals(dftGroup)) {
                        val name = dftGroup.getGroupName()
                        makeOkOnlyBuilder(
                            R.string.cannot_delete_default_group_fmt,
                            name
                        )
                            .show()
                    } else {
                        Assert.assertTrue(0 < groupIDs.size)
                        val namesLst = groupIDs.map{GroupRef(it).getGroupName()}
                        val names = TextUtils.join(", ", namesLst)
                        var msg = getQuantityString(R.plurals.groups_confirm_del_fmt,
                                                    groupIDs.size, names)

                        val nGames = groupIDs.sumOf{GroupRef(it).getGroupGamesCount()}
                        if (0 < nGames) {
                            msg += getQuantityString(R.plurals.groups_confirm_del_games_fmt,
                                                     nGames, nGames)
                        }
                        makeConfirmThenBuilder(Action.DELETE_GROUPS, msg)
                            .setParams(groupIDs)
                            .show()
                    }
                }
                R.id.games_group_default -> GameMgr.makeGroupDefault(groupID)
                R.id.games_group_rename -> launch {
                    val name = groupID.getGroupName()
                    showDialogFragment(DlgID.RENAME_GROUP, groupID, name)
                }
                R.id.games_group_moveup -> GameMgr.raiseGroup(groupID)
                R.id.games_group_movedown -> GameMgr.lowerGroup(groupID)
                else -> handled = false
            }
        }
        return handled
    }

    private fun setupButtons() {
        val hidden = XWPrefs.getHideNewgameButtons(mActivity)
        val isSolos = booleanArrayOf(true, false)
        val buttons = m_newGameButtons!!
        for (ii in buttons.indices) {
            val button = buttons[ii]
            if (hidden) {
                button.visibility = View.GONE
            } else {
                button.visibility = View.VISIBLE
                val solo = isSolos[ii]
                button.setOnClickListener { curThis().handleNewGameButton(solo) }
            }
        }
    }

    private fun handleNewGameButton(solo: Boolean) {
        m_mySIS!!.nextIsSolo = solo
        showDialogFragment(DlgID.GAMES_LIST_NEWGAME, solo)
    }

    override fun setTitle() {
        var fmt = 0
        var nSels = m_mySIS!!.selGames.size
        if (0 < nSels) {
            fmt = R.plurals.sel_games_fmt
        } else {
            nSels = m_mySIS!!.selGroupIDs.size
            if (0 < nSels) {
                fmt = R.plurals.sel_groups_fmt
            }
        }

        setTitle((if (0 == fmt) m_origTitle else getQuantityString(fmt, nSels, nSels))!!)
    }

    override fun onKnownPlayersChange() {
        launch {
            mHaveKnowns = Knowns.hasKnownPlayers()
        }
    }

    private fun findHolderFor(gr: GameRef): GameViewHolder? {
        return mAdapter?.findHolderFor(gr)
    }

    private fun findViewFor(gr: GameRef): GameListItem? {
        return mAdapter?.findHolderFor(gr)?.mGameListElem?.getGLI()
    }

    private fun findViewFor(grp: GroupRef): GameListGroup? {
        return mAdapter?.findHolderFor(grp)?.mGameListElem?.getGLG()
    }

    override fun onGameChanged(gr: GameRef, flags: Int) {
        val needsInit = DUtilCtxt.GCE_ADDED or DUtilCtxt.GCE_DELETED
        if ( 0 != (needsInit and flags) ) {
            initGamesView()
        } else {
            runOnUiThread {
                findViewFor(gr)?.forceReload()
            }
        }
    }

    override fun missingDictAdded(gr: GameRef, name: String) {
        Log.d(TAG, "missingDictAdded($gr, $name)")
        runOnUiThread {
            findViewFor(gr)?.forceReload()
        }
    }

    override fun onDictRemoved(gr: GameRef, name: String) {
        Log.d(TAG, "onDictRemoved($gr, $name)")
        runOnUiThread {
            findViewFor(gr)?.forceReload()
        }
    }

    private fun checkWarnNoDict(nli: NetLaunchInfo): Boolean {
        // check that we have the dict required
        val haveDict: Boolean
        if (null == nli.dict) { // can only test for language support

            val dicts = DictLangCache.getHaveLang(mActivity, nli.isoCode())
            haveDict = 0 < dicts.size
            if (haveDict) {
                // Just pick one -- good enough for the period when
                // users aren't using new clients that include the
                // dict name.
                nli.dict = dicts[0]
            }
        } else {
            haveDict =
                DictLangCache.haveDict(mActivity, nli.isoCode(), nli.dict!!)
        }
        if (!haveDict) {
            m_netLaunchInfo = nli
            showDialogFragment(DlgID.WARN_NODICT_INVITED, 0L, nli.dict, nli.isoCode())
        }
        return haveDict
    }

    private fun checkWarnDictElse(gr: GameRef, code: () -> Any?) {
        Log.d(TAG, "checkWarnDictElse()")
        launch {
            val missing = gr.missingDicts()
            Log.d(TAG, "checkWarnDictElse(): missing: $missing")
            if ( null == missing ) {
                code()
            } else {
                Assert.assertTrue(0 < missing.size)
                val gi = gr.getGI()!!
                var missingDictName = gi.dictName!!
                val missingDictLang = gi.isoCode()!!
                m_missingDictGR = gr
                // m_missingDictMenuId = forMenu
                if (0 == DictLangCache.getLangCount(mActivity, missingDictLang)) {
                    showDialogFragment(
                        DlgID.WARN_NODICT_GENERIC, gr,
                        gi, missingDictName
                    )
                } else if (null != missingDictName) {
                    showDialogFragment(
                        DlgID.WARN_NODICT_SUBST, gr, gi, missingDictName
                    )
                } else {
                    val dict =
                        DictLangCache.getHaveLang(mActivity, missingDictLang)[0]!!
                    gi.dictName = dict
                    gr.setGI(gi)
                    code()
                }
            }
        }
    }

    private fun checkWarnNoDict(gr: GameRef, forMenu: Int = -1): Boolean {
        var hasDicts = true
        // val missingNames = arrayOfNulls<Array<String?>?>(1)
        // val missingLang = arrayOf<ISOCode?>(Utils.ISO_EN)
        // var hasDicts = try {
        //     GameUtils.gameDictsHere(
        //         mActivity, gr, missingNames,
        //         missingLang
        //     )
        // } catch (ex: GameLockedException) {
        //     true // irrelevant question
        // } catch (ex: NoSuchGameException) {
        //     true
        // }

        // if (!hasDicts) {
        //     var missingDictName: String? = null
        //     val missingDictLang = missingLang[0]
        //     if (0 < missingNames[0]!!.size) {
        //         missingDictName = missingNames[0]!![0]
        //     }
        //     m_missingDictGR = gr
        //     m_missingDictMenuId = forMenu
        //     if (0 == DictLangCache.getLangCount(mActivity, missingDictLang!!)) {
        //         showDialogFragment(
        //             DlgID.WARN_NODICT_GENERIC, rowid,
        //             missingDictName, missingDictLang
        //         )
        //     } else if (null != missingDictName) {
        //         showDialogFragment(
        //             DlgID.WARN_NODICT_SUBST, rowid, missingDictName,
        //             missingDictLang
        //         )
        //     } else {
        //         val dict =
        //             DictLangCache.getHaveLang(mActivity, missingDictLang)[0]!!
        //         if (GameUtils.replaceDicts(mActivity, rowid, null, dict)) {
        //             launchGameIf()
        //         }
        //     }
        // }
        return hasDicts
    }

    private fun startFirstHasDict(gr: GameRef, extras: Bundle?): Boolean {
        // val handled = DBUtils.haveWithRowID(mActivity, rowid)
        val handled = true // GameUtils.gameDictsHere(mActivity, lock)
        if (handled) {
            launchGame(gr, extras)
        }

        Log.d(TAG, "startFirstHasDict(gr=$gr) => %b", gr, handled)
        return handled
    }

    private fun startFirstHasDict(intent: Intent?): Boolean {
        var result = false
        if (null != intent) {
            val gr = intent.getLongExtra(GAMEREF_EXTRA, 0L)
            if ( 0L != gr ) {
                result = startFirstHasDict(GameRef(gr), intent.extras)
            }
        }
        return result
    }

    private fun startWithInvitee(intent: Intent): Boolean {
        val result = false
        try {
            val invitee = intent
                .getSerializableExtra(INVITEE_REC_EXTRA) as CommsAddrRec?
            invitee?.let {
                val gameName = intent.getStringExtra(REMATCH_NEWNAME_EXTRA)
                Log.d(TAG, "gameName: $gameName")
                makeThenLaunchOrConfigure(gameName, false, false, it)
            }
        } catch (ex: Exception) {
            Log.ex(TAG, ex)
        }

        return result
    }

    private fun postWordlistURL(intent: Intent): Boolean {
        val data = intent.data
        var result = null != data
        if (result) {
            // dfl: In case it's a BYOD download
            val dfl: DownloadFinishedListener =
                object : DownloadFinishedListener {
                    override fun downloadFinished(
                        ignore: ISOCode,
                        name: String,
                        success: Boolean
                    ) {
                        val resid = if (success) R.string.byod_success
                        else R.string.byod_failure
                        val builder =
                            makeOkOnlyBuilder(resid, name)
                        if (success) {
                            builder.setActionPair(
                                Action.OPEN_BYOD_DICT,
                                R.string.button_open_dict
                            )
                                .setParams(name)
                        }
                        builder.show()
                    }
                }
            result = UpdateCheckReceiver
                .postedForDictDownload(mActivity, data!!, dfl)
        }
        return result
    }

    private fun downloadDictUpgrade(intent: Intent): Boolean {
        return UpdateCheckReceiver.downloadPerNotification(mActivity, intent)
    }

    private fun startNewNetGame(nli: NetLaunchInfo): Boolean {
        var handled = nli.isValid

        if ( handled ) {
            launch {
                GameMgr.addForInvite(nli)
            }
        }

        return handled
    } // startNewNetGame

    private fun startNewNetGame(intent: Intent): Boolean {
        var handled = false
        var nli =
            if (MultiService.isMissingDictIntent(intent)) {
                MultiService.getMissingDictData(mActivity, intent)
            } else {
                intent.data?.let {
                    NetLaunchInfo(mActivity, it)
                }
            }
        if (null != nli && nli.isValid) {
            handled = startNewNetGame(nli)
        }
        return handled
    } // startNewNetGame

    private fun loadConfig(intent: Intent): Boolean {
        var success = false
        val data = intent.data
        if (null != data) {
            try {
                val path = data.path
                val prefix = LocUtils.getString(mActivity, R.string.conf_prefix)
                Log.d(TAG, "loadConfig(): path: %s; prefix: %s", path, prefix)
                if (path!!.startsWith(prefix)) {
                    makeConfirmThenBuilder(Action.APPLY_CONFIG, R.string.apply_config)
                        .setPosButton(R.string.button_apply_config)
                        .setNegButton(android.R.string.cancel)
                        .setParams(data.toString())
                        .show()
                    success = true
                }
            } catch (ex: Exception) {
                Log.ex(TAG, ex)
            }
        }

        return success
    }

    private suspend fun startHasGameID(gameID: Int): Boolean {
        var handled = false
        GameMgr.getFor(gameID)?.let { gr ->
            if (checkWarnNoDict(gr)) {
                launchGame(gr)
            }
            handled = true
        }
        return handled
    }

    private suspend fun startHasGameID(intent: Intent): Boolean {
        var handled = false
        val gameID = intent.getIntExtra(GAMEID_EXTRA, 0)
        if (0 != gameID) {
            handled = startHasGameID(gameID)
        }
        return handled
    }

    private fun startConfig(intent: Intent): Boolean {
        val gr = intent.getLongExtra(CONFIG_GAMEREF_EXTRA, 0)
        val handled = 0L != gr
        if (handled) {
            GameConfigDelegate.editForResult(
                getDelegator(), RequestCode.CONFIG_GAME, GameRef(gr)
            )
        }
        return handled
    }

    // Create a new game that's a copy, sending invitations via the means it
    // used to connect.
    private fun startRematch(intent: Intent): Boolean {
        val handled =
            intent.getStringExtra(REMATCH_DATA)?.let {
                showDialogFragment(DlgID.GAMES_LIST_NAME_REMATCH, it)
                true
            } ?: false
        return handled
    }

    private fun startRematchWithName(extras: JSONObject, parent: GameRef, gameName: String?,
                                     ro: RematchOrder, showRationale: Boolean,
                                     archiveAfter: Boolean, deleteAfter: Boolean)
    {
        if (null != gameName && 0 < gameName.length) {
            // should default be 0 not -1, which is all bits set ?? PENDING
            val bits = extras.optInt(REMATCH_ADDRS_EXTRA, -1)
            val addrs =
                if ( bits != -1 ) CommsConnTypeSet(bits)
                else null
            val hasSMS = addrs?.contains(CommsConnType.COMMS_CONN_SMS) ?: false
            if (!hasSMS || null != SMSPhoneInfo.get(mActivity)) {
                rematchWithNameAndPerm(parent, gameName, ro, archiveAfter, deleteAfter)
            } else {
                val id =
                    if (1 == addrs.size) R.string.phone_lookup_rationale_drop
                    else R.string.phone_lookup_rationale_others
                val msg = """
                    ${getString(R.string.phone_lookup_rationale)}
                    
                    ${getString(id)}
                    """.trimIndent()
                Perms23.tryGetPerms(
                    this, Perms23.NBS_PERMS, msg,
                    Action.ASKED_PHONE_STATE, parent, gameName, ro, addrs
                )
            }
        }
    }

    private fun rematchWithNameAndPerm(granted: Boolean, params: Array<Any?>)
    {
        val parent = params[0] as GameRef
        val gameName = params[1] as String
        val ro = params[2] as RematchOrder
        val addrs = params[3] as CommsConnTypeSet
        val archiveAfter = (params.size >= 5) and params[4] as Boolean
        val deleteAfter = (params.size >= 6) and params[5] as Boolean

        if (!granted) {
            addrs.remove(CommsConnType.COMMS_CONN_SMS)
        }
        if (0 < addrs.size) {
            rematchWithNameAndPerm(parent, gameName, ro, archiveAfter, deleteAfter)
        }
    }

    private fun rematchWithNameAndPerm(
        parent: GameRef, gameName: String?, ro: RematchOrder,
        archiveAfter: Boolean, deleteAfter: Boolean
    ) {
        launch {
            val gr = parent.makeRematch(gameName, ro, archiveAfter, deleteAfter)
            doOpenGame(gr)
        }
    }

    private fun tryAlert(intent: Intent): Boolean {
        var handled = false
        val msg = intent.getStringExtra(ALERT_MSG)
        if (null != msg) {
            val builder =
                makeOkOnlyBuilder(msg)
            if (intent.getBooleanExtra(WITH_EMAIL, false)) {
                builder.setActionPair(
                    Action.SEND_EMAIL,
                    R.string.board_menu_file_email
                )
            }
            builder.show()
            handled = true
        }
        return handled
    }

    private fun tryInviteIntent(intent: Intent): Boolean {
        var result = false
        getFromIntent(intent)?.let {
            val nli = NetLaunchInfo.makeFrom(mActivity, it)
            if (null != nli && nli.isValid) {
                startNewNetGame(nli)
                result = true
            } else {
                Assert.failDbg()
            }
        }
        return result
    }

    private fun tryKAConfigIntent(intent: Intent): Boolean {
        val used = KAConfigView.isMyIntent(intent)
        if (used) {
            launchKAConfigOnce()
        }
        return used
    }

    private fun launchKAConfigOnce() {
        if (!KAConfigView.alreadyRunning()) {
            showDialogFragment(DlgID.KACONFIG)
        }
    }

    private fun askDefaultName() {
        val name = CommonPrefs.getDefaultPlayerName(mActivity, 0, true)
        CommonPrefs.setDefaultPlayerName(mActivity, name)
        showDialogFragment(DlgID.GET_NAME)
    }

    private fun getDictForLangIf()
    {
        if (!m_haveShownGetDict &&
                !XWPrefs.getPrefsBoolean(
                    mActivity, R.string.key_got_langdict,
                    false
                )
        ) {
            m_haveShownGetDict = true

            val isoCode = LocUtils.getCurLangCode(mActivity)
            if (!isoCode!!.equals(Utils.ISO_EN)) {
                val names = DictLangCache.getHaveLang(mActivity, isoCode)
                if (0 == names.size) {
                    val lstnr: OnGotLcDictListener = object : OnGotLcDictListener {
                        override fun gotDictInfo(
                            success: Boolean, isoCode: ISOCode,
                            name: String?
                        ) {
                            stopProgress()
                            if (success) {
                                val langName = DictLangCache
                                    .getLangNameForISOCode(mActivity, isoCode!!)
                                makeConfirmThenBuilder(
                                    Action.DWNLD_LOC_DICT,
                                    R.string.confirm_get_locdict_fmt,
                                    langName
                                )
                                    .setPosButton(R.string.button_download)
                                    .setNegButton(R.string.button_no)
                                    .setNAKey(R.string.key_got_langdict)
                                    .setParams(isoCode, name)
                                    .show()
                            }
                        }
                    }

                    val locLang = DictLangCache
                        .getLangNameForISOCode(mActivity, isoCode)
                    val msg = getString(R.string.checking_for_fmt, locLang)
                    startProgress(R.string.checking_title, msg)
                    DictsDelegate.downloadDefaultDict(mActivity, isoCode, lstnr)
                }
            }
        }
    }

    private fun updateField() {
        // val newField = CommonPrefs.getSummaryFieldId(mActivity)
        // if (m_adapter!!.setField(newField)) {
        //     // The adapter should be able to decide whether full
        //     // content change is required.  PENDING
        //     mkListAdapter()
        // }
    }

    private fun buildRenamer(name: String?, labelID: Int): Renamer {
        val renamer = (inflate(R.layout.renamer) as Renamer)
            .setName(name)
            .setLabel(labelID)
        return renamer
    }

    private fun buildRenamer(gr: GameRef): Renamer {
        val renamer = (inflate(R.layout.renamer) as Renamer)
            .setGR(gr)
        return renamer
    }

    private fun showNewGroupIf() {
        val games = m_mySIS!!.moveAfterNewGroup
        if (null != games) {
            launch {
                m_mySIS!!.moveAfterNewGroup = null
                val namesMap = GameMgr.getGroupsMap()
                showDialogFragment(DlgID.CHANGE_GROUP, games, namesMap)
            }
        }
    }

    private fun mkDeleteAlert(msg: String, grs: Array<GameRef>, skipTell: Boolean) {
        makeConfirmThenBuilder(Action.DELETE_GAMES, msg)
            .setPosButton(R.string.button_delete)
            .setParams(grs, skipTell)
            .show()
    }

    private fun deleteIfConfirmed(grs: Array<GameRef>, skipTell: Boolean) {
        val msg = getQuantityString(
            R.plurals.confirm_seldeletes_fmt,
            grs.size, grs.size
        )
        mkDeleteAlert(msg, grs, skipTell)
    }

    private fun deleteNamedIfConfirmed(grs: Array<GameRef>, skipTell: Boolean) {
        launch {
            val names = grs.map{
                it.getGI()!!.gameName
            }
            val namesStr = TextUtils.join(", ", names)
            val msg = getQuantityString(
                R.plurals.confirm_nameddeletes_fmt,
                names.size, namesStr
            )
            mkDeleteAlert(msg, grs, skipTell)
        }
    }

    private fun deleteGames(grs: Array<GameRef>, skipTell: Boolean) {
        for (gr in grs) {
            GameMgr.deleteGame(gr);
            m_mySIS!!.selGames.remove(gr)
        }
        invalidateOptionsMenuIf()
        setTitle()
    }

    private fun makeNewNetGameIf(): Boolean {
        val madeGame = null != m_netLaunchInfo
        if (madeGame) {
            makeNewNetGame(m_netLaunchInfo)
            m_netLaunchInfo = null
        }
        return madeGame
    }

    private fun setSelGame(gr: GameRef) {
        clearSelections(false)
        m_mySIS!!.selGames.add(gr)

        findViewFor(gr)?.setSelected(true)
        invalidateOptionsMenuIf()
        setTitle()
    }

    private fun clearSelections(updateStuff: Boolean = true) {
        var inval = clearSelectedGames()
        inval = clearSelectedGroups() || inval
        if (updateStuff && inval) {
            invalidateOptionsMenuIf()
            setTitle()
        }
    }

    private fun clearGames(grs: Set<GameRef>) {
        grs.map {
            findViewFor(it)?.setSelected(false)
        }
    }

    private fun clearSelectedGames(): Boolean {
        // clear any selection
        val sg = m_mySIS!!.selGames
        val needsClear = 0 < sg.size
        if (needsClear) {
            clearGames(HashSet(sg)) // copy!!
            sg.clear()
        }
        return needsClear
    }

    private fun clearSelectedGroups(): Boolean {
        // clear any selection
        val needsClear = 0 < m_mySIS!!.selGroupIDs.size
        if (needsClear) {
            // m_adapter!!.clearSelectedGroups(m_mySIS!!.selGroupIDs)
            // m_mySIS!!.selGroupIDs.clear()
        }
        return needsClear
    }

    private fun launchGameIf(): Boolean {
        return m_missingDictGR?.let { gr ->
            // save in case checkWarnNoDict needs to set them
            val menuID = m_missingDictMenuId
            m_missingDictGR = null
            m_missingDictMenuId = -1

            if (R.id.games_game_reset == menuID) {
                val grs = Array<GameRef>(1,{gr})
                doConfirmReset(grs)
            } else if (checkWarnNoDict(gr)) {
                GameUtils.launchGame(getDelegator(), gr)
            }
            true
        } ?: false
        // return madeGame
    }

    private fun launchGame(gr: GameRef, extras: Bundle? = null) {
        Log.d(TAG, "launchGame($gr)")
        if (!BoardDelegate.gameIsOpen(gr)) {
            // if (m_adapter!!.inExpandedGroup(gr)) {
            setSelGame(gr)
            // }
            Log.d(TAG, "launchGame($gr): calling GameUtils.launchGame")
            GameUtils.launchGame(getDelegator(), gr, extras)
        }
    }

    private fun makeNewNetGame(nli: NetLaunchInfo?) {
        GameUtils.makeNewMultiGame1(mActivity, nli!!)?.let { gr ->
            launchGame(gr)
        }
    }

    private fun tryStartsFromIntent(intent: Intent) {
        Log.d(TAG, "tryStartsFromIntent(extras={%s})", DbgUtils.extrasToString(intent))
        launch {
            val handled = startFirstHasDict(intent)
                || startWithInvitee(intent)
                || postWordlistURL(intent)
                || downloadDictUpgrade(intent)
                || loadConfig(intent)
                || startNewNetGame(intent)
                || startHasGameID(intent)
                || startRematch(intent)
                || startConfig(intent)
                || tryAlert(intent)
                || tryInviteIntent(intent)
                || tryKAConfigIntent(intent)

            Log.d(TAG, "tryStartsFromIntent() => handled: %b", handled)
        }
    }

    private fun doOpenGame(gr: GameRef) {
        checkWarnDictElse(gr) {
            launchGame(gr)
        }
    }

    private val selRowIDs: Array<GameRef>
        get() {
            return m_mySIS!!.selGames.toTypedArray()

            // val result = Array<GameRef>(m_mySIS!!.selGames.size)
            // var ii = 0
            // val iter: Iterator<GameRef> = m_mySIS!!.selGames.iterator()
            // while (iter.hasNext()) {
            //     result[ii++] = iter.next()
            // }
            // return result
        }

    private val selGroupIDs: IntArray
        get() {
            val result = IntArray(m_mySIS!!.selGroupIDs.size)
            var ii = 0
            val iter: Iterator<Int> = m_mySIS!!.selGroupIDs.iterator()
            while (iter.hasNext()) {
                result[ii++] = iter.next()
            }
            return result
        }

    private fun showItemsIf(items: IntArray, menu: Menu, select: Boolean) {
        for (item in items) {
            Utils.setItemVisible(menu, item, select)
        }
    }

    private fun doConfirmReset(grs: Array<GameRef>) {
        val msg = getQuantityString(
            R.plurals.confirm_reset_fmt,
            grs.size, grs.size
        )
        makeConfirmThenBuilder(Action.RESET_GAMES, msg)
            .setPosButton(R.string.button_reset)
            .setParams(grs)
            .show()
    }

    private fun mkListAdapter() {
        // DbgUtils.logf( "GamesListDelegate.mkListAdapter()" );
        // m_adapter = GameListAdapter()
        // setListAdapterKeepScroll(m_adapter!!)

        // val listView = listView
        // mActivity.registerForContextMenu(listView)

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
    private fun askingChangeName(name: String?, doConfigure: Boolean): Boolean {
        var asking = false
        val skipAsk = XWPrefs.getPrefsBoolean(
            mActivity, R.string.key_notagain_dfltname, false
        )
        if (!skipAsk) {
            val name1 = CommonPrefs.getDefaultPlayerName(
                mActivity, 0, false
            )
            val name2 = CommonPrefs.getDefaultOriginalPlayerName(mActivity, 0)
            if (name1 == name2) {
                asking = true
                makeConfirmThenBuilder(
                    Action.NEW_GAME_DFLT_NAME,
                    R.string.not_again_dfltname_fmt, name2
                )
                    .setNAKey(R.string.key_notagain_dfltname)
                    .setNegButton(R.string.button_later)
                    .setParams(name, doConfigure)
                    .show()
            }
        }
        return asking
    }

    private fun makeThenLaunchOrConfigure(): Boolean {
        val handled = null != m_newGameParams
        if (handled) {
            val params = m_newGameParams!!
            m_newGameParams = null
            val name = params[0] as String
            val doConfigure = params[1] as Boolean
            makeThenLaunchOrConfigure(name, doConfigure, true)
        }
        return handled
    }

    private fun makeThenLaunchOrConfigure(
        name: String?, doConfigure: Boolean,
        skipAsk: Boolean, invitee: CommsAddrRec? = null
    ) {
        if (skipAsk || !askingChangeName(name, doConfigure)) {
            // If we're configuring, we don't create a game yet.

            if (doConfigure) {
                GameConfigDelegate.configNewForResult(
                    getDelegator(),
                    RequestCode.CONFIG_GAME,
                    name, m_mySIS!!.nextIsSolo
                )
            } else {
                // val rowID: Long
                // val groupID =
                //     if (1 == m_mySIS!!.selGroupIDs.size) {
                //         m_mySIS!!.selGroupIDs.iterator().next()
                //     } else {
                //         DBUtils.GROUPID_UNSPEC

                //     }
                val gi = CurGameInfo(mActivity)
                    .addDefaults(mActivity, m_mySIS!!.nextIsSolo, name)
                Log.d(TAG, "makeThenLaunchOrConfigure(): gi: $gi")
                makeAndLaunch(gi, invitee)
            }
        }
    }

    fun makeAndLaunch(gi: CurGameInfo, invitee: CommsAddrRec? = null) {
        launch {
            Log.d(TAG, "makeAndLaunch: calling newFor($gi, invitee=$invitee)")
            val gr = GameMgr.newFor(gi, invitee)
            GameUtils.launchGame(getDelegator(), gr!!)
        }
    }

    private fun launchLikeRematch(hostAddr: CommsAddrRec, gameName: String) {
        val intent = makeSelfIntent(mActivity)
            .putExtra(INVITEE_REC_EXTRA, hostAddr as Serializable)
            .putExtra(REMATCH_NEWNAME_EXTRA, gameName)

        startActivity(intent)
    }

    private fun getFromIntent(intent: Intent): ByteArray? {
        var result: ByteArray? = null

        val action = intent.action
        if (INVITE_ACTION == action) {
            result = intent.getByteArrayExtra(INVITE_DATA)
        }

        // Log.d( TAG, "getFromIntent() => %s", result );
        return result
    }

    inner class GameViewHolder(val mGameListElem: GameListElem):
        RecyclerView.ViewHolder(mGameListElem),
        View.OnClickListener, View.OnAttachStateChangeListener
    {
        private var mPosition = -1
        var mGR: GameRef? = null
        var mGrp: GroupRef? = null

        override fun onViewAttachedToWindow(v: View) { tryLoad() }
        override fun onViewDetachedFromWindow(v: View) {}

        override fun onClick(view: View) {
            mGR?.let {
                GameUtils.launchGame(getDelegator(), it)
            }
        }

        fun bind(position: Int) {
            mPosition = position
            mAdapter!!.note(position, this)
            tryLoad()
        }

        private fun tryLoad() {
            if (0 <= mPosition) {
                launch {
                    val item = GameMgr.getNthItem(mPosition)
                    if ( item.isGame() ) {
                        mGR = item.toGame()
                        val selected = m_mySIS!!.selGames.contains(mGR)
                        mGameListElem.load(mGR!!, this@GamesListDelegate,
                                           mHandler, selected)
                    } else {
                        item.toGroup().let { grp ->
                            mGrp = grp
                            mGameListElem.load(grp, this@GamesListDelegate)
                        }
                    }
                }
            }
        }
    }

    inner class GamesViewAdapter(val nGames: Int) :
        RecyclerView.Adapter<GameViewHolder>() {
        init {
            Log.d(TAG, "GamesViewAdapter.init()")
        }
        override fun getItemCount(): Int { return nGames }
        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int)
            : GameViewHolder
        {
            val view = inflate(R.layout.game_list_elem)
            mActivity.registerForContextMenu(view)
            return GameViewHolder(view as GameListElem)
        }

        override fun onBindViewHolder(holder: GameViewHolder,
                                      position: Int)
        {
            holder.bind(position)
        }

        private val mPositions = HashMap<GameViewHolder, Int>()

        // note():
        // RecyclerViews report a size (# of children) based on what's
        // visible, so a child can be excluded even if the view is still valid
        // and will be used, unchanged, if scrolled back into view. So to
        // support lookup (findHolderFor()) we need to keep track ourselves of
        // what GameViewHolders are in play.
        fun note(position: Int, holder: GameViewHolder) {
            mPositions[holder] = position
        }

        fun findHolderFor(gr: GameRef): GameViewHolder? {
            val result = mPositions.keys.firstNotNullOfOrNull {
                if (it.mGR?.equals(gr)?:false) it else null
            }
            if (null == result) {
                Log.d(TAG, "findHolderFor(gr=$gr) found NOTHING")
            }
            return result
        }

        fun findHolderFor(grp: GroupRef): GameViewHolder? {
            val result = mPositions.keys.firstNotNullOfOrNull {
                if (it.mGrp?.equals(grp)?:false) it else null
            }
            if (null == result) {
                Log.d(TAG, "findHolderFor(grp=$grp) found NOTHING")
            }
            return result
        }
    }

    companion object {
        private val TAG: String = GamesListDelegate::class.java.simpleName

        private const val SAVE_NEXTSOLO = "SAVE_NEXTSOLO"
        private val SAVE_MYSIS = TAG + "/MYSIS"

        private const val ROWID_EXTRA = "rowid"
        private const val GAMEREF_EXTRA = "gr"
        private const val GAMEID_EXTRA = "gameid"
        private const val INVITEE_REC_EXTRA = "invitee_rec"
        private const val REMATCH_GAMEREF_EXTRA = "rm_gr"
        private const val REMATCH_DATA = "rm_data"
        private const val REMATCH_NEWNAME_EXTRA = "rm_nnm"
        private const val REMATCH_DELAFTER_EXTRA = "del_after"
        private const val REMATCH_ARCHIVEAFTER_EXTRA = "arch_after"
        private const val REMATCH_IS_SOLO = "rm_solo"
        private const val REMATCH_ADDRS_EXTRA = "rm_addrs"

        // private const val CONFIG_ROWID_EXTRA = "conf_rowid"
        private const val CONFIG_GAMEREF_EXTRA = "conf_gr"
        private const val INVITE_ACTION = "org.eehouse.action_invite"
        private const val INVITE_DATA = "data_invite"

        private const val ALERT_MSG = "alert_msg"
        private const val WITH_EMAIL = "with_email"

        private val DEBUG_ITEMS = intArrayOf(
            R.id.games_menu_writegit,
            R.id.games_submenu_logs,
            R.id.games_submenu_stats,
            R.id.games_submenu_mqtt,
            R.id.games_menu_restart,
        )
        private val NOSEL_ITEMS = intArrayOf(
            R.id.games_menu_newgroup,
            R.id.games_menu_prefs,
            R.id.games_menu_dicts,
            R.id.games_menu_about,
            R.id.games_menu_email,
        )
        private val ONEGAME_ITEMS = intArrayOf(
            R.id.games_game_config,
            R.id.games_game_rename,
            R.id.games_game_rematch,
        )

        private val ONEGROUP_ITEMS = intArrayOf(
            R.id.games_group_rename,
        )

        private var s_firstShown = false
        private var s_self: WeakReference<GamesListDelegate>? = null
        private var sAsked = false
        private val GROUP_POSNS_KEY = TAG + "/group_posns"
        fun boardDestroyed(gr: GameRef) {
            s_self?.get()?.invalidateOptionsMenuIf()
        }

        fun onGameDictDownload(context: Context, intent: Intent) {
            intent.setClass(context, MainActivity::class.java)
            addLaunchFlags(intent)
            context.startActivity(intent)
        }

        fun makeSelfIntent(context: Context): Intent {
            val intent = Intent(context, MainActivity::class.java)
            addLaunchFlags(intent)
            return intent
        }

        private fun addLaunchFlags(intent: Intent) {
            intent.setFlags(
                Intent.FLAG_ACTIVITY_CLEAR_TOP
                        or Intent.FLAG_ACTIVITY_SINGLE_TOP
            )
            // FLAG_ACTIVITY_CLEAR_TASK -- don't think so
        }

        // fun makeRowidIntent(context: Context, rowid: Long): Intent {
        //     val intent = makeSelfIntent(context)
        //         .putExtra(ROWID_EXTRA, rowid)
        //     return intent
        // }

        fun makeGamerefIntent(context: Context, gr: GameRef): Intent {
            return makeSelfIntent(context)
                .putExtra(GAMEREF_EXTRA, gr.gr)
        }

        fun makeGameIDIntent(context: Context, gameID: Int): Intent {
            val intent = makeSelfIntent(context)
                .putExtra(GAMEID_EXTRA, gameID)
            return intent
        }

        fun makeRematchIntent(
            context: Context, gr: GameRef,
            gi: CurGameInfo, addrTypes: CommsConnTypeSet?,
            archiveAfter: Boolean, deleteAfter: Boolean
        ): Intent {
            val isSolo = gi.serverRole == CurGameInfo.DeviceRole.SERVER_STANDALONE
            val extras = JSONObject()
                .put(REMATCH_GAMEREF_EXTRA, gr.gr)
                .put(REMATCH_IS_SOLO, isSolo)
                .put(REMATCH_ARCHIVEAFTER_EXTRA, archiveAfter)
                .put(REMATCH_DELAFTER_EXTRA, deleteAfter)
            val intent = makeSelfIntent(context)
                .putExtra(REMATCH_DATA, extras.toString())

            if (null != addrTypes) {
                Assert.assertTrueNR(!addrTypes.contains(CommsConnType.COMMS_CONN_RELAY))
                intent.putExtra(REMATCH_ADDRS_EXTRA, addrTypes.toInt())
            }
            return intent
        }

        fun makeAlertIntent(context: Context, msg: String): Intent {
            val intent = makeSelfIntent(context)
                .putExtra(ALERT_MSG, msg)
            return intent
        }

        fun postReceivedInvite(context: Context, data: ByteArray) {
            val intent = makeSelfIntent(context)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)

            populateInviteIntent(context, intent, data)
            context.startActivity(intent)
        }

        private fun populateInviteIntent(
            context: Context, intent: Intent,
            data: ByteArray
        ) {
            val nli = NetLaunchInfo.makeFrom(context, data)
            if (null != nli) {
                intent.setAction(INVITE_ACTION)
                    .putExtra(INVITE_DATA, data)
            } else {
                Assert.failDbg()
            }
        }

        fun openGame(context: Context, data: Uri?) {
            val intent = makeSelfIntent(context)
                .setData(data)
            context.startActivity(intent)
        }

        fun launchGameConfig(context: Context, gr: GameRef) {
            val intent = makeSelfIntent(context)
                .putExtra(CONFIG_GAMEREF_EXTRA, gr.gr)
            context.startActivity(intent)
        }

        fun onGroupChanged(context: Context, grp: GroupRef, flags: Int) {
            Log.d(TAG, "onGroupChanged(grp=$grp, flags=0x%x)".format(flags))

            s_self?.get()?.let {
                        if ( 0 != flags and
                             (DUtilCtxt.GRCE_ADDED
                              or DUtilCtxt.GRCE_DELETED
                              or DUtilCtxt.GRCE_MOVED) ) {
                            it.initGamesView()
                        } else if ( 0 != flags and
                                    (DUtilCtxt.GRCE_RENAMED
                                     or DUtilCtxt.GRCE_COLLAPSED
                                     or DUtilCtxt.GRCE_EXPANDED
                                     or DUtilCtxt.GRCE_GAMES_REORDERED
                                     or DUtilCtxt.GRCE_GAME_ADDED
                                     or DUtilCtxt.GRCE_GAME_REMOVED ) ) {
                            it.runOnUiThread {
                                it.findViewFor(grp)?.reload()
                            }
                        }
                    }
        }

        fun clearThumbnails() {
            GameMgr.clearThumbnails()
            s_self?.get()?.initGamesView()
        }
    } // companion object
}
