/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2017 by Eric House (xwords@eehouse.org).  All rights reserved.
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

import android.app.Activity
import android.app.AlertDialog
import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify
import org.eehouse.android.xw4.TilePickView.TilePickListener
import org.eehouse.android.xw4.loc.LocUtils

import java.io.Serializable

import org.eehouse.android.xw4.DlgDelegate.Action

class TilePickAlert : XWDialogFragment(), TilePickListener {
    private var mView: TilePickView? = null
    private var mState: TilePickState? = null
    private var mAction: Action? = null
    private var mDialog: AlertDialog? = null
    private var mSelTiles = IntArray(0)

    class TilePickState : Serializable {
        @JvmField
        var col = 0
        @JvmField
        var row = 0
        @JvmField
        var playerNum: Int
        @JvmField
        var counts: IntArray? = null
        @JvmField
        var faces: Array<String>
        @JvmField
        var isInitial = false
        @JvmField
        var nToPick: Int

        constructor(player: Int, faces: Array<String>, col: Int, row: Int) {
            this.col = col
            this.row = row
            playerNum = player
            this.faces = faces
            nToPick = 1
        }

        constructor(
            isInitial: Boolean, playerNum: Int, nToPick: Int,
            faces: Array<String>, counts: IntArray?
        ) {
            this.playerNum = playerNum
            this.isInitial = isInitial
            this.nToPick = nToPick
            this.faces = faces
            this.counts = counts
        }

        fun forBlank(): Boolean {
            return null == counts
        }
    }

    override fun onSaveInstanceState(bundle: Bundle) {
        bundle.putSerializable(TPS, mState)
        bundle.putSerializable(ACTION, mAction)
        mView!!.saveInstanceState(bundle)
        super.onSaveInstanceState(bundle)
    }

    override fun onCreateDialog(sis: Bundle?): Dialog {
        var sis = sis
        if (null == sis) {
            sis = arguments
        }
        mState = sis!!.getSerializable(TPS) as TilePickState?
        mAction = sis.getSerializable(ACTION) as Action?
        val activity: Activity? = activity
        Assert.assertNotNull(activity)
        val context = requireContext()
        mView = LocUtils.inflate(context, R.layout.tile_picker) as TilePickView
        mView!!.init(this, mState, sis)
        val resId =
            if (mState!!.forBlank()) R.string.title_blank_picker else R.string.tile_tray_picker
        val ab = LocUtils.makeAlertBuilder(context)
            .setTitle(resId)
            .setView(mView)
        if (!mState!!.forBlank()) {
            val lstnr = DialogInterface.OnClickListener { dialog, which -> onDone() }
            ab.setPositiveButton(buttonTxt(), lstnr)
        }
        mDialog = ab.create()
        return mDialog!!
    }

    override fun getFragTag(): String {
        return TAG
    }

    // TilePickView.TilePickListener interface
    override fun onTilesChanged(nToPick: Int, newTiles: IntArray) {
        mSelTiles = newTiles
        val haveAll = nToPick == newTiles.size
        if (haveAll && mState!!.forBlank()) {
            onDone()
        } else if (null != mDialog) {
            mDialog!!.getButton(AlertDialog.BUTTON_POSITIVE).text = buttonTxt()
        }
    }

    override fun onCancel(dialog: DialogInterface) {
        super.onCancel(dialog)
        val activity: Activity? = activity
        if (activity is DlgClickNotify) {
            val notify = activity as DlgClickNotify
            notify.onDismissed(mAction!!)
        }
    }

    private fun onDone() {
        val activity: Activity? = activity
        if (activity is DlgClickNotify) {
            val notify = activity as DlgClickNotify
            notify.onPosButton(mAction!!, mState, mSelTiles)
        } else {
            Assert.failDbg()
        }
        dismiss()
    }

    private fun buttonTxt(): String {
        val context = requireContext()
        val left = mState!!.nToPick - mSelTiles.size
        return if (0 == left) LocUtils.getString(
            context,
            android.R.string.ok
        ) else LocUtils.getString(context, R.string.tilepick_all_fmt, left)
    }

    companion object {
        private val TAG = TilePickAlert::class.java.getSimpleName()
        private const val TPS = "TPS"
        private const val ACTION = "ACTION"
        fun newInstance(action: Action?, state: TilePickState?): TilePickAlert {
            val result = TilePickAlert()
            val args = Bundle()
            args.putSerializable(ACTION, action)
            args.putSerializable(TPS, state)
            result.setArguments(args)
            return result
        }
    }
}
