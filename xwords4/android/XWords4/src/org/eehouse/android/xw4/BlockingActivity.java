/* -*- compile-command: "cd ../../../../../; ant reinstall"; -*- */

package org.eehouse.android.xw4;

import android.app.Activity;
import android.os.Bundle;
import android.content.Intent;
import android.app.Dialog;
import android.app.AlertDialog;
import android.content.DialogInterface;

public class BlockingActivity extends Activity {

    private String m_query = null;
    private String[] m_tiles = null;
    private static final int DLG_QUERY = 1;
    private int m_butPos = 0;
    private int m_butNeg = 0;
    private int m_title;


    @Override
    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = null;
        switch ( id ) {
        case DLG_QUERY:
            AlertDialog.Builder ab =
                new AlertDialog.Builder( BlockingActivity.this )
                //.setIcon( R.drawable.alert_dialog_icon )
                .setTitle( m_title )
                .setCancelable( false );

            if ( 0 != m_butPos ) {
                ab.setPositiveButton( m_butPos,
                                      new DialogInterface.OnClickListener() {
                                          public void onClick( DialogInterface dialog, 
                                                               int whichButton ) {
                                              Utils.logf( "Yes clicked" );
                                              setResult( 1 );
                                              finish();
                                          }
                                      });
            }

            if ( 0 != m_butNeg ) {
                ab.setNegativeButton( m_butNeg,
                                      new DialogInterface.OnClickListener() {
                                          public void onClick( DialogInterface dialog, 
                                                               int whichButton ) {
                                              Utils.logf( "No clicked" );
                                              setResult( 0 );
                                              finish();
                                          }
                                      });
            }

            if ( null != m_tiles ) {
                Utils.logf( "adding m_tiles; len=" + m_tiles.length );
                ab.setItems( m_tiles, new DialogInterface.OnClickListener() {
                        public void onClick( DialogInterface dialog, int item ) {
                            setResult( RESULT_FIRST_USER + item );
                            finish();
                        }
                    });
            } else if ( null != m_query ) {
                ab.setMessage( m_query );
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
        Bundle bundle = intent.getBundleExtra( XWConstants.BLOCKING_DLG_BUNDLE );
        m_query = bundle.getString( XWConstants.QUERY_QUERY );
        m_tiles = bundle.getStringArray( XWConstants.PICK_TILE_TILES );

        final String action = intent.getAction();
        if ( action.equals( XWConstants.ACTION_INFORM ) ) {
            m_title = R.string.info_title;
            m_butPos = R.string.button_ok;
        } else if ( action.equals( XWConstants.ACTION_QUERY ) ) {
            m_title = R.string.query_title;
            m_butPos = R.string.button_yes;
            m_butNeg = R.string.button_no;
        } else if ( action.equals( XWConstants.ACTION_PICK_TILE ) ) {
            m_title = R.string.title_tile_picker;
            m_butNeg = R.string.button_cancel;
        }

        showDialog( DLG_QUERY );
    }

}
