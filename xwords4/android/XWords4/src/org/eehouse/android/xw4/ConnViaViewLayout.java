/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2015 by Eric House (xwords@eehouse.org).  All
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
import android.util.AttributeSet;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.CheckBox;
import android.widget.LinearLayout;

import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.loc.LocUtils;

public class ConnViaViewLayout extends LinearLayout {
    private static CommsConnTypeSet s_supported;
    private CommsConnTypeSet m_curSet;

    public ConnViaViewLayout( Context context, AttributeSet as ) {
        super( context, as );
    }

    protected void setTypes( CommsConnTypeSet types )
    {
        m_curSet = (CommsConnTypeSet)types.clone();
        addConnections();
    }

    protected CommsConnTypeSet getTypes()
    {
        return m_curSet;
    }

    private void addConnections()
    {
        LinearLayout list = (LinearLayout)findViewById( R.id.conn_types );
        Context context = getContext();
        CommsConnTypeSet supported = getSupported( context );

        for ( CommsConnType typ : supported.getTypes() ) {
            LinearLayout item = (LinearLayout)
                LocUtils.inflate( context, R.layout.btinviter_item );
            CheckBox box = (CheckBox)item.findViewById( R.id.inviter_check );
            box.setText( typ.longName( context ) );
            box.setChecked( m_curSet.contains( typ ) );
            list.addView( item );
            
            final CommsConnType typf = typ;
            box.setOnCheckedChangeListener( new OnCheckedChangeListener() {
                    public void onCheckedChanged( CompoundButton buttonView, 
                                                  boolean isChecked ) {
                        if ( isChecked ) {
                            m_curSet.add( typf );
                        } else {
                            m_curSet.remove( typf );
                        }
                    }
                } );
        }
    }

    private static CommsConnTypeSet getSupported( Context context )
    {
        if ( null == s_supported ) {
            CommsConnTypeSet supported = new CommsConnTypeSet();
            supported.add( CommsConnType.COMMS_CONN_RELAY );
            if ( BTService.BTAvailable() ) {
                supported.add( CommsConnType.COMMS_CONN_BT );
            }
            if ( Utils.isGSMPhone( context ) ) {
                supported.add( CommsConnType.COMMS_CONN_SMS );
            }
            s_supported = supported;
        }
        return s_supported;
    }
}
