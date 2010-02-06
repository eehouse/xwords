/* -*- compile-command: "cd ../../../../../; ant reinstall"; -*- */

package org.eehouse.android.xw4;

import android.preference.PreferenceActivity;
import android.app.Activity;
import android.os.Bundle;
import android.content.Intent;
import android.app.Dialog;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.res.Configuration;
import android.view.View;
import android.widget.Button;

import org.eehouse.android.xw4.jni.*;

public class PrefsActivity extends PreferenceActivity {

    private Button m_doneB;
    private CommonPrefs m_cp;

    @Override
    protected void onCreate( Bundle savedInstanceState )
    {
        super.onCreate(savedInstanceState);

        // Load the preferences from an XML resource
        addPreferencesFromResource( R.xml.xwprefs );  
    }
}
