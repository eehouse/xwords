/* -*- compile-command: "find-and-gradle.sh inXw4dDebug"; -*- */
/*
 * Copyright 2017 by Eric House (xwords@eehouse.org).  All rights reserved.
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

import android.app.AlertDialog;
import android.content.Context;

import org.eehouse.android.xw4.DlgDelegate.ActionPair;

public class NotAgainAlert extends DlgDelegateAlert {
    private static final String TAG = NotAgainAlert.class.getSimpleName();

    public static NotAgainAlert newInstance( DlgState state )
    {
        NotAgainAlert result = new NotAgainAlert();
        result.addStateArgument( state );
        return result;
    }

    public NotAgainAlert() {}

    @Override
    public void populateBuilder( Context context, DlgState state,
                                 AlertDialog.Builder builder )
    {
        NotAgainView naView = addNAView( state, builder );
        builder.setTitle( R.string.newbie_title )
            .setPositiveButton( android.R.string.ok,
                                mkCallbackClickListener( naView ) );

        if ( null != state.m_pair ) {
            ActionPair pair = state.m_pair;
            builder.setNegativeButton( pair.buttonStr,
                                       mkCallbackClickListener( pair, naView ) );
        }
    }
}
