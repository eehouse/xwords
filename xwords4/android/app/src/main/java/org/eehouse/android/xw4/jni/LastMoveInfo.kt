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
package org.eehouse.android.xw4.jni

import android.content.Context
import android.text.TextUtils

import org.eehouse.android.xw4.Log
import org.eehouse.android.xw4.R
import org.eehouse.android.xw4.loc.LocUtils

class LastMoveInfo {
    var isValid: Boolean = false // modified in jni world
    var inDuplicateMode: Boolean = false
    var names: Array<String>? = null
    var moveType: Int = 0
    var score: Int = 0
    var nTiles: Int = 0
    var word: String? = null

    fun format(context: Context): String?
    {
        val result =
            if (isValid) {
                val names = names!!
                when (moveType) {
                    ASSIGN_TYPE -> {
                        if (inDuplicateMode) {
                            LocUtils.getString(context, R.string.lmi_tiles_dup)
                        } else {
                            LocUtils.getString(context, R.string.lmi_tiles_fmt, names[0])
                        }
                    }

                    MOVE_TYPE -> {
                        if (0 == nTiles) {
                            // Nobody scoring in dup mode is usually
                            // followed automatically by a trade. So this first will
                            // be rare.
                            if (inDuplicateMode)
                                LocUtils.getString(context, R.string.lmi_pass_dup)
                            else LocUtils.getString(context, R.string.lmi_pass_fmt, names[0])
                        } else if (inDuplicateMode) {
                            if (names.size == 1) {
                                LocUtils.getString(
                                    context, R.string.lmi_move_one_dup_fmt,
                                    names[0], word, score
                                )
                            } else {
                                val joiner = LocUtils.getString(context, R.string.name_concat_dup)
                                val players = TextUtils.join(joiner, names)
                                LocUtils.getString(
                                    context, R.string.lmi_move_tie_dup_fmt,
                                    players, score, word
                                )
                            }
                        } else {
                            LocUtils.getQuantityString(
                                context, R.plurals.lmi_move_fmt,
                                score, names[0], word, score
                            )
                        }
                    }

                    TRADE_TYPE -> if (inDuplicateMode)
                                      LocUtils.getString(context, R.string.lmi_trade_dup_fmt, nTiles)
                                  else LocUtils.getQuantityString(
                                           context, R.plurals.lmi_trade_fmt,
                                           nTiles, names[0], nTiles
                                       )

                    PHONY_TYPE -> LocUtils.getString(
                                      context, R.string.lmi_phony_fmt,
                                      names[0]
                                  )
                    else -> null
                }                   // when
            } else null
        Log.d(TAG, "format() => $result")
        return result
    }

    companion object {
        private val TAG = LastMoveInfo::class.java.getSimpleName()
        // Keep in sync with StackMoveType in movestak.h
        private const val ASSIGN_TYPE = 0
        private const val MOVE_TYPE = 1
        private const val TRADE_TYPE = 2
        private const val PHONY_TYPE = 3
    }
}
