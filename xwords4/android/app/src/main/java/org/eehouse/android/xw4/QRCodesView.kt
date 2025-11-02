/*
 * Copyright 2025 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.Dialog
import android.content.Context
import android.util.AttributeSet
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView

import java.io.Serializable

import org.eehouse.android.xw4.jni.GameMgr
import org.eehouse.android.xw4.jni.GameRef
import org.eehouse.android.xw4.loc.LocUtils

private val TAG: String = QRCodesView::class.java.simpleName

class QRCodesView(val mContext: Context, attrs: AttributeSet)
    : LinearLayout( mContext, attrs ) {

    private var mGR: GameRef? = null

    override fun onAttachedToWindow() {
        Log.d(TAG, "onAttachedToWindow")
        super.onAttachedToWindow()
        loadData()
    }

    private fun loadData() {
        launch {
            val gr = mGR!!
            val list = findViewById<ViewGroup>(R.id.msgs)
            gr.getAddrs()?.mapNotNull { addr ->
                gr.getPendingPacketsFor(mContext, addr)?.let { url ->
                    val wrapper = LocUtils.inflate(context, R.layout.qrcode_wrap) as ViewGroup
                    list.addView(wrapper)
                    val iv = wrapper.findViewById<ImageView>(R.id.qr_view)
                    val tv =
                        if (BuildConfig.NON_RELEASE) wrapper.findViewById<TextView>(R.id.qr_url)
                        else null
                    Utils.writeQRCode(wrapper, url, true, iv, tv)
                }
            }
        }
    }

    private fun setGR(gr: GameRef) { mGR = gr }

    companion object {
        fun makeDialog(context: Context, gr: GameRef): Dialog? {
            val view = LocUtils.inflate(context, R.layout.qr_codes_view) as QRCodesView
            view.setGR(gr)
            return LocUtils.makeAlertBuilder(context)
                .setView(view)
                .setPositiveButton(R.string.button_done, null)
                .create()
        }
    }
}
