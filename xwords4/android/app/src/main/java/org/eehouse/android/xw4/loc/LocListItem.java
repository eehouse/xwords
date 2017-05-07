/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
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

import android.content.Context;
import android.graphics.Color;
import android.util.AttributeSet;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.eehouse.android.xw4.R;

public class LocListItem extends LinearLayout {

    protected static final int LOCAL_COLOR = Color.argb( 0xFF, 0x7f, 0x00, 0x00 );

    private LocSearcher.Pair m_pair;
    private int m_position;
    private TextView m_xlated;

    public LocListItem( Context cx, AttributeSet as )
    {
        super( cx, as );
    }

    @Override
    protected void onFinishInflate()
    {
        super.onFinishInflate();
        m_xlated = (TextView)findViewById( R.id.xlated_view );
    }

    protected void update()
    {
        setXlated();
    }

    protected String getKey()
    {
        return m_pair.getKey();
    }

    private void setEnglish()
    {
        TextView tv = (TextView)findViewById( R.id.english_view );
        tv.setText( getKey() );
    }

    private void setXlated()
    {
        boolean local = false;
        String key = getKey();
        String xlation = LocUtils.getLocalXlation( getContext(), key, true );
        if ( null != xlation ) {
            local = true;
        } else {
            xlation = LocUtils.getBlessedXlation( getContext(), key, true );
        }

        m_pair.setXlation( xlation );
        m_xlated.setText( xlation );
        if ( local ) {
            m_xlated.setTextColor( LOCAL_COLOR );
        }
    }

    protected static LocListItem create( Context context,
                                         LocSearcher.Pair pair,
                                         int position )
    {
        LocListItem result =
            (LocListItem)LocUtils.inflate( context, R.layout.loc_list_item );
        result.m_pair = pair;
        result.m_position = position;

        result.setEnglish();
        result.setXlated();
        return result;
    }
}
