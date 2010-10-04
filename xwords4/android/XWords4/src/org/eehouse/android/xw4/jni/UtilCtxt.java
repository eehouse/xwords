/* -*- compile-command: "cd ../../../../../../; ant install"; -*- */
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

public interface UtilCtxt {
    static final int BONUS_NONE = 0;
    static final int BONUS_DOUBLE_LETTER = 1;
    static final int BONUS_DOUBLE_WORD = 2;
    static final int BONUS_TRIPLE_LETTER = 3;
    static final int BONUS_TRIPLE_WORD = 4;

    int getSquareBonus( int col, int row );
    int userPickTile( /* PickInfo* pi, add once tile-picking is enabled */
                     int playerNum, String[] texts );

    String askPassword( String name );
    void turnChanged();

    boolean engineProgressCallback();
    void engineStarting( int nBlanks );
    void engineStopping();

    // Values for why; should be enums
    public static final int TIMER_PENDOWN = 1;
    public static final int TIMER_TIMERTICK = 2;
    public static final int TIMER_COMMS = 3;
    public static final int TIMER_SLOWROBOT = 4;
    void setTimer( int why, int when, int handle );
    void clearTimer( int why );

    void requestTime();
    void remSelected();
    void setIsServer( boolean isServer );

    static final int STRD_ROBOT_TRADED =                  1;
    static final int STR_ROBOT_MOVED =                    2;
    static final int STRS_VALUES_HEADER =                 3;
    static final int STRD_REMAINING_TILES_ADD =           4;
    static final int STRD_UNUSED_TILES_SUB =              5;
    static final int STR_REMOTE_MOVED =                   6;
    static final int STRD_TIME_PENALTY_SUB =              7;
    static final int STR_PASS =                           8;
    static final int STRS_MOVE_ACROSS =                   9;
    static final int STRS_MOVE_DOWN =                    10;
    static final int STRS_TRAY_AT_START =                11;
    static final int STRSS_TRADED_FOR =                  12;
    static final int STR_PHONY_REJECTED =                13;
    static final int STRD_CUMULATIVE_SCORE =             14;
    static final int STRS_NEW_TILES =                    15;
    static final int STR_PASSED =                        16;
    static final int STRSD_SUMMARYSCORED =               17;
    static final int STRD_TRADED =                       18;
    static final int STR_LOSTTURN =                      19;
    static final int STR_COMMIT_CONFIRM =                20;
    static final int STR_LOCAL_NAME =                    21;
    static final int STR_NONLOCAL_NAME =                 22;
    static final int STR_BONUS_ALL =                     23;
    static final int STRD_TURN_SCORE =                   24;
    String getUserString( int stringCode );

    static final int QUERY_COMMIT_TURN = 0;
    static final int QUERY_COMMIT_TRADE = 1;
    static final int QUERY_ROBOT_MOVE = 2;
    static final int QUERY_ROBOT_TRADE = 3;
    boolean userQuery( int id, String query );


    // These oughtto be an enum but then I'd have to cons one up in C.
    static final int ERR_NONE = 0;
    static final int ERR_TILES_NOT_IN_LINE = 1;
    static final int ERR_NO_EMPTIES_IN_TURN = 2;
    static final int ERR_TWO_TILES_FIRST_MOVE = 3;
    static final int ERR_TILES_MUST_CONTACT = 4;
    static final int ERR_TOO_FEW_TILES_LEFT_TO_TRADE = 5;
    static final int ERR_NOT_YOUR_TURN = 6;
    static final int ERR_NO_PEEK_ROBOT_TILES = 7;
    static final int ERR_SERVER_DICT_WINS = 8;
    static final int ERR_NO_PEEK_REMOTE_TILES = 9;
    static final int ERR_REG_UNEXPECTED_USER = 10;
    static final int ERR_REG_SERVER_SANS_REMOTE = 11;
    static final int STR_NEED_BT_HOST_ADDR = 12;
    static final int ERR_CANT_TRADE_MID_MOVE = 13;
    static final int ERR_CANT_UNDO_TILEASSIGN = 14;
    static final int ERR_CANT_HINT_WHILE_DISABLED = 15;
    static final int ERR_RELAY_BASE = 16;
    void userError( int id );

    void notifyGameOver();
    // Don't need this unless we have a scroll thumb to indicate position
    //void yOffsetChange( int maxOffset, int oldOffset, int newOffset );

    boolean warnIllegalWord( String[] words, int turn, boolean turnLost );

    void showChat( String msg );
}
