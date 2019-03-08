/* -*- compile-command: "find-and-gradle.sh insXw4Debug"; -*- */
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
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface.OnClickListener;
import android.os.Bundle;

import org.eehouse.android.xw4.DlgDelegate.ActionPair;
import org.eehouse.android.xw4.loc.LocUtils;

public class ConfirmThenAlert extends DlgDelegateAlert {

    public static ConfirmThenAlert newInstance( DlgState state )
    {
        ConfirmThenAlert result = new ConfirmThenAlert();
        result.addStateArgument( state );
        return result;
    }

    public ConfirmThenAlert() {}

    @Override
    public void populateBuilder( Context context, DlgState state,
                                 AlertDialog.Builder builder,
                                 NotAgainView naView )
    {
        OnClickListener lstnr = mkCallbackClickListener( naView );

        builder.setTitle( state.m_titleId == 0 ? R.string.query_title : state.m_titleId )
            .setPositiveButton( state.m_posButton, lstnr )
            .setNegativeButton( state.m_negButton, lstnr );

        if ( null != state.m_pair ) {
            ActionPair pair = state.m_pair;
            builder.setNeutralButton( pair.buttonStr,
                                      mkCallbackClickListener( pair, naView ) );
        }
    }
}
