/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009 - 2012 by Eric House (xwords@eehouse.org).  All
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

import android.app.Activity;
import android.content.Context;
import android.preference.CheckBoxPreference;
import android.util.AttributeSet;
import android.view.View;

import org.eehouse.android.xw4.DlgDelegate.Action;

public class SMSCheckBoxPreference extends CheckBoxPreference {

    private Context m_context;
    private boolean m_attached = false;
    private static SMSCheckBoxPreference s_this = null;

    public SMSCheckBoxPreference( Context context, AttributeSet attrs )
    {
        super( context, attrs );
        m_context = context;
        s_this = this;
    }

    @Override
    protected void onAttachedToActivity()
    {
        super.onAttachedToActivity();
        if ( !XWApp.SMSSUPPORTED || !Utils.deviceSupportsSMS( m_context ) ) {
            setEnabled( false );
        }
        m_attached = true;
    }

    @Override
    public void setChecked( boolean checked )
    {
        if ( checked && m_attached && m_context instanceof PrefsActivity ) {
            PrefsActivity activity = (PrefsActivity)m_context;
            activity.showSMSEnableDialog( Action.ENABLE_SMS_DO );
        } else {
            super.setChecked( checked );
        }
    }

    protected static void setChecked()
    {
        if ( null != s_this ) {
            s_this.super_setChecked( true );
        }
    }

    // Because s_this.super.setChecked() isn't allowed...
    private void super_setChecked( boolean checked )
    {
        super.setChecked( checked );
    }
}
