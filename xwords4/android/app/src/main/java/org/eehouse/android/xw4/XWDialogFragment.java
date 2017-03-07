/* -*- compile-command: "find-and-gradle.sh insXw4Debug"; -*- */
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

import android.support.v4.app.DialogFragment;
import android.content.DialogInterface;

class XWDialogFragment extends DialogFragment {
    private OnDismissListener m_onDismiss;

    public interface OnDismissListener {
        void onDismissed( XWDialogFragment frag );
    }

    @Override
    public void onDismiss( DialogInterface dif )
    {
        if ( null != m_onDismiss ) {
            m_onDismiss.onDismissed( this );
        }
        super.onDismiss( dif );
    }
    
    protected void setOnDismissListener( OnDismissListener lstnr )
    {
        m_onDismiss = lstnr;
    }
    
}
