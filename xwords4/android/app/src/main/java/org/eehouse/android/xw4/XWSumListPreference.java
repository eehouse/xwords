/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2016 by Eric House (xwords@eehouse.org).  All rights
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

    private static final int[] s_ADDROWS = {
        R.string.game_summary_field_npackets,
        R.string.game_summary_field_rowid,
        R.string.game_summary_field_gameid,
        R.string.title_addrs_pref,
        R.string.game_summary_field_created,
    };

    public XWSumListPreference( Context context, AttributeSet attrs )
    {
        super( context, attrs );
    }

    // Why I exist: insert the rowid and gameid lines if debug is on
    @Override
    public void onAttached()
    {
        super.onAttached();

        CharSequence[] entries = getEntries();
        CharSequence[] newEntries = LocUtils.xlateStrings( m_context, entries );
        if ( ! newEntries.equals( entries ) ) {
            setEntries( newEntries );
            setEntryValues( newEntries );
        }

        if ( BuildConfig.DEBUG || XWPrefs.getDebugEnabled( m_context ) ) {
            entries = getEntries();
            CharSequence lastRow = entries[entries.length - 1];
            boolean done = false;

            String[] addRows = new String[s_ADDROWS.length];
            for ( int ii = 0; !done && ii < s_ADDROWS.length; ++ii ) {
                String addRow = LocUtils.getString( m_context, s_ADDROWS[ii] );
                done = lastRow.equals( addRow );
                addRows[ii] = addRow;
            }

            if ( !done ) {
                newEntries = new CharSequence[entries.length + addRows.length];
                System.arraycopy( entries, 0, newEntries, 0,
                                  entries.length );
                System.arraycopy( addRows, 0, newEntries, entries.length,
                                  addRows.length );
                setEntries( newEntries );

                setEntryValues( newEntries );
            }
        }
    }

}
