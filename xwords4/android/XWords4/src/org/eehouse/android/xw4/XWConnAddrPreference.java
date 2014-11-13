/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2010 - 2014 by Eric House (xwords@eehouse.org).  All
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

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.preference.DialogPreference;
import android.util.AttributeSet;
import android.view.View;
import android.widget.CheckBox;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.CompoundButton;
import android.widget.LinearLayout;

import org.eehouse.android.xw4.loc.LocUtils;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;

public class XWConnAddrPreference extends DialogPreference {

    private int m_flags;
    private CommsConnTypeSet m_curSet;
    private Context m_context;
    // This stuff probably belongs in CommsConnType
    private static CommsConnTypeSet s_supported;
    static {
        s_supported = new CommsConnTypeSet();
        s_supported.add( CommsConnType.COMMS_CONN_RELAY );
        s_supported.add( CommsConnType.COMMS_CONN_BT );
        s_supported.add( CommsConnType.COMMS_CONN_SMS );
    }

    public XWConnAddrPreference( Context context, AttributeSet attrs )
    {
        super( context, attrs );
        m_context = context;

        // setWidgetLayoutResource( R.layout.conn_types_display );
        setDialogLayoutResource( R.layout.conn_types_display );

        setNegativeButtonText( LocUtils.getString( context, R.string.button_cancel ) );

        m_flags = XWPrefs.getPrefsInt( context, R.string.key_addrs_pref, 0 );
    }

    @Override
    protected void onBindDialogView( View view )
    {
        LocUtils.xlateView( m_context, view );

        LinearLayout list = (LinearLayout)view.findViewById( R.id.conn_types );
        m_curSet = DBUtils.intToConnTypeSet( m_flags );
        for ( CommsConnType typ : s_supported.getTypes() ) {
            CheckBox box = (CheckBox)LocUtils.inflate( m_context, R.layout.btinviter_item );
            box.setText( typ.longName() );
            box.setChecked( m_curSet.contains( typ ) );
            list.addView( box );
            
            final CommsConnType typf = typ;
            box.setOnCheckedChangeListener( new OnCheckedChangeListener() {
                    public void onCheckedChanged( CompoundButton buttonView, 
                                                  boolean isChecked )
                    {
                        if ( isChecked ) {
                            m_curSet.add( typf );
                        } else {
                            m_curSet.remove( typf );
                        }
                    }
                } );
        }
    }
    
    @Override
    public void onClick( DialogInterface dialog, int which )
    {
        if ( AlertDialog.BUTTON_POSITIVE == which ) {
            DbgUtils.logf( "ok pressed" );
            m_flags = DBUtils.connTypeSetToInt( m_curSet );
            XWPrefs.setPrefsInt( m_context, R.string.key_addrs_pref, m_flags );
        }
        super.onClick( dialog, which );
    }
}
