/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2019 by Eric House (xwords@eehouse.org).  All rights reserved.
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
import android.widget.EditText;
import android.widget.SearchView;

import java.util.HashSet;
import java.util.Set;

public class EditWClear extends SearchView
    implements SearchView.OnQueryTextListener {
    private static final String TAG = EditWClear.class.getSimpleName();

    private Set<TextWatcher> mWatchers;
    private EditText mEdit;

    public interface TextWatcher {
        void onTextChanged( String newText );
    }

    public EditWClear( Context context, AttributeSet as )
    {
        super( context, as );
    }

    @Override
    protected void onFinishInflate()
    {
        mEdit = (EditText)Utils.getChildInstanceOf( this, EditText.class );
    }

    synchronized void addTextChangedListener( TextWatcher proc )
    {
        if ( null == mWatchers ) {
            mWatchers = new HashSet<>();
            setOnQueryTextListener( this );
        }
        mWatchers.add( proc );
    }

    void setText( String txt )
    {
        super.setQuery( txt, false );
    }

    CharSequence getText()
    {
        return super.getQuery();
    }

    void insertBlank( String blank )
    {
        // I'm not confident I'll always be able to get the edittext, so to be
        // safe....
        if ( null == mEdit ) {
            setQuery( getQuery() + blank, false );
        } else {
            mEdit.getText().insert(mEdit.getSelectionStart(), blank );
        }
    }

    // from SearchView.OnQueryTextListener
    @Override
    public synchronized boolean onQueryTextChange( String newText )
    {
        for ( TextWatcher proc : mWatchers ) {
            proc.onTextChanged( newText );
        }
        return true;
    }

    // from SearchView.OnQueryTextListener
    @Override
    public boolean onQueryTextSubmit( String query )
    {
        Assert.assertFalse( BuildConfig.DEBUG );
        return true;
    }
}
