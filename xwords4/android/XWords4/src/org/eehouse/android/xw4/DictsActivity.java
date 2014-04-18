/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009 - 2012 by Eric House (xwords@eehouse.org).  All
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
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.ExpandableListActivity;
import android.content.Context;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.database.DataSetObserver;
import android.net.Uri;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.ExpandableListAdapter;
import android.widget.ExpandableListView;
import android.widget.PopupMenu;
import android.widget.TextView;

import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;

import junit.framework.Assert;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DictUtils.DictAndLoc;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.jni.JNIUtilsImpl;
import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.DictUtils.DictLoc;

public class DictsActivity extends ExpandableListActivity {

    private static interface SafePopup {
        public void doPopup( Context context, View button, String curDict );
    }
    private static SafePopup s_safePopup = null;
    // I can't provide a subclass of MenuItem to hold DictAndLoc, so
    // settle for a hash on the side.
    private static HashMap<MenuItem, DictAndLoc> s_itemData;

    private DictsDelegate m_dlgt;

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );
        m_dlgt = new DictsDelegate( this, savedInstanceState );
        m_dlgt.init( savedInstanceState );
    } // onCreate

    @Override
    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = super.onCreateDialog( id );
        if ( null == dialog ) {
            dialog = m_dlgt.createDialog( id );
        }
        return dialog;
    } // onCreateDialog

    @Override
    protected void onPrepareDialog( int id, Dialog dialog )
    {
        super.onPrepareDialog( id, dialog );
        m_dlgt.prepareDialog( id, dialog );
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        m_dlgt.onResume();
    }

    @Override
    protected void onStop() {
        m_dlgt.onStop();
        super.onStop();
    }

    @Override
    public void onBackPressed() {
        if ( !m_dlgt.onBackPressed() ) {
            super.onBackPressed();
        }
    }

    @Override
    public boolean onCreateOptionsMenu( Menu menu )
    {
        return m_dlgt.onCreateOptionsMenu( menu );
    }

    @Override
    public boolean onPrepareOptionsMenu( Menu menu ) 
    {
        return m_dlgt.onPrepareOptionsMenu( menu ) 
            || super.onPrepareOptionsMenu( menu );
    }

    public boolean onOptionsItemSelected( MenuItem item )
    {
        return m_dlgt.onOptionsItemSelected( item )
            || super.onOptionsItemSelected( item );
    }

    private static class SafePopupImpl implements SafePopup {
        public void doPopup( final Context context, View button, 
                             String curDict ) {

            MenuItem.OnMenuItemClickListener listener = 
                new MenuItem.OnMenuItemClickListener() {
                    public boolean onMenuItemClick( MenuItem item )
                    {
                        DictAndLoc dal = s_itemData.get( item );
                        s_itemData = null;

                        if ( null == dal ) {
                            DictsActivity.start( context );
                        } else {
                            DictBrowseDelegate.launch( context, dal.name, 
                                                       dal.loc );
                        }
                        return true;
                    }
                };

            s_itemData = new HashMap<MenuItem, DictAndLoc>();
            PopupMenu popup = new PopupMenu( context, button );
            Menu menu = popup.getMenu();
            menu.add( R.string.show_wordlist_browser )
                .setOnMenuItemClickListener( listener );

            // Add at top but save until have dal info
            MenuItem curItem = 
                menu.add( context.getString( R.string.cur_menu_marker_fmt, 
                                             curDict ) );

            DictAndLoc[] dals = DictUtils.dictList( context );
            for ( DictAndLoc dal : dals ) {
                MenuItem item = dal.name.equals(curDict)
                    ? curItem : menu.add( dal.name );
                item.setOnMenuItemClickListener( listener );
                s_itemData.put( item, dal );
            }
            popup.show();
        }
    }

    public static boolean handleDictsPopup( Context context, View button,
                                            String curDict )
    {
        if ( null == s_safePopup ) {
            int sdkVersion = Integer.valueOf( android.os.Build.VERSION.SDK );
            if ( 11 <= sdkVersion ) {
                s_safePopup = new SafePopupImpl();
            }
        }

        boolean canHandle = null != s_safePopup;
        if ( canHandle ) {
            s_safePopup.doPopup( context, button, curDict );
        }
        return canHandle;
    }

    public static void start( Context context )
    {
        Intent intent = new Intent( context, DictsActivity.class );
        context.startActivity( intent );
    }

}
