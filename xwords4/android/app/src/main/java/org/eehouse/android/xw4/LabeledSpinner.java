/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2020 by Eric House (xwords@eehouse.org).  All
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
import android.view.View;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;


/**
 * This class's purpose is to link a spinner with a textview that's its label
 * such that clicking on the label is the same as clicking on the spinner.
 */

public class LabeledSpinner extends LinearLayout {
    private Spinner mSpinner;

    public LabeledSpinner( Context context, AttributeSet as ) {
        super( context, as );
    }

    @Override
    protected void onFinishInflate()
    {
        mSpinner = (Spinner)Utils.getChildInstanceOf( this, Spinner.class );

        TextView tv = (TextView)Utils.getChildInstanceOf( this, TextView.class );
        tv.setOnClickListener( new OnClickListener() {
                @Override
                public void onClick( View target )
                {
                    mSpinner.performClick();
                }
            } );
    }

    public Spinner getSpinner()
    {
        Assert.assertNotNull( mSpinner );
        return mSpinner;
    }
}
