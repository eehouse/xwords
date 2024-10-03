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

import android.app.Activity
import android.os.Bundle
import android.view.View

import org.eehouse.android.xw4.DBUtils.SentInvitesInfo
import org.eehouse.android.xw4.WiDirService.DevSetListener

class WiDirInviteDelegate(delegator: Delegator) :
    InviteDelegate(delegator), DevSetListener {
    private var m_macsToName: Map<String, String>? = null

    override fun init(savedInstanceState: Bundle?) {
        super.init(savedInstanceState)

        val msg = getQuantityString(
            R.plurals.invite_p2p_desc_fmt, m_nMissing,
            m_nMissing, getString(R.string.button_invite)
        ) + """
            
            
            ${getString(R.string.invite_p2p_desc_extra)}
            """.trimIndent()
        super.init(msg, R.string.empty_p2p_inviter)
    }

    override fun onResume() {
        super.onResume()
        WiDirService.registerDevSetListener(this)
    }

    override fun onPause() {
        super.onPause()
        WiDirService.unregisterDevSetListener(this)
    }

    override fun onBarButtonClicked(id: Int) {
        // not implemented yet as there's no bar button
        Assert.failDbg()
    }

    override fun onChildAdded(child: View, data: InviterItem) {
        val pair = data as TwoStringPair
        (child as TwoStrsItem).setStrings(pair.str2!!, pair.getDev())
    }

    // DevSetListener interface
    override fun setChanged(macToName: Map<String, String>) {
        m_macsToName = macToName
        runOnUiThread { rebuildList() }
    }

    private fun rebuildList() {
        val count = m_macsToName!!.size
        val pairs: MutableList<TwoStringPair> = ArrayList()
        // String[] names = new String[count];
        // String[] addrs = new String[count];
        val iter = m_macsToName!!.keys.iterator()
        for (ii in 0 until count) {
            val mac = iter.next()
            pairs.add(TwoStringPair(mac, m_macsToName!![mac]))
            // addrs[ii] = mac;
            // names[ii] = m_macsToName.get(mac);
        }

        updateList(pairs)
    }

    companion object {
        private val TAG = WiDirInviteDelegate::class.java.getSimpleName()
        private const val SAVE_NAME = "SAVE_NAME"
        fun launchForResult(
            activity: Activity, nMissing: Int,
            info: SentInvitesInfo?,
            requestCode: RequestCode
        ) {
            val intent =
                makeIntent(
                    activity, WiDirInviteActivity::class.java,
                    nMissing, info
                )
            activity.startActivityForResult(intent, requestCode.ordinal)
        }
    }
}
