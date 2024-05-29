/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */ /*
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
import org.eehouse.android.xw4.Assert.assertVarargsNotNullNR
import org.eehouse.android.xw4.Assert.failDbg
import org.eehouse.android.xw4.DBUtils.getBoolFor
import org.eehouse.android.xw4.DBUtils.getSerializableFor
import org.eehouse.android.xw4.DBUtils.setBoolFor
import org.eehouse.android.xw4.DBUtils.setSerializableFor
import org.eehouse.android.xw4.DlgDelegate.HasDlgDelegate
import org.eehouse.android.xw4.KnownPlayersFrag.Companion.newInstance
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.XwJNI.Companion.hasKnownPlayers
import org.eehouse.android.xw4.jni.XwJNI.Companion.kplr_deletePlayer
import org.eehouse.android.xw4.jni.XwJNI.Companion.kplr_getAddr
import org.eehouse.android.xw4.jni.XwJNI.Companion.kplr_getPlayers
import org.eehouse.android.xw4.jni.XwJNI.Companion.kplr_renamePlayer
import org.eehouse.android.xw4.loc.LocUtils
import java.text.DateFormat
import java.util.Date

class KnownPlayersDelegate(delegator: Delegator, sis: Bundle?) :
    DelegateBase(delegator, sis, R.layout.knownplayrs) {
    private val mActivity: Activity = delegator.activity
    private var mList: ViewGroup? = null
    private val mChildren: MutableList<ViewGroup> = ArrayList()
    private var mExpSet: HashSet<String>? = null
    private var mByDate = false

    override fun init(sis: Bundle?) {
        mList = findViewById(R.id.players_list) as ViewGroup

        loadExpanded()

        mByDate = getBoolFor(mActivity, KEY_BY_DATE, false)

        val sortCheck = findViewById(R.id.sort_box) as CheckBox
        sortCheck.setOnCheckedChangeListener { buttonView, checked ->
            setBoolFor(mActivity, KEY_BY_DATE, checked)
            mByDate = checked
            populateList()
        }
        sortCheck.isChecked = mByDate
        populateList()
    }

    override fun onPosButton(action: DlgDelegate.Action, vararg params: Any): Boolean {
        assertVarargsNotNullNR(*params)
        var handled = true
        when (action) {
            DlgDelegate.Action.KNOWN_PLAYER_DELETE -> {
                val name = params[0] as String
                kplr_deletePlayer(name)
                populateList()
            }

            else -> handled = super.onPosButton(action, *params)
        }
        return handled
    }

    override fun makeDialog(alert: DBAlert, vararg params: Any): Dialog {
        assertVarargsNotNullNR(*params)
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
        return dialog!!
    }

    private fun tryRename(oldName: String, newName: String) {
        if (newName != oldName && 0 < newName.length) {
            if (kplr_renamePlayer(oldName, newName)) {
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
        val players = kplr_getPlayers(mByDate)
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

        val iter = mExpSet!!.iterator()
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
        val addr = kplr_getAddr(player, lastMod)

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
            eib.setOnExpandChangedListener { nowExpanded ->
                item.findViewById<View>(R.id.hidden_part).visibility =
                    if (nowExpanded) View.VISIBLE else View.GONE
                if (nowExpanded) {
                    mExpSet!!.add(player)
                } else {
                    mExpSet!!.remove(player)
                }
                saveExpanded()
            }
            eib.setExpanded(mExpSet!!.contains(player))

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
            DlgDelegate.Action.KNOWN_PLAYER_DELETE,
            R.string.knowns_delete_confirm_fmt, name
        )
            .setParams(name)
            .show()
    }

    private fun loadExpanded() {
        var expSet: HashSet<String>?
        try {
            expSet = getSerializableFor(mActivity, KEY_EXPSET) as HashSet<String>?
        } catch (ex: Exception) {
            Log.ex(TAG, ex)
            expSet = null
        }
        if (null == expSet) {
            expSet = HashSet()
        }

        mExpSet = expSet
    }

    private fun saveExpanded() {
        setSerializableFor(mActivity, KEY_EXPSET, mExpSet)
    }

    companion object {
        private val TAG: String = KnownPlayersDelegate::class.java.simpleName
        private val KEY_EXPSET = TAG + "/expset"
        private val KEY_BY_DATE = TAG + "/bydate"

        @JvmStatic
        fun launchOrAlert(
            delegator: Delegator,
            dlg: HasDlgDelegate?
        ) {
            val activity = delegator.activity

            if (hasKnownPlayers()) {
                delegator.addFragment(
                    newInstance(delegator),
                    null
                )
            } else {
                failDbg()
            }
        }
    }
}
