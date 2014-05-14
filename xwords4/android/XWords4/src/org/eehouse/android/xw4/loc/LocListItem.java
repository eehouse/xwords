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
import android.widget.EditText;
import android.view.View;
import android.view.View.OnFocusChangeListener;

import junit.framework.Assert;

import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.Utils;
import org.eehouse.android.xw4.DbgUtils;

public class LocListItem extends LinearLayout
    implements View.OnClickListener {

    private LocSearcher.Pair m_pair;
    private int m_position;
    private TextView m_xlated;
    private String m_xlation;

    public LocListItem( Context cx, AttributeSet as ) 
    {
        super( cx, as );
    }

    @Override
    protected void onFinishInflate()
    {
        super.onFinishInflate();
        m_xlated = (TextView)findViewById( R.id.xlated_view ); 
        setOnClickListener( this );
    }

    private void setEnglish()
    {
        TextView tv = (TextView)findViewById( R.id.english_view );
        tv.setText( m_pair.getKey() );
    }

    private void setXlated()
    {
        m_xlation = LocUtils.getXlation( getContext(), m_pair.getKey(), true );
        if ( null != m_xlation ) {
            m_pair.setXlation( m_xlation );
            m_xlated.setText( m_xlation );
        }
    }

    //////////////////////////////////////////////////
    // View.OnClickListener interface
    //////////////////////////////////////////////////
    @Override
    public void onClick( View view )
    {
        LocItemEditDelegate.launch( getContext(), m_pair );
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
