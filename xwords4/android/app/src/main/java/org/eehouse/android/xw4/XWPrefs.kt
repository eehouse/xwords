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

import android.content.Context
import android.content.res.Configuration
import android.text.TextUtils
import androidx.preference.PreferenceManager

import org.json.JSONException
import org.json.JSONObject

import org.eehouse.android.xw4.DBUtils.getAnyGroup
import org.eehouse.android.xw4.DictUtils.DictLoc
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet
import org.eehouse.android.xw4.jni.XwJNI.RematchOrder

open class XWPrefs {
    companion object {
        private val TAG: String = XWPrefs::class.java.simpleName

        // No reason to put this in xml if they're private to this file!
        private const val key_checked_upgrades = "key_checked_upgrades"

        fun getNBSEnabled(context: Context): Boolean {
            val haveNative = Perms23.haveNativePerms()
            return haveNative || getPrefsBoolean(context, R.string.key_enable_nbs, false)
        }

        fun setNBSEnabled(context: Context, enabled: Boolean) {
            Assert.assertTrue(!Perms23.haveNativePerms() || !BuildConfig.DEBUG)
            setPrefsBoolean(context, R.string.key_enable_nbs, enabled)
        }

        fun getDebugEnabled(context: Context): Boolean {
            return getPrefsBoolean(
                context, R.string.key_enable_debug,
                BuildConfig.DEBUG
            )
        }

        fun moveCountEnabled(context: Context): Boolean {
            return getPrefsBoolean(
                context, R.string.key_enable_pending_count,
                BuildConfig.DEBUG
            )
        }

        fun getSMSToSelfEnabled(context: Context): Boolean {
            return getPrefsBoolean(context, R.string.key_enable_sms_toself, false)
        }

        fun getHideNewgameButtons(context: Context): Boolean {
            return getPrefsBoolean(
                context, R.string.key_hide_newgames,
                false
            )
        }

        fun setHideNewgameButtons(context: Context, set: Boolean) {
            setPrefsBoolean(context, R.string.key_hide_newgames, set)
        }

        fun getDefaultUpdateUrl(context: Context): String {
            val result = getWithHost(context, R.string.key_update_url_path)
            // Log.d( TAG, "getDefaultUpdateUrl() => %s", result );
            return result
        }

        fun getDefaultMQTTUrl(context: Context): String {
            val result = getWithHost(context, R.string.key_mqtt_url_path)
            return result
        }

        fun getHostName(context: Context): String? {
            val host = getPrefsString(context, R.string.key_mqtt_host)
            return NetUtils.forceHost(host)
        }

        fun getMQTTEnabled(context: Context): Boolean {
            val enabled = !getPrefsBoolean(
                context, R.string.key_disable_mqtt,
                false
            )
            // Log.d( TAG, "getMQTTEnabled() => %b", enabled );
            return enabled
        }

        fun setMQTTEnabled(context: Context, enabled: Boolean) {
            setPrefsBoolean(context, R.string.key_disable_mqtt, !enabled)
        }

        fun getBTDisabled(context: Context): Boolean {
            val disabled = getPrefsBoolean(
                context, R.string.key_disable_bt,
                false
            )
            return disabled
        }

        fun setBTDisabled(context: Context, disabled: Boolean) {
            setPrefsBoolean(context, R.string.key_disable_bt, disabled)
        }

        fun getDefaultProxyPort(context: Context): Int {
            val `val` = getPrefsString(context, R.string.key_proxy_port)
            var result = 0
            try {
                result = `val`!!.toInt()
            } catch (ex: Exception) {
            }
            // DbgUtils.logf( "getDefaultProxyPort=>%d", result );
            return result
        }

        fun getDefaultDictURL(context: Context): String {
            val result = getWithHost(context, R.string.key_dict_host_path)
            return result
        }

        fun getSquareTiles(context: Context): Boolean {
            return getPrefsBoolean(context, R.string.key_square_tiles, false)
        }

        fun getDefaultPlayerMinutes(context: Context): Int {
            val value =
                getPrefsString(context, R.string.key_initial_player_minutes)
            var result = try {
                value!!.toInt()
            } catch (ex: Exception) {
                25
            }
            return result
        }

        fun getPrefsInt(context: Context, keyID: Int, defaultValue: Int): Int {
            var result = defaultValue
            if (null != context) {
                val key = context.getString(keyID)
                val sp = PreferenceManager
                    .getDefaultSharedPreferences(context)
                try {
                    result = sp.getInt(key, defaultValue)
                    // If it's in a pref, it'll be a string (editable) So will get CCE
                } catch (cce: ClassCastException) {
                    val asStr = sp.getString(key, String.format("%d", defaultValue))
                    try {
                        result = asStr!!.toInt()
                    } catch (ex: Exception) {
                    }
                }
            }
            return result
        }

        fun setPrefsInt(context: Context, keyID: Int, newValue: Int) {
            val sp = PreferenceManager
                .getDefaultSharedPreferences(context)
            val editor = sp.edit()
            val key = context.getString(keyID)
            editor.putInt(key, newValue)
            editor.commit()
        }

        fun getPrefsBoolean(
            context: Context, keyID: Int,
            defaultValue: Boolean
        ): Boolean {
            val key = context.getString(keyID)
            return getPrefsBoolean(context, key, defaultValue)
        }

        private fun getPrefsBoolean(
            context: Context, key: String,
            defaultValue: Boolean
        ): Boolean {
            val sp = PreferenceManager
                .getDefaultSharedPreferences(context)
            return sp.getBoolean(key, defaultValue)
        }

        fun setPrefsBoolean(
            context: Context, keyID: Int,
            newValue: Boolean
        ) {
            val key = context.getString(keyID)
            setPrefsBoolean(context, key, newValue)
        }

        private fun setPrefsBoolean(
            context: Context, key: String,
            newValue: Boolean
        ) {
            val sp = PreferenceManager
                .getDefaultSharedPreferences(context)
            val editor = sp.edit()
            editor.putBoolean(key, newValue)
            editor.commit()
        }

        fun getPrefsLong(
            context: Context, keyID: Int,
            defaultValue: Long
        ): Long {
            val key = context.getString(keyID)
            val sp = PreferenceManager
                .getDefaultSharedPreferences(context)
            return sp.getLong(key, defaultValue)
        }

        fun setPrefsLong(context: Context, keyID: Int, newVal: Long) {
            val sp = PreferenceManager
                .getDefaultSharedPreferences(context)
            val editor = sp.edit()
            val key = context.getString(keyID)
            editor.putLong(key, newVal)
            editor.commit()
        }

        fun setClosedLangs(context: Context, langs: Array<String?>?) {
            setPrefsString(
                context, R.string.key_closed_langs,
                TextUtils.join("\n", langs!!)
            )
        }

        fun getClosedLangs(context: Context): Array<String>? {
            return getPrefsStringArray(context, R.string.key_closed_langs)
        }

        fun setSMSPhones(context: Context, phones: JSONObject) {
            setPrefsString(context, R.string.key_sms_phones, phones.toString())
        }

        fun getSMSPhones(context: Context): JSONObject {
            val asStr = getPrefsString(context, R.string.key_sms_phones)
            var obj: JSONObject? = null

            if (null != asStr) {
                obj = try {
                    JSONObject(asStr)
                } catch (ex: JSONException) {
                    null
                }
            }

            if (null == obj) {
                obj = JSONObject()
                if (null != asStr) {
                    val numbers = TextUtils.split(asStr, "\n")
                    for (number in numbers) {
                        try {
                            obj.put(number, "") // null removes any entry
                        } catch (ex: JSONException) {
                            Log.ex(TAG, ex)
                        }
                    }
                }
            }

            // Log.d( TAG, "getSMSPhones() => %s", obj.toString() );
            return obj
        }

        fun setBTAddresses(context: Context, addrs: Array<String?>?) {
            setPrefsStringArray(context, R.string.key_bt_addrs, addrs)
        }

        fun getBTAddresses(context: Context): Array<String>? {
            return getPrefsStringArray(context, R.string.key_bt_addrs)
        }

        fun getDefaultLoc(context: Context): DictLoc {
            val internal = getDefaultLocInternal(context)
            val result = if (internal) DictLoc.INTERNAL
            else DictLoc.EXTERNAL
            return result
        }

        fun getMyDownloadDir(context: Context): String? {
            return getPrefsString(context, R.string.key_download_path)
        }

        fun getDefaultLocInternal(context: Context): Boolean {
            return getPrefsBoolean(context, R.string.key_default_loc, true)
        }

        fun getDefaultNewGameGroup(context: Context): Long {
            var groupID = getPrefsLong(
                context, R.string.key_default_group,
                DBUtils.GROUPID_UNSPEC
            )
            if (DBUtils.GROUPID_UNSPEC == groupID) {
                groupID = getAnyGroup(context)
                setPrefsLong(context, R.string.key_default_group, groupID)
            }
            Assert.assertTrue(DBUtils.GROUPID_UNSPEC != groupID)
            return groupID
        }

        fun getDefaultRematchOrder(context: Context): RematchOrder? {
            val storedStr = getPrefsString(context, R.string.key_rematch_order)

            // Let's try to get this from the enum...
            var ro: RematchOrder? = null
            for (one in RematchOrder.entries) {
                val strID = one.strID
                val str = context.getString(strID)
                if (str == storedStr) {
                    ro = one
                    break
                }
            }

            return ro
        }

        fun setDefaultNewGameGroup(context: Context, `val`: Long) {
            Assert.assertTrue(DBUtils.GROUPID_UNSPEC != `val`)
            setPrefsLong(context, R.string.key_default_group, `val`)
        }

        fun getThumbEnabled(context: Context): Boolean {
            return 0 < getThumbPct(context)
        }

        fun getThumbPct(context: Context): Int {
            val pct = getPrefsString(context, R.string.key_thumbsize)
            var result: Int
            if (context.getString(R.string.thumb_off) == pct) {
                result = 0
            } else {
                try {
                    val suffix = context.getString(R.string.pct_suffix)
                    result = pct!!.substring(
                        0, pct.length
                                - suffix.length
                    ).toInt()
                } catch (ex: Exception) {
                    result = 30
                }
            }
            return result
        }

        fun getStudyEnabled(context: Context): Boolean {
            return getPrefsBoolean(context, R.string.key_studyon, true)
        }

        fun getPrefsString(context: Context, keyID: Int, dflt: String?): String? {
            val key = context.getString(keyID)
            val sp = PreferenceManager
                .getDefaultSharedPreferences(context)
            return sp.getString(key, dflt)
        }

        fun getPrefsString(context: Context, keyID: Int): String {
            return getPrefsString(context, keyID, "")!!
        }

        fun getPrefsString(context: Context, keyID: Int, dflt: Int): String? {
            val dfltStr = getPrefsString( context, dflt)
            return getPrefsString(context, keyID, dfltStr)
        }

        fun setPrefsString(
            context: Context, keyID: Int,
            newValue: String?
        ) {
            val sp = PreferenceManager
                .getDefaultSharedPreferences(context)
            val editor = sp.edit()
            val key = context.getString(keyID)
            editor.putString(key, newValue)
            editor.commit()
        }

        protected fun clearPrefsKey(context: Context, keyID: Int) {
            val sp = PreferenceManager
                .getDefaultSharedPreferences(context)
            val editor = sp.edit()
            val key = context.getString(keyID)
            editor.remove(key)
            editor.commit()
        }

        protected fun getPrefsStringArray(context: Context, keyID: Int): Array<String>? {
            val asStr = getPrefsString(context, keyID)
            val result = if (null == asStr) null else TextUtils.split(asStr, "\n")
            return result
        }

        protected fun setPrefsStringArray(
            context: Context, keyID: Int,
            value: Array<String?>?
        ) {
            setPrefsString(context, keyID, TextUtils.join("\n", value!!))
        }

        fun setHaveCheckedUpgrades(context: Context, haveChecked: Boolean) {
            setPrefsBoolean(context, key_checked_upgrades, haveChecked)
        }

        fun getHaveCheckedUpgrades(context: Context): Boolean {
            return getPrefsBoolean(context, key_checked_upgrades, false)
        }

        fun getCanInviteMulti(context: Context): Boolean {
            return getPrefsBoolean(context, R.string.key_invite_multi, false)
        }

        fun getIsTablet(context: Context): Boolean {
            var result = isTablet(context)
            val setting = getPrefsString(context, R.string.key_force_tablet)
            if (setting == context.getString(R.string.force_tablet_default)) {
                // Leave it alone
            } else if (setting == context.getString(R.string.force_tablet_tablet)) {
                result = true
            } else if (setting == context.getString(R.string.force_tablet_phone)) {
                result = false
            }

            // Log.d( TAG, "getIsTablet() => %b (got %s)", result, setting );
            return result
        }

        fun getAddrTypes(context: Context): CommsConnTypeSet {
            val result: CommsConnTypeSet
            val flags = getPrefsInt(context, R.string.key_addrs_pref, -1)
            if (-1 == flags) {
                result = CommsConnTypeSet()
                if (getMQTTEnabled(context)) {
                    result.add(CommsConnType.COMMS_CONN_MQTT)
                }
                if (BTUtils.BTEnabled()) {
                    result.add(CommsConnType.COMMS_CONN_BT)
                }
            } else {
                result = CommsConnTypeSet(flags)
            }

            // Save it if changed
            val originalHash = result.hashCode()
            CommsConnTypeSet.removeUnsupported(context, result)
            if (0 == result.size) {
                result.add(CommsConnType.COMMS_CONN_MQTT)
            }
            if (!BTUtils.havePermissions(context)) {
                result.remove(CommsConnType.COMMS_CONN_BT)
            }
            if (originalHash != result.hashCode()) {
                setAddrTypes(context, result)
            }

            // Log.d( TAG, "getAddrTypes() => %s", result.toString( context, false) );
            return result
        }

        fun getDefaultTraySize(context: Context): Int {
            return getPrefsInt(context, R.string.key_tray_size, XWApp.MIN_TRAY_TILES)
        }

        fun setAddrTypes(context: Context, set: CommsConnTypeSet) {
            val flags = set.toInt()
            setPrefsInt(context, R.string.key_addrs_pref, flags)
        }

        private var s_isTablet: Boolean? = null
        private fun isTablet(context: Context): Boolean {
            if (null == s_isTablet) {
                val screenLayout =
                    context.resources.configuration.screenLayout
                val size = screenLayout and Configuration.SCREENLAYOUT_SIZE_MASK
                s_isTablet = Configuration.SCREENLAYOUT_SIZE_LARGE <= size
            }
            return s_isTablet!!
        }

        private fun getWithHost(context: Context, pathKey: Int): String {
            val host = getHostName(context)
            val path = getPrefsString(context, pathKey)
            val result = String.format("https://%s/%s", host, path)
            return result
        }
    }
}
