/*
 * Copyright 2009 - 2024 by Eric House (xwords@eehouse.org).  All rights
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
package org.eehouse.android.xw4.jni

// Why does this have to be its own class...
class BoardDims {
    var left: Int = 0
    @JvmField
    var top: Int = 0
    var width: Int = 0
    var height: Int = 0 // of the bitmap
    @JvmField
    var scoreLeft: Int = 0
    @JvmField
    var scoreWidth: Int = 0
    @JvmField
    var scoreHt: Int = 0
    @JvmField
    var boardWidth: Int = 0
    var boardHt: Int = 0
    var trayLeft: Int = 0
    var trayTop: Int = 0
    var trayWidth: Int = 0
    var trayHt: Int = 0
    var traySize: Int = 0
    var cellSize: Int = 0
    var maxCellSize: Int = 0
    @JvmField
    var timerWidth: Int = 0 // @Override
    // public String toString()
    // {
    //     StringBuilder sb = new StringBuilder()
    //         .append( "width: " ).append( width )
    //         .append(" height: ").append( height )
    //         .append(" left: " ).append( left )
    //         .append(" top: " ).append( top )
    //         .append(" scoreLeft: " ).append( scoreLeft )
    //         .append(" scoreHt: " ).append( scoreHt )
    //         .append(" scoreWidth: " ).append( scoreWidth )
    //         .append(" boardHt: " ).append( boardHt )
    //         .append(" trayLeft: " ).append( trayLeft )
    //         .append(" trayTop: " ).append( trayTop )
    //         .append(" trayWidth: " ).append( trayWidth )
    //         .append(" trayHt: " ).append( trayHt )
    //         .append(" cellSize: " ).append(cellSize)
    //         .append(" maxCellSize: " ).append(maxCellSize)
    //         ;
    //     return sb.toString();
    // }
}
