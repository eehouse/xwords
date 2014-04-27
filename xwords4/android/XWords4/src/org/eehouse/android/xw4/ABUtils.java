/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
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

package org.eehouse.android.xw4;

import android.app.Activity;
import android.content.Context;
import android.view.ViewConfiguration;

public class ABUtils {
    private static int s_sdkVersion = 
        Integer.valueOf( android.os.Build.VERSION.SDK );

    private static interface SafeInvalOptionsMenu {
        public void doInval( Activity activity );
    }
    private static class SafeInvalOptionsMenuImpl 
        implements SafeInvalOptionsMenu {
        public void doInval( Activity activity ) {
            activity.invalidateOptionsMenu();
        }
    }
    private static SafeInvalOptionsMenu s_safeInval = null;

    private static interface SafeHasMenuKey {
        public boolean hasMenuKey( Context context );
    }
    private static class SafeHasMenuKeyImpl 
        implements SafeHasMenuKey {
        public boolean hasMenuKey( Context context )
        {
            return ViewConfiguration.get(context).hasPermanentMenuKey();
        }
    }
    private static SafeHasMenuKey s_safeHas = null;

    static {
        if ( 11 <= s_sdkVersion ) {
            s_safeInval = new SafeInvalOptionsMenuImpl();
        }
        if ( 14 <= s_sdkVersion ) {
            s_safeHas = new SafeHasMenuKeyImpl();
        }
    }

    public static void invalidateOptionsMenuIf( Activity activity )
    {
        if ( null != s_safeInval ) {
            s_safeInval.doInval( activity );
        }
    }

    public static boolean haveActionBar()
    {
        return null != s_safeInval;
    }

    // http://stackoverflow.com/questions/10929579/how-to-check-if-android-phone-has-hardware-menu-button-in-android-2-1:
    // If SDK <= 10, assume yes; >= 14, use the API; in the middle,
    // assume no
    public static boolean haveMenuKey( Context context )
    {
        boolean result;
        if ( s_sdkVersion <= 10 ) {
            result = true;
        } else if ( s_sdkVersion < 14 ) {
            result = false;
        } else {
            result = s_safeHas.hasMenuKey( context );
        }
        return result;
    }

}
