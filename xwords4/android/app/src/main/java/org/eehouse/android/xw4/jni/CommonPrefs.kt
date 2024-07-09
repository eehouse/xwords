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
package org.eehouse.android.xw4.jni

import android.content.Context
import android.content.SharedPreferences
import android.content.res.Configuration
import android.net.Uri
import android.os.Build
import android.text.TextUtils
import androidx.preference.PreferenceManager

import org.eehouse.android.xw4.Assert
import org.eehouse.android.xw4.DictUtils
import org.eehouse.android.xw4.Log
import org.eehouse.android.xw4.NetUtils
import org.eehouse.android.xw4.R
import org.eehouse.android.xw4.Utils
import org.eehouse.android.xw4.XWPrefs
import org.eehouse.android.xw4.XWSumListPreference
import org.eehouse.android.xw4.jni.CurGameInfo.XWPhoniesChoice
import org.eehouse.android.xw4.loc.LocUtils

class CommonPrefs private constructor() : XWPrefs() {
    // Keep in sync with TileValueType enum in comtypes.h
    enum class TileValueType(val expl: Int) {
        TVT_FACES(R.string.values_faces),
        TVT_VALUES(R.string.values_values),
        TVT_BOTH(R.string.values_both)
    }

    var showBoardArrow: Boolean = false
    var showRobotScores: Boolean = false
    var hideTileValues: Boolean = false
    var skipCommitConfirm: Boolean = false
    var showColors: Boolean = false
    var sortNewTiles: Boolean = false
    var allowPeek: Boolean = false
    var hideCrosshairs: Boolean = false
    var skipMQTTAdd: Boolean = false
    var tvType: TileValueType? = null

    var playerColors: IntArray = IntArray(4)
    var bonusColors: IntArray = IntArray(7)
    var otherColors: IntArray

    private fun refresh(context: Context): CommonPrefs {
        var key: String
        val sp = PreferenceManager
            .getDefaultSharedPreferences(context)

        showBoardArrow = getBoolean(
            context, sp, R.string.key_show_arrow,
            true
        )
        showRobotScores = getBoolean(
            context, sp, R.string.key_explain_robot,
            false
        )
        hideTileValues = getBoolean(
            context, sp, R.string.key_hide_values,
            false
        )
        skipCommitConfirm = getBoolean(
            context, sp,
            R.string.key_skip_confirm, false
        )
        showColors = getBoolean(context, sp, R.string.key_color_tiles, true)
        sortNewTiles = getBoolean(context, sp, R.string.key_sort_tiles, true)
        allowPeek = getBoolean(context, sp, R.string.key_peek_other, false)
        skipMQTTAdd = getBoolean(context, sp, R.string.key_skip_mqtt_add, false)
        hideCrosshairs = getBoolean(context, sp, R.string.key_hide_crosshairs, false)

        val ord = getInt(context, sp, R.string.key_tile_valuetype, 0)
        tvType = TileValueType.entries[ord]

        val theme = getTheme(context, null)
        val colorStrIds = context.resources.getStringArray(theme.arrayID)
        var offset = copyColors(sp, colorStrIds, 0, playerColors, 0)
        offset += copyColors(sp, colorStrIds, offset, bonusColors, 1)
        offset += copyColors(sp, colorStrIds, offset, otherColors, 0)

        return this
    }

    private fun copyColors(
        sp: SharedPreferences, colorStrIds: Array<String>,
        idsStart: Int, colors: IntArray, colorsStart: Int
    ): Int {
        var colorsStart = colorsStart
        var nUsed = 0
        while (colorsStart < colors.size) {
            val key = colorStrIds[idsStart + nUsed++]
            val color = -0x1000000 or sp.getInt(key, 0)
            colors[colorsStart++] = color
        }

        return nUsed
    }

    private fun getBoolean(
        context: Context, sp: SharedPreferences,
        id: Int, dflt: Boolean
    ): Boolean {
        val key = LocUtils.getString(context, id)
        return sp.getBoolean(key, dflt)
    }

    private fun getInt(
        context: Context, sp: SharedPreferences,
        id: Int, dflt: Int
    ): Int {
        val key = LocUtils.getString(context, id)
        return sp.getInt(key, dflt)
    }

    enum class ColorTheme(val arrayID: Int) {
        LIGHT(R.array.color_ids_light),
        DARK(R.array.color_ids_dark)
    }

    init {
        bonusColors[0] = -0xf0f0f10 // garbage
        otherColors = IntArray(COLOR_LAST)
    }

    companion object {
        private val TAG: String = CommonPrefs::class.java.simpleName

        const val COLOR_TILE_BACK: Int = 0
        const val COLOR_TILE_BACK_RECENT: Int = 1
        const val COLOR_NOTILE: Int = 2
        const val COLOR_FOCUS: Int = 3
        const val COLOR_BACKGRND: Int = 4
        const val COLOR_BONUSHINT: Int = 5
        const val COLOR_CELLLINE: Int = 6
        const val COLOR_LAST: Int = 7

        private var s_cp: CommonPrefs? = null

        /*
     * static methods
     */
        fun get(context: Context): CommonPrefs {
            if (null == s_cp) {
                s_cp = CommonPrefs()
            }
            return s_cp!!.refresh(context)
        }

        // Is the OS-level setting on?
        fun darkThemeEnabled(context: Context): Boolean {
            val fromOS = booleanArrayOf(false)
            val theme = getTheme(context, fromOS)
            val result = theme == ColorTheme.DARK && fromOS[0]
            return result
        }

        fun darkThemeInUse(context: Context): Boolean {
            val theme = getTheme(context, null)
            return theme == ColorTheme.DARK
        }

        private fun getTheme(context: Context, fromOSOut: BooleanArray?): ColorTheme {
            var theme = ColorTheme.LIGHT
            val sp = PreferenceManager
                .getDefaultSharedPreferences(context)
            var which: String? = LocUtils.getString(context, R.string.key_theme_which)
            which = sp.getString(which, null)
            if (null != which) {
                try {
                    when (which.toInt()) {
                        0 -> {}
                        1 -> theme = ColorTheme.DARK
                        2 -> {
                            Assert.assertTrueNR(Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q)
                            val res = context.resources
                            val uiMode = res.configuration.uiMode
                            if (Configuration.UI_MODE_NIGHT_YES
                                == (uiMode and Configuration.UI_MODE_NIGHT_MASK)
                            ) {
                                theme = ColorTheme.DARK
                                if (null != fromOSOut) {
                                    fromOSOut[0] = true
                                }
                            }
                        }

                        else -> Assert.failDbg()
                    }
                } catch (nfe: NumberFormatException) {
                    // This happens on Lollipop. I don't care why.
                    Log.d(TAG, "NumberFormatException: %s", nfe)
                } catch (ex: Exception) {
                    // Will happen with old not-an-int saved value
                    Log.ex(TAG, ex)
                }
            }
            return theme
        }

        fun getDefaultBoardSize(context: Context): Int {
            val value = getPrefsString(context, R.string.key_board_size)
            var result = try {
                value!!.substring(0, 2).toInt()
            } catch (ex: Exception) {
                15
            }
            return result
        }

        fun getDefaultHumanDict(context: Context): String {
            var value = getPrefsString(context, R.string.key_default_dict)
            if (value == "" || !DictUtils.dictExists(context, value!!)) {
                value = DictUtils.dictList(context)!![0].name
            }
            return value
        }

        fun getDefaultRobotDict(context: Context): String {
            var value = getPrefsString(context, R.string.key_default_robodict)
            if (value == "" || !DictUtils.dictExists(context, value!!)) {
                value = getDefaultHumanDict(context)
            }
            return value
        }

        fun getDefaultOriginalPlayerName(
            context: Context,
            num: Int
        ): String {
            return LocUtils.getString(context, R.string.player_fmt, num + 1)
        }

        fun getDefaultPlayerName(
            context: Context, num: Int,
            force: Boolean
        ): String {
            var result = getPrefsString(context, R.string.key_player1_name, "")
            if (force && TextUtils.isEmpty(result)) {
                result = getDefaultOriginalPlayerName(context, num)
            }
            return result!!
        }

        fun getDefaultPlayerName(context: Context, num: Int): String {
            return getDefaultPlayerName(context, num, true)
        }

        fun getDefaultRobotName(context: Context): String {
            return getPrefsString(context, R.string.key_robot_name)!!
        }

        fun setDefaultPlayerName(context: Context, value: String?) {
            setPrefsString(context, R.string.key_player1_name, value)
        }

        fun getDefaultPhonies(context: Context): XWPhoniesChoice {
            val value = getPrefsString(context, R.string.key_default_phonies)

            var result =
                XWPhoniesChoice.PHONIES_IGNORE
            val res = context.resources
            val names = res.getStringArray(R.array.phony_names)
            for (ii in names.indices) {
                val name = names[ii]
                if (name == value) {
                    result = XWPhoniesChoice.entries.toTypedArray().get(ii)
                    break
                }
            }
            return result
        }

        fun getDefaultTimerEnabled(context: Context): Boolean {
            return getPrefsBoolean(
                context, R.string.key_default_timerenabled,
                false
            )
        }

        fun getDefaultHintsAllowed(
            context: Context,
            networked: Boolean
        ): Boolean {
            val key =
                if (networked) R.string.key_init_nethintsallowed else R.string.key_init_hintsallowed
            return getPrefsBoolean(context, key, true)
        }

        fun getSub7TradeAllowed(context: Context): Boolean {
            return getPrefsBoolean(context, R.string.key_init_tradeSub7, false)
        }

        fun getDefaultDupMode(context: Context): Boolean {
            return getPrefsBoolean(context, R.string.key_init_dupmodeon, false)
        }

        fun getDupModeHidden(context: Context): Boolean {
            return !getPrefsBoolean(context, R.string.key_unhide_dupmode, false)
        }

        fun getAutoJuggle(context: Context): Boolean {
            return getPrefsBoolean(context, R.string.key_init_autojuggle, false)
        }

        fun getHideTitleBar(context: Context): Boolean {
            val hideByDefault = 11 > Build.VERSION.SDK.toInt()
            return getPrefsBoolean(
                context, R.string.key_hide_title,
                hideByDefault
            )
        }

        fun getSoundNotify(context: Context): Boolean {
            return getPrefsBoolean(context, R.string.key_notify_sound, true)
        }

        fun getVibrateNotify(context: Context): Boolean {
            return getPrefsBoolean(context, R.string.key_notify_vibrate, false)
        }

        fun getKeepScreenOn(context: Context): Boolean {
            return getPrefsBoolean(context, R.string.key_keep_screenon, false)
        }

        fun getSummaryField(context: Context): String? {
            return getPrefsString(context, R.string.key_summary_field)
        }

        fun getSummaryFieldId(context: Context): Int {
            var result = 0
            val str = getSummaryField(context)
            val ids = XWSumListPreference.getFieldIDs(context)
            for (id in ids) {
                if (LocUtils.getString(context, id) == str) {
                    result = id
                    break
                }
            }
            return result
        }

        private const val THEME_KEY = "theme"

        fun colorPrefsToClip(context: Context, theme: ColorTheme) {
            val host = LocUtils.getString(context, R.string.invite_host)
            val ub = Uri.Builder()
                .scheme("https")
                .path(
                    String.format(
                        "//%s%s", NetUtils.forceHost(host),
                        LocUtils.getString(context, R.string.conf_prefix)
                    )
                )
                .appendQueryParameter(THEME_KEY, theme.toString())

            val res = context.resources
            val urlKeys = res.getStringArray(R.array.color_url_keys)
            val dataKeys = res.getStringArray(theme.arrayID)
            Assert.assertTrueNR(urlKeys.size == dataKeys.size)
            val sp = PreferenceManager
                .getDefaultSharedPreferences(context)

            for (ii in urlKeys.indices) {
                val `val` = sp.getInt(dataKeys[ii], 0)
                ub.appendQueryParameter(urlKeys[ii], String.format("%X", `val`))
            }
            val data = ub.build().toString()

            Utils.stringToClip(context, data)
        }

        fun loadColorPrefs(context: Context, uri: Uri) {
            val themeName = uri.getQueryParameter(THEME_KEY)
            var arrayID = 0
            for (theme in ColorTheme.entries) {
                if (theme.toString() == themeName) {
                    arrayID = theme.arrayID
                    break
                }
            }
            Assert.assertTrueNR(0 != arrayID)
            if (0 != arrayID) {
                val res = context.resources
                val urlKeys = res.getStringArray(R.array.color_url_keys)
                val dataKeys = res.getStringArray(arrayID)
                val sp = PreferenceManager
                    .getDefaultSharedPreferences(context)
                val editor = sp.edit()

                for (ii in urlKeys.indices) {
                    val urlKey = urlKeys[ii]
                    try {
                        val `val` = uri.getQueryParameter(urlKey)
                        editor.putInt(dataKeys[ii], `val`!!.toInt(16))
                        Log.d(TAG, "set %s => %s", dataKeys[ii], `val`) // here
                    } catch (ex: Exception) {
                        Log.ex(TAG, ex)
                        Log.d(TAG, "bad/missing data for url key: %s", urlKey)
                    }
                }
                editor.commit()
            }
        }
    }
}
