/* -*- compile-command: "cd ../../../../../; ant reinstall"; -*- */



package org.eehouse.android.xw4;

public interface XWConstants {

    public static final String GAME_EXTN = ".xwg";

    public static final String PICK_TILE_TILES
        = "org.eehouse.android.xw4.PICK_TILE_TILES";
    // public static final String PICK_TILE_TILE
    //     = "org.eehouse.android.xw4.PICK_TILE_TILE";

    // These are duplicated in AndroidManifest.xml.  If change here
    // must change there too to keep in sync.
    public final String ACTION_PICK_TILE
        = "org.eehouse.android.xw4.action.PICK_TILE";
    public final String CATEGORY_PICK_TILE
        = "org.eehouse.android.xw4.category.PICK_TILE";

    public final String ACTION_QUERY = "org.eehouse.android.xw4.action.QUERY";
    public final String ACTION_INFORM= "org.eehouse.android.xw4.action.INFORM";
    public static final String QUERY_QUERY
        = "org.eehouse.android.xw4.QUERY_QUERY";

}
