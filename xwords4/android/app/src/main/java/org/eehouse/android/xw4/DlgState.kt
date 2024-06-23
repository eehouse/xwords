/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2024 by Eric House (xwords@eehouse.org).  All
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

import android.os.Bundle
import android.os.Parcel
import android.os.Parcelable
import android.text.TextUtils
import org.eehouse.android.xw4.Assert.assertFalse
import org.eehouse.android.xw4.Assert.failDbg
import org.eehouse.android.xw4.DlgDelegate.ActionPair
import org.eehouse.android.xw4.Log.d
import java.io.Serializable

class DlgState(val mID: DlgID) : Parcelable {
    var m_msg: String? = null
    var m_posButton: Int = 0
    var m_negButton: Int = 0
    var m_action: DlgDelegate.Action? = null
    var m_pair: ActionPair? = null
    var m_prefsNAKey: Int = 0

    // These can't be serialized!!!!
    private var m_params: Array<Any?>? = arrayOf()
    var m_title: String? = null

    fun setMsg(msg: String?): DlgState {
        m_msg = msg
        return this
    }

    fun setPrefsNAKey(key: Int): DlgState {
        m_prefsNAKey = key
        return this
    }

    fun setAction(action: DlgDelegate.Action): DlgState {
        m_action = action
        return this
    }

    fun setParams(vararg params: Any?): DlgState {
        if (BuildConfig.DEBUG && null != params) {
            for (obj in params) {
                if (null != obj && obj !is Serializable) {
                    d(
                        TAG, "OOPS: %s not Serializable",
                        obj.javaClass.name
                    )
                    failDbg()
                }
            }
        }
        m_params = arrayOf(*params)
        return this
    }

    fun setActionPair(pair: ActionPair?): DlgState {
        m_pair = pair
        return this
    }

    fun setPosButton(id: Int): DlgState {
        m_posButton = id
        return this
    }

    fun setNegButton(id: Int): DlgState {
        m_negButton = id
        return this
    }

    fun setTitle(title: String?): DlgState {
        m_title = title
        return this
    }

    fun getParams(): Array<Any?>
    {
        var params = m_params
        if (null == params) {
            params = arrayOfNulls(0)
        }
        return params
    }

    override fun toString(): String {
        val result: String
        if (BuildConfig.DEBUG) {
            var params: String? = ""
            if (null != m_params) {
                val strs: MutableList<String?> = ArrayList()
                for (obj in m_params!!) {
                    strs.add(String.format("%s", obj))
                }
                params = TextUtils.join(",", strs)
            }
            result = StringBuffer()
                .append("{id: ").append(mID)
                .append(", msg: \"").append(m_msg)
                .append("\", naKey: ").append(m_prefsNAKey)
                .append(", action: ").append(m_action)
                .append(", pair ").append(m_pair)
                .append(", pos: ").append(m_posButton)
                .append(", neg: ").append(m_negButton)
                .append(", title: ").append(m_title)
                .append(", params: [").append(params)
                .append("]}")
                .toString()
        } else {
            result = super.toString()
        }
        return result
    }

    // I only need this if BuildConfig.DEBUG is true...
    override fun equals(it: Any?): Boolean {
        var result: Boolean
        if (BuildConfig.DEBUG) {
            result = it != null && it is DlgState
            if (result) {
                val other = it as DlgState?
                result = other != null
                    && mID == other.mID
                    && TextUtils.equals(m_msg, other.m_msg)
                    && m_posButton == other.m_posButton
                    && m_negButton == other.m_negButton
                    && m_action == other.m_action
                    && (if ((null == m_pair)) null == other.m_pair
                        else m_pair!!.equals(other.m_pair))
                    && m_prefsNAKey == other.m_prefsNAKey
                    && m_params.contentDeepEquals(other.m_params)
                    && TextUtils.equals(m_title,other.m_title)
            }
        } else {
            result = super.equals(it)
        }
        return result
    }

    override fun describeContents(): Int {
        return 0
    }

    fun toBundle(): Bundle {
        testCanParcelize()

        val result = Bundle()
        result.putParcelable(BUNDLE_KEY, this)
        return result
    }

    override fun writeToParcel(out: Parcel, flags: Int) {
        out.writeInt(mID.ordinal)
        out.writeInt(m_posButton)
        out.writeInt(m_negButton)
        out.writeInt(m_action!!.ordinal)
        out.writeInt(m_prefsNAKey)
        out.writeString(m_title)
        out.writeString(m_msg)
        out.writeSerializable(m_params)
        out.writeSerializable(m_pair)
    }

    private fun testCanParcelize() {
        if (BuildConfig.DEBUG) {
            val parcel = Parcel.obtain()
            writeToParcel(parcel, 0)

            parcel.setDataPosition(0)

            val newState = CREATOR.createFromParcel(parcel)
            assertFalse(newState === this)
            if (this != newState) {
                d(TAG, "restore failed!!: %s => %s", this, newState)
                failDbg()
            }
        }
    }

    companion object {
        private val TAG: String = DlgState::class.java.simpleName
        private const val BUNDLE_KEY = "bk"

        fun fromBundle(bundle: Bundle): DlgState? {
            return bundle.getParcelable<Parcelable>(BUNDLE_KEY) as DlgState?
        }

        val CREATOR
                : Parcelable.Creator<DlgState> = object : Parcelable.Creator<DlgState> {
            override fun createFromParcel(parcel: Parcel): DlgState? {
                val id = DlgID.entries[parcel.readInt()]
                val posButton = parcel.readInt()
                val negButton = parcel.readInt()
                val action = DlgDelegate.Action.entries[parcel.readInt()]
                val prefsKey = parcel.readInt()
                val title = parcel.readString()
                val msg = parcel.readString()
                val params = parcel.readSerializable() as Array<Any>?
                val pair = parcel.readSerializable() as ActionPair?
                val state = DlgState(id)
                    .setMsg(msg)
                    .setPosButton(posButton)
                    .setNegButton(negButton)
                    .setAction(action)
                    .setPrefsNAKey(prefsKey)
                    .setTitle(title)
                    .setParams(*params!!)
                    .setActionPair(pair)

                return state
            }

            override fun newArray(size: Int): Array<DlgState?> {
                return arrayOfNulls(size)
            }
        }
    }
}
