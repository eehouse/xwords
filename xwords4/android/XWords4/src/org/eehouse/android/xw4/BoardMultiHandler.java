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

import android.view.MotionEvent;
import android.util.FloatMath;

// DO NOT LOAD THIS on devices running prior to 2.0.  It'll crash.
public class BoardMultiHandler implements BoardView.MultiHandlerIface {

    private int m_lastSpacing = -1; // negative means not in use
    private int m_threshhold;

    public BoardMultiHandler( int threshhold )
    {
        m_threshhold = threshhold;
    }

    public int getSpacing( MotionEvent event ) 
    {
        int result;
        if ( 1 == event.getPointerCount() ) {
            result = -1;
        } else {
            float xx = event.getX( 0 ) - event.getX( 1 );
            float yy = event.getY( 0 ) - event.getY( 1 );
            result = (int)FloatMath.sqrt( (xx * xx) + (yy * yy) );
        }
        return result;
    }

    public boolean inactive()
    {
        return 0 > m_lastSpacing;
    }

    public void activate( MotionEvent event )
    {
        m_lastSpacing = getSpacing( event );
    }

    public void deactivate()
    {
        m_lastSpacing = -1;
    }

    public int figureZoom( MotionEvent event )
    {
        int zoomDir = 0;
        if ( 0 <= m_lastSpacing ) {
            int newSpacing = getSpacing( event );
            int diff = Math.abs( newSpacing - m_lastSpacing );
            if ( diff > m_threshhold ) {
                zoomDir = newSpacing < m_lastSpacing? -1 : 1;
                m_lastSpacing = newSpacing;
            }
        }
        return zoomDir;
    }
}
