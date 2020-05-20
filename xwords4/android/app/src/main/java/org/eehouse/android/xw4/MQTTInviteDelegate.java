/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2012 - 2020 by Eric House (xwords@eehouse.org).  All rights
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

package org.eehouse.android.xw4;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import org.eehouse.android.xw4.DBUtils.SentInvitesInfo;
import org.eehouse.android.xw4.jni.XwJNI;

public class MQTTInviteDelegate extends DevIDInviteDelegate {
    private static final String TAG = MQTTInviteDelegate.class.getSimpleName();
    private static final String RECS_KEY = TAG + "/recs";
    private static final boolean MQTTINVITE_SUPPORTED
        = BuildConfig.DEBUG || !BuildConfig.IS_TAGGED_BUILD;

    private String m_devIDStr;

    public static void launchForResult( Activity activity, int nMissing,
                                        SentInvitesInfo info,
                                        RequestCode requestCode )
    {
        if ( MQTTINVITE_SUPPORTED ) {
            Intent intent =
                InviteDelegate.makeIntent( activity, MQTTInviteActivity.class,
                                           nMissing, info );
            activity.startActivityForResult( intent, requestCode.ordinal() );
        }
    }

    public MQTTInviteDelegate( Delegator delegator, Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState );
    }

    @Override
    String getRecsKey()
    {
        return RECS_KEY;
    }

    @Override
    String getMeDevID()
    {
        if ( null == m_devIDStr ) {
            m_devIDStr = XwJNI.dvc_getMQTTDevID(null);
        }
        return m_devIDStr;
    }
}
