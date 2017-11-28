/* -*- compile-command: "find-and-gradle.sh -PuseCrashlytics insXw4dDeb"; -*- */
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

import android.app.Application;

/**
 * The ancient GCMIntentService I copied from sample code seems to have
 * trouble (burns battery using the WAKELOCK, specifically) when used with an
 * app that doesn't have a registration ID. So let's not use that code.
 */

public class GCMIntentService {
    private static final String TAG = GCMIntentService.class.getSimpleName();

    public static void init( Application app )
    {
        Log.d( TAG, "doing nothing" );
    }
}
