package org.eehouse.android.xw4.jni;

public class CommonPrefs {
    public boolean showBoardArrow;
    public boolean showRobotScores;
    public boolean hideTileValues; 
    public boolean skipCommitConfirm;
    public boolean showColors;

    public CommonPrefs() {
        showBoardArrow = true;
        showRobotScores = true;
        hideTileValues = false; 
        skipCommitConfirm = false;
        showColors = true;
    }

    public CommonPrefs( CommonPrefs src ) {
        this();
        copyFrom( src );
    }

    public void copyFrom( CommonPrefs src )
    {
        showBoardArrow = src.showBoardArrow;
        showRobotScores = src.showRobotScores;
        hideTileValues = src.hideTileValues;
        skipCommitConfirm = src.skipCommitConfirm;
        showColors = src.showColors;
    }
}
