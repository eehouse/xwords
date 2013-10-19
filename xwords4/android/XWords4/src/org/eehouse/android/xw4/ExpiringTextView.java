/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2012 by Eric House (xwords@eehouse.org).  All rights
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

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.util.AttributeSet;
import android.widget.TextView;

class ExpiringTextView extends TextView {
    private ExpiringDelegate m_delegate = null;
    private Context m_context;
    private Drawable m_origDrawable;
    protected boolean m_selected = false;

    public ExpiringTextView( Context context, AttributeSet attrs )
    {
        super( context, attrs );
        m_context = context;
    }

    public void setPct( Handler handler, boolean haveTurn, 
                        boolean haveTurnLocal, long startSecs )
    {
        if ( null == m_delegate ) {
            m_delegate = new ExpiringDelegate( m_context, this, handler );
        }
        setPct( haveTurn, haveTurnLocal, startSecs );
    }

    public void setPct( boolean haveTurn, boolean haveTurnLocal, 
                        long startSecs )
    {
        if ( null != m_delegate ) {
            m_delegate.configure( haveTurn, haveTurnLocal, startSecs );
        }
    }

    protected void toggleSelected()
    {
        m_selected = !m_selected;
        if ( m_selected ) {
            m_origDrawable = getBackground();
            setBackgroundColor( XWApp.SEL_COLOR );
        } else {
            setBackgroundDrawable( m_origDrawable );
        }
    }

    @Override
    protected void onDraw( Canvas canvas ) 
    {
        super.onDraw( canvas );
        if ( null != m_delegate ) {
            m_delegate.onDraw( canvas );
        }
    }
}
