/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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


public class ABUtils {
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
    static {
        int sdkVersion = Integer.valueOf( android.os.Build.VERSION.SDK );
        if ( 11 <= sdkVersion ) {
            s_safeInval = new SafeInvalOptionsMenuImpl();
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

}
