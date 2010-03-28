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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

package org.eehouse.android.xw4.jni;

public class DrawScoreInfo {

    public static final int CELL_NONE = 0x00;
    public static final int CELL_ISBLANK = 0x01;
    public static final int  CELL_HIGHLIGHT = 0x02;
    public static final int  CELL_ISSTAR = 0x04;
    public static final int  CELL_ISCURSOR = 0x08;
    public static final int  CELL_ISEMPTY = 0x10;       /* of a tray tile slot */
    public static final int  CELL_VALHIDDEN = 0x20;     /* show letter only, not value */
    public static final int  CELL_DRAGSRC = 0x40;       /* where drag originated */
    public static final int  CELL_DRAGCUR = 0x80;       /* where drag is now */
    public static final int  CELL_ALL = 0xFF;

    // LastScoreCallback lsc;
    // void* lscClosure;
    public String name;
    public int playerNum;
    public int totalScore;
    public int nTilesLeft;   /* < 0 means don't use */
    public int flags;        // was CellFlags; use CELL_ constants above
    public boolean isTurn;
    public boolean selected;
    public boolean isRemote;
    public boolean isRobot;
};
