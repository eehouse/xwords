/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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

public class XWSumListPreference extends XWListPreference {

    public XWSumListPreference( Context context, AttributeSet attrs )
    {
        super( context, attrs );
    }

    // Why I exist: insert the rowid line if debug is on
    protected void onAttachedToActivity()
    {
        super.onAttachedToActivity();

        if ( XWPrefs.getDebugEnabled( m_context ) ) {
            CharSequence[] entries = getEntries();
            String lastRow
                = m_context.getString( R.string.game_summary_field_rowid );
            if ( !entries[entries.length - 1].equals( lastRow ) ) {
                CharSequence[] newEntries = 
                    new CharSequence[1 + entries.length];
                System.arraycopy( entries, 0, newEntries, 0, 
                                  entries.length );
                newEntries[entries.length] = lastRow;
                setEntries( newEntries );

                setEntryValues( newEntries );
            }
        }
    }

}
