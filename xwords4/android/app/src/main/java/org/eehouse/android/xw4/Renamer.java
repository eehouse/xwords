/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009-2011 by Eric House (xwords@eehouse.org).  All
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
import android.text.method.KeyListener;
import android.util.AttributeSet;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.eehouse.android.xw4.loc.LocUtils;

public class Renamer extends LinearLayout {

    private Context m_context;

    public Renamer( Context cx, AttributeSet as ) {
        super( cx, as );
        m_context = cx;
    }

    public Renamer setLabel( String label )
    {
        TextView view = (TextView)findViewById( R.id.name_label );
        view.setText( label );
        return this;
    }

    public Renamer setLabel( int id )
    {
        setLabel( LocUtils.getString( getContext(), id ) );
        return this;
    }

    public Renamer setName( String text )
    {
        getEdit().setText( text );
        return this;
    }

    public String getName()
    {
        return getEdit().getText().toString();
    }

    private EditWClear getEdit()
    {
        EditWClear view = (EditWClear)findViewById( R.id.name_edit );
        return view;
    }
}
