/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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

import android.app.Dialog
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.content.SharedPreferences
import android.content.SharedPreferences.OnSharedPreferenceChangeListener
import android.os.Bundle
import android.view.MenuItem
import android.view.View
import android.widget.PopupMenu
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.PreferenceManager

import java.io.File
import java.io.Serializable

import org.eehouse.android.xw4.gen.PrefsWrappers
import org.eehouse.android.xw4.jni.CommonPrefs.ColorTheme
import org.eehouse.android.xw4.jni.CommonPrefs

private val TAG: String = PrefsDelegate::class.java.simpleName

class PrefsDelegate(private val mActivity: XWActivity,
                    delegator: Delegator,
                    savedInstanceState: Bundle?) :
    DelegateBase(delegator, R.layout.prefs), OnSharedPreferenceChangeListener,
    View.OnClickListener, PopupMenu.OnMenuItemClickListener
{
    private var mFragment: PreferenceFragmentCompat? = null

    override fun makeDialog(alert: DBAlert, vararg params: Any?): Dialog {
        val dlgID = alert.dlgID
        var lstnr: DialogInterface.OnClickListener? = null
        var confirmID = 0

        when (dlgID) {
            DlgID.REVERT_COLORS -> {
                confirmID = R.string.confirm_revert_colors
                lstnr = DialogInterface.OnClickListener { dlg, item ->
                    val self = curThis() as PrefsDelegate
                    val editor =
                        self.sharedPreferences!!.edit()
                    for (colorKey in getColorKeys(alert.requireContext())) {
                        editor.remove(colorKey)
                    }
                    editor.commit()
                    self.relaunch()
                }
            }

            DlgID.REVERT_ALL -> {
                confirmID = R.string.confirm_revert_all
                lstnr = DialogInterface.OnClickListener { dlg, item ->
                    val self = curThis() as PrefsDelegate
                    val sp = self.sharedPreferences
                    val editor = sp!!.edit()
                    editor.clear()
                    editor.commit()
                    self.relaunch()
                }
            }
            else -> {Log.d(TAG, "unexpected dlgID $dlgID")}
        }
        var dialog: Dialog? = null
        if (null != lstnr) {
            dialog = makeAlertBuilder()
                .setTitle(R.string.query_title)
                .setMessage(confirmID)
                .setPositiveButton(android.R.string.ok, lstnr)
                .setNegativeButton(android.R.string.cancel, null)
                .create()
        }
        return dialog!!
    }

    override fun init(savedInstanceState: Bundle?) {
        if (null == s_keysHash) {
            s_keysHash = HashMap()
            s_keys.map {
                val str = getString(it)
                s_keysHash!![str] = it
            }
        }
    }

    fun setRootFragment(fragment: PreferenceFragmentCompat) {
        mFragment = fragment
    }

    override fun onResume() {
        super.onResume()
        sharedPreferences!!.registerOnSharedPreferenceChangeListener(this)

        // It's too early somehow to do this in init() above
        requireViewById(R.id.prefs_menu).setOnClickListener(this)
    }

    override fun onPause() {
        sharedPreferences!!.unregisterOnSharedPreferenceChangeListener(this)
        super.onPause()
    }

    // interface View.OnClickListener
    override fun onClick(view: View) {
        val dlgID: DlgID? = null
        val id = view.id
        when (id) {
            R.id.prefs_menu -> {
                val popup = PopupMenu(mActivity, view)
                popup.inflate(R.menu.prefs_popup)
                popup.setOnMenuItemClickListener(this)
                popup.show()
            }

            else -> Assert.failDbg()
        }
        if (null != dlgID) {
            showDialogFragment(dlgID)
        }
    }

    override fun onMenuItemClick(item: MenuItem): Boolean {
        var handled = true
        var dlgID: DlgID? = null
        var theme: ColorTheme? = null
        when (item.itemId) {
            R.id.prefs_revert_colors -> dlgID = DlgID.REVERT_COLORS
            R.id.prefs_revert_all -> dlgID = DlgID.REVERT_ALL
            R.id.prefs_copy_light -> theme = ColorTheme.LIGHT
            R.id.prefs_copy_dark -> theme = ColorTheme.DARK
            else -> {
                Assert.failDbg()
                handled = false
            }
        }
        if (null != dlgID) {
            showDialogFragment(dlgID)
        } else if (null != theme) {
            makeNotAgainBuilder(
                R.string.key_na_copytheme,
                DlgDelegate.Action.EXPORT_THEME,
                R.string.not_again_copytheme
            )
                .setParams(theme)
                .show()
        }

        return handled
    }

    // interface SharedPreferences.OnSharedPreferenceChangeListener
    override fun onSharedPreferenceChanged(sp: SharedPreferences, key: String?) {
        if (s_keysHash!!.containsKey(key)) {
            when (s_keysHash!![key]) {
                R.string.key_logging_on -> Log.enable(sp.getBoolean(key, false))
                R.string.key_show_sms -> NBSProto.smsToastEnable(sp.getBoolean(key, false))
                R.string.key_enable_nbs -> if (!sp.getBoolean(key, true)) {
                    NBSProto.stopThreads()
                }

                R.string.key_download_path -> {
                    val value = sp.getString(key, null)
                    if (null != value) {
                        val dir = File(value)
                        var msg: String? = null
                        if (!dir.exists()) {
                            msg = String.format("%s does not exist", value)
                        } else if (!dir.isDirectory) {
                            msg = String.format("%s is not a directory", value)
                        } else if (!dir.canWrite()) {
                            msg = String.format("Cannot write to %s", value)
                        }
                        if (null != msg) {
                            showToast(msg)
                        }
                    }
                    DictUtils.invalDictList()
                }

                R.string.key_thumbsize -> DBUtils.clearThumbnails(mActivity)
                R.string.key_default_language -> {}
                R.string.key_force_radio -> SMSPhoneInfo.reset()
                R.string.key_disable_nag, R.string.key_disable_nag_solo ->
                    NagTurnReceiver.resetNagsDisabled(mActivity)

                R.string.key_disable_mqtt -> {
                    val enabled = !sp.getBoolean(key, true)
                    MQTTUtils.setEnabled(mActivity, enabled)
                }

                R.string.key_disable_bt -> BTUtils.disabledChanged(mActivity)
                R.string.key_force_tablet -> makeOkOnlyBuilder(R.string.after_restart).show()
                R.string.key_mqtt_host, R.string.key_mqtt_port, R.string.key_mqtt_qos ->
                    MQTTUtils.onConfigChanged(mActivity)

                else -> Assert.failDbg()
            }
        }
    }

    override fun onPosButton(action: DlgDelegate.Action, vararg params: Any?): Boolean {
        var handled = true
        when (action) {
            DlgDelegate.Action.ENABLE_NBS_DO -> {
                XWPrefs.setNBSEnabled(mActivity, true)
                SMSCheckBoxPreference.setChecked()
            }

            DlgDelegate.Action.DISABLE_MQTT_DO -> {
                MQTTUtils.setEnabled(mActivity, false)
                MQTTCheckBoxPreference.setChecked()
            }

            DlgDelegate.Action.DISABLE_BT_DO -> {
                BTUtils.setEnabled(mActivity, false)
                BTCheckBoxPreference.setChecked()
            }

            DlgDelegate.Action.EXPORT_THEME -> {
                val theme = params[0] as ColorTheme
                CommonPrefs.colorPrefsToClip(mActivity, theme)
                DbgUtils.showf(mActivity, R.string.theme_data_success)
            }

            else -> handled = super.onPosButton(action, *params)
        }
        return handled
    }

    private fun relaunch() {
        resetPrefs(mActivity, true)

        // Now replace this activity with a new copy
        // so the new values get loaded.
        launch(mActivity)
        finish()
    }

    private val sharedPreferences: SharedPreferences?
        get() = mFragment!!.preferenceScreen.sharedPreferences

    companion object {
        private val PREFS_KEY = TAG + "/prefs"

        private val s_keys = intArrayOf(
            R.string.key_logging_on,
            R.string.key_show_sms,
            R.string.key_enable_nbs,
            R.string.key_download_path,
            R.string.key_thumbsize,
            R.string.key_default_language,
            R.string.key_force_radio,
            R.string.key_disable_nag,
            R.string.key_disable_nag_solo,
            R.string.key_disable_mqtt,
            R.string.key_disable_bt,
            R.string.key_force_tablet,
            R.string.key_mqtt_host,
            R.string.key_mqtt_port,
            R.string.key_mqtt_qos,
        )
        private var s_keysHash: MutableMap<String, Int>? = null

        @JvmOverloads
        fun launch(context: Context, root: Class<*>? = PrefsWrappers.prefs::class.java) {
            val bundle: Bundle? = null
            val intent = Intent(context, PrefsActivity::class.java)
            if (null != root) {
                PrefsActivity.bundleRoot(root, intent)
            }
            context.startActivity(intent)
        }

        fun resetPrefs(context: Context, mustCheck: Boolean) {
            val prefIDs = PrefsWrappers.getPrefsResIDs()
            for (id in prefIDs) {
                PreferenceManager.setDefaultValues(context, id, mustCheck)
            }
        }

        private fun getPrefsWith(context: Context, with: Boolean): Serializable {
            val colorKeys = getColorKeys(context)
            val prefs = PreferenceManager
                .getDefaultSharedPreferences(context)
            val all = prefs.all
            val result = HashMap<String, Any?>()
            for (key in all.keys) {
                if (with == colorKeys.contains(key)) {
                    result[key] = all[key]
                }
            }
            return result
        }

        fun getPrefsColors(context: Context): Serializable {
            return getPrefsWith(context, true)
        }

        fun getPrefsNoColors(context: Context): Serializable {
            return getPrefsWith(context, false)
        }

        fun loadPrefs(context: Context, obj: Serializable?) {
            if (null != obj) {
                val map = obj as HashMap<String, Any>
                val editor =
                    PreferenceManager.getDefaultSharedPreferences(context)
                        .edit()
                for (key in map.keys) {
                    val value = map[key]
                    if (value is Boolean) {
                        editor.putBoolean(key, (value as Boolean?)!!)
                    } else if (value is String) {
                        editor.putString(key, value as String?)
                    } else if (value is Int) {
                        editor.putInt(key, (value as Int?)!!)
                    } else if (value is Long) {
                        editor.putLong(key, (value as Long?)!!)
                    } else {
                        Log.d(TAG, "unexpected class: %s", value!!.javaClass.name)
                        Assert.failDbg()
                    }
                }
                editor.commit()
            }
        }

        private fun getColorKeys(context: Context): Set<String> {
            val res = context.resources
            val result: MutableSet<String> = HashSet()
            val themeKeys = intArrayOf(
                R.array.color_ids_light,
                R.array.color_ids_dark,
            )
            for (themeKey in themeKeys) {
                val colorKeys = res.getStringArray(themeKey)
                for (colorKey in colorKeys) {
                    result.add(colorKey)
                }
            }
            return result
        }
    }
}
