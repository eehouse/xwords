/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2020 - 2024 by Eric House (xwords@eehouse.org).  All rights
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

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Color
import android.util.AttributeSet
import android.view.View
import android.widget.CompoundButton
import android.widget.ImageView
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.ScrollView
import android.widget.TextView
import com.google.zxing.BarcodeFormat
import com.google.zxing.MultiFormatWriter
import com.google.zxing.WriterException

import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify.InviteMeans
import org.eehouse.android.xw4.ExpandImageButton.ExpandChangeListener
import org.eehouse.android.xw4.loc.LocUtils

class InviteView(context: Context, aset: AttributeSet?) :
    ScrollView(context, aset), CompoundButton.OnCheckedChangeListener,
    RadioGroup.OnCheckedChangeListener
{
    interface ItemClicked {
        fun meansClicked(means: InviteMeans)
        fun checkButton()
    }

    private var mProcs: ItemClicked? = null
    private var mIsWho = false
    private var mGroupTab: RadioGroup? = null
    private var mGroupWho: LimSelGroup? = null
    private var mGroupHow: RadioGroup? = null

    // mCurChecked: hack to work around old bugs in RadioButtons not being
    // immediate children of RadioGroup
    private var mCurChecked: CompoundButton? = null
    private val mHowMeans: MutableMap<Int, InviteMeans> = HashMap()
    private var mExpanded = false
    private var mNli: NetLaunchInfo? = null

    fun setChoices(
        meansList: List<InviteMeans>, sel: Int,
        players: Array<String>?, nMissing: Int,
        nInvited: Int
    ): InviteView {
        Log.d(TAG, "setChoices(nInvited=%d, nMissing=%s)", nInvited, nMissing)
        val context = context

        val haveWho = null != players && 0 < players.size

        // top/horizontal group or title first
        if (haveWho) {
            mGroupTab = findViewById<View>(R.id.group_tab) as RadioGroup
            mGroupTab!!.check(R.id.radio_how)
            mGroupTab!!.setOnCheckedChangeListener(this)
            mGroupTab!!.visibility = VISIBLE
        } else {
            findViewById<View>(R.id.title_tab).visibility = VISIBLE
        }

        mGroupHow = findViewById<View>(R.id.group_how) as RadioGroup
        mGroupHow!!.setOnCheckedChangeListener(this)
        val divider = mGroupHow!!.findViewById<View>(R.id.local_divider)
        for (means in meansList) {
            Assert.assertNotNull(means)
            val button = LocUtils.inflate(context, R.layout.invite_radio) as RadioButton
            button.setOnCheckedChangeListener(this)
            button.text = LocUtils.getString(context, means.userDescID)
            val where = if (means.isForLocal // -1: place before QRcode-wrapper
            ) mGroupHow!!.childCount - 1
            else mGroupHow!!.indexOfChild(divider)
            mGroupHow!!.addView(button, where)
            mHowMeans[button.id] = means
        }

        if (haveWho) {
            mGroupWho = (findViewById<View>(R.id.group_who) as LimSelGroup)
                .setLimit(nMissing)
                .addPlayers(players)
        }
        mIsWho = false // start with how
        showWhoOrHow()

        mExpanded = DBUtils.getBoolFor(context, KEY_EXPANDED, false)
        (findViewById<View>(R.id.expander) as ExpandImageButton)
            .setOnExpandChangedListener(object : ExpandChangeListener {
                override fun expandedChanged(nowExpanded: Boolean) {
                    mExpanded = nowExpanded
                    DBUtils.setBoolFor(context, KEY_EXPANDED, nowExpanded)
                    startQRCodeThread(null)
                }
            })
            .setExpanded(mExpanded)

        return this
    }

    fun setNli(nli: NetLaunchInfo?): InviteView {
        startQRCodeThread(nli)
        return this
    }

    fun setCallbacks(procs: ItemClicked?): InviteView {
        mProcs = procs
        if (null != mGroupWho) {
            mGroupWho!!.setCallbacks(procs)
        }
        return this
    }

    fun getChoice(): Any?
    {
        var result = if (mIsWho) {
            mGroupWho!!.selected
        } else {
            mHowMeans[mGroupHow!!.checkedRadioButtonId]
        }
        return result
    }

    override fun onCheckedChanged(group: RadioGroup, checkedId: Int) {
        if (-1 != checkedId) {
            when (group.id) {
                R.id.group_tab -> {
                    mIsWho = checkedId == R.id.radio_who
                    showWhoOrHow()
                }

                R.id.group_how -> {
                    val means = mHowMeans[checkedId]!!
                    if (mCurChecked != null && mCurChecked!!.isChecked) {
                        mProcs!!.meansClicked(means)
                    }
                    setShowQR(means == InviteMeans.QRCODE)
                }

                R.id.group_who -> {}
            }
            mProcs!!.checkButton()
        }
    }

    override fun onCheckedChanged(buttonView: CompoundButton, isChecked: Boolean) {
        if (isChecked) {
            if (null != mCurChecked) {
                mCurChecked!!.isChecked = false
            }
            mCurChecked = buttonView
        }
    }

    private fun setShowQR(show: Boolean) {
        findViewById<View>(R.id.qrcode_stuff).visibility = if (show) VISIBLE else GONE
    }

    private fun showWhoOrHow() {
        if (null != mGroupWho) {
            mGroupWho!!.visibility = if (mIsWho) VISIBLE else INVISIBLE
        }
        mGroupHow!!.visibility = if (mIsWho) INVISIBLE else VISIBLE

        val showEmpty = mIsWho && 0 == mGroupWho!!.childCount
        findViewById<View>(R.id.who_empty).visibility =
            if (showEmpty) VISIBLE else INVISIBLE
    }

    private fun startQRCodeThread(nli: NetLaunchInfo?) {
        if (null != nli) {
            mNli = nli
        }
        if (null != mNli) {
            val url = mNli!!.makeLaunchUri(context).toString()
            Thread {
                try {
                    val qrSize = if (mExpanded) QRCODE_SIZE_LARGE else QRCODE_SIZE_SMALL
                    val multiFormatWriter = MultiFormatWriter()
                    val bitMatrix = multiFormatWriter.encode(
                        url, BarcodeFormat.QR_CODE,
                        qrSize, qrSize
                    )
                    val bitmap = Bitmap.createBitmap(
                        qrSize, qrSize,
                        Bitmap.Config.ARGB_8888
                    )
                    for (ii in 0 until qrSize) {
                        for (jj in 0 until qrSize) {
                            bitmap.setPixel(
                                ii, jj, if (bitMatrix[ii, jj]
                                ) Color.BLACK else Color.WHITE
                            )
                        }
                    }

                    post {
                        val iv = findViewById<View>(R.id.qr_view) as ImageView
                        iv.setImageBitmap(bitmap)
                        if (BuildConfig.NON_RELEASE) {
                            val tv = findViewById<View>(R.id.qr_url) as TextView
                            tv.visibility = VISIBLE
                            tv.text = url
                        }
                        post { scrollTo(0, iv.top) }
                    }
                } catch (we: WriterException) {
                    Log.ex(TAG, we)
                }
            }.start()
        }
    }

    companion object {
        private val TAG: String = InviteView::class.java.simpleName
        private val KEY_EXPANDED = TAG + ":expanded"
        private const val QRCODE_SIZE_SMALL = 320
        private const val QRCODE_SIZE_LARGE = QRCODE_SIZE_SMALL * 2
    }
}
