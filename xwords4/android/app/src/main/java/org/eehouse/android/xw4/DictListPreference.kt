/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2024 by Eric House (xwords@eehouse.org).  All
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
import android.text.TextUtils
import android.util.AttributeSet

import org.eehouse.android.xw4.loc.LocUtils

class DictListPreference(private val mContext: Context, attrs: AttributeSet?) :
    XWListPreference(mContext, attrs)
{
    private val TAG: String = DictListPreference::class.java.simpleName

    init {
        setEntriesForLang()
    }

    fun invalidate() {
        val values = setEntriesForLang()
        value = values[0]
    }

    private fun setEntriesForLang(): Array<String> {
        var curLang = XWPrefs.getPrefsString(mContext, R.string.key_default_language, null)
        if (TextUtils.isEmpty(curLang)) {
            curLang = LocUtils.getString(mContext, R.string.lang_name_english)
        }
        var isoCode = DictLangCache.getLangIsoCode(mContext, curLang)
        if (null == isoCode) { // work around crash reported via Play Store
            isoCode = Utils.ISO_EN
        }

        val dals = DictUtils.dictList(mContext)
        val dictEntries = ArrayList<String>()
        val values = ArrayList<String>()
        dals!!.map {
            if (isoCode!!.equals(DictLangCache.getDictISOCode(mContext, it))) {
                values.add(it.name)
                dictEntries.add(DictLangCache.annotatedDictName(mContext, it))
            }
        }
        entries = dictEntries.toTypedArray<String>()
        val result = values.toTypedArray<String>()
        entryValues = result
        return result
    }
}
