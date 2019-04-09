/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009-2014 by Eric House (xwords@eehouse.org).  All
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
import android.widget.LinearLayout;

import org.eehouse.android.xw4.InviteDelegate.InviterItem;

public class InviterItemFrame extends LinearLayout /*implements InviterItem*/ {
    private InviterItem mItem;

    public InviterItemFrame( Context context, AttributeSet as ) {
        super( context, as );
    }

    void setItem( InviterItem item ) { mItem = item; }
    InviterItem getItem() { return mItem; }

    void setOnCheckedChangeListener( OnCheckedChangeListener listener )
    {
        ((CheckBox)findViewById( R.id.inviter_check ))
            .setOnCheckedChangeListener( listener );
    }

    void setChecked( boolean newVal )
    {
        ((CheckBox)findViewById( R.id.inviter_check ))
            .setChecked( newVal );
    }

    boolean isChecked()
    {
        return ((CheckBox)findViewById( R.id.inviter_check )).isChecked();
    }
}
