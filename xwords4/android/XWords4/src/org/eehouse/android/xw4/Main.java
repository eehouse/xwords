/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2014 - 2016 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View.OnClickListener;
import android.view.View;

public class Main extends Activity {

    @Override
    public void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );

        if ( BuildConfig.DEBUG && getResources().getBoolean(R.bool.dualpane_enabled) ) {
            setContentView( R.layout.main );

            findViewById( R.id.activity_button )
                .setOnClickListener( new OnClickListener() {
                        @Override
                        public void onClick( View v ) {
                            startActivity( GamesListActivity.class );
                        }
                    } );
            findViewById( R.id.fragment_button )
                .setOnClickListener( new OnClickListener() {
                        @Override
                        public void onClick( View v ) {
                            startActivity( FragActivity.class );
                        }
                    } );
        } else {
            startActivity( GamesListActivity.class );
            finish();
        }
    }

    private void startActivity( Class clazz )
    {
        startActivity( new Intent( this, clazz ) );
    }
}
