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
import android.widget.ArrayAdapter;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;

public class NewWithKnowns extends LinearLayout implements OnItemSelectedListener
{
    public interface OnNameChangeListener {
        void onNewName( String name );
    }

    private OnNameChangeListener mListener;

    public NewWithKnowns( Context cx, AttributeSet as )
    {
        super( cx, as );
    }

    void setOnNameChangeListener( OnNameChangeListener listener )
    {
        Assert.assertTrueNR( null == mListener );
        mListener = listener;
    }

    void setNames( String[] knowns, String gameName )
    {
        ArrayAdapter<String> adapter = new
            ArrayAdapter<String>( getContext(),
                                  android.R.layout.simple_spinner_item,
                                  knowns );
        adapter.setDropDownViewResource( android.R.layout
                                         .simple_spinner_dropdown_item );
        Spinner spinner = (Spinner)findViewById( R.id.names );
        spinner.setAdapter( adapter );
        spinner.setOnItemSelectedListener( this );

        EditText et = (EditText)findViewById( R.id.name_edit );
        et.setText( gameName );
    }

    String gameName()
    {
        EditText et = (EditText)findViewById( R.id.name_edit );
        return et.getText().toString();
    }

    @Override
    public void onItemSelected( AdapterView<?> parent, View view,
                                int pos, long id )
    {
        OnNameChangeListener listener = mListener;
        if ( null != listener && view instanceof TextView ) {
            TextView tv = (TextView)view;
            listener.onNewName( tv.getText().toString() );
        }
    }

    @Override
    public void onNothingSelected( AdapterView<?> parent ) {}
}
