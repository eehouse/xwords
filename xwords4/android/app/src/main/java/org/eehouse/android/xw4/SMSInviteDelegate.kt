/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */ /*
 * Copyright 2012 - 2016 by Eric House (xwords@eehouse.org).  All rights
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
import android.content.Intent
import android.os.Bundle
import android.provider.ContactsContract
import android.provider.ContactsContract.CommonDataKinds.Phone
import android.telephony.PhoneNumberUtils
import android.text.method.DialerKeyListener
import android.view.View
import android.widget.Button
import android.widget.EditText
import org.eehouse.android.xw4.Assert.assertVarargsNotNullNR
import org.eehouse.android.xw4.DBUtils.SentInvitesInfo
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify.InviteMeans
import org.eehouse.android.xw4.Perms23.Perm
import org.json.JSONException
import org.json.JSONObject
import java.util.Collections

class SMSInviteDelegate(delegator: Delegator) :
    InviteDelegate(delegator) {
    private var m_phoneRecs: ArrayList<PhoneRec>? = null
    private val m_activity: Activity = delegator.getActivity()!!

    override fun init(savedInstanceState: Bundle?) {
        super.init(savedInstanceState)

        var msg = getString(R.string.button_invite)
        msg = getQuantityString(
            R.plurals.invite_sms_desc_fmt, m_nMissing,
            m_nMissing, msg
        )
        init(msg, R.string.empty_sms_inviter)
        addButtonBar(R.layout.sms_buttons, BUTTONIDS)

        savedState
        rebuildList(true)

        askContactsPermission()
    }

    public override fun getExtra(): Int {
        return R.string.invite_nbs_desc
    }

    override fun onBarButtonClicked(id: Int) {
        when (id) {
            R.id.button_add -> {
                val intent = Intent(
                    Intent.ACTION_PICK,
                    ContactsContract.Contacts.CONTENT_URI
                )
                intent.setType(Phone.CONTENT_TYPE)
                startActivityForResult(intent, RequestCode.GET_CONTACT)
            }

            R.id.manual_add_button -> showDialogFragment(DlgID.GET_NUMBER)
            R.id.button_clear -> {
                val count = getChecked().size
                val msg = getQuantityString(
                    R.plurals.confirm_clear_sms_fmt,
                    count, count
                )
                makeConfirmThenBuilder(DlgDelegate.Action.CLEAR_ACTION, msg).show()
            }
        }
    }

    override fun onActivityResult(
        requestCode: RequestCode, resultCode: Int,
        data: Intent
    ) {
        if (Activity.RESULT_CANCELED != resultCode && data != null) {
            when (requestCode) {
                RequestCode.GET_CONTACT -> post { addPhoneNumbers(data) }
                else -> Log.d(TAG, "onActivityResult(): unexpected code $resultCode")
            }
        }
    }

    override fun makeDialog(alert: DBAlert, vararg params: Any?): Dialog {
        assertVarargsNotNullNR(*params)
        val lstnr: DialogInterface.OnClickListener
        val dialog =
            when (alert.dlgID) {
            DlgID.GET_NUMBER -> {
                val getNumView = inflate(R.layout.get_sms)
                (getNumView.findViewById<View>(R.id.num_field) as EditText).keyListener =
                    DialerKeyListener.getInstance()
                lstnr = DialogInterface.OnClickListener { dlg, item ->
                    val number = (getNumView.findViewById<View>(R.id.num_field) as EditText)
                        .text.toString()
                    if (null != number && 0 < number.length) {
                        val name = (getNumView.findViewById<View>(R.id.name_field) as EditText)
                            .text.toString()
                        postSMSCostWarning(number, name)
                    }
                }
                makeAlertBuilder()
                    .setTitle(R.string.get_sms_title)
                    .setView(getNumView)
                    .setPositiveButton(android.R.string.ok, lstnr)
                    .setNegativeButton(android.R.string.cancel, null)
                    .create()
            }

            else -> super.makeDialog(alert, *params)!!
        }
        return dialog
    }

    override fun onChildAdded(child: View, data: InviterItem) {
        val rec = data as PhoneRec
        (child as TwoStrsItem).setStrings(rec.m_name!!, rec.m_phone)
    }

    override fun tryEnable() {
        super.tryEnable()

        val button = findViewById(R.id.button_clear) as? Button
        button?.isEnabled = 0 < getChecked().size
    }

    // DlgDelegate.DlgClickNotify interface
    override fun onPosButton(action: DlgDelegate.Action, vararg params: Any?): Boolean {
        assertVarargsNotNullNR(*params)
        var handled = true
        when (action) {
            DlgDelegate.Action.CLEAR_ACTION -> clearSelectedImpl()
            DlgDelegate.Action.USE_IMMOBILE_ACTION -> postSMSCostWarning(
                params[0] as String,
                params[1] as String
            )

            DlgDelegate.Action.POST_WARNING_ACTION -> {
                val rec = PhoneRec(
                    params[1] as String,
                    params[0] as String
                )
                m_phoneRecs!!.add(rec)
                clearChecked()
                onItemChecked(rec, true)
                saveAndRebuild()
            }

            else -> handled = super.onPosButton(action, *params)
        }
        return handled
    }

    private fun addPhoneNumbers(intent: Intent) {
        val data = intent.data
        val cursor = m_activity
            .managedQuery(
                data,
                arrayOf(
                    Phone.DISPLAY_NAME,
                    Phone.NUMBER,
                    Phone.TYPE
                ),
                null, null, null
            )
        // Have seen a crash reporting
        // "android.database.StaleDataException: Attempted to access a
        // cursor after it has been closed." when the query takes a
        // long time to return.  Be safe.
        if (null != cursor && !cursor.isClosed) {
            if (cursor.moveToFirst()) {
                val name =
                    cursor.getString(
                        cursor.getColumnIndex
                            (Phone.DISPLAY_NAME)
                    )
                val number =
                    cursor.getString(
                        cursor.getColumnIndex
                            (Phone.NUMBER)
                    )

                val type = cursor.getInt(
                    cursor.getColumnIndex
                        (Phone.TYPE)
                )

                if (Phone.TYPE_MOBILE == type) {
                    postSMSCostWarning(number, name)
                } else {
                    postConfirmMobile(number, name)
                }
            }
        }
    } // addPhoneNumbers

    private fun postSMSCostWarning(number: String, name: String) {
        makeConfirmThenBuilder(
            DlgDelegate.Action.POST_WARNING_ACTION,
            R.string.warn_unlimited
        )
            .setPosButton(R.string.button_yes)
            .setParams(number, name)
            .show()
    }

    private fun postConfirmMobile(number: String, name: String) {
        makeConfirmThenBuilder(
            DlgDelegate.Action.USE_IMMOBILE_ACTION,
            R.string.warn_nomobile_fmt, number, name
        )
            .setPosButton(R.string.button_yes)
            .setParams(number, name)
            .show()
    }

    private fun rebuildList(checkIfAll: Boolean) {
        Collections.sort(m_phoneRecs) { rec1, rec2 ->
            rec1.m_name!!.compareTo(
                rec2.m_name!!
            )
        }

        updateList(m_phoneRecs!!)
        tryEnable()
    }

    private val savedState: Unit
        get() {
            val phones = XWPrefs.getSMSPhones(m_activity)

            m_phoneRecs = ArrayList()
            val iter = phones.keys()
            while (iter.hasNext()) {
                val phone = iter.next()
                val name = phones.optString(phone, null)
                val rec = PhoneRec(name, phone)
                m_phoneRecs!!.add(rec)
            }
        }

    private fun saveAndRebuild() {
        val phones = JSONObject()
        val iter: Iterator<PhoneRec> = m_phoneRecs!!.iterator()
        while (iter.hasNext()) {
            val rec = iter.next()
            try {
                phones.put(rec.m_phone, rec.m_name)
            } catch (ex: JSONException) {
                Log.ex(TAG, ex)
            }
        }
        XWPrefs.setSMSPhones(m_activity, phones)

        rebuildList(false)
    }

    private fun clearSelectedImpl() {
        val checked = getChecked()
        val iter = m_phoneRecs!!.iterator()
        while (iter.hasNext()) {
            if (checked.contains(iter.next().getDev())) {
                iter.remove()
            }
        }
        clearChecked()
        saveAndRebuild()
    }

    private fun askContactsPermission() {
        // We want to ask, and to give the rationale, but behave the same
        // regardless of the answers given. So SKIP_CALLBACK.
        Perms23.tryGetPerms(
            this, Perm.READ_CONTACTS,
            R.string.contacts_rationale,
            DlgDelegate.Action.SKIP_CALLBACK
        )
    }

    private inner class PhoneRec(var m_name: String?, var m_phone: String) : InviterItem {
        constructor(phone: String) : this(null, phone)

        override fun getDev(): String {
            return m_phone
        }

        override fun equals(item: InviterItem?): Boolean {
            var result = false
            if (null != item && item is PhoneRec) {
                val rec = item
                result = (m_name === rec.m_name
                        && PhoneNumberUtils.compare(m_phone, rec.m_phone))
            }
            return result
        }
    }

    companion object {
        private val TAG: String = SMSInviteDelegate::class.java.simpleName
        private val BUTTONIDS = intArrayOf(
            R.id.button_add,
            R.id.manual_add_button,
            R.id.button_clear,
        )

        fun launchForResult(
            activity: Activity, nMissing: Int,
            info: SentInvitesInfo?,
            requestCode: RequestCode
        ) {
            val intent = makeIntent(
                activity, SMSInviteActivity::class.java,
                nMissing, info
            )
            if (null != info) {
                val lastDev = info.getLastDev(InviteMeans.SMS_DATA)
                intent.putExtra(INTENT_KEY_LASTDEV, lastDev)
            }
            activity.startActivityForResult(intent, requestCode.ordinal)
        }
    }
}
