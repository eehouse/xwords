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

import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.Button;
import android.view.LayoutInflater;
import java.util.HashMap;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;

public class Toolbar {

    private LinearLayout m_horLayout;
    private LinearLayout m_vertLayout;

    private enum ORIENTATION { ORIENT_UNKNOWN,
            ORIENT_PORTRAIT,
            ORIENT_LANDSCAPE,
            };
    private ORIENTATION m_curOrient = ORIENTATION.ORIENT_UNKNOWN;

    public Toolbar( View horLayout, View vertLayout )
    {
        m_horLayout = (LinearLayout)horLayout;
        m_vertLayout = (LinearLayout)vertLayout;
    }

    public void orientChanged( boolean landscape )
    {
        if ( landscape && m_curOrient == ORIENTATION.ORIENT_LANDSCAPE ) {
            // do nothing
        } else if ( !landscape && m_curOrient == ORIENTATION.ORIENT_PORTRAIT ) {
            // do nothing
        } else {
            LinearLayout prevLayout, nextLayout;
            if ( landscape ) {
                m_curOrient = ORIENTATION.ORIENT_LANDSCAPE;
                prevLayout = m_horLayout;
                nextLayout = m_vertLayout;
            } else {
                m_curOrient = ORIENTATION.ORIENT_PORTRAIT;
                prevLayout = m_vertLayout;
                nextLayout = m_horLayout;
            }

            prevLayout.setVisibility( View.GONE );
            nextLayout.setVisibility( View.VISIBLE );
        }
    }
}
