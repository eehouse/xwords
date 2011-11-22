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

import android.widget.LinearLayout;
import android.view.View;
import android.widget.TextView;
import android.widget.ImageButton;
import android.content.Context;
import android.util.AttributeSet;
import android.graphics.Rect;

public class XWListItem extends LinearLayout {
    private int m_position;
    private Context m_context;
    private Object m_cached;
    DeleteCallback m_cb;

    public interface DeleteCallback {
        void deleteCalled( XWListItem item );
    }

    public XWListItem( Context cx, AttributeSet as ) {
        super( cx, as );
        m_context = cx;
    }

    public int getPosition() { return m_position; }
    public void setPosition( int indx ) { m_position = indx; }

    public void setText( String text )
    {
        TextView view = (TextView)findViewById( R.id.text_item );
        view.setText( text );
    }

    public String getText()
    {
        TextView view = (TextView)findViewById( R.id.text_item );
        return view.getText().toString();
    }

    public void setComment( String text )
    {
        TextView view = (TextView)findViewById( R.id.text_item2 );
        view.setVisibility( View.VISIBLE );
        view.setText( text );
    }

    public void setDeleteCallback( DeleteCallback cb ) 
    {
        m_cb = cb;
        ImageButton button = (ImageButton)findViewById( R.id.del );
        button.setOnClickListener( new View.OnClickListener() {
                @Override
                public void onClick( View view ) {
                    m_cb.deleteCalled( XWListItem.this );
                }
            } );
        button.setVisibility( View.VISIBLE );
    }

    @Override
    public void setEnabled( boolean enabled ) 
    {
        ImageButton button = (ImageButton)findViewById( R.id.del );
        button.setEnabled( enabled );
        // calling super here means the list item can't be opened for
        // the user to inspect data.  Might want to reconsider this.
        // PENDING
        super.setEnabled( enabled );
    }

    // I can't just extend an object used in layout -- get a class
    // cast exception when inflating it and casting to the subclass.
    // So rather than create a subclass that knows about its purpose
    // I'll extend this with a general mechanism.  Hackery but ok.
    public void cache( Object obj )
    {
        m_cached = obj;
    }

    public Object getCached()
    {
        return m_cached;
    }

}
