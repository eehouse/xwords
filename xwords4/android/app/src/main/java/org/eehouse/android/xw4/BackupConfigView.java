/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2022 by Eric House (xwords@eehouse.org).  All rights reserved.
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

package org.eehouse.android.xw4;

import android.view.ViewGroup;
import android.content.Context;
import android.net.Uri;
// import android.text.TextUtils;
import android.util.AttributeSet;
// import android.view.View;
// import android.widget.AdapterView.OnItemSelectedListener;
// import android.widget.AdapterView;
// import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
// import android.widget.RadioButton;
// import android.widget.RadioGroup;
import android.widget.CheckBox;
// import android.widget.TextView;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.HashMap;

// import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

import org.eehouse.android.xw4.ZipUtils.SaveWhat;

public class BackupConfigView extends LinearLayout
{
    private static final String TAG = BackupConfigView.class.getSimpleName();

    private boolean mIsStore;
    private Uri mLoadFile;
    private Map<SaveWhat, CheckBox> mCheckBoxes = new HashMap<>();
    private List<SaveWhat> mShowWhats;

    public BackupConfigView( Context cx, AttributeSet as )
    {
        super( cx, as );
    }

    void init( Uri uri )
    {
        mLoadFile = uri;
        mIsStore = null == uri;
        if ( null != uri ) {
            mShowWhats = ZipUtils.getHasWhats( getContext(), uri );
        }
    }

    @Override
    protected void onFinishInflate()
    {
        Context context = getContext();
        LinearLayout list = (LinearLayout)findViewById( R.id.whats_list );
        for ( SaveWhat what : SaveWhat.values() ) {
            if ( null == mShowWhats || mShowWhats.contains(what) ) {
                CheckBox box = (CheckBox)
                    LocUtils.inflate( context, R.layout.invite_checkbox );
                box.setText( what.toString() );
                mCheckBoxes.put( what, box );
                list.addView( box );
            }
        }
    }

    String getPosButtonTxt()
    {
        return mIsStore ? "Save" : "Load";
    }

    public List<SaveWhat> getSaveWhat()
    {
        List<SaveWhat> result = new ArrayList<>();
        for ( SaveWhat what : mCheckBoxes.keySet() ) {
            CheckBox box = mCheckBoxes.get( what );
            if ( box.isChecked() ) {
                result.add( what );
                Log.d( TAG, "getSaveWhat(): added %s", what );
            } else {
                Log.d( TAG, "getSaveWhat(): DID NOT add %s", what );
            }
        }
        Log.d( TAG, "getSaveWhat() => %s", result );
        return result;
    }

}
