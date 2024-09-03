/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */ /*
 * Copyright 2024 by Eric House (xwords@eehouse.org).  All
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
import androidx.preference.ListPreference

import com.hivemq.client.mqtt.datatypes.MqttQos

import org.eehouse.android.xw4.loc.LocUtils
import org.eehouse.android.xw4.jni.XwJNI

class QOSListPreference(private val mContext: Context,
                        attrs: AttributeSet?) :
    ListPreference(mContext, attrs)
{
    private val TAG = QOSListPreference::class.java.simpleName

    override fun onAttached() {
        super.onAttached()
        summary = getPersistedString("")
    }

    override fun getEntries(): Array<CharSequence> { return getEntriesImpl() }
    override fun getEntryValues(): Array<CharSequence> { return getEntriesImpl() }

    override fun persistString(value: String): Boolean {
        summary = value
        return super.persistString(value)
    }
    
    private var mEntries: Array<CharSequence>? = null
    private fun getEntriesImpl() :Array<CharSequence>
    {
        if ( null == mEntries ) {
            val enums = MqttQos.entries.map{it.toString()}.toMutableList()
            val curQos = MqttQos.entries[XwJNI.dvc_getQOS()]
            val str = LocUtils
                .getString(mContext, R.string.qos_prefs_default_expl_fmt, curQos)
            enums.add(str)
            mEntries = enums.toTypedArray()
        }
        return mEntries!!
    }
}
