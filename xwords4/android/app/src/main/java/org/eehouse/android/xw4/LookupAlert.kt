/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2017-2024 by Eric House (xwords@eehouse.org).  All rights
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
import android.app.Dialog
import android.os.Bundle
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.loc.LocUtils

class LookupAlert : XWDialogFragment() {
    private var m_view: LookupAlertView? = null
    override fun onSaveInstanceState(bundle: Bundle) {
        m_view!!.saveInstanceState(bundle)
        super.onSaveInstanceState(bundle)
    }

    override fun onCreateDialog(sis: Bundle?): Dialog {
        var sis = sis
        val activity: Activity? = activity
        if (null == sis) {
            sis = requireArguments()
        }
        val context = requireContext()
        m_view = LocUtils.inflate(context, R.layout.lookup) as LookupAlertView
        m_view!!.init(object:LookupAlertView.OnDoneListener {
            override fun onDone() { dismiss() }
        }, sis)
        val result: Dialog = LocUtils.makeAlertBuilder(context)
            .setView(m_view)
            .create()
        result.setOnKeyListener(m_view)
        return result
    }

    override fun getFragTag(): String {
        return TAG
    }

    companion object {
        private val TAG = LookupAlert::class.java.getSimpleName()
        @JvmStatic
        fun newInstance(words: Array<String>, isoCode: ISOCode?,
                        noStudy: Boolean): LookupAlert {
            val result = LookupAlert()
            val bundle = LookupAlertView.makeParams(words, isoCode, noStudy)
            result.setArguments(bundle)
            return result
        }
    }
}
