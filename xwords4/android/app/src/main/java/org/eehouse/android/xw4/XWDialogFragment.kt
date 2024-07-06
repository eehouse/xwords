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
import android.content.DialogInterface
import android.view.View
import androidx.fragment.app.DialogFragment

private val TAG: String = XWDialogFragment::class.java.simpleName

abstract class XWDialogFragment : DialogFragment() {
    private var m_onDismiss: OnDismissListener? = null
    private var m_onCancel: OnCancelListener? = null
    private var m_buttonMap: MutableMap<Int, DialogInterface.OnClickListener>? = null

    interface OnDismissListener {
        fun onDismissed(frag: XWDialogFragment)
    }

    interface OnCancelListener {
        fun onCancelled(frag: XWDialogFragment)
    }

    abstract fun getFragTag(): String

    override fun onResume() {
        super.onResume()

        if (null != m_buttonMap) {
            val dialog = dialog as AlertDialog?
            if (null != dialog) {
                for (but in m_buttonMap!!.keys) {
                    // final int fbut = but;
                    dialog.getButton(but) // NPE!!!
                        .setOnClickListener { view -> dialogButtonClicked(view, but) }
                }
            }
        }
    }

    override fun onCancel(dialog: DialogInterface) {
        super.onCancel(dialog)
        // Log.d( TAG, "%s.onCancel() called", this::class.java.getSimpleName() )
        m_onCancel?.onCancelled(this)
    }

    override fun onDismiss(dif: DialogInterface) {
        super.onDismiss(dif)

        m_onDismiss?.onDismissed(this)
    }

    open fun belongsOnBackStack(): Boolean {
        return false
    }

    fun setOnDismissListener(lstnr: OnDismissListener?) {
        Assert.assertTrueNR(null == lstnr || null == m_onDismiss)
        m_onDismiss = lstnr
    }

    fun setOnCancelListener(lstnr: OnCancelListener?) {
        Assert.assertTrueNR(null == lstnr || null == m_onCancel)
        m_onCancel = lstnr
    }

    fun setNoDismissListenerPos(
        ab: AlertDialog.Builder, buttonID: Int,
        lstnr: DialogInterface.OnClickListener
    ) {
        ab.setPositiveButton(buttonID, null)
        buttonMap[AlertDialog.BUTTON_POSITIVE] = lstnr
    }

    protected fun setNoDismissListenerNeut(
        ab: AlertDialog.Builder, buttonID: Int,
        lstnr: DialogInterface.OnClickListener
    ) {
        ab.setNeutralButton(buttonID, null)
        buttonMap[AlertDialog.BUTTON_NEUTRAL] = lstnr
    }

    fun setNoDismissListenerNeg(
        ab: AlertDialog.Builder, buttonID: Int,
        lstnr: DialogInterface.OnClickListener
    ) {
        ab.setNegativeButton(buttonID, null)
        buttonMap[AlertDialog.BUTTON_NEGATIVE] = lstnr
    }

    private val buttonMap: MutableMap<Int, DialogInterface.OnClickListener>
        get() {
            if (null == m_buttonMap) {
                m_buttonMap = HashMap()
            }
            return m_buttonMap!!
        }

    private fun dialogButtonClicked(view: View, button: Int) {
        val listener = m_buttonMap!![button]
        Assert.assertTrueNR(null != listener)
        listener?.onClick(dialog, button)
    }
}
