/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2009-2013 by Eric House (xwords@eehouse.org).  All
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

import org.eehouse.android.xw4.jni.GameSummary;

// Interfaces used for (list) items that are selectable via
// long-tapping.

public interface SelectableItem {

    public interface LongClickHandler {
        public void longClicked();
    }

    public void itemClicked( LongClickHandler clicked, GameSummary summary );
    public void itemToggled( LongClickHandler toggled, boolean selected );
    public boolean getSelected( LongClickHandler obj );
}

