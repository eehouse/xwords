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
import android.widget.LinearLayout
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
    RadioGroup.OnCheckedChangeListener
{
    private data class GroupGames(val groupID: Long, val games: ArrayList<Long>)
    private var mMap: Map<String, GroupGames>? = null
    private var mGroup: RadioGroup? = null
    private var mGroupName: String? = null
    private var mGroupIndex: Int = -1
    private var mAllText: String? = LocUtils.getString(mContext, R.string.loc_filters_all)

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        findViewById<Button>(R.id.convert_one).setOnClickListener(this)
        findViewById<Button>(R.id.convert_all).setOnClickListener(this)
        mGroup = findViewById<RadioGroup>(R.id.groups_group)
        mGroup!!.setOnCheckedChangeListener(this@GameConvertView)
        updateExpl()
    }

    override fun onClick(view: View?) {
        view?.let { view ->
            launch {
                when (view.id) {
                    R.id.convert_all -> convert(true, mGroupName)
                    R.id.convert_one -> convert(false, mGroupName)
                }
            }
        }
    }

    private suspend fun convert(doAll: Boolean, groupName: String? = null) {
        Log.d(TAG, "convert($groupName)")
        val mmap = mMap!!
        if (null == groupName || groupName.equals(mAllText) ) {
            for (key in mmap.keys) {
                convert(doAll, key)
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
                grp.setGroupCollapsed(true)
                for (rowid in groupGames.games) {
                    val newGr = DBUtils.loadGame(context, rowid)?.let {
                        GameMgr.convertGame(it.name, grp, it.bytes)
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
    
    private fun setClicked(name: String, indx: Int) {
        mGroupName = name
        mGroupIndex = indx
        // Log.d(TAG, "radio $indx called (name = $name)")
        updateButtons()
    }

    private fun addButton(groupName: String, count: Int) {
        mGroup!!.let { group ->
            val radio = RadioButton(context).also {
                val curIndex = group.childCount
                val txt = LocUtils
                    .getString(mContext, R.string.convert_button_fmt, groupName, count )
                it.setText(txt)
                group.addView(it)
                it.setOnClickListener { view ->
                    setClicked(groupName, curIndex)
                }
                if (curIndex == mGroupIndex) {
                    it.performClick()
                }
            }
        }
    }

    private fun updateExpl() {
        mGroup?.let { group ->
            launch {
                mMap = updateState()
                val mmap = mMap!!
                val numGames = mmap.values.sumOf{it.games.size}
                val txt = LocUtils.getString(context, R.string.game_convert_expl, numGames )
                findViewById<TextView>(R.id.convert_expl).text = txt

                group.removeAllViews()
                addButton(mAllText!!, numGames)
                mmap.map { (name, groupGames) ->
                    addButton(name, groupGames.games.size)
                }
            }
        }
    }

    // PENDING: should cache this state and modify the map as the conversion
    // progresses. We'll see if my 500-game archive requires something more
    // effecient.
    private suspend fun updateState(): Map<String, GroupGames> {
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
                    Log.d(TAG, "added ${needsConvert.size} games for group $groupName")
                }
            }
        }
        return result
    }

    override fun onCheckedChanged(group: RadioGroup?, checkedId: Int) {
        Log.d(TAG, "onCheckedChanged()")
    }

    companion object {
        fun makeDialog(context: Context): Dialog? {
            val view = LocUtils.inflate(context, R.layout.game_convert_view)
            return LocUtils.makeAlertBuilder(context)
                .setView(view)
                .setPositiveButton(R.string.button_done, null)
                .create()
        }
    }
}
