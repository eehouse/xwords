/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2014 by Eric House (xwords@eehouse.org).  All rights reserved.
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

import android.view.View;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.widget.Spinner;
import android.widget.ArrayAdapter;

public class StudyList extends XWListActivity {
    private Spinner mSpinner;

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );

        setContentView( R.layout.studylist );

        mSpinner = (Spinner)findViewById( R.id.pick_language );
        int[] langs = DBUtils.studyListLangs( this );
        if ( 0 == langs.length ) {
            finish();
        } else if ( 1 == langs.length ) {
            mSpinner.setVisibility( View.GONE );
        } else {
            String[] names = DictLangCache.getLangNames( this );
            String[] myNames = new String[langs.length];
            for ( int ii = 0; ii < langs.length; ++ii ) {
                myNames[ii] = names[langs[ii]];
            }

            ArrayAdapter<String> adapter = new
                ArrayAdapter<String>( this, 
                                      android.R.layout.simple_spinner_item,
                                      myNames );
            adapter.setDropDownViewResource( android.R.layout.
                                             simple_spinner_dropdown_item );
            mSpinner.setAdapter( adapter );
        }
    }

    public static void launch( Context context )
    {
        Intent intent = new Intent( context, StudyList.class );
        context.startActivity( intent );
    }
}
