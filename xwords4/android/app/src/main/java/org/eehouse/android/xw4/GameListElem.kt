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

import android.content.Context
import android.os.Handler
import android.util.AttributeSet
import android.view.View
import android.widget.FrameLayout
import androidx.lifecycle.LifecycleCoroutineScope

import org.eehouse.android.xw4.jni.GameMgr
import org.eehouse.android.xw4.jni.GameRef

class GameListElem(mContext: Context, aset: AttributeSet?) :
    FrameLayout(mContext, aset) {

    var isGame: Boolean = false
        private set
    private val TAG: String = GameListElem::class.java.simpleName
    private var mGLI: GameListItem? = null
    private var mGLG: GameListGroup? = null

    fun getGLI(): GameListItem? {
        // Log.d(TAG, "getGLI() => $mGLI")
        return mGLI
    }

    fun getGLG(): GameListGroup? {
        // Log.d(TAG, "getGLG() => $mGLG")
        return mGLG
    }

    fun load(gr: GameRef, delegate: GamesListDelegate,
             field: Int, handler: Handler, selected: Boolean,
             scope: LifecycleCoroutineScope) {
        isGame = true
        findViewById<View>(R.id.group).visibility = GONE
        mGLG = null

        findViewById<GameListItem>(R.id.game)!!.let {
            mGLI = it
            it.visibility = VISIBLE
            it.load(gr, delegate, field, handler, selected, scope)
        }
    }

    fun load(grp: GameMgr.GroupRef, gcb: GroupStateListener) {
        Log.d(TAG, "load($grp)")
        isGame = false
        mGLI = null
        findViewById<View>(R.id.game).visibility = GONE

        findViewById<GameListGroup>(R.id.group)!!.let {
            mGLG = it
            it.visibility = VISIBLE
            it.load(grp, gcb)
        }
    }
}
