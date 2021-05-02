/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009-2012 by Eric House (xwords@eehouse.org).  All rights
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

import android.os.Bundle;
import android.view.Window;

public class DwnldActivity extends XWActivity {

    @Override
    protected void onCreate( Bundle savedInstanceState )
    {
        requestWindowFeature( Window.FEATURE_NO_TITLE );
        requestWindowFeature( Window.FEATURE_LEFT_ICON );
        getWindow().setFeatureDrawableResource( Window.FEATURE_LEFT_ICON,
                                                R.drawable.icon48x48 );

        DwnldDelegate dlgt =
            new DwnldDelegate( this, savedInstanceState );
        super.onCreate( savedInstanceState, dlgt, false );
    }
}
