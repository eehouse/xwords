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
import android.view.View;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.AdapterView;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;

import java.util.HashMap;
import java.util.Map;

import org.eehouse.android.xw4.jni.XwJNI.RematchOrder;
import org.eehouse.android.xw4.loc.LocUtils;

public class RematchConfigView extends LinearLayout
{
    private Context mContext;
    private RadioGroup mGroup;
    Map<Integer, RematchOrder> mRos = new HashMap<>();

    public RematchConfigView( Context cx, AttributeSet as )
    {
        super( cx, as );
        mContext = cx;
    }

    @Override
    protected void onFinishInflate()
    {
        mGroup = (RadioGroup)findViewById( R.id.group );
        for ( RematchOrder ro : RematchOrder.values() ) {
            RadioButton button =  new RadioButton( mContext );
            button.setText( LocUtils.getString( mContext, ro.getStrID() ) );
            mGroup.addView( button );
            mRos.put( button.getId(), ro );
            if ( 1 == mRos.size() ) {
                button.setChecked( true );
            }
        }
    }

    public RematchOrder onOkClicked()
    {
        int id = mGroup.getCheckedRadioButtonId();
        RematchOrder ro = mRos.get(id);

        // Save it if default button checked
        CheckBox check = (CheckBox)findViewById( R.id.make_default );
        if ( check.isChecked() ) {
            XWPrefs.setDefaultRematchOrder( mContext, ro );
        }

        return ro;
    }
}
