/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2014 - 2024 by Eric House (xwords@eehouse.org).  All rights
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
package org.eehouse.android.xw4.loc

import android.app.Activity
import android.app.AlertDialog
import android.content.Context
import android.text.TextUtils
import android.view.LayoutInflater
import android.view.Menu
import android.view.View

import java.util.Locale

import org.eehouse.android.xw4.BuildConfig
import org.eehouse.android.xw4.DbgUtils
import org.eehouse.android.xw4.Log
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.XWApp
import org.eehouse.android.xw4.XWPrefs

object LocUtils {
    private val TAG: String = LocUtils::class.java.simpleName
    internal const val RES_FORMAT: String = "%[\\d]\\$[ds]"
    private var s_curLocale: String? = null
    private var s_curLang: ISOCode? = null
    private val s_langMap: Map<String, String>? = null

    // Problem: prefs are saved as strings, which may be localized, so when
    // you change locales you can't make sense of the preference. This method
    // takes an array of possible values (all localizable strings) and gets
    // the current value then compares against the set. If it's not there,
    // meaning the pref setting isn't knowable, the default is returned.
    //
    // Obviously I *should* be storing stringIDs rather than strings, but
    // don't see a way in the prefs system to do that, and can't find anybody
    // else complaining about the problem.
    private fun getCheckPref(
        context: Context, values: Array<String>, key: Int, default: Int
    ): String {
        var curVal = XWPrefs.getPrefsString(context, key, default)
        if (!values.contains(curVal)) {
            Log.d(TAG, "getCheckPref(): val $curVal not "
                  + "in ${DbgUtils.fmtAny(values)}")
            curVal = getString(context, default)!!
            Log.d(TAG, "getCheckPref(): swapped in $curVal")
            XWPrefs.setPrefsString(context, key, curVal)
        }
        return curVal!!
    }

    fun getCheckPref(
        context: Context, valuesKey: Int, key: Int, default: Int
    ): String? {
        val arr = getStringArray(context, valuesKey)
            .filterNotNull()    // should never happen
            .toTypedArray()
        return getCheckPref(context, arr, key, default)
    }

    fun inflate(context: Context, resID: Int): View {
        val factory = LayoutInflater.from(context)
        return factory.inflate(resID, null)
    }

    fun xlateTitle(activity: Activity?) {
    }

    fun xlateView(activity: Activity?) {
    }

    fun xlateView(context: Context, view: View?) {
    }

    fun xlateMenu(activity: Activity?, menu: Menu?) {
    }

    private fun xlateString(context: Context, str: CharSequence?): String? {
        var result: String? = null
        if (null != str) {
            result = xlateString(context, str.toString())
        }
        return result
    }

    private fun xlateString(
        context: Context, str: String,
        associate: Boolean
    ): String {
        return str
    }

    fun xlateString(context: Context, str: String): String {
        var str = str
        if (BuildConfig.LOCUTILS_ENABLED) {
            str = xlateString(context, str, true)
        }
        return str
    }

    fun getStringArray(context: Context, resID: Int): Array<String?> {
        val res = context.resources
        val arr = res.getStringArray(resID)
        return xlateStrings(context, arr)
    }

    fun xlateStrings(context: Context, strs: Array<String>): Array<String?> {
        val result = arrayOfNulls<String>(strs.size)
        for (ii in strs.indices) {
            result[ii] = xlateString(context, strs[ii].toString())
        }
        return result
    }

    fun getString(context: Context, id: Int): String {
        return context.getString(id)
    }

    fun getStringOrNull(id: Int): String? {
        var result: String? = null
        if (0 != id) {
            result = getString(XWApp.getContext(), true, id)
        }
        return result
    }

    fun getString(context: Context, canUseDB: Boolean, id: Int): String {
        return getString(context, id)
    }

    fun getString(context: Context, id: Int, vararg params: Any?): String {
        return context.getString(id, *params)
    }

    fun getQuantityString(
        context: Context, id: Int,
        quantity: Int
    ): String {
        val result = context.resources.getQuantityString(id, quantity)
        return result
    }

    fun getQuantityString(
        context: Context, id: Int,
        quantity: Int, vararg params: Any?
    ): String {
        val result = context.resources
            .getQuantityString(id, quantity, *params)
        return result
    }

    internal fun getCurLocaleName(context: Context): String {
        val locale_code = getCurLocale(context)
        val locale = Locale(locale_code)
        val name = locale.getDisplayLanguage(locale)
        return name
    }

    fun getCurLangCode(context: Context): ISOCode? {
        if (null == s_curLang) {
            var lang = Locale.getDefault().language

            // sometimes I get "en-us", i.e. the locale's there too. Strip it.
            if (lang.contains("-")) {
                lang = TextUtils.split(lang, "-")[0]
            }
            // Sometimes getLanguage() returns "". Let's just fall back to
            // English for now.
            if (TextUtils.isEmpty(lang)) {
                lang = "en"
            }
            s_curLang = ISOCode(lang)
        }
        return s_curLang
    }

    fun getCurLocale(context: Context): String {
        if (null == s_curLocale) {
            s_curLocale = Locale.getDefault().toString()
        }
        return s_curLocale!!
    }

    private fun xlateView(
        context: Context, contextName: String,
        view: View, depth: Int
    ) {
    }

    fun makeAlertBuilder(context: Context): AlertDialog.Builder {
        return AlertDialog.Builder(context)
    }
}
