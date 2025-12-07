/*
 * Copyright 2025 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.Dialog
import android.content.Context
import android.util.AttributeSet
import android.view.View
import android.widget.Button
import android.widget.CheckBox
import android.widget.CompoundButton
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.TextView

import kotlinx.coroutines.Dispatchers

import java.io.Serializable

import org.eehouse.android.xw4.jni.GameMgr
import org.eehouse.android.xw4.jni.GameMgr.GroupRef
import org.eehouse.android.xw4.jni.GameRef
import org.eehouse.android.xw4.loc.LocUtils

private val TAG: String = GameConvertView::class.java.simpleName

class GameConvertView(val mContext: Context, attrs: AttributeSet)
    : LinearLayout( mContext, attrs ), View.OnClickListener,
      RadioGroup.OnCheckedChangeListener, CompoundButton.OnCheckedChangeListener
{
    private data class GroupGames(val groupID: Long, val games: ArrayList<Long>):
        Serializable
    private var mGroup: RadioGroup? = null
    private var mGroupName: String? = null
    private var mGroupIndex: Int = -1
    private var mGroupCount: Int = 0
    private var mAllText: String? = LocUtils.getString(mContext, R.string.loc_filters_all)
    private var mDialog: Dialog? = null

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        findViewById<Button>(R.id.convert_one).setOnClickListener(this)
        findViewById<Button>(R.id.convert_all).setOnClickListener(this)
        mGroup = findViewById<RadioGroup>(R.id.groups_group).also {
            it.setOnCheckedChangeListener(this@GameConvertView)
        }
        findViewById<CheckBox>(R.id.menuonly_check).also {
            it.setChecked(skipSet(context))
            it.setOnCheckedChangeListener(this@GameConvertView)
        }
        updateExpl()
    }

    // RadioGroup.OnCheckedChangeListener
    override fun onClick(view: View?) {
        view?.let { view ->
            launch {
                when (view.id) {
                    R.id.convert_all, R.id.convert_one -> {
                        convert(R.id.convert_all == view.id, mGroupName)
                        invalData()
                    }
                }
            }
        }
    }

    // CompoundButton.OnCheckedChangeListener
    override fun onCheckedChanged(view: CompoundButton, isChecked: Boolean) {
        Assert.assertTrue(view.id == R.id.menuonly_check)
        DBUtils.setBoolFor(context, SKIP_ON_LAUNCH, isChecked)
    }

    private fun setDialog(dialog: Dialog) { mDialog = dialog }

    private suspend fun convert(doAll: Boolean, groupName: String? = null,
                                pbar: ProgressBar? = null ) {
        Log.d(TAG, "convert($groupName)")
        val pbar =
            if (null == pbar) {
                findViewById<ProgressBar>(R.id.progress).also {
                    it.setMax(mGroupCount)
                    it.setProgress(0)
                }
            } else pbar

        val mmap = loadState(context)
        if (null == groupName || groupName.equals(mAllText) ) {
            for (key in mmap.keys) {
                convert(doAll, key, pbar)
                if (!doAll) break
            }
        } else {
            mmap[groupName]?.let { groupGames ->
                // First create the group if necessary
                val grp =
                    if (groupGames.groupID == DBUtils
                            .getArchiveGroup(context)) {
                        GroupRef.GROUP_ARCHIVE
                    } else {
                        GameMgr.getGroup(groupName)
                            ?: GameMgr.addGroup(groupName)
                    }

                // Now add games
                var nTried = pbar.getProgress()
                grp.setGroupCollapsed(true)
                for (rowid in groupGames.games) {
                    pbar.setProgress(++nTried)
                    val newGr = DBUtils.loadGame(context, rowid)?.also {
                        GameMgr.convertGame(it.name, grp, it.bytes)?.let { gr ->
                            gr.getGI()?.let { gi ->
                                val locs = gi.playersLocal()
                                DBUtils.getChatHistory(context, rowid, locs).map { item ->
                                    gr.addConvertChat(item)
                                }
                            }
                        }
                    }
                    if (!doAll && null != newGr) break
                    Log.d(TAG, "convert(): continuing")
                }
            }
        }
        updateExpl()
    }

    private fun updateButtons() {
        mGroupName?.let {
            findViewById<Button>(R.id.convert_one).text = "Convert next in " + it
            findViewById<Button>(R.id.convert_all).text = "Convert " + it
        }
    }
    
    private fun setClicked(name: String, indx: Int, count: Int) {
        mGroupName = name
        mGroupIndex = indx
        mGroupCount = count
        // Log.d(TAG, "radio $indx called (name = $name)")
        updateButtons()
    }

    private fun addButton(groupName: String, count: Int, click: Boolean = false) {
        mGroup!!.let { group ->
            val radio = RadioButton(context).also {
                val curIndex = group.childCount
                val txt = LocUtils
                    .getString(mContext, R.string.convert_button_fmt, groupName, count )
                it.setText(txt)
                group.addView(it)
                it.setOnClickListener { view ->
                    setClicked(groupName, curIndex, count)
                }
                if (click || curIndex == mGroupIndex) {
                    it.performClick()
                }
            }
        }
    }

    private fun updateExpl() {
        mGroup?.let { group ->
            launch {
                val mmap = loadState(mContext)
                val numGames = mmap.values.sumOf{it.games.size}
                if ( 0 == numGames ) {
                    mDialog?.dismiss()
                } else {
                    findViewById<TextView>(R.id.convert_expl).text =
                        LocUtils.getString(context, R.string.game_convert_expl, numGames)

                    group.removeAllViews()
                    addButton(mAllText!!, numGames, true)
                    mmap.map { (name, groupGames) ->
                        addButton(name, groupGames.games.size)
                    }
                }
            }
        }
    }

    // PENDING: should cache this state and modify the map as the conversion
    // progresses. We'll see if my 500-game archive requires something more
    // effecient.
    override fun onCheckedChanged(group: RadioGroup?, checkedId: Int) {
        Log.d(TAG, "onCheckedChanged()")
    }

    companion object {
        private val DATA_KEY = TAG + "_DATA"
        private val SKIP_ON_LAUNCH = TAG + "_SKIP"
        suspend fun haveToConvert(context: Context, isLaunch: Boolean = false): Boolean {
            val result =
                if (isLaunch && skipSet(context)) false
                else {
                    val state = loadState(context)
                    val numGames = state.values.sumOf{it.games.size}
                    0 < numGames
                }
            return result
        }

        private fun skipSet(context: Context): Boolean {
            return DBUtils.getBoolFor(context, SKIP_ON_LAUNCH, false)
        }

        fun makeDialog(context: Context): Dialog? {
            val view = LocUtils.inflate(context, R.layout.game_convert_view)
                as GameConvertView
            val dialog = LocUtils.makeAlertBuilder(context)
                .setView(view)
                .setPositiveButton(R.string.button_done, null)
                .create()
            view.setDialog(dialog)
            return dialog
        }

        private var sData: HashMap<String, GroupGames>? = null
        private suspend fun loadState(context: Context): HashMap<String, GroupGames> {
            if (null == sData) {
                sData =
                    DBUtils.getSerializableFor(context, DATA_KEY)?.let {
                        it as HashMap<String, GroupGames>
                    } ?: run {
                        val result = HashMap<String, GroupGames>()
                        DBUtils.getGroups(context).map { (groupID, info) ->
                            info?.let { info ->
                                val groupName = info.m_name
                                val needsConvert = ArrayList<Long>()
                                DBUtils.getGroupGames(context, groupID).map { rowid ->
                                    DBUtils.loadGame(context, rowid)?.let { gv ->
                                        val gr = GameMgr.figureGR(gv.bytes)
                                        val exists = GameMgr.gameExists(gr)
                                        if (!exists) {
                                            needsConvert.add(rowid)
                                        }
                                    }
                                }
                                if ( 0 < needsConvert.size ) {
                                    result[groupName] = GroupGames(groupID, needsConvert)
                                    Log.d(TAG, "added ${needsConvert.size} games for group " +
                                                   "$groupName")
                                }
                            }
                        }
                        result
                    }
            }
            return sData!!
        }

        private fun invalData() { sData = null }
    }
}
