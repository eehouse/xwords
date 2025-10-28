/*
 * Copyright 2017 - 2025 by Eric House (xwords@eehouse.org).  All rights
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
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

import java.lang.ref.WeakReference

import org.eehouse.android.xw4.DlgDelegate.Action
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify.InviteMeans
import org.eehouse.android.xw4.InviteView.ItemClicked
import org.eehouse.android.xw4.NFCUtils.nfcAvail
import org.eehouse.android.xw4.jni.GameRef
import org.eehouse.android.xw4.jni.Knowns
import org.eehouse.android.xw4.loc.LocUtils

class InviteChoicesAlert : DlgDelegateAlert(), ItemClicked {
    private var mInviteView: InviteView? = null
    private var mDialog: AlertDialog? = null
    override fun onDestroy() {
        sSelf = null
        super.onDestroy()
    }

    public override fun populateBuilder(
        context: Context, state: DlgState?,
        builder: AlertDialog.Builder
    ) {
        val means = ArrayList<InviteMeans>()
        val lastMeans: InviteMeans? = null
        var gr: GameRef? = null
        val params = state!!.getParams()
        var nMissing = 0
        var nInvited = 0
        if (0 < params.size && params[0] is GameRef) {
            gr = params[0] as GameRef
        }
        if (1 < params.size && params[1] is Int) {
            nMissing = params[1] as Int
        }
        if (2 < params.size && params[2] is Int) {
            nInvited = params[2] as Int
        }
        Log.d(
            TAG, "populateBuilder(): nMissing=%d, nInvited=%d",
            nMissing, nInvited
        )
        means.add(InviteMeans.EMAIL)
        means.add(InviteMeans.SMS_USER)
        if (Utils.deviceSupportsNBS(context)) {
            means.add(InviteMeans.SMS_DATA)
        }
        if (BuildConfig.NON_RELEASE) {
            means.add(InviteMeans.MQTT)
        }
        means.add(InviteMeans.QRCODE)
        if (BTUtils.BTAvailable()) {
            means.add(InviteMeans.BLUETOOTH)
        }
        if (WiDirWrapper.enabled()) {
            means.add(InviteMeans.WIFIDIRECT)
        }
        if (nfcAvail(context)[0]) {
            means.add(InviteMeans.NFC)
        }
        means.add(InviteMeans.CLIPBOARD)
        var lastSelMeans = -1
        lastMeans?.let {
            for (ii in means.indices) {
                if (it == means[ii]) {
                    lastSelMeans = ii
                    break
                }
            }
        }
        mInviteView = LocUtils
            .inflate(context, R.layout.invite_view) as InviteView
        val okClicked = DialogInterface.OnClickListener { dlg, pos ->
            Assert.assertTrue(Action.SKIP_CALLBACK != state.m_action)
            val inviteView = mInviteView!!
            inviteView.getChoice()?.let { choice ->
                val activity = context as XWActivity
                if (choice is InviteMeans) {
                    activity.inviteChoiceMade(
                        state.m_action!!,
                        choice, *state.getParams()
                    )
                } else if (choice is Array<*> && choice.isArrayOf<String>()) {
                    inviteView.launch {
                        val addrs = choice
                            .map{player -> Knowns.getAddr(player as String) as Any}
                            .toTypedArray()
                        withContext(Dispatchers.Main) {
                            activity.onPosButton(state.m_action!!, *addrs)
                        }
                    }
                } else {
                    Assert.failDbg()
                }
            }
        }
        builder
            .setTitle(R.string.invite_choice_title)
            .setView(mInviteView)
            .setPositiveButton(android.R.string.ok, okClicked)
            .setNegativeButton(android.R.string.cancel, null)
        mInviteView!!.setChoices(
            means, nMissing, nInvited
        )
            .setGR(gr)
            .setCallbacks(this)

        // if ( BuildConfig.DEBUG ) {
        //     OnClickListener ocl = new OnClickListener() {
        //             @Override
        //             public void onClick( DialogInterface dlg, int pos ) {
        //                 Object[] params = state.getParams();
        //                 if ( params[0] instanceof SentInvitesInfo ) {
        //                     SentInvitesInfo sii = (SentInvitesInfo)params[0];
        //                     sii.setRemotesRobots();
        //                 }
        //                 okClicked.onClick( dlg, pos );
        //             }
        //         };
        //     builder.setNeutralButton( R.string.ok_with_robots, ocl );
        // }
    }

    public override fun create(builder: AlertDialog.Builder): AlertDialog {
        mDialog = super.create(builder)
        mDialog!!.setOnShowListener { enableOkButton() }
        return mDialog!!
    }

    override fun meansClicked(means: InviteMeans) {
        val activity = activity as XWActivity
        var builder = when (means) {
            InviteMeans.SMS_USER -> activity
                .makeNotAgainBuilder(
                    R.string.key_na_sms_invite_flakey,
                    R.string.sms_invite_flakey
                )

            InviteMeans.CLIPBOARD -> {
                val msg = getString(
                    R.string.not_again_clip_expl_fmt,
                    getString(R.string.slmenu_copy_sel)
                )
                activity.makeNotAgainBuilder(R.string.key_na_clip_expl, msg)
            }

            InviteMeans.QRCODE -> activity.makeNotAgainBuilder(
                    R.string.key_na_qrcode_invite,
                    R.string.qrcode_invite_expl
                )

            InviteMeans.SMS_DATA -> {
                if (Perms23.NBSPermsInManifest(activity)
                        && !XWPrefs.getNBSEnabled(requireContext())) {
                    activity.makeConfirmThenBuilder(
                        Action.ENABLE_NBS_ASK,
                        R.string.warn_sms_disabled
                    )
                    .setPosButton(R.string.button_enable_sms)
                    .setNegButton(R.string.button_later)
                } else {
                    null
                }
            }
            else -> null
        }
        builder?.show()
    }

    override fun checkButton() {
        enableOkButton()
    }

    private fun enableOkButton() {
        val enable = null != mInviteView!!.getChoice()
        Utils.enableAlertButton(mDialog!!, AlertDialog.BUTTON_POSITIVE, enable)
    }

    companion object {
        private val TAG = InviteChoicesAlert::class.java.getSimpleName()
        private var sSelf: WeakReference<InviteChoicesAlert>? = null
        fun newInstance(state: DlgState?): InviteChoicesAlert {
            val result = InviteChoicesAlert()
            result.addStateArgument(state!!)
            sSelf = WeakReference(result)
            return result
        }

        fun dismissAny(): Boolean {
            var dismissed = false
            val ref = sSelf
            ref?.let {
                val self = it.get()
                self?.let {
                    it.dismiss()
                    dismissed = true
                }
            }
            return dismissed
        }
    }
}
