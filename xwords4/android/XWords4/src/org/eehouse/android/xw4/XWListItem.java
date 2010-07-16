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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

package org.eehouse.android.xw4;

import android.widget.LinearLayout;
import android.view.View;
import android.widget.TextView;
import android.widget.ImageButton;
import android.content.Context;
import android.util.AttributeSet;
import android.graphics.Rect;

public class XWListItem extends LinearLayout {
    private int m_position;
    private ImageButton m_button;
    private Context m_context;
    DeleteCallback m_cb;

    public interface DeleteCallback {
        void deleteCalled( int myPosition );
    }

    public XWListItem( Context cx, AttributeSet as ) {
        super( cx, as );
        m_context = cx;
    }

    public int getPosition() { return m_position; }
    public void setPosition( int indx ) { m_position = indx; }

    public void setText( String text )
    {
        TextView view = (TextView)getChildAt( 0 );
        view.setText( text );
    }

    public void setDeleteCallback( DeleteCallback cb ) 
    {
        m_cb = cb;
        ImageButton button = (ImageButton)getChildAt( 1 );
        button.setOnClickListener( new View.OnClickListener() {
                @Override
                    public void onClick( View view ) {
                    m_cb.deleteCalled( m_position );
                }
            } );
        button.setVisibility( View.VISIBLE );
    }
}
