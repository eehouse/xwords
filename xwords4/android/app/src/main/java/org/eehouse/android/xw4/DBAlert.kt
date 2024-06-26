/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */ /*
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

import android.R
import android.app.Dialog
import android.os.Bundle

import java.io.Serializable
import java.util.ArrayList

import org.eehouse.android.xw4.loc.LocUtils

private val TAG = DBAlert::class.java.getSimpleName()

class DBAlert : XWDialogFragment() {
    private var mParams: Array<Any?>? = null
    private var mDlgID: DlgID? = null
    val dlgID: DlgID
        get() {
            if (null == mDlgID) {
                mDlgID = DlgID.entries[arguments!!.getInt(DLG_ID_KEY, -1)]
            }
            return mDlgID!!
        }

    override fun belongsOnBackStack(): Boolean {
        return dlgID.belongsOnBackStack()
    }

    public override fun getFragTag(): String {
        return dlgID.toString()
    }

    override fun onSaveInstanceState(bundle: Bundle) {
        bundle.putIntAnd(DLG_ID_KEY, dlgID.ordinal)
			.putSerializableAnd(PARMS_KEY, mParams)
        super.onSaveInstanceState(bundle)
    }

    override fun onCreateDialog(sis: Bundle?): Dialog {
        var sis = sis
        if (null == sis) {
            sis = arguments
        }
        val lst = sis!!.getSerializable(PARMS_KEY)
        if (null != lst) {
            mParams = (lst as Array<Any?>?)
        }
        val activity = activity as XWActivity?
        var dialog = activity!!.makeDialog(this, *mParams.orEmpty())
        if (null == dialog) {
            Log.e(TAG, "no dialog for %s from %s", dlgID, activity)
            // Assert.failDbg();   // remove: better to see what users will see
            dialog = LocUtils.makeAlertBuilder(activity)
                .setMessage("Unable to create $dlgID Alert")
                .setPositiveButton(R.string.ok, null)
                .setNegativeButton("Try again") { dlg, button ->
                    val alrt = newInstance(dlgID, arrayOf(*mParams!!))
                    (getActivity() as MainActivity?)!!.show(alrt)
                }
                .create()
        }
        return dialog
    }

    companion object {
        private const val DLG_ID_KEY = "DLG_ID_KEY"
        private const val PARMS_KEY = "PARMS_KEY"

        @JvmStatic
        fun newInstance(dlgID: DlgID, params: Array<Any?>): DBAlert {
            if (BuildConfig.DEBUG) {
                for (obj in params) {
                    if (null != obj && obj !is Serializable) {
                        Log.d(
                            TAG, "OOPS: %s not Serializable",
                            obj.javaClass.getName()
                        )
                        Assert.failDbg()
                    }
                }
            }
            val bundle = Bundle()
				.putIntAnd(DLG_ID_KEY, dlgID.ordinal)
				.putSerializableAnd(PARMS_KEY, params)
            val result = DBAlert()
            result.setArguments(bundle)
            return result
        }
    }
}
