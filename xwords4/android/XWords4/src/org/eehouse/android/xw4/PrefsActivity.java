/* -*- compile-command: "cd ../../../../../; ant reinstall"; -*- */

package org.eehouse.android.xw4;

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

public class PrefsActivity extends Activity implements View.OnClickListener {

    private Button m_doneB;
    private CommonPrefs m_cp;

    @Override
    protected void onCreate( Bundle savedInstanceState )
    {
        Utils.logf( "PrefsActivity::onCreate() called" );
        super.onCreate( savedInstanceState );

        m_cp = new CommonPrefs( Utils.getCP() );

        setContentView( R.layout.prefs );

        Utils.setChecked( this, R.id.color_tiles, m_cp.showColors );
        Utils.setChecked( this, R.id.show_arrow, m_cp.showBoardArrow );
        Utils.setChecked( this, R.id.explain_robot, m_cp.showRobotScores );
        Utils.setChecked( this, R.id.skip_confirm_turn, m_cp.skipCommitConfirm );
        Utils.setChecked( this, R.id.hide_values, m_cp.hideTileValues );

        m_doneB = (Button)findViewById(R.id.prefs_done);
        m_doneB.setOnClickListener( this );
    }

    public void onClick( View view ) 
    {
        if ( m_doneB == view ) {

            m_cp.showColors = Utils.getChecked( this, R.id.color_tiles );
            m_cp.showBoardArrow = Utils.getChecked( this, R.id.show_arrow );
            m_cp.showRobotScores = Utils.getChecked( this, R.id.explain_robot );
            m_cp.skipCommitConfirm = Utils.getChecked( this, R.id.skip_confirm_turn );
            m_cp.hideTileValues = Utils.getChecked( this, R.id.hide_values );

            Utils.setCP( m_cp );

            finish();
        }
    } // onClick
}
