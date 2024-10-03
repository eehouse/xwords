/*
 * Copyright 2020 - 2024 by Eric House (xwords@eehouse.org).  All rights
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
import android.widget.CheckBox
import android.widget.CompoundButton
import android.widget.LinearLayout
import android.widget.RadioButton
import org.eehouse.android.xw4.InviteView.ItemClicked
import org.eehouse.android.xw4.loc.LocUtils

class LimSelGroup(context: Context, aset: AttributeSet?) :
    LinearLayout(context, aset), CompoundButton.OnCheckedChangeListener
{
    private var mLimit = 0
    private var mProcs: ItemClicked? = null

    fun setLimit(limit: Int): LimSelGroup {
        Log.d(TAG, "setLimit(limit=%d)", limit)
        Assert.assertTrueNR(0 < limit)
        mLimit = limit
        return this
    }

    fun setCallbacks(procs: ItemClicked?) {
        mProcs = procs
    }

    fun getSelected(): Array<String>
    {
        val result = mChecked.map{it.text.toString()}
        return result.toTypedArray()
    }

    fun setPlayers(names: Array<String>)
    {
        removeAllViews()
        val context = context
        names.map {
            val button =
                if (1 == mLimit) {
                    LocUtils.inflate(context, R.layout.invite_radio)
                } else {
                    LocUtils.inflate(context, R.layout.invite_checkbox)
                } as CompoundButton
            button.text = it
            button.setOnCheckedChangeListener(this)
            addView(button)
        }
    }

    override fun onCheckedChanged(buttonView: CompoundButton, isChecked: Boolean) {
        Log.d(TAG, "onCheckedChanged(%s, %b)", buttonView, isChecked)
        addToSet(buttonView, isChecked)
        if (null != mProcs) {
            mProcs!!.checkButton()
        }
    }

    val mChecked: ArrayList<CompoundButton> = ArrayList()
    private fun addToSet(button: CompoundButton, nowChecked: Boolean) {
        val iter = mChecked.iterator()
        while (iter.hasNext()) {
            val but = iter.next()
            if (nowChecked) {
                Assert.assertTrueNR(but != button)
            } else if (but == button) {
                iter.remove()
            }
        }
        if (nowChecked) {
            mChecked.add(button)
            while (mLimit < mChecked.size) {
                val oldButton = mChecked.removeAt(0)
                oldButton.isChecked = false
            }
        }
    }

    companion object {
        private val TAG: String = LimSelGroup::class.java.simpleName
    }
}
