/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */ /*
 * Copyright 2009-2010 by Eric House (xwords@eehouse.org).  All
 * rights reserved.
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
import android.content.DialogInterface
import android.content.DialogInterface.OnShowListener
import android.content.res.TypedArray
import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.util.AttributeSet
import android.view.View
import android.widget.EditText
import android.widget.SeekBar
import android.widget.SeekBar.OnSeekBarChangeListener
import androidx.preference.DialogPreference
import androidx.preference.PreferenceViewHolder

import org.eehouse.android.xw4.PrefsActivity.DialogProc
import org.eehouse.android.xw4.loc.LocUtils

class EditColorPreference(private val mContext: Context, attrs: AttributeSet?) : DialogPreference(
    mContext, attrs
), DialogProc {
    private var mCurColor = 0
    private var mWidget: View? = null

    // m_updateText: prevent loop that resets edittext cursor
    private var m_updateText = true

    private inner class SBCL(parent: View?, var m_editTxt: EditText?, var m_index: Int) :
        OnSeekBarChangeListener {
        var m_sample: View

        init {
            m_sample = parent!!.findViewById(R.id.color_edit_sample)
        }

        override fun onProgressChanged(
            seekBar: SeekBar, progress: Int,
            fromUser: Boolean
        ) {
            if (m_updateText) {
                m_editTxt!!.setText(String.format("%d", progress))
            }
            val shift = 16 - m_index * 8
            // mask out the byte we're changing
            var color = mCurColor and (0xFF shl shift).inv()
            // add in the new version of the byte
            color = color or (progress shl shift)
            mCurColor = color
            m_sample.setBackgroundColor(mCurColor)
        }

        override fun onStartTrackingTouch(seekBar: SeekBar) {}
        override fun onStopTrackingTouch(seekBar: SeekBar) {}
    }

    private inner class TCL(private val m_seekBar: SeekBar?) : TextWatcher {
        override fun afterTextChanged(s: Editable) {}
        override fun beforeTextChanged(s: CharSequence, st: Int, cnt: Int, a: Int) {}
        override fun onTextChanged(
            s: CharSequence, start: Int,
            before: Int, count: Int
        ) {
            val `val`: Int
            `val` = try {
                s.toString().toInt()
            } catch (nfe: NumberFormatException) {
                0
            }
            m_updateText = false // don't call me recursively inside seekbar
            m_seekBar!!.progress = `val`
            m_updateText = true
        }
    }

    init {
        widgetLayoutResource = R.layout.color_display
    }

    override fun onBindViewHolder(holder: PreferenceViewHolder) {
        mWidget = holder.itemView
        setWidgetColor()
        super.onBindViewHolder(holder)
    }

    private fun setWidgetColor() {
        mWidget!!.findViewById<View>(R.id.color_display_sample)
            .setBackgroundColor(persistedColor)
    }

    override fun onGetDefaultValue(arr: TypedArray, index: Int): Any? {
        return arr.getInteger(index, 0)
    }

    override fun onSetInitialValue(restoreValue: Boolean, defaultValue: Any?) {
        if (!restoreValue) {
            persistInt((defaultValue as Int))
        }
    }

    // PrefsActivity.DialogProc interface
    override fun makeDialogFrag(): XWDialogFragment {
        return ColorEditDialogFrag(this)
    }

    class ColorEditDialogFrag internal constructor(private val mPref: EditColorPreference) :
        XWDialogFragment(), OnShowListener {
        private var mView: View? = null
        override fun onCreateDialog(sis: Bundle?): Dialog {
            mView = LocUtils.inflate(mPref.context, R.layout.color_edit)
            val onOk = DialogInterface.OnClickListener { di, which ->
                Log.d(TAG, "onClick()")
                val color = (getOneByte(di, R.id.seek_red) shl 16
                        or (getOneByte(di, R.id.seek_green) shl 8)
                        or getOneByte(di, R.id.seek_blue))
                mPref.persistInt(color)
                mPref.setWidgetColor()
                // notifyChanged();
            }
            val dialog: Dialog = LocUtils.makeAlertBuilder(mPref.context)
                .setView(mView)
                .setTitle(mPref.title)
                .setPositiveButton(android.R.string.ok, onOk)
                .setNegativeButton(android.R.string.cancel, null)
                .create()
            dialog.setOnShowListener(this)
            return dialog
        }

        override fun onShow(dlg: DialogInterface) {
            mPref.onBindDialogView(mView)
        }

        override fun getFragTag(): String {
            return javaClass.getSimpleName()
        }
    }

    private fun onBindDialogView(view: View?) {
        LocUtils.xlateView(mContext, view)
        mCurColor = persistedColor
        setOneByte(view, 0)
        setOneByte(view, 1)
        setOneByte(view, 2)
        view!!.findViewById<View>(R.id.color_edit_sample)
            .setBackgroundColor(mCurColor)
    }

    private fun setOneByte(parent: View?, indx: Int) {
        val shift = 16 - indx * 8
        val byt = mCurColor shr shift and 0xFF
        val seekbar = parent!!.findViewById<View>(m_seekbarIds[indx]) as SeekBar
        val edittext = parent.findViewById<View>(m_editIds[indx]) as EditText
        if (null != seekbar) {
            seekbar.progress = byt
            seekbar.setOnSeekBarChangeListener(
                SBCL(
                    parent, edittext,
                    indx
                )
            )
        }
        if (null != edittext) {
            edittext.setText(String.format("%d", byt))
            edittext.addTextChangedListener(TCL(seekbar))
        }
    }

    private val persistedColor: Int
        private get() = -0x1000000 or getPersistedInt(0)

    companion object {
        private val TAG = EditColorPreference::class.java.getSimpleName()
        private val m_seekbarIds = intArrayOf(
            R.id.seek_red, R.id.seek_green,
            R.id.seek_blue
        )
        private val m_editIds = intArrayOf(
            R.id.edit_red, R.id.edit_green,
            R.id.edit_blue
        )

        private fun getOneByte(parent: DialogInterface, id: Int): Int {
            var `val` = 0
            val dialog = parent as Dialog
            val seekbar = dialog.findViewById<View>(id) as SeekBar
            if (null != seekbar) {
                `val` = seekbar.progress
            }
            return `val`
        }
    }
}
