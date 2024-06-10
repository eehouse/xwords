/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2015 - 2024 by Eric House (xwords@eehouse.org).  All
 * rights reserved.
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
import android.util.AttributeSet
import android.view.View
import android.widget.CheckBox
import android.widget.LinearLayout
import org.eehouse.android.xw4.Assert.failDbg
import org.eehouse.android.xw4.DlgDelegate.HasDlgDelegate
import org.eehouse.android.xw4.XWPrefs.Companion.getMQTTEnabled
import org.eehouse.android.xw4.XWPrefs.Companion.getNBSEnabled
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet.Companion.getSupported

private val TAG: String = ConnViaViewLayout::class.java.simpleName

class ConnViaViewLayout(context: Context, aset: AttributeSet?) :
    LinearLayout(context, aset)
{
    var types: CommsConnTypeSet? = null
        private set
    private var m_dlgDlgt: HasDlgDelegate? = null
    private var mParent: DelegateBase? = null

    interface CheckEnabledWarner {
        fun warnDisabled(typ: CommsConnType)
    }

    interface SetEmptyWarner {
        fun typeSetEmpty()
    }

    private var m_disabledWarner: CheckEnabledWarner? = null
    private var m_emptyWarner: SetEmptyWarner? = null

    fun configure(
        parent: DelegateBase, types: CommsConnTypeSet,
        cew: CheckEnabledWarner, sew: SetEmptyWarner?,
        dlgDlgt: HasDlgDelegate
    ) {
        this.types = types.clone() as CommsConnTypeSet

        addConnections()

        m_disabledWarner = cew
        m_emptyWarner = sew
        m_dlgDlgt = dlgDlgt
        mParent = parent
    }

    private fun addConnections() {
        val list = findViewById<View>(R.id.conn_types) as LinearLayout
        list.removeAllViews() // in case being reused

        val context = context
        val supported = getSupported(context)

        for (typ in supported) {
            if (!typ.isSelectable) {
                continue
            }
            val box = CheckBox(context)
            box.text = typ.longName(context)
            box.isChecked = types!!.contains(typ)
            list.addView(box)

            val typf = typ
            box.setOnCheckedChangeListener { buttonView, isChecked ->
                if (isChecked) {
                    if (disableUntil(context, typ)) {
                        buttonView.isChecked = false
                    } else {
                        showNotAgainTypeTip(typf)
                        enabledElseWarn(typf)
                        types!!.add(typf)
                    }
                } else {
                    types!!.remove(typf)
                    if (null != m_emptyWarner && 0 == types!!.size) {
                        m_emptyWarner!!.typeSetEmpty()
                    }
                }
            }
        }
    }

    private fun disableUntil(context: Context, typ: CommsConnType): Boolean {
        var disable = false
        if (typ == CommsConnType.COMMS_CONN_BT
            && !BTUtils.havePermissions(context)
        ) {
            if (null != mParent) {
                Perms23.tryGetPerms(
                    mParent, BTUtils.BTPerms,
                    R.string.nearbydev_rationale,
                    DlgDelegate.Action.SKIP_CALLBACK
                )
            }
            disable = true
        }
        return disable
    }

    private fun enabledElseWarn(typ: CommsConnType) {
        var enabled = true
        val context = context
        when (typ) {
            CommsConnType.COMMS_CONN_SMS -> enabled = getNBSEnabled(context)
            CommsConnType.COMMS_CONN_BT -> enabled = BTUtils.BTEnabled()
            CommsConnType.COMMS_CONN_RELAY -> {
                failDbg()
                enabled = false
            }

            CommsConnType.COMMS_CONN_P2P -> enabled = WiDirWrapper.enabled()
            CommsConnType.COMMS_CONN_MQTT -> enabled = getMQTTEnabled(context)
            else -> failDbg()
        }
        if (!enabled && null != m_disabledWarner) {
            m_disabledWarner!!.warnDisabled(typ)
        }
    }

    private fun showNotAgainTypeTip(typ: CommsConnType) {
        if (null != m_dlgDlgt) {
            var keyID = 0
            var msgID = 0
            when (typ) {
                CommsConnType.COMMS_CONN_RELAY -> failDbg()
                CommsConnType.COMMS_CONN_SMS -> if (Perms23.haveNBSPerms(context)) {
                    msgID = R.string.not_again_comms_sms
                    keyID = R.string.key_na_comms_sms
                }

                CommsConnType.COMMS_CONN_BT -> {
                    msgID = R.string.not_again_comms_bt
                    keyID = R.string.key_na_comms_bt
                }

                CommsConnType.COMMS_CONN_P2P -> {
                    msgID = R.string.not_again_comms_p2p
                    keyID = R.string.key_na_comms_p2p
                }

                CommsConnType.COMMS_CONN_MQTT -> {
                    msgID = R.string.not_again_comms_mqtt
                    keyID = R.string.key_na_comms_mqtt
                }

                else -> failDbg()
            }
            if (0 != msgID) {
                val dlgDlgt = m_dlgDlgt!!
                val builder =
                    if (0 != keyID) dlgDlgt.makeNotAgainBuilder(keyID, msgID)
                    else dlgDlgt.makeOkOnlyBuilder(msgID)
                builder.show()
            }
        }
    }
}
