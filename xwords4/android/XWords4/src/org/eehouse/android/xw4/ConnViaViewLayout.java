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
import android.widget.CheckBox;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.CompoundButton;
import android.widget.LinearLayout;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.HasDlgDelegate;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.loc.LocUtils;

import junit.framework.Assert;

public class ConnViaViewLayout extends LinearLayout {
    private CommsConnTypeSet m_curSet;
    private DlgDelegate.HasDlgDelegate m_dlgDlgt;

    public interface CheckEnabledWarner {
        public void warnDisabled( CommsConnType typ );
    }
    public interface SetEmptyWarner {
        public void typeSetEmpty();
    }

    private CheckEnabledWarner m_disabledWarner;
    private SetEmptyWarner m_emptyWarner;

    public ConnViaViewLayout( Context context, AttributeSet as ) {
        super( context, as );
    }

    protected void configure( CommsConnTypeSet types,
                              CheckEnabledWarner cew, 
                              SetEmptyWarner sew,
                              DlgDelegate.HasDlgDelegate dlgDlgt )
    {
        m_curSet = (CommsConnTypeSet)types.clone();

        addConnections();

        m_disabledWarner = cew;
        m_emptyWarner = sew;
        m_dlgDlgt = dlgDlgt;
    }

    protected CommsConnTypeSet getTypes()
    {
        return m_curSet;
    }

    private void addConnections()
    {
        LinearLayout list = (LinearLayout)findViewById( R.id.conn_types );
        list.removeAllViews();  // in case being reused

        Context context = getContext();
        CommsConnTypeSet supported = CommsConnTypeSet.getSupported( context );

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
                            showNotAgainTypeTip( typf );
                            enabledElseWarn( typf );
                            m_curSet.add( typf );
                        } else {
                            m_curSet.remove( typf );
                            if ( null != m_emptyWarner && 0 == m_curSet.size()) {
                                m_emptyWarner.typeSetEmpty();
                            }
                        }
                    }
                } );
        }
    }

    private void enabledElseWarn( CommsConnType typ )
    {
        boolean enabled = true;
        Context context = getContext();
        switch( typ ) {
        case COMMS_CONN_SMS:
            enabled = XWPrefs.getSMSEnabled( context );
            break;
        case COMMS_CONN_BT:
            enabled = BTService.BTEnabled();
            break;
        case COMMS_CONN_RELAY:
            enabled = RelayService.relayEnabled( context );
            break;
        default:
            Assert.fail();
            break;
        }

        if ( !enabled && null != m_disabledWarner ) {
            m_disabledWarner.warnDisabled( typ );
        }
    }

    private void showNotAgainTypeTip( CommsConnType typ )
    {
        if ( null != m_dlgDlgt ) {
            int keyID = 0;
            int msgID = 0;
            switch( typ ) {
            case COMMS_CONN_RELAY:
                msgID = R.string.not_again_comms_relay;
                keyID = R.string.key_na_comms_relay;
                break;
            case COMMS_CONN_SMS:
                msgID = R.string.not_again_comms_sms;
                keyID = R.string.key_na_comms_sms;
                break;
            case COMMS_CONN_BT:
                msgID = R.string.not_again_comms_bt;
                keyID = R.string.key_na_comms_bt;
                break;
            default:
                Assert.fail();
                break;
            }
            m_dlgDlgt.showNotAgainDlgThen( msgID, keyID, 
                                           DlgDelegate.Action.SKIP_CALLBACK );

        }
    }
}
