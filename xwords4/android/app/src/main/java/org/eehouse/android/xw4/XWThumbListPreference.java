/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2011 by Eric House (xwords@eehouse.org).  All
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

package org.eehouse.android.xw4;

import android.content.Context;
import android.util.AttributeSet;

import org.eehouse.android.xw4.loc.LocUtils;

public class XWThumbListPreference extends XWListPreference {
    private Context m_context;

    public XWThumbListPreference( Context context, AttributeSet attrs )
    {
        super( context, attrs );
        m_context = context;
    }

    // Why I exist: insert the rowid and gameid lines if debug is on
    @Override
    public void onAttached()
    {
        super.onAttached();

        CharSequence[] newEntries = new CharSequence[7];
        newEntries[0] = LocUtils.getString( m_context, R.string.thumb_off );
        CharSequence[] newEntryValues = new CharSequence[7];
        newEntryValues[0] = "0";
        String suffix = LocUtils.getString( m_context, R.string.pct_suffix );
        for ( int ii = 1; ii < newEntries.length; ++ii ) {
            int pct = 15 + (ii * 5);
            newEntries[ii] = String.format( "%d%s", pct, suffix );
            newEntryValues[ii] = String.format( "%d", pct );
        }
        setEntries( newEntries );
        setEntryValues( newEntries );
    }
}
