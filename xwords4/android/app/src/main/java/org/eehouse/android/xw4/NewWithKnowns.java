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
import android.widget.ArrayAdapter;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.Spinner;

public class NewWithKnowns extends LinearLayout {

    public NewWithKnowns( Context cx, AttributeSet as )
    {
        super( cx, as );
    }

    void setNames( String[] knowns, String gameName )
    {
        final ArrayAdapter<String> adapter =
            new ArrayAdapter<>( getContext(), android.R.layout.simple_spinner_item );
        for ( String msg : knowns ) {
            adapter.add( msg );
        }
        Spinner spinner = (Spinner)findViewById( R.id.names );
        spinner.setAdapter( adapter );

        EditText et = (EditText)findViewById( R.id.name_edit );
        et.setText( gameName );
    }

    String getSelPlayer()
    {
        Spinner spinner = (Spinner)findViewById( R.id.names );
        return spinner.getSelectedItem().toString();
    }

    String gameName()
    {
        EditText et = (EditText)findViewById( R.id.name_edit );
        return et.getText().toString();
    }
}
