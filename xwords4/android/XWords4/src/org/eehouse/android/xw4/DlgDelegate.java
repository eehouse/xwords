/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
/*
 * Copyright 2009-2010 by Eric House (xwords@eehouse.org).  All
 * rights reserved.
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

import org.eehouse.android.xw4.jni.CommonPrefs;


public class DlgDelegate {

    public static final int DIALOG_ABOUT = 1;
    public static final int DIALOG_OKONLY = 2;
    public static final int DIALOG_NOTAGAIN = 3;
    public static final int DIALOG_LAST = DIALOG_NOTAGAIN;

    private int m_msgID;
    private Runnable m_proc = null;
    private int m_prefsKey;
    private Activity m_activity;

    public DlgDelegate( Activity activity ) {
        m_activity = activity;
    }
    
    public Dialog onCreateDialog( int id )
    {
        Dialog dialog = null;
        switch( id ) {
        case DIALOG_ABOUT:
            dialog = doAboutDialog();
            break;
        case DIALOG_OKONLY:
            dialog = doOKDialog();
            break;
        case DIALOG_NOTAGAIN:
            dialog = doNotAgainDialog();
            break;
        }
        return dialog;
    }

    public void showOKOnlyDialog( int msgID )
    {
        m_msgID = msgID;
        m_activity.showDialog( DIALOG_OKONLY );
    }

    public void showAboutDialog()
    {
        m_activity.showDialog( DIALOG_ABOUT );
    }

    public void showNotAgainDlgThen( int msgID, int prefsKey,
                                     Runnable proc )
    {
        boolean set = CommonPrefs.getPrefsBoolean( m_activity, prefsKey, false );
        if ( set ) {
            proc.run();
        } else {
            m_msgID = msgID;
            m_proc = proc;
            m_prefsKey = prefsKey;
            m_activity.showDialog( DIALOG_NOTAGAIN );
        }
    }

    private Dialog doAboutDialog()
    {
        LayoutInflater factory = LayoutInflater.from( m_activity );
        final View view = factory.inflate( R.layout.about_dlg, null );
        TextView vers = (TextView)view.findViewById( R.id.version_string );
        vers.setText( String.format( m_activity.getString(R.string.about_versf), 
                                     XWConstants.VERSION_STR, 
                                     GitVersion.VERS ) );

        TextView xlator = (TextView)view.findViewById( R.id.about_xlator );
        String str = m_activity.getString( R.string.xlator );
        if ( str.length() > 0 ) {
            xlator.setText( str );
        } else {
            xlator.setVisibility( View.GONE );
        }

        return new AlertDialog.Builder( m_activity )
            .setIcon( R.drawable.icon48x48 )
            .setTitle( R.string.app_name )
            .setView( view )
            .setPositiveButton( R.string.changes_button,
                                new DialogInterface.OnClickListener() {
                                    @Override
                                    public void onClick( DialogInterface dlg, 
                                                         int which )
                                    {
                                        FirstRunDialog.show( m_activity, true );
                                    }
                                } )
            .create();
    }

    private Dialog doOKDialog()
    {
        return new AlertDialog.Builder( m_activity )
            .setTitle( R.string.info_title )
            .setMessage( m_msgID )
            .setPositiveButton( R.string.button_ok, null )
            .create();
    }

    private Dialog doNotAgainDialog()
    {
        DialogInterface.OnClickListener lstnr_p = 
            new DialogInterface.OnClickListener() {
                public void onClick( DialogInterface dlg, int item ) {
                    if ( null != m_proc ) {
                        m_proc.run();
                    }
                }
            };

        DialogInterface.OnClickListener lstnr_n = 
            new DialogInterface.OnClickListener() {
                public void onClick( DialogInterface dlg, int item ) {
                    CommonPrefs.setPrefsBoolean( m_activity, m_prefsKey, true );
                    if ( null != m_proc ) {
                        m_proc.run();
                    }
                }
            };

        return new AlertDialog.Builder( m_activity )
            .setTitle( R.string.info_title )
            .setMessage( m_msgID )
            .setPositiveButton( R.string.button_ok, lstnr_p )
            .setNegativeButton( R.string.button_notagain, lstnr_n )
            .create();
    }

    // public void setDialogMsgID( int msgID )
    // {
    //     m_msgID = msgID;
    // }

    // public void setDialogRunnable( Runnable runnable )
    // {
    //     m_dialogRunnable = runnable;
    // }

    // public void setDialogPrefsKey( int prefsKey )
    // {
    //     m_prefsKey = prefsKey;
    // }

}
