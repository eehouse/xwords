/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */ /*
 * Copyright 2009 - 2016 by Eric House (xwords@eehouse.org).  All
 * rights reserved.
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
import android.app.Dialog
import android.content.Intent
import android.os.Bundle
import android.view.ContextMenu
import android.view.ContextMenu.ContextMenuInfo
import android.view.MenuItem
import android.view.View

import org.eehouse.android.xw4.DlgDelegate.Action
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify.InviteMeans

class DualpaneDelegate(delegator: Delegator) :
    DelegateBase(delegator, R.layout.dualcontainer) {
    private val m_activity: Activity = delegator.getActivity()!!

    override fun init(savedInstanceState: Bundle?) {}

    override fun makeDialog(alert: DBAlert, vararg params: Any?): Dialog {
        var dialog: Dialog? = null
        val main = m_activity as MainActivity
        val frags = main.getFragments(false)
        for (frag in frags) {
            dialog = frag!!.getDelegate()!!.makeDialog(alert, *params)
            if (null != dialog) {
                break
            }
        }
        return dialog!!
    }

    override fun handleNewIntent(intent: Intent) {
        val main = m_activity as MainActivity
        main.dispatchNewIntent(intent)
        Log.i(TAG, "handleNewIntent()")
    }

    override fun handleBackPressed(): Boolean {
        val main = m_activity as MainActivity
        val handled = main.dispatchBackPressed()
        Log.i(TAG, "handleBackPressed() => %b", handled)
        return handled
    }

    override fun onActivityResult(requestCode: RequestCode,
                                  resultCode: Int,
                                  data: Intent)
    {
        val main = m_activity as MainActivity
        main.dispatchOnActivityResult(requestCode, resultCode, data)
    }

    override fun onCreateContextMenu(
        menu: ContextMenu, view: View,
        menuInfo: ContextMenuInfo
    ) {
        val main = m_activity as MainActivity
        main.dispatchOnCreateContextMenu(menu, view, menuInfo)
    }

    override fun onContextItemSelected(item: MenuItem): Boolean {
        val main = m_activity as MainActivity
        return main.dispatchOnContextItemSelected(item)
    }

    override fun onPosButton(action: Action, vararg params: Any?): Boolean {
        var handled = false
        val main = m_activity as MainActivity
        val frags = main.visibleFragments
        for (frag in frags) {
            handled = frag!!.getDelegate()!!.onPosButton(action, *params)
            if (handled) {
                break
            }
        }
        return handled
    }

    override fun onNegButton(action: Action, vararg params: Any?): Boolean
    {
        var handled = false
        val main = m_activity as MainActivity
        val frags = main.visibleFragments
        for (frag in frags) {
            handled = frag!!.getDelegate()!!.onNegButton(action, *params)
            if (handled) {
                break
            }
        }
        return handled
    }

    override fun onDismissed(action: Action, vararg params: Any?): Boolean
    {
        var handled = false
        val main = m_activity as MainActivity
        val frags = main.visibleFragments
        for (frag in frags) {
            handled = frag!!.getDelegate()!!.onDismissed(action, *params)
            if (handled) {
                break
            }
        }
        return handled
    }

    override fun inviteChoiceMade(
        action: Action, means: InviteMeans,
        vararg params: Any?
    ) {
        val main = m_activity as MainActivity
        val frags = main.visibleFragments
        for (frag in frags) {
            frag!!.getDelegate()!!.inviteChoiceMade(action, means, *params)
        }
    }

    companion object {
        private val TAG: String = DualpaneDelegate::class.java.simpleName
    }
}
