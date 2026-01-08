/*
 * Copyright 2026 by Eric House (xwords@eehouse.org).  All rights
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
import android.util.AttributeSet
import android.view.View
import android.widget.CheckBox
import android.widget.CompoundButton
import android.widget.ImageButton
import android.widget.TableRow
import android.widget.TextView

import org.eehouse.android.xw4.jni.GameMgr.GroupRef.SortOrderElem
import org.eehouse.android.xw4.loc.LocUtils

private val TAG = SortTableRow::class.java.simpleName

class SortTableRow(context: Context, aset: AttributeSet?) :
    TableRow(context, aset), View.OnClickListener,
    CompoundButton.OnCheckedChangeListener
{
    private var mElem: SortOrderElem? = null
    private var mAllShown: Boolean = false
    private var mSelected: Boolean = false
    private var mOnMovedBy: SortConfigView.OnMovedBy? = null

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        Log.d(TAG, "onAttachedToWindow()")
        mElem?.let { elem ->
            findViewById<TextView>(R.id.so_name)
                .setText(LocUtils.getString(context, elem.so!!.resID))
            if (mAllShown) {
                findViewById<View>(R.id.so_inverted).visibility = GONE
                findViewById<CheckBox>(R.id.so_selected).also {
                    it.setChecked(mSelected)
                    it.setOnCheckedChangeListener(this)
                }
            } else {
                findViewById<View>(R.id.so_selected).visibility = GONE
                findViewById<CheckBox>(R.id.so_inverted).also {
                    it.setChecked(elem.inverted)
                    it.setOnCheckedChangeListener(this)
                }
            }
        } ?: Log.d(TAG, "onAttachedToWindow(): mElem not set")
    }

    override fun onClick(view: View) {
        when (view.id) {
            R.id.arrow_up -> mOnMovedBy?.movedBy(this, -1)
            R.id.arrow_down -> mOnMovedBy?.movedBy(this, 1)
            else -> Log.d(TAG, "onClick()")
        }
    }

    override fun onCheckedChanged(view: CompoundButton,
                                  isChecked: Boolean) {
        when (view.id) {
            R.id.so_selected -> mSelected = isChecked
            R.id.so_inverted -> mElem!!.inverted = isChecked
            else -> Assert.failDbg()
        }
    }

    fun configure(elem: SortOrderElem, onMovedBy: SortConfigView.OnMovedBy,
                  allShown: Boolean, selected: Boolean) {
        mElem = elem
        mOnMovedBy = onMovedBy
        mAllShown = allShown
        mSelected = selected
    }

    fun getElem(): SortOrderElem { return mElem!! }

    fun enabled(): Boolean {
        return findViewById<CheckBox>(R.id.so_selected).isChecked()
    }

    private fun updateArrow(arrow: ImageButton, enable: Boolean ) {
        if ( enable ) {
            arrow.visibility = VISIBLE
            arrow.setOnClickListener(this)
        } else arrow.visibility = INVISIBLE
    }

    fun updateArrows(pos: Int, total: Int) {
        if ( mAllShown ) {
            findViewById<View>(R.id.arrow_up).visibility = GONE
            findViewById<View>(R.id.arrow_down).visibility = GONE
        } else {
            findViewById<ImageButton>(R.id.arrow_up).also {
                updateArrow(it, 0 < pos)
            }
            findViewById<ImageButton>(R.id.arrow_down).also {
                updateArrow(it, pos < total - 1)
            }
        }
    }
}
