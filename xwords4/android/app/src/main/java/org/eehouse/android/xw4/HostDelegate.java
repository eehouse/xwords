/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
/*
 * Copyright 2014-2016 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

import android.os.Bundle;
import android.app.Activity;
import android.content.Intent;

import org.eehouse.android.xw4.DlgDelegate.Action;

import junit.framework.Assert;

class HostDelegate extends DelegateBase {
    private static final String ACTION = "ACTION";
    private static final String IS_POS_BUTTON = "POS_BUTTON";
    private static final String STATE = "STATE";

    private Bundle mArgs;

    public HostDelegate( Delegator delegator, Bundle sis )
    {
        super( delegator, sis, 0 );
        mArgs = delegator.getArguments();
    }

    @Override
    protected void init( Bundle savedInstanceState )
    {
        show( (DlgState)mArgs.getParcelable( STATE ) );
    }

    @Override
    public boolean onPosButton( Action action, Object[] params )
    {
        setResult( action, true );
        return true;
    }

    @Override
    public boolean onNegButton( Action action, Object[] params )
    {
        setResult( action, false );
        return true;
    }

    @Override
    public boolean onDismissed( Action action, Object[] params )
    {
        finish();
        return true;
    }

    private void setResult( Action action, boolean wasPos )
    {
        Intent intent = new Intent();
        intent.putExtra( ACTION, action.ordinal() );
        intent.putExtra( IS_POS_BUTTON, wasPos );
        setResult( Activity.RESULT_OK, intent );
        finish();
    }

    protected static void showForResult( Activity parent, DlgState state )
    {
        Intent intent = new Intent( parent, HostActivity.class );
        intent.putExtra( STATE, state );
        parent.startActivityForResult( intent,
                                       RequestCode.HOST_DIALOG.ordinal() );
    }

    protected static void resultReceived( DlgDelegate.DlgClickNotify target,
                                          RequestCode requestCode,
                                          Intent data )
    {
        Action action = Action.values()[data.getIntExtra(ACTION, -1)];
        if ( data.getBooleanExtra( IS_POS_BUTTON, false ) ) {
            target.onPosButton( action );
        } else {
            target.onNegButton( action );
        }
    }
}
