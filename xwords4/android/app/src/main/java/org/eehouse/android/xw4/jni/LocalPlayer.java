/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009-2010 by Eric House (xwords@eehouse.org).  All
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

package org.eehouse.android.xw4.jni;

import android.content.Context;
import android.text.TextUtils;
import java.io.Serializable;

import org.eehouse.android.xw4.Assert;
import org.eehouse.android.xw4.BuildConfig;
import org.eehouse.android.xw4.Log;
import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.Utils;

public class LocalPlayer implements Serializable {
    private static final String TAG = LocalPlayer.class.getSimpleName();
    public String name;
    public String password;
    public String dictName;
    public int secondsUsed;
    public int robotIQ;
    public boolean isLocal;

    public LocalPlayer( Context context, int num )
    {
        isLocal = true;
        robotIQ = 0;            // human
        name = CommonPrefs.getDefaultPlayerName( context, num, true );
        password = "";

        // Utils.testSerialization( this );
    }

    public LocalPlayer( final LocalPlayer src )
    {
        isLocal = src.isLocal;
        robotIQ = src.robotIQ;
        name = src.name;
        password = src.password;
        dictName = src.dictName;
        secondsUsed = src.secondsUsed;

        // Utils.testSerialization( this );
    }

    @Override
    public boolean equals( Object obj )
    {
        boolean result;
        if ( BuildConfig.DEBUG ) {
            LocalPlayer other = null;
            result = null != obj && obj instanceof LocalPlayer;
            if ( result ) {
                other = (LocalPlayer)obj;
                result = secondsUsed == other.secondsUsed
                    && robotIQ == other.robotIQ
                    && isLocal == other.isLocal
                    && TextUtils.equals( name, other.name )
                    && TextUtils.equals( password, other.password )
                    && TextUtils.equals( dictName, other.dictName )
                    ;
            }
        } else {
            result = super.equals( obj );
        }
        return result;
    }

    public boolean isRobot()
    {
        return robotIQ > 0;
    }

    public void setIsRobot( boolean isRobot )
    {
        robotIQ = isRobot ? 1 : 0;
    }

    public void setRobotSmartness( int iq )
    {
        Assert.assertTrue( iq > 0 );
        robotIQ = iq;
    }

    @Override
    public String toString()
    {
        String result = BuildConfig.DEBUG
            ? String.format( "{name: %s, isLocal: %b, robotIQ: %d}",
                             name, isLocal, robotIQ )
            : super.toString();
        return result;
    }
}
