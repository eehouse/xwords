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
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.TableRow;
import android.widget.TextView.OnEditorActionListener;
import android.widget.TextView;

import org.eehouse.android.xw4.jni.XwJNI.PatDesc;

public class PatTableRow extends TableRow implements OnEditorActionListener {
    private static final String TAG = PatTableRow.class.getSimpleName();
    private EditText mEdit;
    private CheckBox mCheck;
    private EnterPressed mEnterProc;

    public interface EnterPressed {
        public boolean enterPressed();
    }

    public PatTableRow( Context context, AttributeSet as )
    {
        super( context, as );
    }

    void setOnEnterPressed( EnterPressed proc ) { mEnterProc = proc; }

    @Override
    protected void onFinishInflate()
    {
        mCheck = (CheckBox)Utils.getChildInstanceOf( this, CheckBox.class );
        mEdit = (EditText)Utils.getChildInstanceOf( this, EditText.class );
        mEdit.setOnEditorActionListener(this);
    }

    @Override
    public boolean onEditorAction( TextView tv, int actionId, KeyEvent event )
    {
        return EditorInfo.IME_ACTION_SEND == actionId
            && null != mEnterProc
            && mEnterProc.enterPressed();
    }

    public void getToDesc( PatDesc out )
    {
        String strPat = mEdit.getText().toString();
        out.strPat = strPat;
        out.anyOrderOk = mCheck.isChecked();
    }

    public void setFromDesc( PatDesc desc )
    {
        mEdit.setText(desc.strPat);
        mCheck.setChecked(desc.anyOrderOk);
    }

    public boolean addBlankToFocussed( String blank )
    {
        boolean handled = mEdit.hasFocus();
        if ( handled ) {
            mEdit.getText().insert(mEdit.getSelectionStart(), blank );
        }
        return handled;
    }

    // Return the label (the first column)
    public String getFieldName()
    {
        String result = "";
        TextView tv = (TextView)Utils.getChildInstanceOf( this, TextView.class );
        Assert.assertTrueNR( null != tv );
        if ( null != tv ) {
            result = tv.getText().toString();
        }
        return result;
    }

    void setOnFocusGained( final Runnable proc )
    {
        mEdit.setOnFocusChangeListener( new View.OnFocusChangeListener() {
                @Override
                public void onFocusChange( View view, boolean hasFocus )
                {
                    if ( hasFocus ) {
                        proc.run();
                    }
                }
            } );
    }

}
