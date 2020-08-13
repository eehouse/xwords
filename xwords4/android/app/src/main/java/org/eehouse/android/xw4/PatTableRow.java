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
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.TableRow;

import org.eehouse.android.xw4.jni.XwJNI.PatDesc;

public class PatTableRow extends TableRow {
    private static final String TAG = PatTableRow.class.getSimpleName();
    private EditText mEdit;
    private CheckBox mCheck;

    public PatTableRow( Context context, AttributeSet as )
    {
        super( context, as );
    }

    public void getToDesc( PatDesc out )
    {
        getFields();

        // PatDesc result = null;
        String strPat = mEdit.getText().toString();
        out.strPat = strPat;
        out.anyOrderOk = mCheck.isChecked();
        // if ( null != strPat && 0 < strPat.length() ) {
        //     result = new PatDesc();
        //     result.strPat = strPat;
        //     result.anyOrderOk = mCheck.isChecked();
        // }
        // return result;
    }

    public void setFromDesc( PatDesc desc )
    {
        getFields();

        mEdit.setText(desc.strPat);
        mCheck.setChecked(desc.anyOrderOk);
    }

    public boolean addBlankToFocussed( String blank )
    {
        getFields();

        boolean handled = mEdit.hasFocus();
        if ( handled ) {
            mEdit.getText().insert(mEdit.getSelectionStart(), blank );
        }
        return handled;
    }

    private void getFields()
    {
        mEdit = (EditText)Utils.getChildInstanceOf( this, EditText.class );
        mCheck = (CheckBox)Utils.getChildInstanceOf( this, CheckBox.class );
    }
}
