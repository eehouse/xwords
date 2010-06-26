/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

package org.eehouse.android.xw4;

import android.view.View;

import org.eehouse.android.xw4.jni.*;

public class Toolbar {

    private View m_horLayout;
    private View m_vertLayout;
    private JNIThread m_jniThread;

    private enum ORIENTATION { ORIENT_UNKNOWN,
            ORIENT_PORTRAIT,
            ORIENT_LANDSCAPE,
            };
    private ORIENTATION m_curOrient = ORIENTATION.ORIENT_UNKNOWN;

    public Toolbar( JNIThread jniThread, View horLayout, 
                    View vertLayout )
    {
        m_jniThread = jniThread;
        m_horLayout = horLayout;
        m_vertLayout = vertLayout;
    }

    public void orientChanged( boolean landscape )
    {
        if ( landscape && m_curOrient == ORIENTATION.ORIENT_LANDSCAPE ) {
            // do nothing
        } else if ( !landscape && m_curOrient == ORIENTATION.ORIENT_PORTRAIT ) {
            // do nothing
        } else {
            if ( landscape ) {
                m_curOrient = ORIENTATION.ORIENT_LANDSCAPE;
                m_horLayout.setVisibility( View.GONE );
                m_vertLayout.setVisibility( View.VISIBLE );
            } else {
                m_curOrient = ORIENTATION.ORIENT_PORTRAIT;
                m_horLayout.setVisibility( View.VISIBLE );
                m_vertLayout.setVisibility( View.GONE );
            }
        }
    }

}
