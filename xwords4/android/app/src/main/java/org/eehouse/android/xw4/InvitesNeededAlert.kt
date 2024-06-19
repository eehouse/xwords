/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2024 by Eric House (xwords@eehouse.org).  All rights
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
import org.eehouse.android.xw4.Assert.assertTrueNR
import org.eehouse.android.xw4.Assert.assertVarargsNotNullNR
import org.eehouse.android.xw4.Assert.failDbg
import org.eehouse.android.xw4.DBUtils.getInvitesFor
import org.eehouse.android.xw4.DbgUtils.assertOnUIThread
import org.eehouse.android.xw4.InviteChoicesAlert.Companion.dismissAny
import org.eehouse.android.xw4.jni.CommsAddrRec
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.XwJNI.Companion.kplr_nameForMqttDev
import org.eehouse.android.xw4.jni.XwJNI.GamePtr
import org.eehouse.android.xw4.loc.LocUtils.getQuantityString
import org.eehouse.android.xw4.loc.LocUtils.getString
import java.io.Serializable

private val TAG: String = InvitesNeededAlert::class.java.simpleName

internal class InvitesNeededAlert private constructor(
    private val mDelegate: DelegateBase, private val mState: State)
{
    private var mAlert: DBAlert? = null

     // PENDING should be calling gamePtr.retain()?  Would need to release
     // then somewhere
    internal class Wrapper(private val mCallbacks: Callbacks,
                           private val mGamePtr: GamePtr)
    {
        private var mSelf: InvitesNeededAlert? = null
        private var mHostAddr: CommsAddrRec? = null

        fun showOrHide(
            hostAddr: CommsAddrRec?, nPlayersMissing: Int, nInvited: Int,
            isRematch: Boolean
        ) {
            mHostAddr = hostAddr
            val isServer = null == hostAddr
            assertOnUIThread()
            Log.d(TAG, "showOnceIf(nPlayersMissing=%d); self: %s", nPlayersMissing, mSelf)

            if (null == mSelf && 0 == nPlayersMissing) {
                // cool: need and have nothing, so do nothing
            } else if (0 < nPlayersMissing && null == mSelf) { // Need but don't have
                makeNew(isServer, nPlayersMissing, nInvited, isRematch)
            } else if (0 == nPlayersMissing && null != mSelf) { // Have and need to close
                mSelf!!.close()
            } else if (null != mSelf &&
                           nPlayersMissing != mSelf!!.mState.mNPlayersMissing) {
                mSelf!!.close()
                makeNew(isServer, nPlayersMissing, nInvited, isRematch)
            } else if (null != mSelf &&
                           nPlayersMissing == mSelf!!.mState.mNPlayersMissing) {
                // nothing to do
            } else {
                failDbg()
            }
        }

        fun make(alert: DBAlert, vararg params: Any?): AlertDialog {
            assertOnUIThread()
            return mSelf!!.makeImpl(mCallbacks, alert, mHostAddr,
                                    mGamePtr, *params)
        }

        fun dismiss() {
            Log.d(TAG, "dismiss()")
            assertOnUIThread()
            if (null != mSelf && mSelf!!.close()) {
                mSelf = null
            }
        }

        private fun makeNew(
            isServer: Boolean, nPlayersMissing: Int,
            nInvited: Int, isRematch: Boolean
        ) {
            Log.d(
                TAG, "makeNew(nPlayersMissing=%d, nInvited=%d)",
                nPlayersMissing, nInvited
            )
            val state = State(isServer, nPlayersMissing, nInvited, isRematch)
            val delegate = mCallbacks.getDelegate()
            mSelf = InvitesNeededAlert(delegate, state)
            delegate.showDialogFragment(DlgID.DLG_INVITE, state)
        }
    }

    // Must be kept separate from this because gets passed as param to
    // showDialogFragment()
    private class State(
        val mIsServer: Boolean,
        val mNPlayersMissing: Int,
        val mNInvited: Int,
        val mIsRematch: Boolean
    ) : Serializable

    internal interface Callbacks {
        fun getDelegate(): DelegateBase
        fun getRowID(): Long
        fun onCloseClicked()
        fun onInviteClicked()
    }

    private fun close(): Boolean {
        var dismissed = false
        assertOnUIThread()
        if (null != mAlert) {
            dismissed = dismissAny()
            try {
                mAlert!!.dismiss() // I've seen this throw a NPE inside
            } catch (ex: Exception) {
                Log.ex(TAG, ex)
            }
        }
        return dismissed
    }

    private fun makeImpl(
        callbacks: Callbacks, alert: DBAlert,
        hostAddr: CommsAddrRec?, gamePtr: GamePtr,
        vararg params: Any?
    ): AlertDialog {
        val state = params[0] as State
        val ab = mDelegate.makeAlertBuilder()
        mAlert = alert
        val closeLoc = intArrayOf(AlertDialog.BUTTON_NEGATIVE)

        if (state.mIsServer) {
            makeImplHost(ab, callbacks, alert, state, gamePtr, closeLoc)
        } else {
            makeImplGuest(ab, state, hostAddr)
        }

        alert.setOnCancelListener(object : XWDialogFragment.OnCancelListener {
            override fun onCancelled(frag: XWDialogFragment?) {
                // Log.d( TAG, "onCancelled(frag=%s)", frag );
                callbacks.onCloseClicked()
                close()
            }
        })

        val onClose = DialogInterface.OnClickListener { dlg, item ->
            callbacks.onCloseClicked()
        }
        when (closeLoc[0]) {
            AlertDialog.BUTTON_NEGATIVE -> alert.setNoDismissListenerNeg(
                ab,
                R.string.button_close_game,
                onClose
            )

            AlertDialog.BUTTON_POSITIVE -> alert.setNoDismissListenerPos(
                ab,
                R.string.button_close_game,
                onClose
            )

            else -> failDbg()
        }
        val result = ab.create()
        result.setCanceledOnTouchOutside(false)
        return result
    }

    private fun makeImplGuest(
        ab: AlertDialog.Builder, state: State,
        hostAddr: CommsAddrRec?
    ) {
        val context: Context = mDelegate.getActivity()
        var message = getString(context, R.string.waiting_host_expl)

        if (1 < state.mNPlayersMissing) {
            message += """
                
                
                ${getString(context, R.string.waiting_host_expl_multi)}
                """.trimIndent()
        }

        if (BuildConfig.NON_RELEASE && null != hostAddr && hostAddr.contains(CommsConnType.COMMS_CONN_MQTT)) {
            val name = kplr_nameForMqttDev(hostAddr.mqtt_devID)
            if (null != name) {
                message += "\n\n" + getString(context, R.string.missing_host_fmt, name)
            }
        }

        ab.setTitle(R.string.waiting_host_title)
            .setMessage(message)
    }

    private fun makeImplHost(
        ab: AlertDialog.Builder, callbacks: Callbacks,
        alert: DBAlert, state: State, gamePtr: GamePtr,
        closeLoc: IntArray
    ) {
        val context: Context = mDelegate.getActivity()
        val nPlayersMissing = state.mNPlayersMissing

        val rowid = callbacks.getRowID()
        val sentInfo = getInvitesFor(context, rowid)

        val nSent = state.mNInvited + sentInfo.minPlayerCount
        val invitesNeeded = nPlayersMissing > nSent && !state.mIsRematch

        val title: String
        val isRematch = state.mIsRematch
        title = if (isRematch) {
            getString(context, R.string.waiting_rematch_title)
        } else {
            getQuantityString(
                context, R.plurals.waiting_title_fmt,
                nPlayersMissing, nPlayersMissing
            )
        }
        ab.setTitle(title)

        var message: String
        var inviteButtonTxt = 0
        if (invitesNeeded) {
            assertTrueNR(!isRematch)
            message = getString(context, R.string.invites_unsent)
            inviteButtonTxt = R.string.newgame_invite
        } else {
            message = getQuantityString(
                context, R.plurals.invite_msg_fmt,  // here
                nPlayersMissing, nPlayersMissing
            )
            if (isRematch) {
                message += """
                    
                    
                    ${getString(context, R.string.invite_msg_extra_rematch)}
                    """.trimIndent()
            } else {
                inviteButtonTxt = R.string.newgame_reinvite // here
                message += """
                    
                    
                    ${getString(context, R.string.invite_msg_extra_norematch)}
                    """.trimIndent()
            }
        }
        ab.setMessage(message)

        // If user needs to act, emphasize that by having the positive button
        // be Invite. If not, have the positive button be Close
        val onInvite = DialogInterface.OnClickListener { dlg, item -> callbacks.onInviteClicked() }

        if (invitesNeeded) {
            alert.setNoDismissListenerPos(ab, inviteButtonTxt, onInvite)
        } else if (0 != inviteButtonTxt) {
            alert.setNoDismissListenerNeg(ab, inviteButtonTxt, onInvite)
            closeLoc[0] = DialogInterface.BUTTON_POSITIVE
        }

        // if ( BuildConfig.NON_RELEASE && 0 < nSent ) {
        //     alert.setNoDismissListenerNeut( ab, R.string.button_invite_history,
        //                                     new OnClickListener() {
        //                                         @Override
        //                                         public void onClick( DialogInterface dlg, int item ) {
        //                                             callbacks.onInfoClicked( sentInfo );
        //                                         }
        //                                     } );
        // }
    }
}
