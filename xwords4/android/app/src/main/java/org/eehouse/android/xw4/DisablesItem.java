/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
/*
 * Copyright 2017 by Eric House (xwords@eehouse.org).  All rights reserved.
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

import android.view.View;
import android.app.Activity;
import android.content.Context;
import android.util.AttributeSet;
import android.widget.CheckBox;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.CompoundButton;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;


public class DisablesItem extends LinearLayout {
    private CommsConnType m_type;
    private boolean[] m_state;

    public DisablesItem( Context context, AttributeSet as ) {
        super( context, as );
    }

    void init( CommsConnType typ, boolean[] state )
    {
        if ( BuildConfig.DEBUG ) {
            m_state = state;
            m_type = typ;
            ((TextView)findViewById(R.id.addr_type)).setText( typ.shortName() );

            setupCheckbox( R.id.send, true );
            setupCheckbox( R.id.receive, false );
        }
    }

    private void setupCheckbox( int id, final boolean forSend )
    {
        if ( BuildConfig.DEBUG ) {
            final CheckBox cb = (CheckBox)findViewById( id );
            cb.setChecked( m_state[forSend?1:0] );
            cb.setOnClickListener( new View.OnClickListener() {
                    @Override
                    public void onClick( View view ) {
                        m_state[forSend?1:0] = cb.isChecked();
                    }
                } );
        }
    }
}
