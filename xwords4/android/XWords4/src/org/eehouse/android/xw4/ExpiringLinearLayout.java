/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2012 by Eric House (xwords@eehouse.org).  All
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
import android.graphics.Canvas;
import android.util.AttributeSet;
import android.widget.LinearLayout;

public class ExpiringLinearLayout extends LinearLayout {
    private ExpiringDelegate m_delegate;
    private Context m_context;

    public ExpiringLinearLayout( Context context, AttributeSet as ) {
        super( context, as );
        m_context = context;
    }

    public void setPct( int pct, boolean haveTurn, boolean haveTurnLocal )
    {
        m_delegate = new ExpiringDelegate( m_context, this, pct, haveTurn, 
                                           haveTurnLocal );
    }

    @Override
    // not called unless setWillNotDraw( false ) called
    protected void onDraw( Canvas canvas ) 
    {
        super.onDraw( canvas );
        m_delegate.onDraw( canvas );
    }
}
