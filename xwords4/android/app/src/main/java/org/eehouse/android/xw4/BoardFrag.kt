/*
 * Copyright 2014 - 2016 by Eric House (xwords@eehouse.org).  All rights
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

package org.eehouse.android.xw4

import android.os.Bundle
import android.view.View
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.LifecycleOwner

private val TAG = BoardFrag::class.java.getSimpleName()
class BoardFrag(): XWFragment()
// , DefaultLifecycleObserver, LifecycleEventObserver
{

	companion object {
		fun newInstance( parent: Delegator ): XWFragment
		{
			return BoardFrag().setParentName( parent )
		}
	}

    override fun onCreate( sis: Bundle? )
    {
        super<XWFragment>.onCreate( BoardDelegate( this ), sis, true )
    }

    // override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
    //     super.onViewCreated(view, savedInstanceState)
    //     Log.d(TAG, "onViewCreated()")
    //     getViewLifecycleOwner().getLifecycle().addObserver(this)
    // }

    // override fun onDestroyView() {
    //     super.onDestroyView()
    //     getViewLifecycleOwner().getLifecycle().removeObserver(this);
    // }

    // override fun onStateChanged(source: LifecycleOwner, event: Lifecycle.Event) {
    //     Log.d(TAG, "onStateChanged($event)")
    // }
}
