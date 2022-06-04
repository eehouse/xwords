/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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

import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.os.Bundle;

import java.util.Collections;
import java.util.concurrent.ConcurrentHashMap;
import java.util.Set;

import org.eehouse.android.xw4.loc.LocUtils;

public class MultiService {
    private static final String TAG = MultiService.class.getSimpleName();

    public static final String FORCECHANNEL = "FC";
    public static final String LANG = "LANG";
    public static final String ISO = "ISO";
    public static final String DICT = "DICT";
    public static final String GAMEID = "GAMEID";
    public static final String INVITEID = "INVITEID"; // relay only
    public static final String ROOM = "ROOM";
    public static final String GAMENAME = "GAMENAME";
    public static final String NPLAYERST = "NPLAYERST";
    public static final String NPLAYERSH = "NPLAYERSH";
    public static final String REMOTES_ROBOTS = "RR";
    public static final String INVITER = "INVITER";
    private static final String OWNER = "OWNER";
    public static final String BT_NAME = "BT_NAME";
    public static final String BT_ADDRESS = "BT_ADDRESS";
    public static final String P2P_MAC_ADDRESS = "P2P_MAC_ADDRESS";
    public static final String MQTT_DEVID = "MQTT_DEVID";
    private static final String NLI_DATA = "nli";
    public static final String DUPEMODE = "du";

    public enum DictFetchOwner { _NONE,
                                 OWNER_SMS,
                                 OWNER_RELAY,
                                 OWNER_BT,
                                 OWNER_P2P,
                                 OWNER_MQTT,
    };

    private static final String ACTION_FETCH_DICT = "_afd";
    private static final String FOR_MISSING_DICT = "_fmd";

    private Set<MultiEventListener> m_lis = Collections
        .newSetFromMap(new ConcurrentHashMap<MultiEventListener, Boolean>());

    // these do not currently pass between devices so they can change.
    public enum MultiEvent { _INVALID,
                             BAD_PROTO_BT,
                             BAD_PROTO_SMS,
                             APP_NOT_FOUND_BT,
                             BT_ENABLED,
                             BT_DISABLED,
                             NEWGAME_SUCCESS,
                             NEWGAME_FAILURE,
                             NEWGAME_DUP_REJECTED,
                             MESSAGE_ACCEPTED,
                             MESSAGE_REFUSED,
                             MESSAGE_NOGAME,
                             MESSAGE_RESEND,
                             MESSAGE_FAILOUT,
                             MESSAGE_DROPPED,

                             SMS_RECEIVE_OK,
                             SMS_SEND_OK,
                             SMS_SEND_FAILED,
                             SMS_SEND_FAILED_NORADIO,
                             SMS_SEND_FAILED_NOPERMISSION,

                             BT_GAME_CREATED,

                             RELAY_ALERT,
    };

    public interface MultiEventListener {
        public void eventOccurred( MultiEvent event, Object ... args );
    }
    // public interface MultiEventSrc {
    //     public void setBTEventListener( BTEventListener li );
    // }

    public void setListener( MultiEventListener li )
    {
        m_lis.add( li );
    }

    public void clearListener( MultiEventListener li )
    {
        Assert.assertTrue( m_lis.contains( li ) || ! BuildConfig.DEBUG );
        m_lis.remove( li );
    }

    public int postEvent( MultiEvent event, Object ... args )
    {
        // don't just return size(): concurrency doesn't guarantee isn't
        // changed
        int count = 0;
        for ( MultiEventListener listener : m_lis ) {
            listener.eventOccurred( event, args );
            ++count;
        }
        return count;
    }

    public static Intent makeMissingDictIntent( Context context, NetLaunchInfo nli,
                                                DictFetchOwner owner )
    {
        Intent intent = new Intent( context, MainActivity.class ); // PENDING TEST THIS!!!
        intent.setAction( ACTION_FETCH_DICT );
        intent.putExtra( ISO, nli.isoCode );
        intent.putExtra( DICT, nli.dict );
        intent.putExtra( OWNER, owner.ordinal() );
        intent.putExtra( NLI_DATA, nli.toString() );
        intent.putExtra( FOR_MISSING_DICT, true );
        return intent;
    }

    public static boolean isMissingDictBundle( Bundle args )
    {
        boolean result = args.getBoolean( FOR_MISSING_DICT, false );
        return result;
    }

    public static boolean isMissingDictIntent( Intent intent )
    {
        String action = intent.getAction();
        boolean result = null != action && action.equals( ACTION_FETCH_DICT )
            && isMissingDictBundle( intent.getExtras() );
        // DbgUtils.logf( "isMissingDictIntent(%s) => %b", intent.toString(), result );
        return result;
    }

    public static NetLaunchInfo getMissingDictData( Context context,
                                                    Intent intent )
    {
        Assert.assertTrue( isMissingDictIntent( intent ) );
        String nliData = intent.getStringExtra( NLI_DATA );
        NetLaunchInfo nli = NetLaunchInfo.makeFrom( context, nliData );
        Assert.assertTrue( nli != null || !BuildConfig.DEBUG );
        return nli;
    }

    public static Dialog missingDictDialog( Context context, Intent intent,
                                            OnClickListener onDownload,
                                            OnClickListener onDecline )
    {
        int lang = intent.getIntExtra( LANG, -1 );
        String isoCode = intent.getStringExtra( ISO );
        String langStr = DictLangCache.getLangNameForISOCode( context, isoCode );
        String dict = intent.getStringExtra( DICT );
        String inviter = intent.getStringExtra( INVITER );
        int msgID = (null == inviter) ? R.string.invite_dict_missing_body_noname_fmt
            : R.string.invite_dict_missing_body_fmt;
        String msg = LocUtils.getString( context, msgID, inviter, dict,
                                         LocUtils.xlateLang( context, langStr));

        return LocUtils.makeAlertBuilder( context )
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
    public static boolean returnOnDownload( Context context, Intent intent )
    {
        boolean downloaded = isMissingDictIntent( intent );
        if ( downloaded ) {
            String isoCode = intent.getStringExtra( ISO );
            String dict = intent.getStringExtra( DICT );
            downloaded = DictLangCache.haveDict( context, isoCode, dict );
            if ( downloaded ) {
                int ordinal = intent.getIntExtra( OWNER, -1 );
                if ( -1 == ordinal ) {
                    Log.w( TAG, "unexpected OWNER" );
                } else {
                    DictFetchOwner owner = DictFetchOwner.values()[ordinal];
                    switch ( owner ) {
                    case OWNER_SMS:
                        NBSProto.onGameDictDownload( context, intent );
                        break;
                    case OWNER_RELAY:
                    case OWNER_BT:
                    case OWNER_MQTT:
                        GamesListDelegate.onGameDictDownload( context, intent );
                        break;
                    default:
                        Assert.failDbg();
                    }
                }
            }
        }
        return downloaded;
    }
}
