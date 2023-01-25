/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2023 by Eric House (xwords@eehouse.org).  All rights
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
import android.telephony.TelephonyManager;

public class SMSPhoneInfo {
    private static final String TAG = SMSPhoneInfo.class.getSimpleName();

    public boolean isPhone;
    public String number;
    public boolean isGSM;

    public SMSPhoneInfo( boolean isAPhone, String num, boolean gsm )
    {
        isPhone = isAPhone;
        number = num;
        isGSM = gsm;
    }

    private static SMSPhoneInfo s_phoneInfo;
    public static SMSPhoneInfo get( Context context )
    {
        if ( null == s_phoneInfo ) {
            try {
                String number = null;
                boolean isGSM = false;
                boolean isPhone = false;
                TelephonyManager mgr = (TelephonyManager)
                    context.getSystemService(Context.TELEPHONY_SERVICE);
                if ( null != mgr ) {
                    number = mgr.getLine1Number(); // needs permission
                    int type = mgr.getPhoneType();
                    isGSM = TelephonyManager.PHONE_TYPE_GSM == type;
                    isPhone = true;
                }

                String radio =
                    XWPrefs.getPrefsString( context, R.string.key_force_radio );
                int[] ids = { R.string.radio_name_real,
                              R.string.radio_name_tablet,
                              R.string.radio_name_gsm,
                              R.string.radio_name_cdma,
                };

                // default so don't crash before set
                int id = R.string.radio_name_real;
                for ( int ii = 0; ii < ids.length; ++ii ) {
                    if ( radio.equals(context.getString(ids[ii])) ) {
                        id = ids[ii];
                        break;
                    }
                }

                switch( id ) {
                case R.string.radio_name_real:
                    break;          // go with above
                case R.string.radio_name_tablet:
                    number = null;
                    isPhone = false;
                    break;
                case R.string.radio_name_gsm:
                case R.string.radio_name_cdma:
                    isGSM = id == R.string.radio_name_gsm;
                    if ( null == number ) {
                        number = "000-000-0000";
                    }
                    isPhone = true;
                    break;
                }

                s_phoneInfo = new SMSPhoneInfo( isPhone, number, isGSM );
            } catch ( SecurityException se ) {
                Log.e( TAG, "got SecurityException: %s", se );
            }
        }
        return s_phoneInfo;
    }

    public static void reset()
    {
        s_phoneInfo = null;
    }
}
