/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;

import org.eehouse.android.xw4.jni.XwJNI;

public class ThumbCanvas extends BoardCanvas {

    public ThumbCanvas( Context context, Bitmap bitmap )
    {
        super( context, bitmap );
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

    // Unlike superclass, where the game was loaded on the main thread
    // but dictChanged() is called on the JNI thread, this instance
    // will have been created on the same background thread that's
    // calling us.  So don't switch threads for the dict_getChars()
    // call
    @Override
    public void dictChanged( long dictPtr )
    {
        if ( 0 != dictPtr ) {
            m_fontDims = null;
            m_dictChars = XwJNI.dict_getChars( dictPtr );
        }
    }
}
