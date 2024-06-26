/* -*- compile-command: "find-and-gradle.sh inXw4dDebug"; -*- */
/*
 * Copyright 2017 - 2024 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.AlertDialog
import android.content.Context
import android.content.DialogInterface
import android.view.View
import android.widget.AdapterView
import android.widget.AdapterView.OnItemSelectedListener
import android.widget.Spinner
import org.eehouse.android.xw4.Utils.OnNothingSelDoesNothing
import org.eehouse.android.xw4.loc.LocUtils

class EnableSMSAlert : DlgDelegateAlert() {

    private var mSpinner: Spinner? = null

    public override fun populateBuilder(
        context: Context, state: DlgState?,
        builder: AlertDialog.Builder
    ) {
        val layout = LocUtils.inflate(context, R.layout.confirm_sms)
        val spinner = layout.findViewById<View>(R.id.confirm_sms_reasons) as Spinner
        mSpinner = spinner

        val onItemSel: OnItemSelectedListener = object : OnNothingSelDoesNothing() {
            override fun onItemSelected(
                parent: AdapterView<*>?, view: View,
                position: Int, id: Long
            ) {
                checkEnableButton(dialog as AlertDialog)
            }
        }
        spinner.onItemSelectedListener = onItemSel
        val lstnr = DialogInterface.OnClickListener { dlg, item ->
            Assert.assertTrue(0 < spinner.selectedItemPosition)
            val xwact = activity as XWActivity?
            xwact!!.onPosButton(state!!.m_action!!, *state.getParams())
        }
        builder.setTitle(R.string.confirm_sms_title)
            .setView(layout)
            .setPositiveButton(R.string.button_enable, lstnr)
            .setNegativeButton(android.R.string.cancel, null)
    }

    public override fun create(builder: AlertDialog.Builder): AlertDialog {
        val dialog = super.create(builder)
        dialog.setOnShowListener { dialog -> checkEnableButton(dialog as AlertDialog) }
        return dialog
    }

    private fun checkEnableButton(dialog: AlertDialog) {
        val enabled = 0 < mSpinner!!.selectedItemPosition
        Utils.enableAlertButton(dialog, AlertDialog.BUTTON_POSITIVE, enabled)
    }

    companion object {
        private val TAG = EnableSMSAlert::class.java.getSimpleName()
        fun newInstance(state: DlgState?): EnableSMSAlert {
            val result = EnableSMSAlert()
            result.addStateArgument(state!!)
            return result
        }
    }
}
