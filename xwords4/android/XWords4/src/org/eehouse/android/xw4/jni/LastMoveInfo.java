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

package org.eehouse.android.xw4.jni;

import android.content.Context;

import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.DbgUtils;
import org.eehouse.android.xw4.loc.LocUtils;

public class LastMoveInfo {

    // Keep in sync with StackMoveType in movestak.h
    private static final int ASSIGN_TYPE = 0;
    private static final int MOVE_TYPE = 1;
    private static final int TRADE_TYPE = 2;
    private static final int PHONY_TYPE = 3;

    public boolean isValid = false; // modified in jni world
    public String name;
    public int moveType;
    public int score;
    public int nTiles;
    public String word;

    public String format( Context context ) 
    {
        String result = null;
        if ( isValid ) {
            switch( moveType ) {
            case ASSIGN_TYPE:
                result = LocUtils.getString( context, R.string.lmi_tiles_fmt, name );
                break;
            case MOVE_TYPE:
                if ( 0 == nTiles ) {
                    result = LocUtils.getString( context, R.string.lmi_pass_fmt, 
                                                 name );
                } else {
                    result = LocUtils.getString( context, R.string.lmi_move_fmt, 
                                                 name, word, score );
                }
                break;
            case TRADE_TYPE:
                result = LocUtils.getString( context, R.string.lmi_trade_fmt, 
                                             name, nTiles );
                break;
            case PHONY_TYPE:
                result = LocUtils.getString( context, R.string.lmi_phony_fmt, 
                                             name );
                break;
            }
        }
        return result;
    }
}
