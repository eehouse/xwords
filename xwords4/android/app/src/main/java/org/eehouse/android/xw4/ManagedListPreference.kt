/*
 * Copyright 2026 by Eric House (xwords@eehouse.org).  All
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

// Designed/coded with the help of Gemini, which was actually fairly helpful.

package org.eehouse.android.xw4

import android.content.Context
import android.util.AttributeSet

import org.eehouse.android.xw4.ListPrefsModels.PrefKey
import org.eehouse.android.xw4.ListPrefsModels.PrefsRegistry

private val TAG: String = ManagedListPreference::class.java.simpleName
class ManagedListPreference(context: Context, attrs: AttributeSet?) :
    XWListPreference(context, attrs)
{
    init {
        PrefsRegistry.getDefinition(PrefKey.keyFor(context, this.key)).also { def ->
            entries = def.entries(context)
            entryValues = def.entryValues()
            if (value == null) value = def.default.stableKey
        }
    }
}
