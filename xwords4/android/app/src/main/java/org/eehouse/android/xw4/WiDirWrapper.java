/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
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

import android.app.Activity;
import android.content.Context;

// On older Android we can't load the class WiDirService. So do it with
// try/catch here
public class WiDirWrapper {

    private static boolean s_working = false;

    public static void init( Context context )
    {
        try {
            WiDirService.init( context );
            s_working = true;
        } catch ( java.lang.VerifyError err ) {
        }
    }

    public static boolean enabled()
    {
        return s_working && WiDirService.enabled();
    }

    public static void activityResumed( Activity activity )
    {
        if ( s_working ) {
            WiDirService.activityResumed( activity );
        }
    }

    public static void activityPaused( Activity activity )
    {
        if ( s_working ) {
            WiDirService.activityPaused( activity );
        }
    }
}
