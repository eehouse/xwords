/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2014 by Eric House (xwords@eehouse.org).  All rights
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

package org.eehouse.android.xw4.loc;

import android.widget.LinearLayout;
import android.content.Context;
import android.util.AttributeSet;
import android.widget.TextView;

import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.Utils;
import org.eehouse.android.xw4.DbgUtils;

public class LocListItem extends LinearLayout {

    private Context m_context;
    private String m_key;
    private int m_position;

    public LocListItem( Context cx, AttributeSet as ) 
    {
        super( cx, as );
        m_context = cx;
    }

    private void setEnglish()
    {
        int id = LocIDs.get( m_key );
        String str = m_context.getString( id );
        TextView tv = (TextView)findViewById( R.id.english_view );
        tv.setText( str );
        DbgUtils.logf( "setEnglish: set to %s", str );
    }

    protected static LocListItem create( Context context, String key, 
                                         int position )
    {
        LocListItem result = 
            (LocListItem)Utils.inflate( context, R.layout.loc_list_item );
        result.m_key = key;
        result.m_position = position;
        result.setEnglish();
        return result;
    }
}
