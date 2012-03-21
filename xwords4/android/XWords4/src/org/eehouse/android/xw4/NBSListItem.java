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

import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.content.Context;
import android.util.AttributeSet;
import android.widget.CompoundButton.OnCheckedChangeListener;

public class NBSListItem extends LinearLayout  {
        
    public NBSListItem( Context cx, AttributeSet as ) 
    {
        super( cx, as );
    }

    public void setContents( String name, String number )
    {
        TextView tv = (TextView)findViewById( R.id.name );
        tv.setText( name );
        tv = (TextView)findViewById( R.id.number );
        tv.setText( number );
    }

    public void setOnCheckedChangeListener( OnCheckedChangeListener lstnr )
    {
        CheckBox cb = (CheckBox)findViewById( R.id.checkbox );
        cb.setOnCheckedChangeListener( lstnr );
    }

    public String getNumber()
    {
        TextView tv = (TextView)findViewById( R.id.number );
        return tv.getText().toString();
    }

    public boolean isChecked()
    {
        CheckBox cb = (CheckBox)findViewById( R.id.checkbox );
        return cb.isChecked();
    }
}

