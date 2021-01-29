/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2016 by Eric House (xwords@eehouse.org).  All
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
import androidx.preference.CheckBoxPreference;
import android.util.AttributeSet;

public abstract class ConfirmingCheckBoxPreference extends CheckBoxPreference {
    private boolean m_attached = false;

    public ConfirmingCheckBoxPreference( Context context, AttributeSet attrs )
    {
        super( context, attrs );
    }

    @Override
    public void onAttached()
    {
        super.onAttached();
        m_attached = true;
    }

    abstract void checkIfConfirmed();

    @Override
    public void setChecked( boolean checked )
    {
        if ( checked && m_attached && getContext() instanceof PrefsActivity ) {
            checkIfConfirmed();
        } else {
            super.setChecked( checked );
        }
    }

    // Because s_this.super.setChecked() isn't allowed...
    protected void super_setChecked( boolean checked )
    {
        super.setChecked( checked );
    }
}
