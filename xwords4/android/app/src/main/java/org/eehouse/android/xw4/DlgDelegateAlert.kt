/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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

import android.app.Activity
import android.app.AlertDialog
import android.content.Context
import android.content.DialogInterface
import android.os.Bundle
import android.os.Parcelable
import org.eehouse.android.xw4.DlgDelegate.ActionPair
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify
import org.eehouse.android.xw4.loc.LocUtils

/** Abstract superclass for Alerts that have moved from and are still created
 * inside DlgDelegate
 */
open class DlgDelegateAlert : XWDialogFragment() {
    private var mState: DlgState? = null
    protected fun getState(sis: Bundle?): DlgState {
        if (mState == null) {
            mState = if (null != sis) {
                sis.getParcelable<Parcelable>(STATE_KEY) as DlgState?
            } else {
                val args = requireArguments()
                Assert.assertNotNull(args)
                DlgState.fromBundle(args)
            }
        }
        return mState!!
    }

    protected fun addStateArgument(state: DlgState) {
        setArguments(state.toBundle())
    }

    protected open fun populateBuilder(
        context: Context, state: DlgState?,
        builder: AlertDialog.Builder
    ) {
        Log.d(TAG, "populateBuilder()")
        val naView = addNAView(state, builder)
        val lstnr = mkCallbackClickListener(naView)
        if (0 != state!!.m_posButton) {
            builder.setPositiveButton(state.m_posButton, lstnr)
        }
        if (0 != state.m_negButton) {
            builder.setNegativeButton(state.m_negButton, lstnr)
        }
        if (null != state.m_pair) {
            val pair = state.m_pair!!
            builder.setNeutralButton(
                pair.buttonStr,
                mkCallbackClickListener(pair, naView)
            )
        }
    }

    open protected fun create(builder: AlertDialog.Builder): AlertDialog {
        return builder.create()
    }

    private fun addNAView(state: DlgState?, builder: AlertDialog.Builder): NotAgainView {
        val context: Context = requireActivity()
        val naView = (LocUtils.inflate(context, R.layout.not_again_view) as NotAgainView)
            .setMessage(state!!.m_msg)
            .setShowNACheckbox(0 != state.m_prefsNAKey)
        builder.setView(naView)
        return naView
    }

    override fun onCreateDialog(sis: Bundle?): AlertDialog {
        val context = activity as Context
        val state = getState(sis)
        val builder = LocUtils.makeAlertBuilder(context)
        if (null != state.m_title) {
            builder.setTitle(state.m_title)
        }
        populateBuilder(context, state, builder)
        return create(builder)
    }

    override fun onSaveInstanceState(bundle: Bundle) {
        bundle.putParcelable(STATE_KEY, mState)
        super.onSaveInstanceState(bundle)
    }

    override fun onDismiss(dif: DialogInterface) {
        super.onDismiss(dif)
        val activity: Activity? = activity
        if (activity is DlgClickNotify) {
            (activity as DlgClickNotify)
                .onDismissed(mState!!.m_action!!, *mState!!.params)
        }
    }

    override fun getFragTag(): String {
        return getState(null).mID.toString()
    }

    override fun belongsOnBackStack(): Boolean {
        return getState(null).mID.belongsOnBackStack()
    }

    protected fun checkNotAgainCheck(state: DlgState?, naView: NotAgainView?) {
        if (null != naView && naView.getChecked()) {
            if (0 != state!!.m_prefsNAKey) {
                XWPrefs.setPrefsBoolean(
                    activity!!, mState!!.m_prefsNAKey,
                    true
                )
            }
        }
    }

    protected fun mkCallbackClickListener(
        pair: ActionPair,
        naView: NotAgainView? = null
    ): DialogInterface.OnClickListener {
        return DialogInterface.OnClickListener { dlg, button ->
            checkNotAgainCheck(mState, naView)
            val xwact = activity as DlgClickNotify?
            xwact!!.onPosButton(pair.action, *mState!!.params)
        }
    }

    protected fun mkCallbackClickListener(naView: NotAgainView?): DialogInterface.OnClickListener {
        val cbkOnClickLstnr: DialogInterface.OnClickListener
        cbkOnClickLstnr = DialogInterface.OnClickListener { dlg, button ->
            checkNotAgainCheck(mState, naView)
            val activity: Activity? = activity
            if (DlgDelegate.Action.SKIP_CALLBACK != mState!!.m_action
                && activity is DlgClickNotify
            ) {
                val notify = activity as DlgClickNotify
                when (button) {
                    AlertDialog.BUTTON_POSITIVE -> notify.onPosButton(
                        mState!!.m_action!!, *mState!!.params
                    )

                    AlertDialog.BUTTON_NEGATIVE -> {
                        Log.d(TAG, "calling onNegButton(action=${mState!!.m_action})")
                        notify.onNegButton(mState!!.m_action!!, *mState!!.params)
                    }

                    else -> {
                        Log.e(TAG, "unexpected button %d", button)
                        // ignore on release builds
                        Assert.failDbg()
                    }
                }
            }
        }
        return cbkOnClickLstnr
    }

    companion object {
        private val TAG = DlgDelegateAlert::class.java.getSimpleName()
        private const val STATE_KEY = "STATE_KEY"

        @JvmStatic
        fun newInstance(state: DlgState): DlgDelegateAlert {
            val result = DlgDelegateAlert()
            result.addStateArgument(state)
            return result
        }
    }
}
