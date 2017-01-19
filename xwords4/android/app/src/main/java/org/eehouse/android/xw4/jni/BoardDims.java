/* -*- compile-command: "find-and-gradle.sh installXw4Debug"; -*- */
/*
 * Copyright 2009-2013 by Eric House (xwords@eehouse.org).  All rights
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

package org.eehouse.android.xw4.jni;


// Why does this have to be its own class...
public class BoardDims {
    public int left, top;
    public int width, height;       // of the bitmap
    public int scoreLeft, scoreWidth, scoreHt;
    public int boardWidth, boardHt;
    public int trayLeft, trayTop, trayWidth, trayHt;
    public int cellSize, maxCellSize;
    public int timerWidth;

    // public String toString()
    // {
    //     return "width: " + width
    //         + " height: " + height
    //         + " left: " + left
    //         + " top: " + top
    //         + " scoreLeft: " + scoreLeft
    //         + " scoreHt: " + scoreHt
    //         + " scoreWidth: " + scoreWidth
    //         + " boardHt: " + boardHt
    //         + " trayLeft: " + trayLeft
    //         + " trayTop: " + trayTop
    //         + " trayWidth: " + trayWidth
    //         + " trayHt: " + trayHt
    //         + " cellSize: " + cellSize
    //         + " maxCellSize: " + maxCellSize;
    // }
}
