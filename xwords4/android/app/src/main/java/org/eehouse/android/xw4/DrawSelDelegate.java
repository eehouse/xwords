/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
/*
 * Copyright 2009-2014 by Eric House (xwords@eehouse.org).  All
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

import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;

public class DrawSelDelegate {
    private View m_view;
    private Drawable m_origDrawable;
    private static ColorDrawable s_selDrawable =
        new ColorDrawable( XWApp.SEL_COLOR );

    protected DrawSelDelegate( View view )
    {
        m_view = view;
    }

    protected void showSelected( boolean selected )
    {
        if ( selected ) {
            m_origDrawable = m_view.getBackground();
            m_view.setBackgroundDrawable( s_selDrawable );
        } else {
            m_view.setBackgroundDrawable( m_origDrawable );
        }
    }
}
