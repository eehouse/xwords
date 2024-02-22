/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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

import org.eehouse.android.xw4.Utils.ISOCode;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;

public interface UtilCtxt {
    static final int BONUS_NONE = 0;
    static final int BONUS_DOUBLE_LETTER = 1;
    static final int BONUS_DOUBLE_WORD = 2;
    static final int BONUS_TRIPLE_LETTER = 3;
    static final int BONUS_TRIPLE_WORD = 4;
    static final int BONUS_QUAD_LETTER = 5;
    static final int BONUS_QUAD_WORD = 6;

    public static final int TRAY_HIDDEN = 0;
    public static final int TRAY_REVERSED = 1;
    public static final int TRAY_REVEALED = 2;

    // must match defns in util.h
    public static final int PICKER_PICKALL = -1;
    public static final int PICKER_BACKUP = -2;

    void notifyPickTileBlank( int playerNum, int col, int row, String[] texts );

    void informNeedPickTiles( boolean isInitial, int playerNum, int nToPick,
                              String[] texts, int[] counts );

    void informNeedPassword( int player, String name );

    void turnChanged( int newTurn );

    boolean engineProgressCallback();

    // Values for why; should be enums
    public static final int TIMER_PENDOWN = 1;
    public static final int TIMER_TIMERTICK = 2;
    public static final int TIMER_COMMS = 3;
    public static final int TIMER_SLOWROBOT = 4;
    public static final int TIMER_DUP_TIMERCHECK = 5;
    public static final int NUM_TIMERS_PLUS_ONE = 6;
    void setTimer( int why, int when, int handle );
    void clearTimer( int why );

    void requestTime();
    void remSelected();
    void getMQTTIDsFor(String[] relayID);
    void timerSelected( boolean inDuplicateMode, boolean canPause );
    void informWordsBlocked( int nWords, String words, String dict );
    String getInviteeName( int index );

    void bonusSquareHeld( int bonus );
    void playerScoreHeld( int player );
    void cellSquareHeld( String words );

    void notifyMove( String query );
    void notifyTrade( String[] tiles );
    void notifyDupStatus( boolean amHost, String msg );

    // These can't be an ENUM! The set is open-ended, with arbitrary values
    // added to ERR_RELAY_BASE, so no way to create an enum from an int in the
    // jni world. int has to be passed into userError(). Trust me: I
    // tried. :-)
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
    static final int ERR_NO_EMPTY_TRADE = 13;
    static final int ERR_TOO_MANY_TRADE = 14;
    static final int ERR_CANT_UNDO_TILEASSIGN = 15;
    static final int ERR_CANT_HINT_WHILE_DISABLED = 16;
    static final int ERR_NO_HINT_FOUND = 17;

    static final int ERR_RELAY_BASE = 18;
    void userError( int id );

    void informMove( int turn, String expl, String words );
    void informUndo();

    void informNetDict( String isoCodeStr, String oldName, String newName,
                        String newSum, CurGameInfo.XWPhoniesChoice phonies );

    void informMissing( boolean isServer, CommsAddrRec hostAddr,
                        CommsConnTypeSet connTypes, int nDevs,
                        int nMissingPlayers, int nInvited, boolean fromRematch );

    void notifyGameOver();
    // Don't need this unless we have a scroll thumb to indicate position
    //void yOffsetChange( int maxOffset, int oldOffset, int newOffset );

    void notifyIllegalWords( String dict, String[] words, int turn,
                             boolean turnLost );

    void showChat( String msg, int fromPlayer, int tsSeconds );

    String formatPauseHistory( int pauseTyp, int player, int whenPrev,
                               int whenCur, String msg );
}
