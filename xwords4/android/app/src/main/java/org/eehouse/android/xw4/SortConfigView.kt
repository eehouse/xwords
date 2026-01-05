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
import android.text.TextUtils
import android.util.AttributeSet
import android.view.View
import android.widget.LinearLayout
import android.widget.TableLayout
import android.widget.TextView
import android.widget.CompoundButton
import android.widget.CheckBox
import androidx.core.view.doOnAttach

import java.util.HashMap
import java.util.Map

// import org.eehouse.android.xw4.jni.CurGameInfo
import org.eehouse.android.xw4.jni.GameMgr.GroupRef
import org.eehouse.android.xw4.jni.GameMgr.GroupRef.SortOrderState
import org.eehouse.android.xw4.jni.GameMgr.GroupRef.SortOrderElem
import org.eehouse.android.xw4.loc.LocUtils

class SortConfigView(val mContext: Context, attrs: AttributeSet)
	: LinearLayout( mContext, attrs ), CompoundButton.OnCheckedChangeListener
{
    private val TAG = SortConfigView::class.java.simpleName
    private var mConfigured = false
    private var mGrp: GroupRef? = null
    // private var mSortOrderState: SortOrderState? = null
    private var mInUse: ArrayList<SortOrderElem>? = null
    private var mAvail: ArrayList<SortOrderElem>? = null
    private var mGetDefaults: Boolean = false
    private var mShowAll: Boolean = false
    private var mTable: TableLayout? = null

    interface OnMovedBy {
        fun movedBy(row: SortTableRow, nRows: Int)
    }
    
    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        mTable = findViewById<TableLayout>(R.id.so_list)
        findViewById<CheckBox>(R.id.check_showAll).also {
            it.setOnCheckedChangeListener(this@SortConfigView)
        }
        trySetup()
    }

    // CompoundButton.OnCheckedChangeListener
    override fun onCheckedChanged(view: CompoundButton, isChecked: Boolean) {
        Assert.assertTrueNR(view.id == R.id.check_showAll)
        mShowAll = isChecked
        if ( !isChecked ) {
            getActive()
        }
        populateList()
    }

	public fun configure(grp: GroupRef, getDefaults: Boolean): SortConfigView
    {
        mGrp = grp
        mGetDefaults = getDefaults
        return this
    }

    public fun save() {
        Log.d(TAG, "save() called" )
        getInUse().also {
            mGrp!!.setSortOrder(it)
        }
    }

    private fun trySetup() {
        mGrp!!.let { grp ->
            if (!mConfigured) {
                mConfigured = true;
                launch {
                    val sos = grp.getSortOrder(mGetDefaults)
                    mInUse = sos.inUse
                    mAvail = sos.avail
                    populateList()
                }
            }
        }
    }

    private fun getInUse(): ArrayList<SortOrderElem> {
        val result = ArrayList<SortOrderElem>()
        mTable!!.let { table ->
            val count = table.getChildCount()
            for (ii in 0 ..< count) {
                val elem = table.getChildAt(ii) as SortTableRow
                if (!mShowAll || elem.enabled()) {
                    result.add(elem.getElem())
                }
            }
        }
        return result
    }

    private fun getActive() {
        val avail = mAvail!!
        val inUse = mInUse!!
        avail.clear()
        inUse.clear()
        mTable!!.let { table ->
            val count = table.getChildCount()
            for (ii in 0 ..< count) {
                val elem = table.getChildAt(ii) as SortTableRow
                val soe = elem.getElem()
                if (elem.enabled()) inUse.add(soe)
                else avail.add(soe)
            }
        }
    }

    private fun populateList() {
        mTable!!.removeAllViews()

        mInUse!!.map { addElem(it, true) }
        if (mShowAll) {
            mAvail!!.map { addElem(it, false) }
        }
    }

    private val mOnMovedBy = object: OnMovedBy {
        override fun movedBy(row: SortTableRow, moveBy: Int) {
            Log.d(TAG, "movedBy(moveBy=$moveBy)")
            Assert.assertTrueNR(moveBy != 0)
            mTable?.let { table ->
                val curPos = table.indexOfChild(row)
                if (0 <= curPos) {
                    if ( -1 == moveBy && 0 == curPos ) {
                    } else if ( 1 == moveBy && curPos == table.getChildCount() - 1 ) {
                    } else  {
                        table.removeView(row)
                        table.addView(row, curPos+moveBy)
                    }
                }
            }
        }
    }

    private fun addElem(one: SortOrderElem, selected: Boolean) {
        val elem = LocUtils.inflate(context, R.layout.sort_elem)
            as SortTableRow
        elem.configure(one, mOnMovedBy, mShowAll, selected)
        mTable!!.addView(elem)
    }
}
