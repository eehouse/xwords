/*
 * Copyright 2009-2024 by Eric House (xwords@eehouse.org).  All rights
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
import android.os.Bundle
import android.text.TextUtils
import android.util.AttributeSet
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import org.eehouse.android.xw4.TilePickAlert.TilePickState
import org.eehouse.android.xw4.loc.LocUtils

class TilePickView(context: Context, aset: AttributeSet?) : LinearLayout(context, aset) {
    interface TilePickListener {
        fun onTilesChanged(nToPick: Int, newTiles: IntArray)
    }

    private var mPendingTiles: ArrayList<Int>? = null
    private var mListner: TilePickListener? = null
    private var mState: TilePickState? = null
    private val mButtons: MutableMap<Int, Button> = HashMap()
    internal fun init(
        lstn: TilePickListener?, state: TilePickState?,
        bundle: Bundle
    ) {
        mState = state
        mListner = lstn
        mPendingTiles = bundle.getSerializableSafe<ArrayList<Int>>(NEW_TILES)
        if (null == mPendingTiles) {
            Log.d(TAG, "creating new mPendingTiles")
            mPendingTiles = ArrayList()
        }
        showPending()
        addTileButtons()
        updateDelButton()
        findViewById<View>(R.id.del).setOnClickListener(object : OnClickListener {
            override fun onClick(view: View) {
                removePending()
                updateDelButton()
                mListner!!.onTilesChanged(mState!!.nToPick, pending)
            }
        })
        mListner!!.onTilesChanged(mState!!.nToPick, pending)
    }

    // NOT @Override!!!
    fun saveInstanceState(bundle: Bundle) {
        bundle.putSerializable(NEW_TILES, mPendingTiles)
    }

    private val pending: IntArray
        private get() {
            val result = IntArray(mPendingTiles!!.size)
            for (ii in result.indices) {
                result[ii] = mPendingTiles!![ii]
            }
            return result
        }

    private fun addTileButtons() {
        val context = context
        val container = findViewById<View>(R.id.button_bar_container) as LinearLayout
        var bar: LinearLayout? = null
        var barLen = 0
        var nShown = 0
        for (ii in mState!!.faces.indices) {
            if (null != mState!!.counts && mState!!.counts!![ii] == 0 && !SHOW_UNAVAIL) {
                continue
            }
            val visIndex = nShown++
            if (null == bar || 0 == visIndex % barLen) {
                bar = LocUtils.inflate(context, R.layout.tile_picker_bar) as LinearLayout
                container.addView(bar)
                barLen = bar.childCount
            }
            val button = bar!!.getChildAt(visIndex % barLen) as Button
            mButtons[ii] = button
            button.visibility = VISIBLE
            updateButton(ii, 0)
            button.setOnClickListener { view -> onTileClicked(view, ii) }
        }
    }

    private fun onTileClicked(view: View, index: Int) {
        // replace the last pick if we don't have room to add a new one
        if (mPendingTiles!!.size == mState!!.nToPick) {
            removePending()
        }
        mPendingTiles!!.add(index)
        updateDelButton()
        updateButton(index, -1)
        showPending()
        mListner!!.onTilesChanged(mState!!.nToPick, pending)
    }

    private fun showPending() {
        val desc = findViewById<View>(R.id.pending_desc) as TextView
        if (mState!!.forBlank()) {
            desc.visibility = GONE
        } else {
            val faces: MutableList<String?> = ArrayList()
            for (indx in mPendingTiles!!) {
                faces.add(mState!!.faces[indx])
            }
            desc.text = LocUtils.getString(
                context,
                R.string.tile_pick_summary_fmt,
                TextUtils.join(",", faces)
            )
        }
    }

    private fun pendingCount(dataIndex: Int): Int {
        var count = 0
        for (index in mPendingTiles!!) {
            if (index == dataIndex) {
                ++count
            }
        }
        return count
    }

    private fun updateButton(index: Int, adjust: Int) {
        val button = mButtons[index]
        val context = context
        var face: String? = mState!!.faces[index]
        if (!mState!!.forBlank()) {
            val count = mState!!.counts!![index] - pendingCount(index)
            face = LocUtils.getString(
                context, R.string.tile_button_txt_fmt,
                face, count
            )
            val vis = if (count == 0) INVISIBLE else VISIBLE
            button!!.visibility = vis
        }
        button!!.text = face
    }

    private fun removePending() {
        val tile = mPendingTiles!!.removeAt(mPendingTiles!!.size - 1)
        updateButton(tile, 1)
        showPending()
    }

    private fun updateDelButton() {
        val vis = if (mState!!.forBlank() || mPendingTiles!!.size == 0) INVISIBLE else VISIBLE
        findViewById<View>(R.id.del).visibility = vis
    }

    companion object {
        private val TAG = TilePickView::class.java.getSimpleName()
        private const val NEW_TILES = "NEW_TILES"
        private const val SHOW_UNAVAIL = false
    }
}
