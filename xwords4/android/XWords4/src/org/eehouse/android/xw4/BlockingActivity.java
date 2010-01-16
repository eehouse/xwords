/* -*- compile-command: "cd ../../../../../; ant reinstall"; -*- */

package org.eehouse.android.xw4;

import android.app.Activity;
import android.os.Bundle;
import android.content.Intent;
import android.app.Dialog;
import android.app.AlertDialog;
import android.content.DialogInterface;

public class BlockingActivity extends Activity {

    private String m_query;
    private static final int DLG_QUERY = 1;
    private boolean m_infoOnly;

    @Override
    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = null;
        switch ( id ) {
        case DLG_QUERY:
            AlertDialog.Builder ab =
                new AlertDialog.Builder( BlockingActivity.this )
                //.setIcon( R.drawable.alert_dialog_icon )
                .setTitle( m_infoOnly ? 
                           R.string.info_title : R.string.query_title )
                .setMessage( m_query )
                .setCancelable( false )
                .setPositiveButton( m_infoOnly ? 
                                    R.string.button_ok : R.string.button_yes,
                                    new DialogInterface.OnClickListener() {
                                        public void onClick( DialogInterface dialog, 
                                                             int whichButton ) {
                                            Utils.logf( "Yes clicked" );
                                            setResult( 1 );
                                            finish();
                                        }
                                    });
            if ( !m_infoOnly ) {
                ab.setNegativeButton( R.string.button_no, 
                                    new DialogInterface.OnClickListener() {
                                        public void onClick( DialogInterface dialog, 
                                                             int whichButton ) {
                                            Utils.logf( "No clicked" );
                                            setResult( 0 );
                                            finish();
                                        }
                                    });
            }
            dialog = ab.create();
            break;
        }
        return dialog;
    } // onCreateDialog
    

    @Override
    protected void onCreate( Bundle savedInstanceState )
    {
        Utils.logf( "BlockingActivity::onCreate() called" );
        super.onCreate( savedInstanceState );

        Intent intent = getIntent();
        Bundle bundle = intent.getBundleExtra( XWConstants.QUERY_QUERY );
        m_query = bundle.getString( XWConstants.QUERY_QUERY );
        m_infoOnly = XWConstants.ACTION_INFORM.equals(intent.getAction());

        showDialog( DLG_QUERY );
    }

}
