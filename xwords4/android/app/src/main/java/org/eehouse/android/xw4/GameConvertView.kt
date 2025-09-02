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
import android.widget.LinearLayout
import android.widget.TextView

import java.io.Serializable

import org.eehouse.android.xw4.jni.GameMgr
import org.eehouse.android.xw4.loc.LocUtils

private val TAG: String = GameConvertView::class.java.simpleName

class GameConvertView(val mContext: Context, attrs: AttributeSet)
    : LinearLayout( mContext, attrs ) {
    private var mState: GameConvertState? = null

    class GameConvertState(val names: List<String>,
                           val allGames: ArrayList<String>): Serializable {
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        findViewById<TextView>(R.id.names)
            .setText(mState!!.names.joinToString(separator=", "))
        findViewById<TextView>(R.id.games)
            .setText(mState!!.allGames.joinToString(separator=", "))
    }

    private fun configure(state: GameConvertState) {
        mState = state
    }

    private fun convert() {
        launch {
            val groupID = XWPrefs.getDefaultNewGameGroup(context)
            DBUtils.getGroups(context).map { entry ->
                val ggi = entry.value!!
                val grp = GameMgr.addGroup(ggi.m_name)
                grp.setGroupCollapsed(!ggi.m_expanded)
                if (entry.key == groupID) {
                    GameMgr.makeGroupDefault(grp)
                }
            }
        }
    }
    
    companion object {
        suspend fun needed(context: Context): GameConvertState? {
            val groups = DBUtils.getGroups(context)
            val names =
                groups.keys.mapNotNull { rowid ->
                    groups[rowid]?.let {
                        val name = it.m_name
                        Log.d(TAG, "got group name $name")
                        name
                    }
                }

            val allGames: ArrayList<String> = ArrayList<String>()
            for (groupID in groups.keys) {
                DBUtils.getGroupGames(context, groupID)
                    .map{ allGames.add(it.toString()) }
            }

            return GameConvertState(names, allGames)
        }

        fun makeDialog(context: Context, state: GameConvertState): Dialog? {
            Log.d(TAG, "makeDialog($state)")
            val view = LocUtils.inflate(context, R.layout.game_convert_view)
                as GameConvertView
            view.configure(state)
            val dialog = LocUtils.makeAlertBuilder(context)
                .setView(view)
                .setNegativeButton(android.R.string.cancel, null)
                .setPositiveButton(R.string.button_convert) {dlg, button ->
                    view.convert()
                }
                .create()
            return dialog
        }
    }
}
