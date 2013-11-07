/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2013 by Eric House (xwords@eehouse.org).  All
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
import android.graphics.Bitmap;
import android.graphics.Rect;

public class ThumbCanvas extends BoardCanvas {

    public ThumbCanvas( Activity activity, Bitmap bitmap )
    {
        super( activity, bitmap, null );
        DbgUtils.logf( "creating new ThumbCanvas" );
    }

    // These should not be needed if common code gets fixed!  So the
    // whole class should go away. PENDING
    @Override
    public boolean scoreBegin( Rect rect, int numPlayers, int[] scores, 
                               int remCount )
    {
        return false;
    }

    @Override
    public boolean trayBegin( Rect rect, int owner, int score ) 
    {
        return false;
    }

}
