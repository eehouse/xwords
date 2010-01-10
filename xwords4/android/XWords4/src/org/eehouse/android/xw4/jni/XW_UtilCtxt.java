
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

    void setTimer( int why, int when, int handle );
    void clearTimer( int why );
    void requestTime();

    // Don't need this unless we have a scroll thumb to indicate position
    //void yOffsetChange( int oldOffset, int newOffset );

}
