/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009 - 2012 by Eric House (xwords@eehouse.org).  All
 * rights reserved.
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

import android.app.ProgressDialog;
import android.content.Context;
import android.os.AsyncTask;
import android.widget.ArrayAdapter;
import android.widget.Spinner;

import org.eehouse.android.xw4.loc.LocUtils;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.net.Socket;
import java.util.ArrayList;

public class RefreshNamesTask extends AsyncTask<Void, Void, String[]> {

    public interface NoNameFound {
        public void NoNameFound();
    };

    private Context m_context;
    private Spinner m_resultSpinner;
    private int m_lang;
    private int m_nInGame;
    private ProgressDialog m_progress;
    private NoNameFound m_nnf;

    public RefreshNamesTask( Context context, NoNameFound nnf,
                             int lang, int nInGame,
                             Spinner getsResults )
    {
        super();
        m_context = context;
        m_nnf = nnf;
        m_resultSpinner = getsResults;
        m_lang = lang;
        m_nInGame = nInGame;

        String langName = DictLangCache.getLangName( context, lang );
        langName = LocUtils.xlateLang( context, langName );
        String msg = LocUtils.getString( context, R.string.public_names_progress_fmt,
                                         nInGame, langName );
        m_progress = ProgressDialog.show( context, msg, null, true, true );
    }

    @Override
    protected String[] doInBackground( Void...unused )
    {
        ArrayList<String> names = new ArrayList<String>();
        DbgUtils.logi( getClass(), "doInBackground()" );

        try {
            Socket socket = NetUtils.makeProxySocket( m_context, 15000 );
            if ( null != socket ) {
                DataOutputStream outStream =
                    new DataOutputStream( socket.getOutputStream() );

                outStream.writeShort( 4 );                // total packet length
                outStream.writeByte( NetUtils.PROTOCOL_VERSION );
                outStream.writeByte( NetUtils.PRX_PUB_ROOMS );
                outStream.writeByte( (byte)m_lang );
                outStream.writeByte( (byte)m_nInGame );
                outStream.flush();

                // read result -- will block
                DataInputStream dis =
                    new DataInputStream(socket.getInputStream());
                short len = dis.readShort();
                short nRooms = dis.readShort();
                DbgUtils.logi( getClass(), "doInBackground(): got %d rooms", nRooms );

                // Can't figure out how to read a null-terminated string
                // from DataInputStream so parse it myself.
                byte[] bytes = new byte[len];
                dis.read( bytes );

                int index = -1;
                for ( int ii = 0; ii < nRooms; ++ii ) {
                    int lastIndex = ++index; // skip the null
                    while ( bytes[index] != '\n' ) {
                        ++index;
                    }
                    String name = new String( bytes, lastIndex, index - lastIndex );
                    DbgUtils.logi( getClass(), "got public room name: %s", name );
                    int indx = name.lastIndexOf( "/" );
                    indx = name.lastIndexOf( "/", indx-1 );
                    names.add( name.substring(0, indx ) );
                }
            }
        } catch ( java.io.IOException ioe ) {
            DbgUtils.logex( ioe );
        }
        DbgUtils.logi( getClass(), "doInBackground() returning" );
        return names.toArray( new String[names.size()] );
    }

     // protected void onProgressUpdate(Integer... progress) {
     //     setProgressPercent(progress[0]);
     // }

    @Override
     protected void onPostExecute( String[] result )
     {
         DbgUtils.logi( getClass(), "onPostExecute()" );
         ArrayAdapter<String> adapter =
             new ArrayAdapter<String>( m_context,
                                       android.R.layout.simple_spinner_item,
                                       result );
         int resID = android.R.layout.simple_spinner_dropdown_item;
         adapter.setDropDownViewResource( resID );
         m_resultSpinner.setAdapter( adapter );

         m_progress.cancel();

         if ( result.length == 0 ) {
             m_nnf.NoNameFound();
         }

         DbgUtils.logi( getClass(), "onPostExecute() done" );
     }
}
