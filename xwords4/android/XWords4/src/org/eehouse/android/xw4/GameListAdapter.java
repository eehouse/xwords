/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2009-2012 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

import android.content.Context;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListAdapter;
import android.widget.ListView;

import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;

public class GameListAdapter extends XWListAdapter {
    private Context m_context;
    private ListView m_list;
    private LayoutInflater m_factory;
    private int m_fieldID;
    private Handler m_handler;
    private LoadItemCB m_cb;

    public interface LoadItemCB {
        public void itemClicked( long rowid, GameSummary summary );
    }

    public GameListAdapter( Context context, ListView list, 
                            Handler handler, LoadItemCB cb, String fieldName ) {
        super( DBUtils.gamesList(context).length );
        m_context = context;
        m_list = list;
        m_handler = handler;
        m_cb = cb;
        m_factory = LayoutInflater.from( context );

        m_fieldID = fieldToID( fieldName );
    }

    @Override
    public int getCount() {
        return DBUtils.gamesList(m_context).length;
    }

    // Views.  A view depends on a summary, which takes time to load.
    // When one needs loading it's done via an async task.
    public View getView( int position, View convertView, ViewGroup parent ) 
    {
        GameListItem result = (GameListItem)
            m_factory.inflate( R.layout.game_list_item, null );
        result.init( m_handler, DBUtils.gamesList(m_context)[position],
                     m_fieldID, m_cb );
        return result;
    }

    public void inval( long rowid )
    {
        GameListItem child = getItemFor( rowid );
        if ( null != child && child.getRowID() == rowid ) {
            child.forceReload();
        } else {
            DbgUtils.logf( "no child for rowid %d", rowid );
            GameListItem.inval( rowid );
            m_list.invalidate();
        }
    }

    public void invalName( long rowid )
    {
        GameListItem item = getItemFor( rowid );
        if ( null != item ) {
            item.invalName();
        }
    }

    public boolean setField( String fieldName )
    {
        boolean changed = false;
        int newID = fieldToID( fieldName );
        if ( -1 == newID ) {
            if ( XWApp.DEBUG ) {
                DbgUtils.logf( "GameListAdapter.setField(): unable to match"
                               + " fieldName %s", fieldName );
            }
        } else if ( m_fieldID != newID ) {
            if ( XWApp.DEBUG ) {
                DbgUtils.logf( "setField: clearing views cache for change"
                               + " from %d to %d", m_fieldID, newID );
            }
            m_fieldID = newID;
            // return true so caller will do onContentChanged.
            // There's no other way to signal GameListItem instances
            // since we don't maintain a list of them.
            changed = true;
        }
        return changed;
    }

    private GameListItem getItemFor( long rowid )
    {
        GameListItem result = null;
        int position = positionFor( rowid );
        if ( 0 <= position ) {
            result = (GameListItem)m_list.getChildAt( position );
        }
        return result;
    }

    private int fieldToID( String fieldName )
    {
        int[] ids = {
            R.string.game_summary_field_empty
            ,R.string.game_summary_field_language
            ,R.string.game_summary_field_opponents
            ,R.string.game_summary_field_state
        };
        int result = -1;
        for ( int id : ids ) {
            if ( m_context.getString( id ).equals( fieldName ) ) {
                result = id;
                break;
            }
        }
        return result;
    }

    private int positionFor( long rowid )
    {
        int position = -1;
        long[] rowids = DBUtils.gamesList( m_context );
        for ( int ii = 0; ii < rowids.length; ++ii ) {
            if ( rowids[ii] == rowid ) {
                position = ii;
                break;
            }
        }
        return position;
    }
}