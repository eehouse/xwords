/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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

import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.util.AttributeSet;
import android.view.View;
import androidx.preference.DialogPreference;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.loc.LocUtils;

public class XWConnAddrPreference extends DialogPreference
    implements PrefsActivity.DialogProc {
    private static final String TAG = XWConnAddrPreference.class.getSimpleName();
    private Context m_context;
    private ConnViaViewLayout m_view;

    public XWConnAddrPreference( Context context, AttributeSet attrs )
    {
        super( context, attrs );

        CommsConnTypeSet curSet = XWPrefs.getAddrTypes( context );
        setSummary( curSet.toString( context, true ) );
    }

    @Override
    public XWDialogFragment makeDialogFrag()
    {
        return new XWConnAddrDialogFrag( this );
    }

    public static class XWConnAddrDialogFrag extends XWDialogFragment {
        private XWConnAddrPreference mSelf;

        public XWConnAddrDialogFrag( XWConnAddrPreference self )
        {
            mSelf = self;
        }

        @Override
        public Dialog onCreateDialog( Bundle sis )
        {
            final PrefsActivity activity = (PrefsActivity)getContext();
            View view = LocUtils.inflate( activity, R.layout.conn_types_display );

            final ConnViaViewLayout cvl = (ConnViaViewLayout)view.findViewById( R.id.conn_types );
            cvl.configure( activity.getDelegate(), XWPrefs.getAddrTypes( activity ),
                          new ConnViaViewLayout.CheckEnabledWarner() {
                              @Override
                              public void warnDisabled( CommsConnType typ ) {
                                  String msg = null;
                                  int msgID = 0;
                                  Action action = null;
                                  int buttonID = 0;
                                  switch( typ ) {
                                  case COMMS_CONN_SMS:
                                      msgID = R.string.warn_sms_disabled;
                                      action = Action.ENABLE_NBS_ASK;
                                      buttonID = R.string.button_enable_sms;
                                      break;
                                  case COMMS_CONN_BT:
                                      msgID = R.string.warn_bt_disabled;
                                      action = Action.ENABLE_BT_DO;
                                      buttonID = R.string.button_enable_bt;
                                      break;
                                  case COMMS_CONN_MQTT:
                                      msg = LocUtils
                                          .getString( activity, R.string
                                                      .warn_mqtt_disabled );
                                      msg += "\n\n" + LocUtils
                                          .getString( activity,
                                                      R.string.warn_mqtt_later );
                                      action = Action.ENABLE_MQTT_DO;
                                      buttonID = R.string.button_enable_mqtt;
                                      break;
                                  default:
                                      Assert.failDbg();
                                      break;
                                  }

                                  if ( 0 != msgID ) {
                                      Assert.assertTrueNR( null == msg );
                                      msg = LocUtils.getString( activity, msgID );
                                  }
                                  if ( null != msg ) {
                                      activity.makeConfirmThenBuilder( action, msg )
                                          .setPosButton( buttonID )
                                          .setNegButton( R.string.button_later )
                                          .show();
                                  }
                              }
                          },
                          new ConnViaViewLayout.SetEmptyWarner() {
                              @Override
                              public void typeSetEmpty() {
                                  activity
                                      .makeOkOnlyBuilder( R.string.warn_no_comms )
                                      .show();
                              }
                          }, activity );

            DialogInterface.OnClickListener onOk =
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick( DialogInterface di,
                                         int which )
                    {
                        Log.d( TAG, "onClick()" );
                        CommsConnTypeSet curSet = cvl.getTypes();
                        XWPrefs.setAddrTypes( activity, curSet );
                        mSelf.setSummary( curSet.toString( activity, true ) );
                    }
                };

            return LocUtils.makeAlertBuilder( activity )
                .setTitle( R.string.title_addrs_pref )
                .setView( view )
                .setPositiveButton( android.R.string.ok, onOk )
                .setNegativeButton( android.R.string.cancel, null )
                .create();
        }

        @Override
        protected String getFragTag() { return getClass().getSimpleName(); }
    }
}
