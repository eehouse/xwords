/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
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

import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;

public interface UtilCtxt {
    static final int BONUS_NONE = 0;
    static final int BONUS_DOUBLE_LETTER = 1;
    static final int BONUS_DOUBLE_WORD = 2;
    static final int BONUS_TRIPLE_LETTER = 3;
    static final int BONUS_TRIPLE_WORD = 4;

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
    void setTimer( int why, int when, int handle );
    void clearTimer( int why );

    void requestTime();
    void remSelected();
    void setIsServer( boolean isServer );

    void bonusSquareHeld( int bonus );
    void playerScoreHeld( int player );
    void cellSquareHeld( String words );

    void notifyMove( String query );
    void notifyTrade( String[] tiles );

    // Must be kept in sync with enum in common/util.h
    enum UtilErrID {
        ERR_NONE,                   /* 0 is special case */
        ERR_TILES_NOT_IN_LINE,   /* scoring a move where tiles aren't in line */
        ERR_NO_EMPTIES_IN_TURN,
        ERR_TWO_TILES_FIRST_MOVE,
        ERR_TILES_MUST_CONTACT,
        ERR_TOO_FEW_TILES_LEFT_TO_TRADE,
        ERR_NOT_YOUR_TURN,
        ERR_NO_PEEK_ROBOT_TILES,
        ERR_SERVER_DICT_WINS,
        ERR_NO_PEEK_REMOTE_TILES,
        ERR_REG_UNEXPECTED_USER, /* server asked to register too many remote
                                    users */
        ERR_REG_SERVER_SANS_REMOTE,
        STR_NEED_BT_HOST_ADDR,
        ERR_NO_EMPTY_TRADE,
        ERR_TOO_MANY_TRADE,
        ERR_CANT_UNDO_TILEASSIGN,
        ERR_CANT_HINT_WHILE_DISABLED,
        ERR_NO_HINT_FOUND,          /* not really an error... */
    };

    static final int ERR_RELAY_BASE = 17;
    void userError( UtilErrID id );

    void informMove( int turn, String expl, String words );
    void informUndo();

    void informNetDict( int lang, String oldName, String newName,
                        String newSum, CurGameInfo.XWPhoniesChoice phonies );

    void informMissing( boolean isServer, CommsConnTypeSet connTypes,
                        int nDevs, int nMissingPlayers );

    void notifyGameOver();
    // Don't need this unless we have a scroll thumb to indicate position
    //void yOffsetChange( int maxOffset, int oldOffset, int newOffset );

    void notifyIllegalWords( String dict, String[] words, int turn,
                             boolean turnLost );

    void showChat( String msg, int fromIndx, String fromName, int tsSeconds );
}
