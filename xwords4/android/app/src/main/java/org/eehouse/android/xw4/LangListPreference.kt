/*
 * Copyright 2010 - 2024 by Eric House (xwords@eehouse.org).  All rights
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

import android.content.Context
import android.os.Handler
import android.util.AttributeSet
import androidx.preference.Preference

class LangListPreference(private val mContext: Context, attrs: AttributeSet?)
    : XWListPreference(mContext, attrs),
      Preference.OnPreferenceChangeListener
{
    private val TAG: String = LangListPreference::class.java.simpleName
    private val mKey = mContext.getString(R.string.key_default_language)

    override fun onAttached() {
        super.onAttached()
        onPreferenceChangeListener = this
        setupLangPref()
    }

    override fun onPreferenceChange(preference: Preference, newValue: Any): Boolean {
        val newLang = newValue as String
        Handler().post { forceDictsMatch(newLang) }
        return true
    }

    private fun setupLangPref() {
        val keyLangs = mContext.getString(R.string.key_default_language)
        val value = value
        var curLang = value?.toString()
        var haveDictForLang = false

        val langs = DictLangCache.listLangs(mContext)
        val langsLoc = arrayOfNulls<String>(langs.size)
        for (ii in langs.indices) {
            val lang = langs[ii]
            haveDictForLang = haveDictForLang || lang == curLang
            langsLoc[ii] = lang
        }

        if (!haveDictForLang) {
            curLang = DictLangCache.getLangNameForISOCode(mContext, Utils.ISO_EN)!!
            setValue(curLang)
        }
        forceDictsMatch(curLang)

        entries = langsLoc
        setDefaultValue(curLang)
        entryValues = langs
    }

    private fun forceDictsMatch(newLang: String?)
    {
        if (null != newLang) {
            val isoCode = DictLangCache.getLangIsoCode(mContext, newLang)
            val keyIds = intArrayOf(
                R.string.key_default_dict,
                R.string.key_default_robodict
            )
            for (id in keyIds) {
                val key = mContext.getString(id)

                val mgr = preferenceManager
                Assert.assertNotNull(mgr)

                val pref = mgr.findPreference<Preference>(key) as DictListPreference?
                Assert.assertNotNull(pref)

                val curDict = pref!!.value.toString()
                if (!DictUtils.dictExists(mContext, curDict)
                    || !isoCode!!.equals(
                        DictLangCache.getDictISOCode(
                            mContext,
                            curDict
                        )
                    )
                ) {
                    pref.invalidate()
                }
            }
        }
    }
}
