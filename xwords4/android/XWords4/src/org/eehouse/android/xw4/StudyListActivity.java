/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
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

import android.app.Dialog;
import android.app.ListActivity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import junit.framework.Assert;

public class StudyListActivity extends ListActivity {

    private StudyListDelegate m_dlgt;

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );

        m_dlgt = new StudyListDelegate( this, savedInstanceState );
    }

    @Override
    public void onBackPressed() {
        if ( !m_dlgt.backPressed() ) {
            super.onBackPressed();
        }
    }

    @Override
    public boolean onCreateOptionsMenu( Menu menu )
    {
        getMenuInflater().inflate( R.menu.studylist, menu );
        return true;
    }

    @Override
    public boolean onPrepareOptionsMenu( Menu menu ) 
    {
        return m_dlgt.onPrepareOptionsMenu( menu )
            || super.onPrepareOptionsMenu( menu );
    }

    @Override
    public boolean onOptionsItemSelected( MenuItem item )
    {
        return m_dlgt.onOptionsItemSelected( item )
            || super.onOptionsItemSelected( item );
    }

    @Override
    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = m_dlgt.createDialog( id );
        if ( null == dialog ) {
            dialog = super.onCreateDialog( id );
        }
        return dialog;
    } // onCreateDialog

    public static void launchOrAlert( Context context, int lang, 
                                      DlgDelegate.HasDlgDelegate dlg )
    {
        String msg = null;
        if ( 0 == DBUtils.studyListLangs( context ).length ) {
            msg = context.getString( R.string.study_no_lists );
        } else if ( StudyListDelegate.NO_LANG != lang && 
                    0 == DBUtils.studyListWords( context, lang ).length ) {
            String langname = DictLangCache.getLangName( context, lang );
            msg = context.getString( R.string.study_no_langf, langname );
        } else {
            Intent intent = new Intent( context, StudyListActivity.class );
            if ( StudyListDelegate.NO_LANG != lang ) {
                intent.putExtra( StudyListDelegate.START_LANG, lang );
            }
            context.startActivity( intent );
        }

        if ( null != msg ) {
            dlg.showOKOnlyDialog( msg );
        }
    }
}
