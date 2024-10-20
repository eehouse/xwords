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
import org.eehouse.android.xw4.jni.XwJNI
import org.eehouse.android.xw4.loc.LocUtils

class KnownPlayersDelegate(delegator: Delegator) :
    DelegateBase(delegator, R.layout.knownplayrs) {
    private val mActivity: Activity = delegator.getActivity()!!
    private var mList: ViewGroup? = null
    private val mChildren: MutableList<ViewGroup> = ArrayList()
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
        var handled = true
        when (action) {
            Action.KNOWN_PLAYER_DELETE -> {
                val name = params[0] as String
                XwJNI.kplr_deletePlayer(name)
                populateList()
            }

            else -> handled = super.onPosButton(action, *params)
        }
        return handled
    }

    override fun makeDialog(alert: DBAlert, vararg params: Any?): Dialog? {
        var dialog: Dialog? = null

        val dlgID = alert.dlgID
        when (dlgID) {
            DlgID.RENAME_PLAYER -> {
                val oldName = params[0] as String
                val namer = (inflate(R.layout.renamer) as Renamer)
                    .setName(oldName)


                val lstnr =
                    DialogInterface.OnClickListener { dlg, item -> tryRename(oldName, namer.name) }
                dialog = buildNamerDlg(namer, lstnr, null, dlgID)
            }
            else -> Log.d(TAG, "makeDialog(): unexpected dlgid $dlgID")
        }
        if (null == dialog) {
            dialog = super.makeDialog(alert, *params)
        }
        return dialog
    }

    private fun tryRename(oldName: String, newName: String) {
        if (newName != oldName && 0 < newName.length) {
            if (XwJNI.kplr_renamePlayer(oldName, newName)) {
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

    private fun populateList() {
        val players = XwJNI.kplr_getPlayers(mByDate)
        if (null == players) {
            finish()
        } else {
            mChildren.clear()
            players.map{
                val child = makePlayerElem(it)
                if (null != child) {
                    mChildren.add(child)
                }
            }
            addInOrder()
            pruneExpanded()
        }
    }

    private fun addInOrder() {
        mList!!.removeAllViews()
        for (child in mChildren!!) {
            mList!!.addView(child)
        }
    }

    private fun pruneExpanded() {
        var doSave = false

        val children: MutableSet<String> = HashSet()
        for (child in mChildren!!) {
            children.add(getName(child))
        }

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
        val tv = item.findViewById<View>(R.id.player_name) as TextView
        tv.text = name
    }

    private fun getName(item: ViewGroup): String {
        val tv = item.findViewById<View>(R.id.player_name) as TextView
        return tv.text.toString()
    }

    private fun makePlayerElem(player: String): ViewGroup? {
        var view: ViewGroup? = null
        val lastMod = intArrayOf(0)
        val addr = XwJNI.kplr_getAddr(player, lastMod)

        if (null != addr) {
            val item = LocUtils
                .inflate(mActivity, R.layout.knownplayrs_item) as ViewGroup
            setName(item, player)
            view = item

            // Iterate over address types
            val conTypes = addr.conTypes
            val list = item.findViewById<View>(R.id.items) as ViewGroup

            val timeStmp = 1000L * lastMod[0]
            if (BuildConfig.NON_RELEASE && 0 < timeStmp) {
                val str = DateFormat.getDateTimeInstance()
                    .format(Date(timeStmp))
                addListing(list, R.string.knowns_ts_fmt, str)
            }

            if (conTypes!!.contains(CommsConnType.COMMS_CONN_BT)) {
                addListing(list, R.string.knowns_bt_fmt, addr.bt_hostName)
                if (BuildConfig.NON_RELEASE) {
                    addListing(list, R.string.knowns_bta_fmt, addr.bt_btAddr)
                }
            }
            if (conTypes.contains(CommsConnType.COMMS_CONN_SMS)) {
                addListing(list, R.string.knowns_smsphone_fmt, addr.sms_phone)
            }
            if (BuildConfig.NON_RELEASE) {
                if (conTypes.contains(CommsConnType.COMMS_CONN_MQTT)) {
                    addListing(list, R.string.knowns_mqtt_fmt, addr.mqtt_devID)
                }
            }

            item.findViewById<View>(R.id.player_edit_name)
                .setOnClickListener { showDialogFragment(DlgID.RENAME_PLAYER, getName(item)) }
            item.findViewById<View>(R.id.player_delete)
                .setOnClickListener { confirmAndDelete(getName(item)) }

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
        var expSet: HashSet<String>? = try {
            DBUtils.getSerializableFor(mActivity, KEY_EXPSET)
                as HashSet<String>
        } catch (ex: Exception) {
            Log.ex(TAG, ex)
            null
        }
        if (null == expSet) {
            expSet = HashSet()
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
            val activity = delegator.getActivity()

            if (XwJNI.hasKnownPlayers()) {
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
