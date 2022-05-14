/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009-2020 by Eric House (xwords@eehouse.org).  All
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

package org.eehouse.android.xw4.jni;

import android.content.Context;
import android.text.TextUtils;

import org.eehouse.android.xw4.Assert;
import org.eehouse.android.xw4.BTUtils;
import org.eehouse.android.xw4.BuildConfig;
import org.eehouse.android.xw4.DbgUtils;
import org.eehouse.android.xw4.GameUtils;
import org.eehouse.android.xw4.Log;
import org.eehouse.android.xw4.NFCUtils;
import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.SMSPhoneInfo;
import org.eehouse.android.xw4.Utils;
import org.eehouse.android.xw4.WiDirService;
import org.eehouse.android.xw4.WiDirWrapper;
import org.eehouse.android.xw4.XWPrefs;
import org.eehouse.android.xw4.loc.LocUtils;

import java.net.InetAddress;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.io.Serializable;

public class CommsAddrRec implements Serializable {
    private static final String TAG = CommsAddrRec.class.getSimpleName();

    public enum CommsConnType {
        _COMMS_CONN_NONE,
        COMMS_CONN_IR,
        COMMS_CONN_IP_DIRECT,
        COMMS_CONN_RELAY(!BuildConfig.NO_NEW_RELAY),
        COMMS_CONN_BT,
        COMMS_CONN_SMS,
        COMMS_CONN_P2P,
        COMMS_CONN_NFC(false),
        COMMS_CONN_MQTT(BuildConfig.NO_NEW_RELAY);

        private boolean mIsSelectable = true;

        private CommsConnType(boolean isSelectable) {
            mIsSelectable = isSelectable;
        }

        private CommsConnType() {
            this(true);
        }

        public boolean isSelectable() { return mIsSelectable; }

        public String longName( Context context )
        {
            int id = 0;
            switch( this ) {
            case COMMS_CONN_RELAY:
                id = R.string.connstat_relay; break;
            case COMMS_CONN_BT:
                id = R.string.invite_choice_bt; break;
            case COMMS_CONN_SMS:
                id = R.string.invite_choice_data_sms; break;
            case COMMS_CONN_P2P:
                id = R.string.invite_choice_p2p; break;
            case COMMS_CONN_NFC:
                id = R.string.invite_choice_nfc; break;
            case COMMS_CONN_MQTT:
                id = R.string.invite_choice_mqtt; break;
            default:
                Assert.failDbg();
            }

            return ( 0 == id ) ? toString() : LocUtils.getString( context, id );
        }

        public String shortName()
        {
            String[] parts = TextUtils.split( toString(), "_" );
            return parts[parts.length - 1];
        }
    };

    // Pairs how and name of device in that context
    public static class ConnExpl implements Serializable {
        public final CommsConnType mType;
        public final String mName;

        public ConnExpl( CommsConnType typ, String name )
        {
            mType = typ;
            mName = name;
        }

        public String getUserExpl( Context context )
        {
            Assert.assertTrueNR(  BuildConfig.NON_RELEASE );
            return String.format( "(Msg src: {%s: %s})", mType, mName );
        }
    }

    public static class CommsConnTypeSet extends HashSet<CommsConnType> {
        private static final int BIT_VECTOR_MASK = 0x8000;

        public CommsConnTypeSet() { this(BIT_VECTOR_MASK); }

        public CommsConnTypeSet( final int inBits )
        {
            boolean isVector = 0 != (BIT_VECTOR_MASK & inBits);
            int bits = inBits & ~BIT_VECTOR_MASK;
            CommsConnType[] values = CommsConnType.values();
            // Deal with games saved before I added the BIT_VECTOR_MASK back
            // in. This should be removable before ship. Or later of course.
            if ( !isVector && bits >= values.length ) {
                isVector = true;
            }
            if ( isVector ) {
                for ( CommsConnType value : values ) {
                    int ord = value.ordinal();
                    if ( 0 != (bits & (1 << (ord - 1)))) {
                        add( value );
                        if ( BuildConfig.NON_RELEASE
                             && CommsConnType.COMMS_CONN_RELAY == value ) {
                            // I've seen this....
                            Log.e( TAG, "still have RELAY bit" );
                            DbgUtils.printStack( TAG );
                        }
                    }
                }
            } else if ( bits < values.length ) { // don't crash
                add( values[bits] );
            } else {
                Log.e( TAG, "<init>: bad bits value: 0x%x", inBits );
            }
        }

        public int toInt()
        {
            int result = BIT_VECTOR_MASK;
            for ( Iterator<CommsConnType> iter = iterator(); iter.hasNext(); ) {
                CommsConnType typ = iter.next();
                result |= 1 << (typ.ordinal() - 1);
            }
            return result;
        }

        /**
         * Return supported types in display order, i.e. with the easiest to
         * use or most broadly useful first. DATA_SMS comes last because it
         * depends on permissions that are banned on PlayStore variants of the
         * game.
         *
         * @return ordered list of types supported by this device as
         * configured.
         */
        public static List<CommsConnType> getSupported( Context context )
        {
            List<CommsConnType> supported = new ArrayList<>();
            supported.add( CommsConnType.COMMS_CONN_RELAY );
            supported.add( CommsConnType.COMMS_CONN_MQTT );
            if ( BTUtils.BTAvailable() ) {
                supported.add( CommsConnType.COMMS_CONN_BT );
            }
            if ( WiDirWrapper.enabled() ) {
                supported.add( CommsConnType.COMMS_CONN_P2P );
            }
            if ( Utils.isGSMPhone( context ) ) {
                supported.add( CommsConnType.COMMS_CONN_SMS );
            }
            if ( NFCUtils.nfcAvail( context )[0] ) {
                supported.add( CommsConnType.COMMS_CONN_NFC );
            }
            return supported;
        }

        public static void removeUnsupported( Context context,
                                              CommsConnTypeSet set )
        {
            // Remove anything no longer supported. This probably only
            // happens when key_force_radio is being messed with
            List<CommsConnType> supported = getSupported( context );
            for ( CommsConnType typ : set.getTypes() ) {
                if ( ! supported.contains( typ ) ) {
                    set.remove( typ );
                }
            }
        }

        // Called from jni world, where making and using an iterator is too
        // much trouble.
        public CommsConnType[] getTypes()
        {
            return toArray( s_hint );
        }

        @Override
        public boolean add( CommsConnType typ )
        {
            // DbgUtils.logf( "CommsConnTypeSet.add(%s)", typ.toString() );
            // Assert.assertFalse( CommsConnType._COMMS_CONN_NONE == typ );
            boolean result = CommsConnType._COMMS_CONN_NONE == typ ? true
                : super.add( typ );
            return result;
        }

        @Override
        public String toString()
        {
            if ( BuildConfig.NON_RELEASE ) {
                List<String> tmp = new ArrayList<>();
                for ( CommsConnType typ : getTypes() ) {
                    tmp.add( typ.toString() );
                }
                return TextUtils.join(",", tmp );
            } else {
                return super.toString();
            }
        }

        public String toString( Context context, boolean longVersion )
        {
            String result;
            CommsConnType[] types = getTypes();
            if ( 0 == types.length ) {
                result = LocUtils.getString( context, R.string.note_none );
            } else {
                List<String> strs = new ArrayList<>();
                for ( CommsConnType typ : types ) {
                    if ( typ.isSelectable() ) {
                        String str = longVersion?
                            typ.longName( context ) : typ.shortName();
                        strs.add( str );
                    }
                }
                String sep = longVersion ? " + " : ",";
                result = TextUtils.join( sep, strs );
            }
            return result;
        }

        private static final CommsConnType[] s_hint = new CommsConnType[0];
    }

    public CommsConnTypeSet conTypes;

    // relay case
    public String ip_relay_invite;
    public String ip_relay_hostName;
    public InetAddress ip_relay_ipAddr;    // a cache, maybe unused in java
    public int ip_relay_port;
    public boolean ip_relay_seeksPublicRoom;
    public boolean ip_relay_advertiseRoom;

    // bt case
    public String bt_hostName;
    public String bt_btAddr;

    // sms case
    public String sms_phone;
    public int sms_port;                // SMS port, if they still use those

    // wifi-direct
    public String p2p_addr;

    // mqtt
    public String mqtt_devID;

    public CommsAddrRec( CommsConnType cTyp )
    {
        this();
        conTypes.add( cTyp );
    }

    public CommsAddrRec()
    {
        conTypes = new CommsConnTypeSet();
    }

    public CommsAddrRec( CommsConnTypeSet types )
    {
        this();
        conTypes.addAll( types );
    }

    public CommsAddrRec( String host, int port )
    {
        this( CommsConnType.COMMS_CONN_RELAY );
        setRelayParams( host, port );
    }

    public CommsAddrRec( String btName, String btAddr )
    {
        this( CommsConnType.COMMS_CONN_BT );
        setBTParams( btAddr, btName );
    }

    public CommsAddrRec( String phone )
    {
        this( CommsConnType.COMMS_CONN_SMS );
        sms_phone = phone;
        sms_port = 2;           // something other that 0 (need to fix comms)
    }

    public CommsAddrRec( final CommsAddrRec src )
    {
        this.copyFrom( src );
    }

    public boolean contains( CommsConnType typ )
    {
        return null != conTypes && conTypes.contains( typ );
    }

    public void setRelayParams( String host, int port, String room )
    {
        setRelayParams( host, port );
        ip_relay_invite = room;
    }

    public void setRelayParams( String host, int port )
    {
        ip_relay_hostName = host;
        ip_relay_port = port;
        ip_relay_seeksPublicRoom = false;
        ip_relay_advertiseRoom = false;
    }

    public void setBTParams( String btAddr, String btName )
    {
        bt_hostName = btName;
        if ( ! BTUtils.isBogusAddr( btAddr ) ) {
            bt_btAddr = btAddr;
        }
    }

    public void setSMSParams( String phone )
    {
        sms_phone = phone;
        sms_port = 1;           // so don't assert in comms....
    }

    public CommsAddrRec setP2PParams( String macAddress )
    {
        p2p_addr = macAddress;
        return this;
    }

    public CommsAddrRec setMQTTParams( String devID )
    {
        mqtt_devID = devID;
        return this;
    }

    public void populate( Context context, CommsConnTypeSet newTypes )
    {
        for ( CommsConnType typ : newTypes.getTypes() ) {
            if ( ! conTypes.contains( typ ) ) {
                conTypes.add( typ );
                addTypeDefaults( context, typ );
            }
        }
    }

    public void populate( Context context )
    {
        for ( CommsConnType typ : conTypes.getTypes() ) {
            addTypeDefaults( context, typ );
        }
    }

    public void remove( CommsConnType typ )
    {
        conTypes.remove( typ );
    }

    public boolean changesMatter( final CommsAddrRec other )
    {
        boolean matter = ! conTypes.equals( other.conTypes );
        Iterator<CommsConnType> iter = conTypes.iterator();
        while ( !matter && iter.hasNext() ) {
            CommsConnType conType = iter.next();
            switch( conType ) {
            case COMMS_CONN_RELAY:
                    matter = null == ip_relay_invite
                        || ! ip_relay_invite.equals( other.ip_relay_invite )
                        || ! ip_relay_hostName.equals( other.ip_relay_hostName )
                        || ip_relay_port != other.ip_relay_port;
                break;
            default:
                Log.w( TAG, "changesMatter: not handling case: %s",
                       conType.toString() );
                break;
            }
        }
        return matter;
    }

    private void copyFrom( CommsAddrRec src )
    {
        conTypes = src.conTypes;
        ip_relay_invite = src.ip_relay_invite;
        ip_relay_hostName = src.ip_relay_hostName;
        ip_relay_port = src.ip_relay_port;
        ip_relay_seeksPublicRoom = src.ip_relay_seeksPublicRoom;
        ip_relay_advertiseRoom = src.ip_relay_advertiseRoom;

        bt_hostName = src.bt_hostName;
        bt_btAddr = src.bt_btAddr;

        sms_phone = src.sms_phone;
        sms_port = src.sms_port;
    }

    private void addTypeDefaults( Context context, CommsConnType typ )
    {
        switch ( typ ) {
        case COMMS_CONN_RELAY:
            Assert.failDbg();
            break;
        case COMMS_CONN_BT:
            String[] strs = BTUtils.getBTNameAndAddress();
            if ( null != strs ) {
                bt_hostName = strs[0];
                bt_btAddr = strs[1];
            }
            break;
        case COMMS_CONN_SMS:
            SMSPhoneInfo pi = SMSPhoneInfo.get( context );
            // Do we have phone permission? If not, shouldn't be set at all!
            if ( null != pi ) {
                sms_phone = pi.number;
                sms_port = 3;   // fix comms already...
            }
            break;
        case COMMS_CONN_P2P:
            p2p_addr = WiDirService.getMyMacAddress( context );
            break;
        case COMMS_CONN_MQTT:
            mqtt_devID = XwJNI.dvc_getMQTTDevID( null );
            break;
        case COMMS_CONN_NFC:
            break;
        default:
            Assert.failDbg();
        }
    }
}
