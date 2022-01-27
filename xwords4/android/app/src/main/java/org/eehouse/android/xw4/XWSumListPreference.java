/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2022 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

package org.eehouse.android.xw4;

import android.content.Context;
import android.util.AttributeSet;

import org.eehouse.android.xw4.loc.LocUtils;

public class XWSumListPreference extends XWListPreference {

    private static int[] _s_game_summary_values = {
        R.string.game_summary_field_empty,
        R.string.game_summary_field_language,
        R.string.game_summary_field_opponents,
        R.string.game_summary_field_state,
    };

    private static int[] _s_game_summary_values_dbg = {
        R.string.game_summary_field_npackets,
        R.string.game_summary_field_rowid,
        R.string.game_summary_field_gameid,
        R.string.title_addrs_pref,
        R.string.game_summary_field_created,
    };

    private static int[] s_game_summary_values = null;
    public static int[] getFieldIDs( Context context )
    {
        if ( null == s_game_summary_values ) {
            int len = _s_game_summary_values.length;
            boolean addDbg = BuildConfig.NON_RELEASE
                || XWPrefs.getDebugEnabled( context );
            if ( addDbg ) {
                len += _s_game_summary_values_dbg.length;
            }
            s_game_summary_values = new int[len];
            int ii = 0;
            for ( int id : _s_game_summary_values ) {
                s_game_summary_values[ii++] = id;
            }
            if ( addDbg ) {
                for ( int id : _s_game_summary_values_dbg ) {
                    s_game_summary_values[ii++] = id;
                }
            }
        }
        return s_game_summary_values;
    }

    public XWSumListPreference( Context context, AttributeSet attrs )
    {
        super( context, attrs );
    }

    // Why I exist: insert the rowid and gameid lines if debug is on
    @Override
    public void onAttached()
    {
        super.onAttached();

        int[] ids = getFieldIDs( m_context );
        CharSequence[] rows = new String[ids.length];
        for ( int ii = 0; ii < ids.length; ++ii ) {
            rows[ii] = LocUtils.getString( m_context, ids[ii] );
        }

        setEntries( rows );
        setEntryValues( rows );
    }
}
