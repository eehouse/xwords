/* -*- compile-command: "find-and-gradle.sh insXw4Debug"; -*- */
/*
 * Copyright 2017 by Eric House (xwords@eehouse.org).  All rights reserved.
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
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import java.io.Serializable;

import junit.framework.Assert;

import org.eehouse.android.xw4.loc.LocUtils;

public class LookupAlert extends XWDialogFragment {
    private LookupAlertView m_view;

    public static LookupAlert newInstance( String[] words, int lang, boolean noStudy )
    {
        LookupAlert result = new LookupAlert();
        Bundle bundle = LookupAlertView.makeParams( words, lang, noStudy );
        result.setArguments( bundle );
        return result;
    }

    public LookupAlert() {}

    @Override
    public void onSaveInstanceState( Bundle bundle )
    {
        super.onSaveInstanceState( bundle );
        m_view.saveInstanceState( bundle );
    }

    @Override
    public Dialog onCreateDialog( Bundle sis )
    {
        Activity activity = getActivity();
        if ( null == sis ) {
            sis = getArguments();
        }
        
        m_view = (LookupAlertView)LocUtils.inflate( activity, R.layout.lookup );
        m_view.init( new LookupAlertView.OnDoneListener() {
                @Override
                public void onDone() {
                    dismiss();
                }
            }, sis );

        Dialog result = LocUtils.makeAlertBuilder( activity )
            .setTitle( R.string.lookup_title )
            .setView( m_view )
            .create();
        result.setOnKeyListener( m_view );
        return result;
    }
}
