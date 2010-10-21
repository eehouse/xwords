/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
/*
 * Copyright 2010 by Eric House (xwords@eehouse.org).  All rights
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
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.view.LayoutInflater;
import android.net.Uri;
import junit.framework.Assert;
import android.view.View;
import android.widget.TextView;
import android.app.AlertDialog;
import android.os.Bundle;

public class XWActivity extends Activity {

    public static final int DIALOG_ABOUT = 1;
    public static final int DIALOG_OKONLY = 2;
    public static final int DIALOG_NOTAGAIN = 3;
    public static final int DIALOG_LAST = DIALOG_NOTAGAIN;

    public static Bundle s_dialogBundle = null;

    @Override
    protected void onStart()
    {
        Utils.logf( "XWActivity::onStart()" );
        super.onStart();
        DispatchNotify.SetRunning( this );
    }

    @Override
    protected void onStop()
    {
        Utils.logf( "XWActivity::onStop()" );
        super.onStop();
        DispatchNotify.ClearRunning( this );
    }

    @Override
    protected Dialog onCreateDialog( int id )
    {
        return onCreateDialog( this, id );
    }

    public static Dialog onCreateDialog( Context context, int id )
    {
        Dialog dialog = null;
        switch( id ) {
        case DIALOG_ABOUT:
            dialog = doAboutDialog( context );
            break;
        case DIALOG_OKONLY:
            dialog = doOKDialog( context );
            break;
        case DIALOG_NOTAGAIN:
            break;
        }
        return dialog;
    }

    private static Dialog doAboutDialog( final Context context )
    {
        LayoutInflater factory = LayoutInflater.from( context );
        final View view = factory.inflate( R.layout.about_dlg, null );
        TextView vers = (TextView)view.findViewById( R.id.version_string );
        vers.setText( String.format( context.getString(R.string.about_versf), 
                                     XWConstants.VERSION_STR, 
                                     GitVersion.VERS ) );

        TextView xlator = (TextView)view.findViewById( R.id.about_xlator );
        String str = context.getString( R.string.xlator );
        if ( str.length() > 0 ) {
            xlator.setText( str );
        } else {
            xlator.setVisibility( View.GONE );
        }

        return new AlertDialog.Builder( context )
            .setIcon( R.drawable.icon48x48 )
            .setTitle( R.string.app_name )
            .setView( view )
            .setPositiveButton( R.string.changes_button,
                                new DialogInterface.OnClickListener() {
                                    @Override
                                    public void onClick( DialogInterface dlg, 
                                                         int which )
                                    {
                                        FirstRunDialog.show( context, true );
                                    }
                                } )
            .create();
    }

    private static Dialog doOKDialog( final Context context )
    {
        Bundle bundle = s_dialogBundle;
        Assert.assertTrue( null  != bundle );
        int msgID = bundle.getInt( "msgID" );
        return new AlertDialog.Builder( context )
            .setTitle( R.string.info_title )
            .setMessage( msgID )
            .setPositiveButton( R.string.button_ok, null )
            .create();
    }

    public static void setDialogBundle( Bundle bundle )
    {
        s_dialogBundle = bundle;
    }

}
