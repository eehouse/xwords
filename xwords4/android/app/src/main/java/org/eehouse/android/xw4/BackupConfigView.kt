/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2022 - 2024 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

import android.app.AlertDialog
import android.content.Context
import android.net.Uri
import android.util.AttributeSet
import android.view.View
import android.view.ViewGroup
import android.widget.CheckBox
import android.widget.CompoundButton
import android.widget.LinearLayout
import android.widget.TextView
import org.eehouse.android.xw4.ZipUtils.SaveWhat
import org.eehouse.android.xw4.loc.LocUtils

class BackupConfigView(cx: Context, aset: AttributeSet?) : LinearLayout(cx, aset),
    CompoundButton.OnCheckedChangeListener {
    private var mIsStore: Boolean? = null
    private var mLoadFile: Uri? = null
    private val mCheckBoxes: MutableMap<SaveWhat, CheckBox> = HashMap()
    private var mShowWhats: List<SaveWhat>? = null
    private var mDialog: AlertDialog? = null

    fun init(uri: Uri?) {
        mLoadFile = uri
        mIsStore = null == uri
        if (null != uri) {
            mShowWhats = ZipUtils.getHasWhats(context, uri)
        }
        initOnce()
    }

    fun setDialog(dialog: AlertDialog): AlertDialog {
        mDialog = dialog
        dialog.setOnShowListener { countChecks() }
        return dialog
    }

    // Usually called before init(), but IIRC wasn't on older Android versions
    override fun onFinishInflate() {
        initOnce()
    }

    override fun onCheckedChanged(buttonView: CompoundButton, isChecked: Boolean)
        = countChecks()

    private fun countChecks() {
        if (null != mDialog) {
            var haveCheck = false
            for (box in mCheckBoxes.values) {
                if (box.isChecked) {
                    haveCheck = true
                    break
                }
            }
            Utils.enableAlertButton(
                mDialog, AlertDialog.BUTTON_POSITIVE,
                haveCheck
            )
        }
    }

    private fun initOnce() {
        if (null != mIsStore) {
            val tv = (findViewById<View>(R.id.explanation) as TextView)
            if (null != tv) {
                val context = context
                if (mIsStore!!) {
                    tv.setText(R.string.archive_expl_store)
                } else {
                    val name = ZipUtils.getFileName(context, mLoadFile)
                    val msg = LocUtils
                        .getString(context, R.string.archive_expl_load_fmt, name)
                    tv.text = msg
                }

                val list = findViewById<View>(R.id.whats_list) as LinearLayout
                for (what in SaveWhat.entries) {
                    if (null == mShowWhats || mShowWhats!!.contains(what)) {
                        val item =
                            LocUtils.inflate(context, R.layout.backup_config_item) as ViewGroup
                        list.addView(item)
                        val box = item.findViewById<View>(R.id.check) as CheckBox
                        box.setText(what.titleID())
                        mCheckBoxes[what] = box
                        box.isChecked = !mIsStore!!
                        box.setOnCheckedChangeListener(this)
                        (item.findViewById<View>(R.id.expl) as TextView)
                            .setText(what.explID())
                    }
                }
            }
            countChecks()
        }
    }

    val alertTitle: Int
        get() = if (mIsStore!!
        ) R.string.gamel_menu_storedb else R.string.gamel_menu_loaddb

    val posButtonTxt: Int
        get() = if (mIsStore!!
        ) R.string.archive_button_store else R.string.archive_button_load

    val saveWhat: ArrayList<SaveWhat>
        get() {
            val result = ArrayList<SaveWhat>()
            for (what in mCheckBoxes.keys) {
                val box = mCheckBoxes[what]
                if (box!!.isChecked) {
                    result.add(what)
                }
            }
            return result
        }

    companion object {
        private val TAG: String = BackupConfigView::class.java.simpleName
    }
}
