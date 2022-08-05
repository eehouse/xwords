/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2022 by Eric House (xwords@eehouse.org).  All
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

import java.lang.ref.WeakReference;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.loc.LocUtils;

public class MQTTCheckBoxPreference extends ConfirmingCheckBoxPreference {
    private static final String TAG = MQTTCheckBoxPreference.class.getSimpleName();
    private static WeakReference<MQTTCheckBoxPreference> s_this = null;

    public MQTTCheckBoxPreference( Context context, AttributeSet attrs )
    {
        super( context, attrs );
        s_this = new WeakReference<>( this );
    }

    @Override
    protected void checkIfConfirmed()
    {
        PrefsActivity activity = (PrefsActivity)getContext();
        String msg = LocUtils.getString( activity,
                                         R.string.warn_mqtt_havegames );

        int count = DBUtils
            .getGameCountUsing( activity, CommsConnType.COMMS_CONN_MQTT );
        if ( 0 < count ) {
            msg += LocUtils.getQuantityString( activity, R.plurals.warn_mqtt_games_fmt,
                                               count, count );
        }
        activity.makeConfirmThenBuilder( Action.DISABLE_MQTT_DO, msg )
            .setPosButton( R.string.button_disable_mqtt )
            .show();
    }

    protected static void setChecked()
    {
        if ( null != s_this ) {
            MQTTCheckBoxPreference self = s_this.get();
            if ( null != self ) {
                self.super_setChecked( true );
            }
        }
    }
}
