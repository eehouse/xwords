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

import android.app.Activity
import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.view.View
import android.view.ViewGroup
import android.widget.CheckBox
import android.widget.TextView

import java.text.DateFormat
import java.util.Date

import org.eehouse.android.xw4.DlgDelegate.Action
import org.eehouse.android.xw4.DlgDelegate.HasDlgDelegate
import org.eehouse.android.xw4.ExpandImageButton.ExpandChangeListener
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.Device
import org.eehouse.android.xw4.jni.Knowns
import org.eehouse.android.xw4.loc.LocUtils

class KnownPlayersDelegate(delegator: Delegator) :
    DelegateBase(delegator, R.layout.knownplayrs) {
    private val mActivity: Activity = delegator.getActivity()!!
    private var mList: ViewGroup? = null
    private var mChildren: List<ViewGroup>? = null
    private val mExpSet = loadExpanded()

    private var mByDate = false

    override fun init(sis: Bundle?) {
        mList = findViewById(R.id.players_list) as ViewGroup

        mByDate = DBUtils.getBoolFor(mActivity, KEY_BY_DATE, false)

        val sortCheck = findViewById(R.id.sort_box) as CheckBox
        sortCheck.setOnCheckedChangeListener { buttonView, checked ->
            DBUtils.setBoolFor(mActivity, KEY_BY_DATE, checked)
            mByDate = checked
            populateList()
        }
        sortCheck.isChecked = mByDate
        populateList()
    }

    override fun onPosButton(action: Action, vararg params: Any?): Boolean {
        val handled =
            when (action) {
                Action.KNOWN_PLAYER_DELETE -> {
                    val name = params[0] as String
                    Knowns.deletePlayer(name)
                    populateList()
                    true
                }

                else -> super.onPosButton(action, *params)
            }
        return handled
    }

    override fun makeDialog(alert: DBAlert, vararg params: Any?): Dialog? {
        val dlgID = alert.dlgID
        val dialog =
            when (dlgID) {
                DlgID.RENAME_PLAYER -> {
                    val oldName = params[0] as String
                    val namer = (inflate(R.layout.renamer) as Renamer)
                        .setName(oldName)


                    val lstnr =
                        DialogInterface.OnClickListener { dlg, item -> tryRename(oldName, namer.name) }
                    buildNamerDlg(namer, lstnr, null, dlgID)
                }
                else -> {
                    Log.d(TAG, "makeDialog(): unexpected dlgid $dlgID")
                    super.makeDialog(alert, *params)
                }
            }
        return dialog
    }

    private fun tryRename(oldName: String, newName: String) {
        if (newName != oldName && 0 < newName.length) {
            mList?.launch {
                val renamed = Knowns.renamePlayer(oldName, newName)
                if ( renamed ) {
                    populateList()
                } else {
                    makeOkOnlyBuilder(
                        R.string.knowns_dup_name_fmt,
                        oldName, newName
                    )
                        .show()
                }
            }
        }
    }

    private fun populateList() {
        mList?.launch {
            val players = Knowns.getPlayers(mByDate)
            if (null == players) {
                finish()
            } else {
                mChildren = players.mapNotNull { makePlayerElem(it) }
                addInOrder()
                pruneExpanded()
            }
        }
    }

    private fun addInOrder() {
        mList!!.removeAllViews()
        mChildren!!.map {
            mList!!.addView(it)
        }
    }

    private fun pruneExpanded() {
        var doSave = false

        val children = mChildren!!.map {getName(it)}

        val iter = mExpSet.iterator()
        while (iter.hasNext()) {
            val child = iter.next()
            if (!children.contains(child)) {
                iter.remove()
                doSave = true
            }
        }
        if (doSave) {
            saveExpanded()
        }
    }

    private fun setName(item: ViewGroup, name: String) {
        val tv = item.findViewById<TextView>(R.id.player_name)
        tv.text = name
    }

    private fun getName(item: ViewGroup): String {
        val tv = item.findViewById<TextView>(R.id.player_name)
        return tv.text.toString()
    }

    private suspend fun makePlayerElem(player: String): ViewGroup? {
        var view: ViewGroup? = null
        val lastMod = intArrayOf(0)
        val addr = Knowns.getAddr(player, lastMod)

        addr?.let { addr ->
            val item = LocUtils
                .inflate(mActivity, R.layout.knownplayrs_item) as ViewGroup
            setName(item, player)
            view = item

            // Iterate over address types
            val conTypes = addr.conTypes!!
            val list = item.findViewById<ViewGroup>(R.id.items)

            val timeStmp = 1000L * lastMod[0]
            if (BuildConfig.NON_RELEASE && 0 < timeStmp) {
                val str = DateFormat.getDateTimeInstance()
                    .format(Date(timeStmp))
                addListing(list, R.string.knowns_ts_fmt, str)
            }

            if (conTypes.contains(CommsConnType.COMMS_CONN_BT)) {
                addListing(list, R.string.knowns_bt_fmt, addr.bt_hostName)
                if (BuildConfig.NON_RELEASE) {
                    addListing(list, R.string.knowns_bta_fmt, addr.bt_btAddr)
                }
            }
            if (conTypes.contains(CommsConnType.COMMS_CONN_SMS)) {
                addListing(list, R.string.knowns_smsphone_fmt, addr.sms_phone)
            }
            val hasMQTT = conTypes.contains(CommsConnType.COMMS_CONN_MQTT)
            if (BuildConfig.NON_RELEASE && hasMQTT) {
                addListing(list, R.string.knowns_mqtt_fmt, addr.mqtt_devID)
            }

            item.findViewById<View>(R.id.player_edit_name)
                .setOnClickListener { showDialogFragment(DlgID.RENAME_PLAYER, getName(item)) }
            item.findViewById<View>(R.id.player_delete)
                .setOnClickListener { confirmAndDelete(getName(item)) }
            item.findViewById<View>(R.id.player_copyDevid).apply {
                if ( BuildConfig.NON_RELEASE && hasMQTT ) {
                    setOnClickListener { Utils.stringToClip(mActivity, addr.mqtt_devID) }
                } else {
                    visibility = View.GONE
                }
            }

            val eib = item.findViewById<View>(R.id.expander) as ExpandImageButton
            val ecl: ExpandChangeListener = object: ExpandChangeListener {
                override fun expandedChanged(nowExpanded: Boolean) {
                    item.findViewById<View>(R.id.hidden_part).visibility =
                        if (nowExpanded) View.VISIBLE else View.GONE
                    if (nowExpanded) {
                        mExpSet.add(player)
                    } else {
                        mExpSet.remove(player)
                    }
                    saveExpanded()
                }
            }
            eib.setOnExpandChangedListener(ecl)
                .setExpanded(mExpSet.contains(player))

            item.findViewById<View>(R.id.player_line)
                .setOnClickListener { eib.toggle() }
        }
        return view
    }

    private fun addListing(parent: ViewGroup, fmtID: Int, elem: String?) {
        val content = LocUtils.getString(mActivity, fmtID, elem)
        val item = LocUtils.inflate(mActivity, R.layout.knownplayrs_item_line) as TextView
        item.text = content
        parent.addView(item)
    }

    private fun editName(name: String) {
        Log.d(TAG, "editName(%s) not implemented yet", name)
    }

    private fun confirmAndDelete(name: String) {
        makeConfirmThenBuilder(
            Action.KNOWN_PLAYER_DELETE,
            R.string.knowns_delete_confirm_fmt, name
        )
            .setParams(name)
            .show()
    }

    private fun loadExpanded(): HashSet<String> {
        val expSet =
            try {
                DBUtils.getSerializableFor(mActivity, KEY_EXPSET)
                    as HashSet<String>
            } catch (ex: Exception) {
                Log.ex(TAG, ex)
                HashSet()
            }
        return expSet
    }

    private fun saveExpanded() {
        DBUtils.setSerializableFor(mActivity, KEY_EXPSET, mExpSet)
    }

    companion object {
        private val TAG: String = KnownPlayersDelegate::class.java.simpleName
        private val KEY_EXPSET = TAG + "/expset"
        private val KEY_BY_DATE = TAG + "/bydate"

        fun launchOrAlert(
            delegator: Delegator,
            dlg: HasDlgDelegate?
        ) {
            Utils.launch {
                if (Knowns.hasKnownPlayers()) {
                    delegator.addFragment(
                        KnownPlayersFrag.newInstance(delegator),
                        null
                    )
                } else {
                    Assert.failDbg()
                }
            }
        }
    }
}
