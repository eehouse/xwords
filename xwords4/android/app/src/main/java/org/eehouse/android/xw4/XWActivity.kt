/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2014 - 2024 by Eric House (xwords@eehouse.org).  All rights
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

import android.R
import android.app.Activity
import android.app.Dialog
import android.content.Intent
import android.content.pm.ActivityInfo
import android.os.Build
import android.os.Bundle
import android.view.ContextMenu
import android.view.ContextMenu.ContextMenuInfo
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.widget.ListAdapter
import android.widget.ListView
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.FragmentActivity
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify.InviteMeans

open class XWActivity : FragmentActivity(), Delegator, DlgClickNotify {
    private var mDlgt: DelegateBase? = null

    protected fun onCreate(savedInstanceState: Bundle?, dlgt: DelegateBase)
        = onCreate(savedInstanceState, dlgt, true)

    protected fun onCreate(
        savedInstanceState: Bundle?, dlgt: DelegateBase,
        setOrientation: Boolean
    ) {
        if (BuildConfig.LOG_LIFECYLE) {
            Log.i(
                TAG, "%s.onCreate(this=%H,sis=%s)", javaClass.getSimpleName(),
                this, savedInstanceState
            )
        }
        super.onCreate(savedInstanceState)
        Assert.assertNotNull(dlgt)
        mDlgt = dlgt
        Assert.assertTrueNR(applicationContext === XWApp.getContext())

        // Looks like there's an Oreo-only bug
        if (setOrientation && Build.VERSION_CODES.O != Build.VERSION.SDK_INT) {
            var orientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
            orientation = if (XWPrefs.getIsTablet(this)) {
                ActivityInfo.SCREEN_ORIENTATION_USER
            } else {
                Assert.assertTrueNR(9 <= Build.VERSION.SDK.toInt())
                ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT
            }
            if (ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED != orientation) {
                requestedOrientation = orientation
            }
        }
        val layoutID = mDlgt!!.layoutID
        if (0 < layoutID) {
            Log.d(TAG, "onCreate() calling setContentView()")
            mDlgt!!.setContentView(layoutID)
        }
        dlgt.init(savedInstanceState)
    }

    override fun onSaveInstanceState(outState: Bundle) {
        if (BuildConfig.LOG_LIFECYLE) {
            Log.i(
                TAG, "%s.onSaveInstanceState(this=%H)",
                javaClass.getSimpleName(), this
            )
        }
        mDlgt!!.onSaveInstanceState(outState)
        super.onSaveInstanceState(outState)
    }

    override fun onPause() {
        if (BuildConfig.LOG_LIFECYLE) {
            Log.i(
                TAG, "%s.onPause(this=%H)", javaClass.getSimpleName(),
                this
            )
        }
        mDlgt!!.onPause()
        super.onPause()
        WiDirWrapper.activityPaused(this)
    }

    override fun onResume() {
        if (BuildConfig.LOG_LIFECYLE) {
            Log.i(
                TAG, "%s.onResume(this=%H)", javaClass.getSimpleName(),
                this
            )
        }
        super.onResume()
        WiDirWrapper.activityResumed(this)
        mDlgt!!.onResume()
    }

    override fun onPostResume() {
        if (BuildConfig.LOG_LIFECYLE) {
            Log.i(
                TAG, "%s.onPostResume(this=%H)",
                javaClass.getSimpleName(), this
            )
        }
        super.onPostResume()
    }

    override fun onStart() {
        if (BuildConfig.LOG_LIFECYLE) {
            Log.i(TAG, "%s.onStart(this=%H)", javaClass.getSimpleName(), this)
        }
        super.onStart()
        Assert.assertNotNull(mDlgt)
        mDlgt!!.onStart() // m_dlgt null?
    }

    override fun onStop() {
        if (BuildConfig.LOG_LIFECYLE) {
            Log.i(TAG, "%s.onStop(this=%H)", javaClass.getSimpleName(), this)
        }
        mDlgt!!.onStop()
        super.onStop()
    }

    override fun onDestroy() {
        if (BuildConfig.LOG_LIFECYLE) {
            Log.i(TAG, "%s.onDestroy(this=%H)", javaClass.getSimpleName(), this)
        }
        mDlgt!!.onDestroy()
        super.onDestroy()
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        perms: Array<String>,
        rslts: IntArray
    ) {
        Perms23.gotPermissionResult(this, requestCode, perms, rslts)
        super.onRequestPermissionsResult(requestCode, perms, rslts)
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        mDlgt!!.onWindowFocusChanged(hasFocus)
    }

    override fun onBackPressed() {
        if (!mDlgt!!.handleBackPressed()) {
            super.onBackPressed()
        }
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        return mDlgt!!.onCreateOptionsMenu(menu)
    }

    override fun onPrepareOptionsMenu(menu: Menu): Boolean {
        return (mDlgt!!.onPrepareOptionsMenu(menu)
                || super.onPrepareOptionsMenu(menu))
    } // onPrepareOptionsMenu

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return (mDlgt!!.onOptionsItemSelected(item)
                || super.onOptionsItemSelected(item))
    }

    override fun onCreateContextMenu(
        menu: ContextMenu, view: View,
        menuInfo: ContextMenuInfo
    ) {
        mDlgt!!.onCreateContextMenu(menu, view, menuInfo)
    }

    override fun onContextItemSelected(item: MenuItem): Boolean {
        return mDlgt!!.onContextItemSelected(item)
    }

    override fun onActivityResult(
        requestCode: Int, resultCode: Int,
        data: Intent?
    ) {
        val rc = RequestCode.entries[requestCode]
        mDlgt!!.onActivityResult(rc, resultCode, data.orEmpty())
    }

    // This are a hack! I need some way to build fragment-based alerts from
    // inside fragment-based alerts.
    fun makeNotAgainBuilder(keyID: Int, msg: String): DlgDelegate.Builder {
        return mDlgt!!.makeNotAgainBuilder(keyID, msg)
    }

    fun makeNotAgainBuilder(
        keyID: Int, msgID: Int,
        vararg params: Any?
    ): DlgDelegate.Builder {
        Assert.assertVarargsNotNullNR(params)
        return mDlgt!!.makeNotAgainBuilder(keyID, msgID, *params)
    }

    fun makeConfirmThenBuilder(
        action: DlgDelegate.Action, msgID: Int,
        vararg params: Any?
    ): DlgDelegate.Builder {
        Assert.assertVarargsNotNullNR(params)
        return mDlgt!!.makeConfirmThenBuilder(action, msgID, *params)
    }

    fun makeOkOnlyBuilder(msgID: Int, vararg params: Any?): DlgDelegate.Builder {
        Assert.assertVarargsNotNullNR(params)
        return mDlgt!!.makeOkOnlyBuilder(msgID, *params)
    }

    //////////////////////////////////////////////////////////////////////
    // Delegator interface
    //////////////////////////////////////////////////////////////////////
    override fun getActivity(): Activity {
        return this
    }

    override fun getArguments(): Bundle {
        return intent.extras!!
    }

    override fun getListView(): ListView {
        return findViewById<View>(R.id.list) as ListView
    }

    override fun setListAdapter(adapter: ListAdapter) {
        getListView().setAdapter(adapter)
    }

    override fun getListAdapter(): ListAdapter {
        return getListView().adapter
    }

    override fun addFragment(fragment: XWFragment, extras: Bundle?) {
        Assert.failDbg()
    }

    override fun addFragmentForResult(
        fragment: XWFragment, extras: Bundle,
        request: RequestCode
    ) {
        Assert.failDbg()
    }

    fun show(df: XWDialogFragment) {
        val fm = supportFragmentManager
        val tag = df.getFragTag()
        // Log.d( TAG, "show(%s); tag: %s", df.getClass().getSimpleName(), tag );
        try {
            if (df.belongsOnBackStack()) {
                val trans = fm.beginTransaction()
                val prev = fm.findFragmentByTag(tag)
                if (null != prev && prev is DialogFragment) {
                    prev.dismiss()
                }
                trans.addToBackStack(tag)
                df.show(trans, tag)
            } else {
                df.show(fm, tag)
            }
        } catch (ise: IllegalStateException) {
            Log.d(TAG, "error showing tag %s (df: %s; msg: %s)", tag, df, ise)
            // DLG_SCORES is causing this for non-belongsOnBackStack() case
            // Assert.assertFalse( BuildConfig.DEBUG );
        }
    }

    internal fun makeDialog(alert: DBAlert, vararg params: Any?): Dialog {
        Assert.assertVarargsNotNullNR(params)
        return mDlgt!!.makeDialog(alert, *params)!!
    }

    fun getDelegate(): DelegateBase { return mDlgt!! }

    ////////////////////////////////////////////////////////////
    // DlgClickNotify interface
    ////////////////////////////////////////////////////////////
    override fun onPosButton(action: DlgDelegate.Action, vararg params: Any?): Boolean {
        Assert.assertVarargsNotNullNR(params)
        return mDlgt!!.onPosButton(action, *params)
    }

    override fun onNegButton(action: DlgDelegate.Action, vararg params: Any?): Boolean {
        Assert.assertVarargsNotNullNR(params)
        return mDlgt!!.onNegButton(action, *params)
    }

    override fun onDismissed(action: DlgDelegate.Action, vararg params: Any?): Boolean {
        Assert.assertVarargsNotNullNR(params)
        return mDlgt!!.onDismissed(action, *params)
    }

    override fun inviteChoiceMade(
        action: DlgDelegate.Action, means: InviteMeans,
        vararg params: Any?
    ) {
        Assert.assertVarargsNotNullNR(params)
        mDlgt!!.inviteChoiceMade(action, means, *params)
    }

    companion object {
        private val TAG = XWActivity::class.java.getSimpleName()
    }
}
