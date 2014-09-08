/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2012 - 2014 by Eric House (xwords@eehouse.org).  All rights
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
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;

import junit.framework.Assert;

import org.eehouse.android.xw4.loc.LocUtils;

public class HeaderWithExpander extends LinearLayout 
    implements View.OnClickListener {

    private boolean m_expanded;
    private ImageButton m_expandButton;
    private TextView m_label;
    private OnExpandedListener m_listener;

    public interface OnExpandedListener {
        public void expanded( boolean expanded );
    }

    public HeaderWithExpander( Context cx, AttributeSet as ) 
    {
        super( cx, as );
    }

    @Override
    protected void onFinishInflate()
    {
        m_expandButton = (ImageButton)findViewById( R.id.expander );
        m_label = (TextView)findViewById( R.id.label );
    }

    public void setExpanded( boolean expanded ) 
    {
        m_expanded = expanded;
        if ( null != m_listener ) {
            m_listener.expanded( m_expanded );
        }
        setButton();
    }

    public void setText( int id )
    {
        String text = LocUtils.getString( getContext(), id );
        m_label.setText( text );
    }

    public void setOnExpandedListener( OnExpandedListener listener )
    {
        setOnClickListener( this );
        m_expandButton.setOnClickListener( this );
        Assert.assertNull( m_listener );
        m_listener = listener;
    }

    //////////////////////////////////////////////////
    // View.OnClickListener interface
    //////////////////////////////////////////////////
    public void onClick( View view ) 
    {
        setExpanded( !m_expanded );
    }

    private void setButton()
    {
        if ( null != m_expandButton ) {
            m_expandButton.setImageResource( m_expanded ?
                                             R.drawable.expander_ic_maximized :
                                             R.drawable.expander_ic_minimized);
        }
    }
}
