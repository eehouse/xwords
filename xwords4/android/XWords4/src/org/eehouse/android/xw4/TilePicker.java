/* -*- compile-command: "cd ../../../../../; ant reinstall"; -*- */


package org.eehouse.android.xw4;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.content.Intent;
import android.widget.Button;
import android.widget.TableLayout;
import android.widget.TableRow;

public class TilePicker extends Activity implements View.OnClickListener {
    static final int N_COLS = 5;
    private String[] m_tiles;
    private Button m_cancel;
    private int m_tileIndex = 0;

    private TableLayout m_table;

    class Finisher implements View.OnClickListener {
        int m_index;
        Activity m_activity;
        Finisher( Activity activity, int index ) {
            m_activity = activity;
            m_index = index;
        }
        public void onClick(View v) {
            m_activity.setResult( RESULT_FIRST_USER + m_index );
            m_activity.finish();
        }
    }

    protected void onCreate( Bundle savedInstanceState )
    {
        Utils.logf( "TilePicker::onCreate() called" );
        super.onCreate( savedInstanceState );

        setContentView( R.layout.tile_picker );

        Intent intent = getIntent();
        Bundle bundle = intent.getBundleExtra( XWConstants.PICK_TILE_TILES );
        m_tiles = bundle.getStringArray( XWConstants.PICK_TILE_TILES );

        m_cancel = (Button)findViewById( R.id.cancel );
        m_cancel.setOnClickListener( this );

        m_table = (TableLayout)findViewById( R.id.tile_table );

        TableRow row = null;
        int ii;
        for ( ii = 0; ii < m_tiles.length; ) {
            if ( null == row ) {
                row = new TableRow( this );
            }
            Button button = new Button( this );
            button.setText( m_tiles[ii] );
            button.setOnClickListener( new Finisher( this, ii ) );
            row.addView( button );

            ++ii;
            if ( ii == m_tiles.length || ii % N_COLS == 0 ) {
                m_table.addView( row );
                row = null;
            }
        }

    }

    public void onClick( View v ) 
    {
        if ( m_cancel == v ) {
            setResult( RESULT_FIRST_USER + m_tileIndex );
            finish();
        }
    }

}
