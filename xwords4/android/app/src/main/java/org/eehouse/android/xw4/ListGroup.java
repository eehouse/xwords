/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
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
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.eehouse.android.xw4.loc.LocUtils;

public class ListGroup extends LinearLayout
    implements View.OnClickListener {

    private boolean m_expanded;
    private ImageButton m_expandButton;
    private TextView m_text;
    private String m_desc;
    private GroupStateListener m_listener;
    private int m_posn;

    public ListGroup( Context cx, AttributeSet as )
    {
        super( cx, as );
    }

    @Override
    protected void onFinishInflate()
    {
        super.onFinishInflate();
        m_expandButton = (ImageButton)findViewById( R.id.expander );
        m_text = (TextView)findViewById( R.id.game_name );

        m_expandButton.setOnClickListener( this );
        setOnClickListener( this );

        setButtonImage();
        setText();
    }

    protected int getPosition()
    {
        return m_posn;
    }

    //////////////////////////////////////////////////
    // View.OnClickListener interface
    //////////////////////////////////////////////////
    public void onClick( View view )
    {
        m_expanded = !m_expanded;
        m_listener.onGroupExpandedChanged( this, m_expanded );
        setButtonImage();
    }

    private void setButtonImage()
    {
        if ( null != m_expandButton ) {
            m_expandButton.setImageResource( m_expanded ?
                                             R.drawable.expander_ic_maximized :
                                             R.drawable.expander_ic_minimized);
        }
    }

    private void setText()
    {
        if ( null != m_text ) {
            m_text.setText( m_desc );
        }
    }

    public static ListGroup make( Context context, View convertView,
                                  GroupStateListener lstnr, int posn,
                                  String desc, boolean expanded )
    {
        ListGroup result;
        if ( null != convertView && convertView instanceof ListGroup ) {
            result = (ListGroup)convertView;
        } else {
            result = (ListGroup)
                LocUtils.inflate( context, R.layout.list_group );
        }
        result.m_posn = posn;
        result.m_expanded = expanded;
        result.m_desc = desc;
        result.m_listener = lstnr;

        result.setButtonImage();     // in case onFinishInflate already called
        result.setText();

        return result;
    }
}
