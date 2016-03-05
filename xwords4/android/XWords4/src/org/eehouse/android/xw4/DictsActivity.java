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

public class DictsActivity extends XWActivity {

    private static interface SafePopup {
        public void doPopup( Context context, View button, 
                             String curDict, int lang );
    }
    private static SafePopup s_safePopup = null;
    // I can't provide a subclass of MenuItem to hold DictAndLoc, so
    // settle for a hash on the side.
    private DictsDelegate m_dlgt;

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        m_dlgt = new DictsDelegate( this, savedInstanceState );
        super.onCreate( savedInstanceState, m_dlgt );
    } // onCreate

    private static class SafePopupImpl implements SafePopup {
        public void doPopup( final Context context, View button, 
                             String curDict, int lang ) {

            final HashMap<MenuItem, DictAndLoc> itemData
                = new HashMap<MenuItem, DictAndLoc>();

            MenuItem.OnMenuItemClickListener listener = 
                new MenuItem.OnMenuItemClickListener() {
                    public boolean onMenuItemClick( MenuItem item )
                    {
                        DictAndLoc dal = itemData.get( item );

                        DictBrowseDelegate.launch( context, dal.name, 
                                                   dal.loc );
                        return true;
                    }
                };

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
                itemData.put( item, dal );
            }

            popup.show();
        }
    }

    public static boolean handleDictsPopup( Context context, View button,
                                            String curDict, int lang )
    {
        int nDicts = DictLangCache.getLangCount( context, lang );
        if ( null == s_safePopup && 1 < nDicts ) {
            int sdkVersion = Integer.valueOf( android.os.Build.VERSION.SDK );
            if ( 11 <= sdkVersion ) {
                s_safePopup = new SafePopupImpl();
            }
        }

        boolean canHandle = null != s_safePopup && 1 < nDicts;
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
