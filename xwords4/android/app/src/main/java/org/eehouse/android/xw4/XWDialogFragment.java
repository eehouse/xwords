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

import android.content.Context;
import android.content.DialogInterface;
import android.support.v4.app.DialogFragment;

import junit.framework.Assert;

class XWDialogFragment extends DialogFragment {
    private static final String TAG = XWDialogFragment.class.getSimpleName();
    private static int s_count = 0;

    private OnDismissListener m_onDismiss;
    private OnCancelListener m_onCancel;
    private static OnLastGoneListener s_onLast;

    public interface OnDismissListener {
        void onDismissed( XWDialogFragment frag );
    }
    public interface OnCancelListener {
        void onCancelled( XWDialogFragment frag );
    }
    public interface OnLastGoneListener {
        void onLastGone();
    }

    @Override
    public void onCancel( DialogInterface dialog )
    {
        if ( null != m_onCancel ) {
            m_onCancel.onCancelled( this );
        }
        super.onCancel( dialog );
    }

    @Override
    public void onDismiss( DialogInterface dif )
    {
        if ( null != m_onDismiss ) {
            m_onDismiss.onDismissed( this );
        }
        super.onDismiss( dif );
    }

    @Override
    public void onAttach( Context context )
    {
        ++s_count;
        super.onAttach( context );
        // DbgUtils.logd(TAG, "%s added to %s; now %d", toString(),
        //               context.getClass().getSimpleName(), s_count );
    }

    @Override
    public void onDetach()
    {
        --s_count;
        Assert.assertTrue( s_count >= 0 || !BuildConfig.DEBUG );
        super.onDetach();
        // DbgUtils.logd(TAG, "%s removed from %s; now %d", toString(),
        //               getActivity().getClass().getSimpleName(), s_count );
        if ( 0 == s_count && null != s_onLast ) {
            s_onLast.onLastGone();
        }
    }
    
    protected void setOnDismissListener( OnDismissListener lstnr )
    {
        Assert.assertTrue( null == lstnr || null == m_onDismiss || !BuildConfig.DEBUG );
        m_onDismiss = lstnr;
    }

    protected void setOnCancelListener( OnCancelListener lstnr )
    {
        Assert.assertTrue( null == lstnr || null == m_onCancel || !BuildConfig.DEBUG );
        m_onCancel = lstnr;
    }

    protected static void setOnLastGoneListener( OnLastGoneListener lstnr )
    {
        Assert.assertTrue( null == lstnr || null == s_onLast || !BuildConfig.DEBUG );
        s_onLast = lstnr;
    }

    protected static int inviteAlertCount()
    {
        DbgUtils.logd( TAG, "inviteAlertCount() => %d", s_count );
        return s_count;
    }
}
