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
import android.widget.TextView
import kotlinx.coroutines.Dispatchers

import java.io.Serializable

import org.eehouse.android.xw4.jni.GameMgr
import org.eehouse.android.xw4.jni.GameRef
import org.eehouse.android.xw4.loc.LocUtils

private val TAG: String = GameConvertView::class.java.simpleName

class GameConvertView(val mContext: Context, attrs: AttributeSet)
    : LinearLayout( mContext, attrs ), View.OnClickListener {
    private var mMap: Map<GameRef, Long>? = null

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        findViewById<Button>(R.id.convert_one).setOnClickListener(this)
        findViewById<Button>(R.id.convert_all).setOnClickListener(this)
        updateExpl()
    }

    override fun onClick(view: View?) {
        view?.let { view ->
            when (view.id) {
                R.id.convert_all -> convert(true)
                R.id.convert_one -> convert(false)
            }
        }
    }

    private fun convert(doAll: Boolean) {
        launch() {
            val map = mMap!!

            // create a groups mapping to use with games
            val gmap = HashMap<Long, GameMgr.GroupRef>()
            val groupID = XWPrefs.getDefaultNewGameGroup(context)
            DBUtils.getGroups(context).map { entry ->
                val ggi = entry.value!!
                var grp = GameMgr.getGroup(ggi.m_name)
                if (null == grp) {
                    Log.d(TAG, "converting group ${ggi.m_name}")
                    grp = GameMgr.addGroup(ggi.m_name)
                } else {
                    Log.d(TAG, "group ${ggi.m_name} already exists")
                }
                gmap.put(entry.key, grp)
            }

            // games...
            var done = false
            for (gr in map.keys) {
                val rowid = map[gr]!!
                if (! done) {
                    DBUtils.loadGame(context, rowid)?.let { gv ->
                        val group = gmap.get(gv.group)!!
                        Log.d(TAG, "converting to $gr")
                        val newGr = GameMgr.convertGame(gv.name, group, gv.bytes)
                        done = newGr != null && !doAll
                        Assert.assertTrueNR(null == newGr || newGr!!.equals(gr))
                    }
                }
            }
        }
        updateExpl()
    }

    private fun updateExpl() {
        launch {
            mMap = updateState()
            val numGames = mMap!!.size
            val txt = LocUtils.getString(context, R.string.game_convert_expl, numGames )
            findViewById<TextView>(R.id.convert_expl).text = txt
        }
    }

    // PENDING: should cache this state and modify the map as the conversion
    // progresses. We'll see if my 500-game archive requires something more
    // effecient.
    private suspend fun updateState(): Map<GameRef, Long> {
        // Run all old-format games through to generate a GameRef for them
        val rowids = DBUtils.getGroups(context).map { groupID ->
            DBUtils.getGroupGames(context, groupID.key).map { rowid ->
                rowid
            }
        }.flatten()

        val needsConvert = HashMap<GameRef, Long>()
        rowids.mapNotNull { rowid ->
            DBUtils.loadGame(context, rowid)?.let { gv ->
                val gr = GameMgr.figureGR(gv.bytes)
                val exists = GameMgr.gameExists(gr)
                Log.d(TAG, "got gr: $gr; exists: $exists")
                if ( !exists ) {
                    needsConvert[gr] = rowid
                }
            }
        }
        Log.d(TAG, "got ${needsConvert.keys} of len ${needsConvert.size}")
        return needsConvert
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
