/*
 * Copyright 2026 by Eric House (xwords@eehouse.org).  All rights
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

// Designed/coded with the help of Gemini, which was actually fairly helpful.

package org.eehouse.android.xw4

import android.content.Context
import androidx.annotation.StringRes

private val TAG: String = ListPrefsModels::class.java.simpleName
object ListPrefsModels {
    interface PrefsItem {
        @get:StringRes val visibleResID: Int
        val isDefault: Boolean

        val stableKey: String
            get() = (this as Enum<*>).name

        fun getString(context: Context): String? {
            return context.getString(visibleResID)
        }
    }

    class PrefsDef(entries: List<PrefsItem>) {
        val items: Array<out PrefsItem> = entries.toTypedArray()
        val default: PrefsItem = items.find{it.isDefault}!!

        fun entries(context: Context): Array<String?> {
            return items.map { it.getString(context) }.toTypedArray()
        }
        fun entryValues(): Array<String?> {
            return items.map { it.stableKey }.toTypedArray()
        }
    }

    enum class PrefKey(private val keyID: Int) {
        MARGINS_KEY(R.string.key_gesture_margins),
        KAMENU_KEY(R.string.key_ka_menuwhen),
        SCHEME_KEY(R.string.key_url_scheme),
        REMATCH_ORDER(R.string.key_rematch_order),
        FORCE_TABLET(R.string.key_force_tablet),
        PHONIES(R.string.key_default_phonies),
        ;

        fun getKey(context: Context): String { return context.getString(keyID) }

        companion object {
            fun keyFor(context: Context, key: String): PrefKey {
                return values().find { context.getString(it.keyID).equals(key) }!!
            }
        }
    }

    // Each new ListPreference using this system gets an enum here, like Margins
    enum class Margins(override val visibleResID: Int,
                       override val isDefault: Boolean = false) : PrefsItem {
        SHRINK(R.string.gesture_margins_shrink, true),
        IGNORE(R.string.gesture_margins_ignore)
        ;
    }

    enum class KAWhen(override val visibleResID: Int,
                      override val isDefault: Boolean = false) : PrefsItem {
        NEVER(R.string.ka_menuwhen_never),
        AVAIL(R.string.ka_menuwhen_available, true),
        RUNNING(R.string.ka_menuwhen_running),
        ALWAYS(R.string.ka_menuwhen_always);
    }

    enum class URLScheme(override val visibleResID: Int,
                         override val isDefault: Boolean = false) : PrefsItem {
        DEFAULT(R.string.url_scheme_default, true),
        HTTP(R.string.url_scheme_http),
        HTTPS(R.string.url_scheme_https),
    }

    enum class RematchOrderPref(override val visibleResID: Int,
                                override val isDefault: Boolean = false) : PrefsItem {
        SAME(R.string.ro_same, true),
        LOW_FIRST(R.string.ro_low_score_first),
        HIGH_FIRST(R.string.ro_high_score_first),
        JUGGLE(R.string.ro_juggle),
        ;
    }

    enum class ForceTab(override val visibleResID: Int,
                        override val isDefault: Boolean = false) : PrefsItem {
        DEFAULT(R.string.force_tablet_default, true),
        PHONE(R.string.force_tablet_phone),
        TABLET(R.string.force_tablet_tablet),
        ;
    }

    enum class Phonies(override val visibleResID: Int,
                       override val isDefault: Boolean = false) : PrefsItem {
        IGNORE(R.string.phonies_ignore),
        WARN(R.string.phonies_warn, true),
        DISALLOW(R.string.phonies_disallow),
        BLOCK(R.string.phonies_block),
        ;
    }

    object PrefsRegistry {
        private val registry = mapOf(
            PrefKey.MARGINS_KEY to PrefsDef(Margins.entries),
            PrefKey.KAMENU_KEY to PrefsDef(KAWhen.entries),
            PrefKey.SCHEME_KEY to PrefsDef(URLScheme.entries),
            PrefKey.REMATCH_ORDER to PrefsDef(RematchOrderPref.entries),
            PrefKey.FORCE_TABLET to PrefsDef(ForceTab.entries),
            PrefKey.PHONIES to PrefsDef(Phonies.entries),
        )

        fun getDefinition(key: PrefKey): PrefsDef = registry[key]!!
    }

    private fun updatePrefItem(context: Context, key: PrefKey, item: PrefsItem) {
        XWPrefs.setPrefsString(context, key.getKey(context), item.stableKey)
    }

    fun getPrefItem(context: Context, key: PrefKey): PrefsItem {
        val def = PrefsRegistry.getDefinition(key)
        val keyStr = key.getKey(context)
        val str = XWPrefs.getPrefsString(context, keyStr, def.default.stableKey)
        var result = def.items.find { str.equals(it.stableKey) }
        if (null == result) {
            for (item in def.items) {
                if (context.getString(item.visibleResID).equals(str)) {
                    result = item
                    break
                }
            }
            if (null == result) {
                result = def.default
            }
            updatePrefItem(context, key, result)
        }
        return result!!
    }

    // Iterate through our "managed" data and set any missing values to defaults
    fun setDefaultValues(context: Context, forceRevert: Boolean = false) {
        PrefKey.entries.map { key ->
            val keyStr = key.getKey(context)
            if (forceRevert || null == XWPrefs.getPrefsString(context, keyStr, null)) {
                PrefsRegistry.getDefinition(key).default.let { def ->
                    XWPrefs.setPrefsString(context, keyStr, def.stableKey)
                }
            }
        }
    }

    fun resetAll(context: Context) {
        setDefaultValues(context, true)
    }
}
