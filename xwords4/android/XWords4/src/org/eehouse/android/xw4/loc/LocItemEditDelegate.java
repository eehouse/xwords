/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2014 by Eric House (xwords@eehouse.org).  All rights reserved.
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

package org.eehouse.android.xw4.loc;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.text.Editable;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.TextWatcher;
import android.text.style.BackgroundColorSpan;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.EditText;
import android.widget.TextView;
import java.util.HashSet;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.eehouse.android.xw4.DbgUtils;
import org.eehouse.android.xw4.DelegateBase;
import org.eehouse.android.xw4.Delegator;
import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.Utils;
import org.eehouse.android.xw4.Delegator;

public class LocItemEditDelegate extends DelegateBase implements TextWatcher {

    private static final String KEY = "KEY";
    private Activity m_activity;
    private String m_key;
    private HashSet<String> m_keyFmts;
    private EditText m_edit;
    private boolean m_haveBlessed;
    private static Pattern s_patFormat = Pattern.compile( LocUtils.RES_FORMAT );

    protected LocItemEditDelegate( Delegator delegator, Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState, R.layout.loc_item_edit, 
               R.menu.loc_item_menu );
        m_activity = delegator.getActivity();
    }

    protected void init( Bundle savedInstanceState )
    {
        // setContentView( R.layout.loc_item_edit );

        String key = getIntent().getStringExtra( KEY );
        m_key = key;

        TextView view = (TextView)findViewById( R.id.english_view );
        m_keyFmts = getFmtSet( key, view );
        if ( 0 < m_keyFmts.size() ) {
            showNotAgainDlg( R.string.not_again_fmt_expl, 
                             R.string.key_na_fmt_expl );
        }

        view = (TextView)findViewById( R.id.xlated_view_blessed );
        String blessed = LocUtils.getBlessedXlation( m_activity, key, true );
        m_haveBlessed = null != blessed && 0 < blessed.length();
        view.setText( blessed );
        m_edit = (EditText)findViewById( R.id.xlated_view_local );
        m_edit.addTextChangedListener( this );
        m_edit.setTextColor( LocListItem.LOCAL_COLOR );
        m_edit.setText( LocUtils.getLocalXlation( m_activity, key, true ) );

        view = (TextView)findViewById( R.id.english_label );
        view.setText( LocUtils.getString( m_activity, R.string.loc_main_english ) );

        String langName = LocUtils.getCurLocaleName( m_activity );
        view = (TextView)findViewById( R.id.blessed_label );
        view.setText( LocUtils.getString( m_activity, R.string.loc_lang_blessed,
                                          langName ) );
        view = (TextView)findViewById( R.id.local_label );
        view.setText( LocUtils.getString( m_activity, R.string.loc_lang_local,
                                          langName ) );
    }

    @Override
    protected void onPause()
    {
        // Save any local translation
        LocUtils.setXlation( m_activity, m_key, m_edit.getText() );

        super.onPause();
    }

    @Override
    public boolean onPrepareOptionsMenu( Menu menu ) 
    {
        CharSequence editTxt = m_edit.getText();
        boolean haveTxt = null != editTxt && 0 < editTxt.length();

        Utils.setItemVisible( menu, R.id.loc_item_clear, haveTxt );
        Utils.setItemVisible( menu, R.id.loc_item_check, haveTxt );
        Utils.setItemVisible( menu, R.id.loc_item_copy_eng, !haveTxt );
        Utils.setItemVisible( menu, R.id.loc_item_copy_bless, 
                              m_haveBlessed && !haveTxt );

        return true;
    }

    @Override
    public boolean onOptionsItemSelected( MenuItem item ) 
    {
        String newText = null;
        int id = item.getItemId();
        switch ( id ) {
        case R.id.loc_item_clear:
            newText = "";
            break;
        case R.id.loc_item_check:
            checkLocal();
            break;
        case R.id.loc_item_copy_eng:
            newText = m_key;
            break;
        case R.id.loc_item_copy_bless:
            newText = LocUtils.getBlessedXlation( m_activity, m_key, true );
            break;
        }

        if ( null != newText ) {
            m_edit.setText( newText );
            invalidateOptionsMenuIf();
        }

        return true;
    }

    // return true to prevent exiting
    protected boolean backPressed()
    {
        return !checkLocal();
    }

    // Check that syntax is legal, and alert if not
    private boolean checkLocal()
    {
        boolean ok = true;
        CharSequence cs = m_edit.getText();
        if ( null != cs && 0 < cs.length() ) {
            ok = m_keyFmts.equals( getFmtSet( cs.toString(), null ) );

            if ( !ok ) {
                showOKOnlyDialog( R.string.loc_fmts_mismatch );
            }
        }
        return ok;
    }

    // Performance hack: set up highlighting of format specifiers since we're
    // searching for them anyway.
    private HashSet<String> getFmtSet( String txt, TextView owner )
    {
        HashSet<String> fmts = new HashSet<String>();
        Spannable spanText = null; // null unless used

        Matcher matcher = s_patFormat.matcher( txt );
        while ( matcher.find() ) {
            int start = matcher.start();
            int end = matcher.end();
            fmts.add( txt.substring( start, end ) );

            if ( null == owner ) {
                continue;
            }

            if ( null == spanText ) {
                spanText = new SpannableString( txt );
            }
            spanText.setSpan( new BackgroundColorSpan(Color.BLUE), start, end,
                              Spannable.SPAN_EXCLUSIVE_EXCLUSIVE );
        }

        if ( null != owner ) {
            if ( null != spanText ) {
                owner.setText( spanText );
            } else {
                owner.setText( txt );
            }
        }

        return fmts;
    }

    //////////////////////////////////////////////////////////////////////
    // TextWatcher interface
    //////////////////////////////////////////////////////////////////////
    public void afterTextChanged( Editable s )
    {
        invalidateOptionsMenuIf();
    }
    public void beforeTextChanged( CharSequence s, int start, int count, int after ){}
    public void onTextChanged( CharSequence s, int start, int before, int count ){}

    protected static void launch( Context context, String key )
    {
        Intent intent = new Intent( context, LocItemEditActivity.class );

        intent.putExtra( KEY, key );

        context.startActivity( intent );
    }
}
