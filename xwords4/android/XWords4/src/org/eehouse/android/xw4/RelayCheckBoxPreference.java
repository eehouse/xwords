/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009 - 2012 by Eric House (xwords@eehouse.org).  All
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

import android.app.Activity;
import android.content.Context;
import android.preference.CheckBoxPreference;
import android.util.AttributeSet;
import android.view.View;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.loc.LocUtils;

public class RelayCheckBoxPreference extends ConfirmingCheckBoxPreference {
    private static ConfirmingCheckBoxPreference s_this = null;

    public RelayCheckBoxPreference( Context context, AttributeSet attrs )
    {
        super( context, attrs );
        s_this = this;
    }

    @Override
    protected void checkIfConfirmed() {
        PrefsActivity activity = (PrefsActivity)getContext();
        int count = DBUtils.getRelayGameCount( activity );
        if ( 0 < count ) {
            String msg = LocUtils.getString( activity,
                                             R.string.warn_relay_havegames );
            msg += LocUtils.getQuantityString( activity, R.plurals.warn_relay_games_fmt,
                                               count, count );
            activity.showConfirmThen( msg, R.string.button_disable_relay,
                                      android.R.string.cancel,
                                      Action.DISABLE_RELAY_DO );
        }
    }

    protected static void setChecked()
    {
        if ( null != s_this ) {
            s_this.super_setChecked( true );
        }
    }
}
