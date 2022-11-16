/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2020 by Eric House (xwords@eehouse.org).  All rights reserved.
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
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import org.eehouse.android.xw4.loc.LocUtils;

public class LimSelGroup extends LinearLayout
    implements OnCheckedChangeListener {
    private static final String TAG = LimSelGroup.class.getSimpleName();

    private int mLimit;
    private InviteView.ItemClicked mProcs;

    public LimSelGroup( Context context, AttributeSet as )
    {
        super( context, as );
    }

    LimSelGroup setLimit( int limit )
    {
        Log.d( TAG, "setLimit(limit=%d)", limit );
        Assert.assertTrueNR( 0 < limit );
        mLimit = limit;
        return this;
    }

    void setCallbacks( InviteView.ItemClicked procs )
    {
        mProcs = procs;
    }

    String[] getSelected()
    {
        String[] result = null;
        int len = mChecked.size();
        if ( 0 < len ) {
            result = new String[mChecked.size()];
            for ( int ii = 0; ii < result.length; ++ii ) {
                result[ii] = mChecked.get(ii).getText().toString();
            }
        }
        return result;
    }

    LimSelGroup addPlayers( String[] names )
    {
        Context context = getContext();
        for ( String name : names ) {
            CompoundButton button;
            if ( 1 == mLimit ) {
                button = (RadioButton)LocUtils.inflate( context, R.layout.invite_radio );
            } else {
                button = (CheckBox)LocUtils.inflate( context, R.layout.invite_checkbox );
            }
            button.setText( name );
            button.setOnCheckedChangeListener( this );
            addView( button );
        }
        return this;
    }

    @Override
    public void onCheckedChanged( CompoundButton buttonView, boolean isChecked )
    {
        Log.d( TAG, "onCheckedChanged(%s, %b)", buttonView, isChecked );
        addToSet( buttonView, isChecked );
        if ( null != mProcs ) {
            mProcs.checkButton();
        }
    }

    ArrayList<CompoundButton> mChecked = new ArrayList<>();
    private void addToSet( CompoundButton button, boolean nowChecked )
    {
        for ( Iterator<CompoundButton> iter = mChecked.iterator();
              iter.hasNext(); ) {
            CompoundButton but = iter.next();
            if ( nowChecked ) {
                Assert.assertTrueNR( ! but.equals(button) );
            } else if ( but.equals(button) ) {
                iter.remove();
            }
        }
        if ( nowChecked ) {
            mChecked.add( button );
            while ( mLimit < mChecked.size() ) {
                CompoundButton oldButton = mChecked.remove( 0 );
                oldButton.setChecked( false );
            }
        }
    }
}
