/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2016 - 2024 by Eric House (xwords@eehouse.org).  All rights
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

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat

import java.io.Serializable
import java.util.Arrays

import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.loc.LocUtils

object Perms23 {
    private val TAG: String = Perms23::class.java.simpleName

    @JvmField
    val NBS_PERMS: Array<Perm> = arrayOf(
        Perm.SEND_SMS,
        Perm.READ_SMS,
        Perm.RECEIVE_SMS,
        Perm.READ_PHONE_NUMBERS,
        Perm.READ_PHONE_STATE,
    )

    private val sManifestMap: MutableMap<Perm?, Boolean> = HashMap()
    fun permInManifest(context: Context, perm: Perm): Boolean {
        var result = false
        if (sManifestMap.containsKey(perm)) {
            result = sManifestMap[perm]!!
        } else {
            val pm = context.packageManager
            try {
                val pis = pm
                    .getPackageInfo(
                        BuildConfig.APPLICATION_ID,
                        PackageManager.GET_PERMISSIONS
                    )
                    .requestedPermissions
                if (pis == null) {
                    Assert.failDbg()
                } else {
                    val manifestName = perm.string
                    var ii = 0
                    while (!result && ii < pis.size) {
                        result = pis[ii] == manifestName
                        ++ii
                    }
                }
            } catch (nnfe: PackageManager.NameNotFoundException) {
                Log.e(TAG, "permInManifest() nnfe: %s", nnfe.message)
            }
            sManifestMap[perm] = result
        }
        return result
    }

    fun permsInManifest(context: Context, perms: Array<Perm>): Boolean {
        var result = true
        var ii = 0
        while (result && ii < perms.size) {
            result = permInManifest(context, perms[ii])
            ++ii
        }

        return result
    }

    // Is the OS supporting runtime permission natively, i.e. version 23/M or
    // later.
    fun haveNativePerms(): Boolean {
        val result = Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
        return result
    }

    /**
     * Request permissions, giving rationale once, then call with action and
     * either positive or negative, the former if permission granted.
     */
    private fun tryGetPermsImpl(
        delegate: DelegateBase, perms: Array<Perm>,
        rationaleMsg: String?, naKey: Int,
        action: DlgDelegate.Action, vararg params: Any?
    ) {
        // Log.d( TAG, "tryGetPermsImpl(${DbgUtils.fmtAny(perms)})")
        if (0 != naKey &&
            XWPrefs.getPrefsBoolean(delegate.getActivity(), naKey, false)
        ) {
            postNeg(delegate, action, *params)
        } else {
            QueryInfo(
                delegate, action, perms, rationaleMsg,
                naKey, arrayOf(*params)
            ).doIt(true)
        }
    }

    private fun postNeg(
        delegate: DelegateBase,
        action: DlgDelegate.Action, vararg params: Any?
    ) {
        delegate.post { delegate.onNegButton(action, *params) }
    }

    fun tryGetPerms(
        delegate: DelegateBase, perms: Array<Perm>, rationaleId: Int,
        action: DlgDelegate.Action, vararg params: Any?
    ) {
        // Log.d( TAG, "tryGetPerms(perms: $perms, params: ${DbgUtils.fmtAny(params)})")
        val msg = LocUtils.getStringOrNull(rationaleId)
        tryGetPermsImpl(delegate, perms, msg, 0, action, *params)
    }

    fun tryGetPerms(
        delegate: DelegateBase, perms: Array<Perm>,
        rationaleMsg: String?, action: DlgDelegate.Action,
        vararg params: Any?
    ) {
        tryGetPermsImpl(delegate, perms, rationaleMsg, 0, action, *params)
    }

    fun tryGetPerms(
        delegate: DelegateBase, perm: Perm,
        rationaleMsg: String?, action: DlgDelegate.Action,
        vararg params: Any?
    ) {
        tryGetPermsImpl(
            delegate, arrayOf(perm), rationaleMsg, 0,
            action, *params
        )
    }

    fun tryGetPerms(
        delegate: DelegateBase, perm: Perm, rationaleId: Int,
        action: DlgDelegate.Action, vararg params: Any?
    ) {
        tryGetPerms(delegate, arrayOf(perm), rationaleId, action, *params)
    }

    fun tryGetPermsNA(
        delegate: DelegateBase, perm: Perm,
        rationaleId: Int, naKey: Int,
        action: DlgDelegate.Action, vararg params: Any?
    ) {
        tryGetPermsImpl(
            delegate, arrayOf(perm),
            LocUtils.getStringOrNull(rationaleId), naKey,
            action, *params
        )
    }

    class GotPermsState(val action: DlgDelegate.Action,
                        val perms: Array<Perm>,
                        val msg: String?,
                        val params: Array<Any?>) : Serializable

    fun onGotPermsAction(
        delegate: DelegateBase, positive: Boolean,
        state: GotPermsState
    ) {
        // Log.d(TAG, "onGotPermsAction(params=${DbgUtils.fmtAny(state)})")
        val info = QueryInfo(
            delegate, state.action,
            state.perms, state.msg, 0,
            state.params
        )
        info.handleButton(positive)
    }

    private val s_map: MutableMap<Int, PermCbck?> = HashMap()
    fun gotPermissionResult(
        context: Context, code: Int,
        perms: Array<String>, granteds: IntArray
    ) {
        // Log.d( TAG, "gotPermissionResult(%s)", perms.toString() );
        var shouldResend = false
        var allGood = true
        for (ii in perms.indices) {
            val perm = Perm.getFor(perms[ii])!!
            Assert.assertTrueNR(permInManifest(context, perm))
            val granted = PackageManager.PERMISSION_GRANTED == granteds[ii]
            allGood = allGood && granted

            // Hack. If SMS has been granted, resend all moves. This should be
            // replaced with an api allowing listeners to register
            // Perm-by-Perm, but I'm in a hurry.
            if (granted && Arrays.asList<Perm?>(*NBS_PERMS).contains(perm)) {
                shouldResend = true
            }

            // Log.d( TAG, "calling %s.onPermissionResult(%s, %b)",
            //                record.cbck.getClass().getSimpleName(), perm.toString(),
            //                granted );
        }

        if (shouldResend) {
            GameUtils.resendAllIf(
                context, CommsConnType.COMMS_CONN_SMS,
                true, true
            )
        }

        val cbck = s_map.remove(code)
        if (null != cbck) {
            callOPR(cbck, allGood)
        }
    }

    fun havePermissions(context: Context, vararg perms: Perm): Boolean {
        var result = true
        for (perm in perms) {
            val thisResult = (permInManifest(context, perm)
                    && PackageManager.PERMISSION_GRANTED
                    == ContextCompat.checkSelfPermission(
                XWApp.getContext(),
                perm.string!!
            ))
            // Log.d( TAG, "havePermissions(): %s: %b", perm, thisResult );
            result = result && thisResult
        }
        return result
    }

    fun haveNBSPerms(context: Context): Boolean {
        val result = havePermissions(context, *NBS_PERMS)
        Log.d(TAG, "haveNBSPerms() => %b", result)
        return result
    }

    fun NBSPermsInManifest(context: Context): Boolean {
        return permsInManifest(context, NBS_PERMS)
    }

    fun tryGetNBSPermsNA(
        delegate: DelegateBase, rationaleId: Int,
        naKey: Int, action: DlgDelegate.Action, vararg params: Any?
    ) {
        tryGetPermsImpl(
            delegate, NBS_PERMS,
            LocUtils.getStringOrNull(rationaleId), naKey,
            action, *params
        )
    }

    // If two permission requests are made in a row the map may contain more
    // than one entry.
    private var s_nextRecord = 0
    private fun register(cbck: PermCbck?): Int {
        DbgUtils.assertOnUIThread()
        val code = ++s_nextRecord
        s_map[code] = cbck
        return code
    }

    private fun callOPR(cbck: PermCbck, allGood: Boolean) {
        Log.d(TAG, "callOPR(): passing %b to %s", allGood, cbck)
        cbck.onPermissionResult(allGood)
    }

    enum class Perm(str: String) {
        READ_PHONE_STATE(Manifest.permission.READ_PHONE_STATE),
        READ_SMS(Manifest.permission.READ_SMS),
        STORAGE(Manifest.permission.WRITE_EXTERNAL_STORAGE),
        SEND_SMS(Manifest.permission.SEND_SMS),
        RECEIVE_SMS(Manifest.permission.RECEIVE_SMS),
        READ_PHONE_NUMBERS(Manifest.permission.READ_PHONE_NUMBERS),
        READ_CONTACTS(Manifest.permission.READ_CONTACTS),
        BLUETOOTH_CONNECT(Manifest.permission.BLUETOOTH_CONNECT),
        BLUETOOTH_SCAN(Manifest.permission.BLUETOOTH_SCAN),
        REQUEST_INSTALL_PACKAGES(Manifest.permission.REQUEST_INSTALL_PACKAGES),
        POST_NOTIFICATIONS(Manifest.permission.POST_NOTIFICATIONS);

        var string: String? = null

        init {
            string = str
        }

        companion object {
            fun getFor(str: String): Perm? {
                var result: Perm? = null
                for (one in entries) {
                    if (one.string == str) {
                        result = one
                        break
                    }
                }
                return result
            }
        }
    }

    interface PermCbck {
        fun onPermissionResult(allGood: Boolean)
    }

    interface OnShowRationale {
        fun onShouldShowRationale(perms: Set<Perm>)
    }

    class Builder {
        private val m_perms: MutableSet<Perm> = HashSet()
        private var m_onShow: OnShowRationale? = null

        constructor(perms: Set<Perm>?) {
            m_perms.addAll(perms!!)
        }

        constructor(vararg perms: Perm) {
            for (perm in perms) {
                m_perms.add(perm)
            }
        }

        fun setOnShowRationale(onShow: OnShowRationale?): Builder {
            m_onShow = onShow
            return this
        }

        // We have set of permissions. For any of them that needs asking (not
        // granted AND not banned) start an ask.
        //
        // PENDING: I suspect this'll crash if I ask for a banned and
        // non-banned at the same time (and don't have either)
        @JvmOverloads
        fun asyncQuery(activity: Activity, cbck: PermCbck? = null) {
            Log.d(TAG, "asyncQuery(%s)", m_perms)
            var haveAll = true
            val shouldShow = false
            val needShow: MutableSet<Perm> = HashSet()

            val askStrings: MutableList<String?> = ArrayList()
            for (perm in m_perms) {
                val permStr = perm.string
                val inManifest = permInManifest(activity, perm)
                val haveIt = (inManifest
                        && PackageManager.PERMISSION_GRANTED
                        == ContextCompat.checkSelfPermission(activity, permStr!!))

                if (!haveIt && inManifest) {
                    // do not pass banned perms to the OS! They're not in
                    // AndroidManifest.xml so may crash on some devices
                    askStrings.add(permStr)

                    if (null != m_onShow) {
                        needShow.add(perm)
                    }
                }

                haveAll = haveAll && haveIt
            }

            if (haveAll) {
                if (null != cbck) {
                    callOPR(cbck, true)
                }
            } else if (0 < needShow.size && null != m_onShow) {
                // Log.d( TAG, "calling onShouldShowRationale()" );
                m_onShow!!.onShouldShowRationale(needShow)
            } else {
                val permsArray = askStrings.toTypedArray<String?>()
                val code = register(cbck)
                // Log.d( TAG, "calling requestPermissions on %s",
                //                activity.getClass().getSimpleName() );
                ActivityCompat.requestPermissions(activity, permsArray, code)
            }
        }
    }

    private class QueryInfo(
        private val mDelegate: DelegateBase,
        private val mAction: DlgDelegate.Action,
        private val mPerms: Array<Perm>,
        private val mRationaleMsg: String?,
        private val mNAKey: Int,
        private val mParams: Array<Any?>
    ) {
        private fun getParams(): Array<Any?>
        {
            val params = arrayOf(mAction, mPerms, mRationaleMsg, *mParams)
            // Log.d(TAG, "getParams() => ${DbgUtils.fmtAny(params)}")
            return params
        }

        fun doIt(showRationale: Boolean) {
            val validPerms: MutableSet<Perm> = HashSet()
            for (perm in mPerms) {
                if (permInManifest(mDelegate.getActivity(), perm)) {
                    validPerms.add(perm)
                }
            }

            if (0 < validPerms.size) {
                doItAsk(validPerms, showRationale)
            }
        }

        private fun shouldShowAny(perms: Set<Perm>): Boolean {
            var result = false
            val activity = mDelegate.getActivity()
            for (perm in perms) {
                result = result || ActivityCompat
                    .shouldShowRequestPermissionRationale(activity, perm.string!!)
                Log.d(TAG, "shouldShow(%s) => %b", perm, result)
            }
            return result
        }

        private fun mkGotPermsState(): GotPermsState
            = GotPermsState(mAction, mPerms, mRationaleMsg, mParams)

        private fun doItAsk(perms: Set<Perm>, showRationale: Boolean) {
            val builder = Builder(perms)
            if (showRationale && shouldShowAny(perms) && null != mRationaleMsg) {
                builder.setOnShowRationale(object : OnShowRationale {
                    override fun onShouldShowRationale(perms: Set<Perm>) {
                        mDelegate.makeConfirmThenBuilder(
                            DlgDelegate.Action.PERMS_QUERY,
                            mRationaleMsg
                        )
                            .setTitle(R.string.perms_rationale_title)
                            .setPosButton(R.string.button_ask)
                            .setNegButton(R.string.button_deny)
                            .setParams(mkGotPermsState())
                            .setNAKey(mNAKey)
                            .show()
                    }
                })
            }
            builder.asyncQuery(mDelegate.getActivity(), object : PermCbck {
                override fun onPermissionResult(allGood: Boolean) {
                    if (DlgDelegate.Action.SKIP_CALLBACK != mAction) {
                        if (allGood) {
                            mDelegate.onPosButton(mAction, *mParams)
                        } else {
                            mDelegate.onNegButton(mAction, *mParams)
                        }
                    }
                }
            })
        }

        // Post this in case we're called from inside dialog dismiss
        // code. Better to unwind the stack...
        fun handleButton(positive: Boolean) {
            if (positive) {
                mDelegate.post { doIt(false) }
            } else {
                postNeg()
            }
        }

        private fun postNeg() {
            postNeg(mDelegate, mAction, *mParams)
        }
    }
}
