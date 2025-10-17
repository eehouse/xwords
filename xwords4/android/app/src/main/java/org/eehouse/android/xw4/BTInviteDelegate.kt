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

import android.app.Activity
import android.bluetooth.BluetoothDevice
import android.content.Context
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.text.TextUtils
import android.text.format.DateUtils
import android.view.View
import android.widget.Button
import android.widget.ProgressBar
import android.widget.TextView

import java.io.Serializable
import java.util.Collections

import org.eehouse.android.xw4.BleNetwork.ScanListener
import org.eehouse.android.xw4.DBUtils.SentInvitesInfo
import org.eehouse.android.xw4.DlgDelegate.Action
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify.InviteMeans

class BTInviteDelegate(delegator: Delegator) :
    InviteDelegate(delegator), ScanListener {
    private val mActivity: Activity
    private var mProgressBar: ProgressBar? = null
    private val mHandler = Handler(Looper.getMainLooper())
    private var mNDevsThisScan = 0

    private class BTDev internal constructor(private val mName: String) :
        InviterItem, Serializable {
        override fun equals(item: InviterItem): Boolean = item.getDev() == getDev()

        override fun getDev(): String { return mName }
    }

    private class Persisted : Serializable {
		val mDevs = mutableListOf<BTDev>()

        // HashMap: m_stamps is serialized, so can't be abstract type
        val mTimeStamps = HashMap<String, Long>()
        fun add(devName: String) {
            if ( mDevs.filter { it.getDev().equals(devName) }
					 .isEmpty() ) {
                mDevs.add(BTDev(devName))
            }
            mTimeStamps[devName] = System.currentTimeMillis()
            sort()
        }

        fun remove(checked: Set<String>) {
            for (devName in checked) {
                mTimeStamps.remove(devName)
                val iter = mDevs.iterator()
                while (iter.hasNext()) {
                    val dev = iter.next()
                    if (TextUtils.equals(dev.getDev(), devName)) {
                        iter.remove()
                        break
                    }
                }
            }
        }

        fun contains(name: String): Boolean {
            val dev = mDevs.firstOrNull() {
                it.getDev().equals(name)
            }

            val result = dev != null
            Log.d(TAG, "contains($name) => $result")
            return result
        }

        fun empty(): Boolean {
            return mDevs.size == 0
        }

        private fun sort() {
            Collections.sort(mDevs) { rec1, rec2 ->
                var result = 0
                try {
                    val val1 = mTimeStamps[rec1.getDev()]!!
                    val val2 = mTimeStamps[rec2.getDev()]!!
                    if (val2 > val1) {
                        result = 1
                    } else if (val1 > val2) {
                        result = -1
                    }
                } catch (ex: Exception) {
                    Log.e(TAG, "ex %s on %s vs %s", ex, rec1, rec2)
                }
                result
            }
        }
    }

    init {
        mActivity = delegator.getActivity()!!
    }

    override fun init(savedInstanceState: Bundle?) {
        super.init(savedInstanceState)
        val msg = (getQuantityString(
					   R.plurals.invite_bt_desc_fmt_2, m_nMissing,
					   m_nMissing
				   ) + getString(R.string.invite_bt_desc_postscript))
        super.init(msg, R.string.empty_bt_inviter)
        addButtonBar(R.layout.bt_buttons, intArrayOf(
											  R.id.button_scan,
											  R.id.button_settings,
											  R.id.button_clear
										  ))
        load(mActivity)
        if (sPersistedRef[0]!!.empty()) {
            scan()
        } else {
            updateListIn(0)
        }
    }

    override fun onResume() {
        BleNetwork.addScanListener(mActivity, this)
        super.onResume()
    }

    override fun onPause() {
        BleNetwork.removeScanListener(this)
        super.onResume()
    }

    override fun onBarButtonClicked(id: Int) {
        when (id) {
            R.id.button_scan -> scan()
            R.id.button_settings -> BTUtils.openBTSettings(mActivity)
            R.id.button_clear -> {
                val count = getChecked().size
                val msg = (getQuantityString(
                    R.plurals.confirm_clear_bt_fmt,
                    count, count
                )
                        + getString(R.string.confirm_clear_bt_postscript))
                makeConfirmThenBuilder(Action.CLEAR_ACTION, msg)
					.show()
            }
        }
    }

    override fun onChildAdded(child: View, data: InviterItem) {
        val devName = (data as BTDev).getDev()
        var msg: String? = null
        if (sPersistedRef[0]!!.mTimeStamps.containsKey(devName)) {
            val elapsed = DateUtils
                .getRelativeTimeSpanString(
                    sPersistedRef[0]!!.mTimeStamps[devName]!!,
                    System.currentTimeMillis(),
                    DateUtils.SECOND_IN_MILLIS
                )
            msg = getString(R.string.bt_scan_age_fmt, elapsed)
        }
        (child as TwoStrsItem).setStrings(devName, msg)
    }

    override fun tryEnable() {
        super.tryEnable()
        val button = findViewById(R.id.button_clear) as Button?
        button?.setEnabled(0 < getChecked().size)
    }

    // interface ScanListener
    override fun onDeviceScanned(dev: BluetoothDevice) {
        post { processScanResult(dev) }
    }

    private fun scan() {
        if (ENABLE_FAKER && Utils.nextRandomInt() % 5 == 0) {
            sPersistedRef[0]!!.add("Do Not Invite Me")
        }
        BleNetwork.startScan(mActivity)
        mNDevsThisScan = 0
        showProgress(2 * SCAN_SECONDS)
    }

    private fun processScanResult(dev: BluetoothDevice) {
        DbgUtils.assertOnUIThread()
        val name = dev.name
        sPersistedRef!![0]?.let { ref ->
            if ( !ref.contains(name) ) {
                ++mNDevsThisScan
                ref.add(dev.getName())
                store(mActivity)
                updateList()
                tryEnable()
            }
        }
    }

    private fun showProgress(nSeconds: Int) {
        mProgressBar = findViewById(R.id.progress) as ProgressBar
        mProgressBar!!.progress = 0
        mProgressBar!!.setMax(nSeconds)
        val msg = getQuantityString(
            R.plurals.bt_scan_progress_fmt,
            3, 3
        )
        (requireViewById(R.id.progress_msg) as TextView).text = msg
        requireViewById(R.id.progress_line).visibility = View.VISIBLE
        incrementProgressIn(1)
    }

    private fun hideProgress() {
        requireViewById(R.id.progress_line).visibility = View.GONE
        mProgressBar = null
    }

    private fun incrementProgressIn(inSeconds: Int) {
        mHandler.postDelayed({ mProgressBar?.let {
                                   val curProgress = it.progress
                                   if (curProgress >= it.max) {
                                       hideProgress() // create illusion it's done
                                   } else {
                                       it.progress = curProgress + 1
                                       incrementProgressIn(1)
                                   }
                               }
                             }, (1000 * inSeconds).toLong())
    }

    private fun updateListIn(inSecs: Long) {
        mHandler.postDelayed({
            updateList()
            updateListIn(10)
        }, inSecs * 1000)
    }

    private fun updateList()
		= updateList(sPersistedRef[0]!!.mDevs)

    // DlgDelegate.DlgClickNotify interface
    override fun onPosButton(action: Action,
							 vararg params: Any?): Boolean {
        var handled = true
        when (action) {
            Action.OPEN_BT_PREFS_ACTION -> BTUtils.openBTSettings(mActivity)

            Action.CLEAR_ACTION -> {
                sPersistedRef[0]!!.remove(getChecked())
                store(mActivity)
                clearChecked()
                updateList()
                tryEnable()
            }

            else -> handled = super.onPosButton(action, *params)
        }
        return handled
    }

    companion object {
        private val TAG = BTInviteDelegate::class.java.getSimpleName()
        private val KEY_PERSIST = TAG + "_persist"
        private const val ENABLE_FAKER = false
        private const val SCAN_SECONDS = 5
        private val sPersistedRef = arrayOf<Persisted?>(null)
        fun launchForResult(
            activity: Activity, nMissing: Int,
            info: SentInvitesInfo?,
            requestCode: RequestCode
        ) {
            Assert.assertTrue(0 < nMissing) // don't call if nMissing == 0
            val intent = makeIntent(
                activity, BTInviteActivity::class.java,
                nMissing, info
            )
            info?.getLastDev(InviteMeans.BLUETOOTH)?.let {
                intent.putExtra(INTENT_KEY_LASTDEV, it)
            }
            activity.startActivityForResult(intent, requestCode.ordinal)
        }

        private fun removeNotPaired(prs: Persisted?) {
            Log.d(TAG, "removeNotPaired()")
            // Assert.failDbg()
            // val pairedDevs = BTUtils.getCandidates()
            // val paired: MutableSet<String> = HashSet()
            // for (dev in pairedDevs) {
            //     Log.d(TAG, "removeNotPaired(): paired dev: %s", dev.getName())
            //     paired.add(dev.getName())
            // }
            // val toRemove: MutableSet<String> = HashSet()
            // for (dev in prs!!.mDevs) {
            //     val name = dev.getDev()
            //     if (!paired.contains(name)) {
            //         Log.d(TAG, "%s no longer paired; removing", name)
            //         toRemove.add(name)
            //     } else {
            //         Log.d(TAG, "%s STILL paired", name)
            //     }
            // }
            // if (!toRemove.isEmpty()) {
            //     prs.remove(toRemove)
            // }
        }

        @Synchronized
        private fun load(context: Context) {
            if (null == sPersistedRef[0]) {
                var prs: Persisted?
                try {
                    prs = DBUtils.getSerializableFor(context, KEY_PERSIST) as Persisted
                    removeNotPaired(prs)
                } catch (ex: Exception) {
                    // NPE, de-serialization problems, etc.
                    // Log.ex( TAG, ex );
                    prs = null
                }
                if (null == prs) {
                    prs = Persisted()
                }
                sPersistedRef[0] = prs
            }
        }

        @Synchronized
        private fun store(context: Context) {
            DBUtils.setSerializableFor(context, KEY_PERSIST, sPersistedRef[0])
        }

        fun onHeardFromDev(dev: BluetoothDevice) {
            val context = XWApp.getContext()
            load(context)
            sPersistedRef[0]!!.add(dev.getName())
            store(context)
        }
    } // companion object
}
