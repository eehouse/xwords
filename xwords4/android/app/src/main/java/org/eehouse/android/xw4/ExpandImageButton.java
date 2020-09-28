/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2020 by Eric House (xwords@eehouse.org).  All rights
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

import java.util.HashSet;
import java.util.Set;

public class ExpandImageButton extends ImageButton
    implements View.OnClickListener {
    private boolean m_expanded;
    private Set<ExpandChangeListener> mListeners = new HashSet<>();
    
    public interface ExpandChangeListener {
        public void expandedChanged( boolean nowExpanded );
    }

    public ExpandImageButton( Context context, AttributeSet as )
    {
        super( context, as );
    }

    @Override
    protected void onFinishInflate()
    {
        super.onFinishInflate();
        setOnClickListener( this );

        setImageResource();
    }

    @Override
    public void onClick( View view )
    {
        toggle();
    }

    public ExpandImageButton setExpanded( boolean expanded )
    {
        boolean changed = m_expanded != expanded;
        if ( changed ) {
            m_expanded = expanded;

            setImageResource();

            for ( ExpandChangeListener proc : mListeners ) {
                proc.expandedChanged( m_expanded );
            }
        }
        return this;
    }
    
    public ExpandImageButton setOnExpandChangedListener( final ExpandChangeListener listener )
    {
        mListeners.add( listener );
        return this;
    }

    public void toggle()
    {
        setExpanded( ! m_expanded );
    }

    private void setImageResource()
    {
        setImageResource( m_expanded ?
                          R.drawable.expander_ic_maximized :
                          R.drawable.expander_ic_minimized);
    }
}
