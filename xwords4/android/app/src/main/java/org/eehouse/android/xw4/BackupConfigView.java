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
import android.util.AttributeSet;
import android.widget.LinearLayout;
import android.widget.CheckBox;
import android.widget.TextView;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.HashMap;


import org.eehouse.android.xw4.loc.LocUtils;

import org.eehouse.android.xw4.ZipUtils.SaveWhat;

public class BackupConfigView extends LinearLayout
{
    private static final String TAG = BackupConfigView.class.getSimpleName();

    private Boolean mIsStore;
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
        mIsStore = new Boolean(null == uri);
        if ( null != uri ) {
            mShowWhats = ZipUtils.getHasWhats( getContext(), uri );
        }
        initOnce();
    }

    // Usually called before init(), but IIRC wasn't on older Android versions
    @Override
    protected void onFinishInflate()
    {
        initOnce();
    }

    private void initOnce()
    {
        if ( null != mIsStore ) {
            TextView tv = ((TextView)findViewById(R.id.explanation));
            if ( null != tv ) {
                int explID = mIsStore ?
                    R.string.archive_expl_store : R.string.archive_expl_load;
                tv.setText( explID );

                Context context = getContext();
                LinearLayout list = (LinearLayout)findViewById( R.id.whats_list );
                for ( SaveWhat what : SaveWhat.values() ) {
                    if ( null == mShowWhats || mShowWhats.contains(what) ) {
                        ViewGroup item = (ViewGroup)
                            LocUtils.inflate( context, R.layout.backup_config_item );
                        list.addView( item );
                        CheckBox box = (CheckBox)item.findViewById( R.id.check );
                        box.setText( what.titleID() );
                        mCheckBoxes.put( what, box );
                        box.setChecked( !mIsStore );
                        ((TextView)(item.findViewById(R.id.expl)))
                            .setText( what.explID() );
                    }
                }
            }
        }
    }

    int getPosButtonTxt()
    {
        return mIsStore
            ? R.string.archive_button_store : R.string.archive_button_load;
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
