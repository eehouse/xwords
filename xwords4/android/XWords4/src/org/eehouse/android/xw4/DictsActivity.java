/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009 - 2014 by Eric House (xwords@eehouse.org).  All
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

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.PopupMenu;

import java.util.HashMap;

import org.eehouse.android.xw4.DictUtils.DictAndLoc;
import org.eehouse.android.xw4.loc.LocUtils;

public class DictsActivity extends XWListActivity {

    private static interface SafePopup {
        public void doPopup( Context context, View button, 
                             String curDict, int lang );
    }
    private static SafePopup s_safePopup = null;
    // I can't provide a subclass of MenuItem to hold DictAndLoc, so
    // settle for a hash on the side.
    private static HashMap<MenuItem, DictAndLoc> s_itemData;

    private DictsDelegate m_dlgt;

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        m_dlgt = new DictsDelegate( this, savedInstanceState );
        super.onCreate( savedInstanceState, m_dlgt );
    } // onCreate

    @Override
    public void onBackPressed() {
        if ( !m_dlgt.onBackPressed() ) {
            super.onBackPressed();
        }
    }

    private static class SafePopupImpl implements SafePopup {
        public void doPopup( final Context context, View button, 
                             String curDict, int lang ) {

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

            // Add at top but save until have dal info
            MenuItem curItem =
                menu.add( LocUtils.getString( context, 
                                              R.string.cur_menu_marker_fmt, 
                                              curDict ) );

            DictAndLoc[] dals = DictLangCache.getDALsHaveLang( context, lang );
            for ( DictAndLoc dal : dals ) {
                MenuItem item = dal.name.equals(curDict)
                    ? curItem : menu.add( dal.name );
                item.setOnMenuItemClickListener( listener );
                s_itemData.put( item, dal );
            }

            menu.add( R.string.show_wordlist_browser )
                .setOnMenuItemClickListener( listener );

            popup.show();
        }
    }

    public static boolean handleDictsPopup( Context context, View button,
                                            String curDict, int lang )
    {
        if ( null == s_safePopup ) {
            int sdkVersion = Integer.valueOf( android.os.Build.VERSION.SDK );
            if ( 11 <= sdkVersion ) {
                s_safePopup = new SafePopupImpl();
            }
        }

        boolean canHandle = null != s_safePopup;
        if ( canHandle ) {
            s_safePopup.doPopup( context, button, curDict, lang );
        }
        return canHandle;
    }

    public static void start( Context context )
    {
        Intent intent = new Intent( context, DictsActivity.class );
        context.startActivity( intent );
    }

}
