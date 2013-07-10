/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.content.DialogInterface.OnClickListener;

public class MultiService {

    public static final String LANG = "LANG";
    public static final String DICT = "DICT";
    public static final String GAMEID = "GAMEID";
    public static final String INVITEID = "INVITEID"; // relay only
    public static final String ROOM = "ROOM";
    public static final String GAMENAME = "GAMENAME";
    public static final String NPLAYERST = "NPLAYERST";
    public static final String NPLAYERSH = "NPLAYERSH";
    public static final String INVITER = "INVITER";
    public static final String OWNER = "OWNER";

    public static final int OWNER_SMS = 1;
    public static final int OWNER_RELAY = 2;

    private MultiEventListener m_li;

    public enum MultiEvent { BAD_PROTO
                          , BT_ENABLED
                          , BT_DISABLED
                          , SCAN_DONE
                          , HOST_PONGED
                          , NEWGAME_SUCCESS
                          , NEWGAME_FAILURE
                          , MESSAGE_ACCEPTED
                          , MESSAGE_REFUSED
                          , MESSAGE_NOGAME
                          , MESSAGE_RESEND
                          , MESSAGE_FAILOUT
                          , MESSAGE_DROPPED

                          , SMS_RECEIVE_OK
                          , SMS_SEND_OK
                          , SMS_SEND_FAILED
                          , SMS_SEND_FAILED_NORADIO

                          , RELAY_ALERT
            };

    public interface MultiEventListener {
        public void eventOccurred( MultiEvent event, Object ... args );
    }
    // public interface MultiEventSrc {
    //     public void setBTEventListener( BTEventListener li );
    // }

    public void setListener( MultiEventListener li )
    {
        synchronized( this ) {
            m_li = li;
        }
    }

    public void sendResult( MultiEvent event, Object ... args )
    {
        synchronized( this ) {
            if ( null != m_li ) {
                m_li.eventOccurred( event, args );
            }
        }
    }

    public static void fillInviteIntent( Intent intent, String gameName, 
                                         int lang, String dict, 
                                         int nPlayersT, int nPlayersH )
    {
        intent.putExtra( GAMENAME, gameName );
        intent.putExtra( LANG, lang );
        intent.putExtra( DICT, dict );
        intent.putExtra( NPLAYERST, nPlayersT ); // both of these used
        intent.putExtra( NPLAYERSH, nPlayersH );
    }

    public static Intent makeMissingDictIntent( Context context, String gameName, 
                                                int lang, String dict, 
                                                int nPlayersT, int nPlayersH )
    {
        Intent intent = new Intent( context, DictsActivity.class );
        fillInviteIntent( intent, gameName, lang, dict, nPlayersT, nPlayersH );
        return intent;
    }

    public static Intent makeMissingDictIntent( Context context, NetLaunchInfo nli )
    {
        Intent intent = makeMissingDictIntent( context, null, nli.lang, nli.dict, 
                                               nli.nPlayersT, 1 );
        intent.putExtra( ROOM, nli.room );
        return intent;
    }

    public static boolean isMissingDictIntent( Intent intent )
    {
        return intent.hasExtra( LANG )
            // && intent.hasExtra( DICT )
            && (intent.hasExtra( GAMEID ) || intent.hasExtra( ROOM ))
            && intent.hasExtra( GAMENAME )
            && intent.hasExtra( NPLAYERST )
            && intent.hasExtra( NPLAYERSH );
    }

    public static Dialog missingDictDialog( Context context, Intent intent,
                                            OnClickListener onDownload,
                                            OnClickListener onDecline )
    {
        int lang = intent.getIntExtra( LANG, -1 );
        String langStr = DictLangCache.getLangName( context, lang );
        String dict = intent.getStringExtra( DICT );
        String inviter = intent.getStringExtra( INVITER );
        int msgID = (null == inviter) ? R.string.invite_dict_missing_body_nonamef
            : R.string.invite_dict_missing_bodyf;
        String msg = context.getString( msgID, inviter, dict, langStr );

        return new AlertDialog.Builder( context )
            .setTitle( R.string.invite_dict_missing_title )
            .setMessage( msg)
            .setPositiveButton( R.string.button_download, onDownload )
            .setNegativeButton( R.string.button_decline, onDecline )
            .create();
    }

    public static void postMissingDictNotification( Context content, 
                                                    Intent intent, int id )
    {
        Utils.postNotification( content, intent, R.string.missing_dict_title, 
                                R.string.missing_dict_detail, id );
    }

    // resend the intent, but only if the dict it names is here.  (If
    // it's not, we may need to try again later, e.g. because our cue
    // was a focus gain.)
    static boolean returnOnDownload( Context context, Intent intent )
    {
        boolean downloaded = isMissingDictIntent( intent );
        if ( downloaded ) {
            int lang = intent.getIntExtra( LANG, -1 );
            String dict = intent.getStringExtra( DICT );
            downloaded = DictLangCache.haveDict( context, lang, dict );
            if ( downloaded ) {
                switch ( intent.getIntExtra( OWNER, -1 ) ) {
                case OWNER_SMS:
                    SMSService.onGameDictDownload( context, intent );
                    break;
                case OWNER_RELAY:
                    GamesList.onGameDictDownload( context, intent );
                    break;
                default:
                    DbgUtils.logf( "unexpected OWNER" );
                }
            }
        }
        return downloaded;
    }

}