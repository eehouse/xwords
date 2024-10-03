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
import android.view.KeyEvent
import android.view.View.OnFocusChangeListener
import android.view.inputmethod.EditorInfo
import android.widget.CheckBox
import android.widget.EditText
import android.widget.TableRow
import android.widget.TextView
import android.widget.TextView.OnEditorActionListener

import org.eehouse.android.xw4.jni.XwJNI.PatDesc

private val TAG: String = PatTableRow::class.java.simpleName

class PatTableRow(context: Context, aset: AttributeSet?) :
    TableRow(context, aset), OnEditorActionListener
{
    private var mEdit: EditText? = null
    private var mCheck: CheckBox? = null
    private var mEnterProc: EnterPressed? = null

    interface EnterPressed {
        fun enterPressed(): Boolean
    }

    fun setOnEnterPressed(proc: EnterPressed?) {
        mEnterProc = proc
    }

    fun hasState(): Boolean {
        return 0 < mEdit!!.text.length || mCheck!!.isChecked
    }

    override fun onFinishInflate() {
        mCheck = Utils.getChildInstanceOf(this, CheckBox::class.java) as CheckBox?
        mEdit = Utils.getChildInstanceOf(this, EditText::class.java) as EditText?
        mEdit!!.setOnEditorActionListener(this)
    }

    override fun onEditorAction(tv: TextView, actionId: Int,
                                event: KeyEvent): Boolean
    {
        return EditorInfo.IME_ACTION_SEND == actionId
            && mEnterProc?.enterPressed() ?: false
    }

    fun getToDesc(out: PatDesc) {
        val strPat = mEdit!!.text.toString()
        out.strPat = strPat
        out.anyOrderOk = mCheck!!.isChecked
    }

    fun setFromDesc(desc: PatDesc) {
        mEdit!!.setText(desc.strPat)
        mCheck!!.isChecked = desc.anyOrderOk
    }

    fun addBlankToFocussed(blank: String?): Boolean {
        val handled = mEdit!!.hasFocus()
        if (handled) {
            mEdit!!.text.insert(mEdit!!.selectionStart, blank)
        }
        return handled
    }

    fun getFieldName(): String
    {
        // Return the label (the first column)
        val tv = Utils.getChildInstanceOf(this, TextView::class.java) as TextView?
        val result = tv?.text.toString() ?: ""
        return result
    }

    fun setOnFocusGained(proc: Runnable) {
        mEdit!!.onFocusChangeListener = OnFocusChangeListener { view, hasFocus ->
            if (hasFocus) {
                proc.run()
            }
        }
    }
}
