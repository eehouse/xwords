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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

package org.eehouse.android.xw4;

import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.Button;
import android.view.LayoutInflater;
import java.util.HashMap;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;

public class Toolbar {

    private LinearLayout m_horLayout;
    private LinearLayout m_vertLayout;
    private JNIThread m_jniThread;
    private HashMap<String,Button> m_buttons;

    private enum ORIENTATION { ORIENT_UNKNOWN,
            ORIENT_PORTRAIT,
            ORIENT_LANDSCAPE,
            };
    private ORIENTATION m_curOrient = ORIENTATION.ORIENT_UNKNOWN;

    public Toolbar( JNIThread jniThread, View horLayout, 
                    View vertLayout )
    {
        m_jniThread = jniThread;
        m_horLayout = (LinearLayout)horLayout;
        m_vertLayout = (LinearLayout)vertLayout;

        m_buttons = new HashMap<String,Button>();
    }

    public void orientChanged( boolean landscape )
    {
        if ( landscape && m_curOrient == ORIENTATION.ORIENT_LANDSCAPE ) {
            // do nothing
        } else if ( !landscape && m_curOrient == ORIENTATION.ORIENT_PORTRAIT ) {
            // do nothing
        } else {
            LinearLayout prevLayout, nextLayout;
            if ( landscape ) {
                m_curOrient = ORIENTATION.ORIENT_LANDSCAPE;
                prevLayout = m_horLayout;
                nextLayout = m_vertLayout;
            } else {
                m_curOrient = ORIENTATION.ORIENT_PORTRAIT;
                prevLayout = m_vertLayout;
                nextLayout = m_horLayout;
            }

            prevLayout.setVisibility( View.GONE );
            
            int nChildren = prevLayout.getChildCount();
            for ( int ii = 0; ii < nChildren; ++ii ) {
                Button button = (Button)prevLayout.getChildAt(0);
                prevLayout.removeView( button );
                nextLayout.addView( button, ii );
            }

            nextLayout.setVisibility( View.VISIBLE );
        }
    }

    public void addButton( Context context, String label, 
                           View.OnClickListener listener )
    {
        Button button = m_buttons.get( label );
        if ( null == button ) {
            LayoutInflater factory = LayoutInflater.from( context );
            button = (Button)factory.inflate( R.layout.toolbar_button, null );
            button.setOnClickListener( listener );
            button.setText( label );

            m_buttons.put( label, button );
        }

        Assert.assertTrue( ORIENTATION.ORIENT_UNKNOWN != m_curOrient );
        LinearLayout layout = ORIENTATION.ORIENT_PORTRAIT == m_curOrient
            ? m_horLayout : m_vertLayout;
        if ( -1 == layout.indexOfChild( button ) ) {
            layout.addView( button );
        }
    }

}
