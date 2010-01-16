
package org.eehouse.android.xw4.jni;

public interface XW_UtilCtxt {
    static final int BONUS_NONE = 0;
    static final int BONUS_DOUBLE_LETTER = 1;
    static final int BONUS_DOUBLE_WORD = 2;
    static final int BONUS_TRIPLE_LETTER = 3;
    static final int BONUS_TRIPLE_WORD = 4;

    int getSquareBonus( int col, int row );
    int userPickTile( /* PickInfo* pi, add once tile-picking is enabled */
                     int playerNum, String[] texts );
    boolean engineProgressCallback();

    void setTimer( int why, int when, int handle );
    void clearTimer( int why );
    void requestTime();
    void remSelected();


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

    // Don't need this unless we have a scroll thumb to indicate position
    //void yOffsetChange( int oldOffset, int newOffset );

}
