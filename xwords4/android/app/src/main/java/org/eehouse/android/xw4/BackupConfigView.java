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

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.net.Uri;
import android.util.AttributeSet;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.CompoundButton;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eehouse.android.xw4.loc.LocUtils;

import org.eehouse.android.xw4.ZipUtils.SaveWhat;

public class BackupConfigView extends LinearLayout
    implements OnCheckedChangeListener
{
    private static final String TAG = BackupConfigView.class.getSimpleName();

    private Boolean mIsStore;
    private Uri mLoadFile;
    private Map<SaveWhat, CheckBox> mCheckBoxes = new HashMap<>();
    private List<SaveWhat> mShowWhats;
    private AlertDialog mDialog;

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

    AlertDialog setDialog( AlertDialog dialog )
    {
        mDialog = dialog;
        dialog.setOnShowListener(new DialogInterface.OnShowListener() {
                @Override
                public void onShow( DialogInterface dialog ) {
                    countChecks();
                }
            });
        return dialog;
    }

    // Usually called before init(), but IIRC wasn't on older Android versions
    @Override
    protected void onFinishInflate()
    {
        initOnce();
    }

    @Override
    public void onCheckedChanged( CompoundButton buttonView,
                                  boolean isChecked )
    {
        countChecks();
    }

    private void countChecks()
    {
        if ( null != mDialog ) {
            boolean haveCheck = false;
            for ( CheckBox box : mCheckBoxes.values() ) {
                if ( box.isChecked() ) {
                    haveCheck = true;
                    break;
                }
            }
            Utils.enableAlertButton( mDialog, AlertDialog.BUTTON_POSITIVE,
                                     haveCheck );
        }
    }

    private void initOnce()
    {
        if ( null != mIsStore ) {
            TextView tv = ((TextView)findViewById(R.id.explanation));
            if ( null != tv ) {
                Context context = getContext();
                if ( mIsStore ) {
                    tv.setText( R.string.archive_expl_store );
                } else {
                    String name = ZipUtils.getFileName( context, mLoadFile );
                    String msg = LocUtils
                        .getString( context, R.string.archive_expl_load_fmt, name );
                    tv.setText( msg );
                }

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
                        box.setOnCheckedChangeListener( this );
                        ((TextView)(item.findViewById(R.id.expl)))
                            .setText( what.explID() );
                    }
                }
            }
            countChecks();
        }
    }

    int getAlertTitle()
    {
        return mIsStore
            ? R.string.gamel_menu_storedb : R.string.gamel_menu_loaddb;
    }

    int getPosButtonTxt()
    {
        return mIsStore
            ? R.string.archive_button_store : R.string.archive_button_load;
    }

    public ArrayList<SaveWhat> getSaveWhat()
    {
        ArrayList<SaveWhat> result = new ArrayList<>();
        for ( SaveWhat what : mCheckBoxes.keySet() ) {
            CheckBox box = mCheckBoxes.get( what );
            if ( box.isChecked() ) {
                result.add( what );
            }
        }
        return result;
    }

}
