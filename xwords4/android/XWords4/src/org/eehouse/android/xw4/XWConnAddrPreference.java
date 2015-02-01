/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2010 - 2015 by Eric House (xwords@eehouse.org).  All
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

import org.eehouse.android.xw4.loc.LocUtils;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;

public class XWConnAddrPreference extends DialogPreference {

    private Context m_context;
    private ConnViaViewLayout m_view;

    public XWConnAddrPreference( Context context, AttributeSet attrs )
    {
        super( context, attrs );
        m_context = context;

        setDialogLayoutResource( R.layout.conn_types_display );

        setNegativeButtonText( LocUtils.getString( context, R.string.button_cancel ) );

        CommsConnTypeSet curSet = XWPrefs.getAddrTypes( context );
        setSummary( curSet.toString( context ) );
    }

    @Override
    protected void onBindDialogView( View view )
    {
        LocUtils.xlateView( m_context, view );
        m_view = (ConnViaViewLayout)view.findViewById( R.id.conn_types );
        m_view.configure( XWPrefs.getAddrTypes( m_context ),
                          new ConnViaViewLayout.CheckEnabledWarner() {
                              public void warnDisabled( CommsConnType typ ) {
                                  int id;
                                  switch( typ ) {
                                  case COMMS_CONN_SMS:
                                      id = R.string.enable_sms_first;
                                      break;
                                  case COMMS_CONN_BT:
                                      id = R.string.enable_bt_first;
                                      break;
                                  default:
                                      id = 0;
                                  }
                                  if ( 0 != id ) {
                                      PrefsActivity activity = (PrefsActivity)m_context;
                                      activity.showOKOnlyDialog( id );
                                  }
                              }
                          },
                          new ConnViaViewLayout.SetEmptyWarner() {
                              public void typeSetEmpty() {
                                  PrefsActivity activity = (PrefsActivity)m_context;
                                  activity.showOKOnlyDialog( R.string.warn_no_comms );
                              }
                          } );
    }
    
    @Override
    public void onClick( DialogInterface dialog, int which )
    {
        if ( AlertDialog.BUTTON_POSITIVE == which ) {
            DbgUtils.logf( "ok pressed" );
            CommsConnTypeSet curSet = m_view.getTypes();
            XWPrefs.setAddrTypes( m_context, curSet );
            setSummary( curSet.toString( m_context ) );
        }
        super.onClick( dialog, which );
    }
}
