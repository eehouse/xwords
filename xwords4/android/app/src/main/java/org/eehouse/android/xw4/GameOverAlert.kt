/*
 * Copyright 2010 - 2024 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.Activity
import android.app.AlertDialog
import android.app.Dialog
import android.content.DialogInterface
import android.content.DialogInterface.OnShowListener
import android.os.Bundle
import android.view.View
import android.view.ViewGroup
import android.widget.CheckBox
import android.widget.CompoundButton
import android.widget.TextView
import org.eehouse.android.xw4.DlgDelegate.HasDlgDelegate
import org.eehouse.android.xw4.jni.GameSummary
import org.eehouse.android.xw4.loc.LocUtils

class GameOverAlert : XWDialogFragment(), DialogInterface.OnClickListener,
    CompoundButton.OnCheckedChangeListener {
    private var mDialog: AlertDialog? = null
    private var mDlgDlgt: HasDlgDelegate? = null
    private var mSummary: GameSummary? = null
    private var mTitleID = 0
    private var mMsg: String? = null
    private var mView: ViewGroup? = null
    private var mInArchive = false
    private var mArchiveBox: CheckBox? = null
    private var mDeleteBox: CheckBox? = null
    private var mHasPending = false

    interface OnDoneProc {
        fun onGameOverDone(
            rematch: Boolean,
            archiveAfter: Boolean,
            deleteAfter: Boolean
        )
    }

    private var mOnDone: OnDoneProc? = null
    override fun onSaveInstanceState(bundle: Bundle) {
        bundle.putSerializableAnd(SUMMARY, mSummary)
			.putIntAnd(TITLE, mTitleID)
			.putStringAnd(MSG, mMsg)
			.putSerializableAnd(IN_ARCH, mInArchive)
			.putSerializableAnd(HAS_PENDING, mHasPending)
        super.onSaveInstanceState(bundle)
    }

    override fun onCreateDialog(sis: Bundle?): Dialog {
        var sis = sis
        Log.d(TAG, "onCreateDialog()")
        if (null == sis) {
            sis = arguments
        }
        mSummary = sis!!.getSerializable(SUMMARY) as GameSummary?
        mTitleID = sis.getInt(TITLE)
        mMsg = sis.getString(MSG)
        mInArchive = sis.getSerializable(IN_ARCH) as Boolean
        mHasPending = sis.getSerializable(HAS_PENDING) as Boolean
        val context = requireContext()
        mView = LocUtils.inflate(context, R.layout.game_over) as ViewGroup
        initView()
        val ab = LocUtils.makeAlertBuilder(context)
            .setTitle(mTitleID)
            .setView(mView)
            .setPositiveButton(android.R.string.ok, this)
            .setNeutralButton(R.string.button_rematch, this)
        mDialog = ab.create()
        mDialog?.setOnShowListener(OnShowListener {
            val nowChecked = mArchiveBox!!.isChecked
            onCheckedChanged(mArchiveBox as CompoundButton, nowChecked)
            updateForPending()
        })
        return mDialog!!
    }

    override fun getFragTag(): String {
        return TAG
    }

    override fun belongsOnBackStack(): Boolean {
        return true
    }

    override fun onClick(dialog: DialogInterface, which: Int) {
        if (null != mOnDone) {
            val rematch = which == AlertDialog.BUTTON_NEUTRAL
            val archiveAfter = mArchiveBox!!.isChecked
            val deleteAfter = mDeleteBox!!.isChecked
            mOnDone!!.onGameOverDone(rematch, archiveAfter, deleteAfter)
        }
    }

    override fun onCheckedChanged(bv: CompoundButton, isChecked: Boolean) {
        if (isChecked) {
            val builder: DlgDelegate.Builder
            val buttonText = bv.getText()
            builder = if (bv === mArchiveBox) {
                mDeleteBox!!.setChecked(false)
                val archiveName = LocUtils
                    .getString(requireContext(), R.string.group_name_archive)
                mDlgDlgt!!.makeNotAgainBuilder(
                    R.string.key_na_archivecheck,
                    R.string.not_again_archivecheck_fmt,
                    buttonText, archiveName
                )
            } else {
                Assert.assertTrueNR(bv === mDeleteBox)
                mArchiveBox!!.setChecked(false)
                mDlgDlgt!!.makeNotAgainBuilder(
                    R.string.key_na_deletecheck,
                    R.string.not_again_deletecheck_fmt,
                    buttonText
                )
            }
            builder.show()
        }
    }

    fun configure(proc: OnDoneProc?, dlgDlgt: HasDlgDelegate?): GameOverAlert {
        mOnDone = proc
        mDlgDlgt = dlgDlgt
        return this
    }

    fun pendingCountChanged(newCount: Int) {
        val hasPending = 0 < newCount
        if (hasPending != mHasPending) {
            mHasPending = hasPending
            updateForPending()
        }
    }

    private fun updateForPending() {
        mArchiveBox!!.visibility =
            if (mHasPending || mInArchive) View.GONE else View.VISIBLE
        Utils.enableAlertButton(mDialog!!, AlertDialog.BUTTON_NEGATIVE, !mHasPending)
        mDeleteBox!!.visibility = if (mHasPending) View.GONE else View.VISIBLE
    }

    private fun initView() {
        (mView!!.findViewById<View>(R.id.msg) as TextView).text = mMsg
        mArchiveBox = mView!!.findViewById<View>(R.id.archive_check) as CheckBox
        mArchiveBox!!.setOnCheckedChangeListener(this)
        if (mInArchive) {
            mArchiveBox!!.visibility = View.GONE
        }
        mDeleteBox = mView!!.findViewById<View>(R.id.delete_check) as CheckBox
        mDeleteBox!!.setOnCheckedChangeListener(this)
    }

    companion object {
        private val TAG = GameOverAlert::class.java.getSimpleName()
        private const val SUMMARY = "SUMMARY"
        private const val TITLE = "TITLE"
        private const val MSG = "MSG"
        private const val IN_ARCH = "IN_ARCH"
        private const val HAS_PENDING = "HAS_PENDING"

        fun newInstance(
            summary: GameSummary?,
            titleID: Int, msg: String?,
            hasPending: Boolean,
            inArchiveGroup: Boolean
        ): GameOverAlert {
            Log.d(TAG, "newInstance(msg=%s)", msg)
            val result = GameOverAlert()
            val args = Bundle()
				.putSerializableAnd(SUMMARY, summary)
				.putIntAnd(TITLE, titleID)
				.putStringAnd(MSG, msg)
				.putSerializableAnd(IN_ARCH, inArchiveGroup)
				.putSerializableAnd(HAS_PENDING, hasPending)
            result.setArguments(args)
            Log.d(TAG, "newInstance() => %s", result)
            return result
        }
    }
}
