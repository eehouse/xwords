/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2014 by Eric House (xwords@eehouse.org).  All rights
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

import android.content.Intent;
import android.widget.Button;
import android.app.Activity;
import android.view.View;
import android.view.View.OnClickListener;
import android.os.Bundle;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentTransaction;

public class Main extends Activity {

    @Override
    public void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );
        setContentView( R.layout.main );

        DbgUtils.logf( "Main.onCreate() called" );

        Button button;
        button = (Button)findViewById( R.id.activity_button );
        button.setOnClickListener( new OnClickListener() {
                @Override
                public void onClick( View v ) {
                    startActivity( GamesListActivity.class );
                }
            } );
        button = (Button)findViewById( R.id.fragment_button );
        button.setOnClickListener( new View.OnClickListener() {
                @Override
                public void onClick( View v ) {
                    startActivity( FragActivity.class );
                }
            } );
    }

    private void startActivity( Class clazz )
    {
        startActivity( new Intent( this, clazz ) );
    }

}
