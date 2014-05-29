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

import android.app.ListActivity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import junit.framework.Assert;

import org.eehouse.android.xw4.loc.LocUtils;

public class StudyListActivity extends XWListActivity {

    private StudyListDelegate m_dlgt;

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        m_dlgt = new StudyListDelegate( this, savedInstanceState );
        super.onCreate( savedInstanceState, m_dlgt );
    }

    public static void launchOrAlert( Context context, int lang, 
                                      DlgDelegate.HasDlgDelegate dlg )
    {
        String msg = null;
        if ( 0 == DBUtils.studyListLangs( context ).length ) {
            msg = LocUtils.getString( context, R.string.study_no_lists );
        } else if ( StudyListDelegate.NO_LANG != lang && 
                    0 == DBUtils.studyListWords( context, lang ).length ) {
            String langname = DictLangCache.getLangName( context, lang );
            msg = LocUtils.getString( context, R.string.study_no_lang_fmt, langname );
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
