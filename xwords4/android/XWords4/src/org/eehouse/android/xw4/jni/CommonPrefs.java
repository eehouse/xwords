package org.eehouse.android.xw4.jni;

public class CommonPrefs {
    public boolean showBoardArrow;
    public boolean showRobotScores;
    public boolean hideTileValues; 
    public boolean skipCommitConfirm;

    public CommonPrefs() {
        showBoardArrow = true;
        showRobotScores = false;
        hideTileValues = false; 
        skipCommitConfirm = true;
    }
}
