/* -*- Compile-command: "make MEMDEBUG=TRUE -j3";-*- */ 
/* 
 * Copyright 2006 - 2012 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef XWFEATURE_BLUETOOTH

/*
  http://www.btessentials.com/examples/examples.html is good for some of this
  stuff.  Copyright allows free use.
*/

#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#if defined BT_USE_L2CAP
# include <bluetooth/l2cap.h>
#elif defined BT_USE_RFCOMM
# include <bluetooth/rfcomm.h>
#endif
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

#include "linuxbt.h"
#include "comms.h"
#include "strutils.h"
#include "uuidhack.h"
#include "gsrcwrap.h"
#include "gameref.h"
#include "device.h"

/* /\* begin new *\/ */
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

/* #include <gio/gio.h> */


#define MAX_CLIENTS 1
#define CHANNEL 1
#define SELF_NAME "localhost"

#if defined BT_USE_L2CAP
# define L2_RF_ADDR struct sockaddr_l2
#elif defined BT_USE_RFCOMM
# define L2_RF_ADDR struct sockaddr_rc
#endif

#define PROFILE_PATH "/org/bluez/my/profile"
#define UUID "00001101-0000-1000-8000-00805f9b34fb"

struct LinuxBTState {
    XP_BtAddrStr myBtAddr;
    GDBusConnection* conn;
} LinBtStuff;

static XP_Bool isLocalSend( const LaunchParams* params,
                            const XP_BtAddrStr* btAddr );

static void
on_dbus_closed( GDBusConnection *connection,
                gboolean XP_UNUSED(remote_peer_vanished),
                GError *error,
                gpointer user_data )
{
    LinuxBTState* btState = (LinuxBTState*)user_data;
    XP_ASSERT( connection == btState->conn );
    XP_LOGFF("D-Bus connection lost: %s", error ? error->message : "unknown");
    btState->conn = NULL;
    XP_ASSERT(0);
    // schedule reconnection
}

/* static void */
/* on_method_call( GDBusConnection *conn, */
/*                 const gchar *sender, */
/*                 const gchar *XP_UNUSED(object_path), */
/*                 const gchar *XP_UNUSED(interface_name), */
/*                 const gchar *method_name, */
/*                 GVariant *parameters, */
/*                 GDBusMethodInvocation *invocation, */
/*                 gpointer user_data ) */
/* { */
/*     LinuxBTState* btState = (LinuxBTState*)user_data; */
/*     XP_ASSERT( conn == btState->conn ); */
/*     XP_LOGFF( "(method_name: %s, sender: %s)", method_name, sender ); */
/*     if (g_strcmp0(method_name, "NewConnection") == 0) { */
/*         const gchar *device_path; */
/*         int fd; */
/*         GVariant *fd_props; */

/*         g_variant_get(parameters, "(&oh@a{sv})", &device_path, &fd, &fd_props); */
/*         g_print("NewConnection from %s, fd=%d\n", device_path, fd); */

/*         // make fd non-blocking (optional) */
/*         int flags = fcntl(fd, F_GETFL, 0); */
/*         fcntl(fd, F_SETFL, flags | O_NONBLOCK); */

/*         // Example: just read 1 byte (for demonstration) */
/*         char buf[128]; */
/*         ssize_t n = read(fd, buf, sizeof(buf)-1); */
/*         if (n > 0) { */
/*             buf[n] = 0; */
/*             g_print("Received: %s\n", buf); */
/*         } */

/*         g_variant_unref(fd_props); */

/*         // return to BlueZ */
/*         g_dbus_method_invocation_return_value(invocation, NULL); */
/*     } else if (g_strcmp0(method_name, "Release") == 0) { */
/*         g_print("Profile released\n"); */
/*         g_dbus_method_invocation_return_value(invocation, NULL); */
/*     } */
/* } */

/* static const GDBusInterfaceVTable vtable = { */
/*     .method_call = on_method_call, */
/*     .get_property = NULL, */
/*     .set_property = NULL */
/* }; */

static void*
serverProc( void* closure )
{
    LaunchParams* params = (LaunchParams*)closure;
    LOG_FUNC();
    struct sockaddr_rc loc_addr = {};

    int s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (s < 0) {
        perror("socket"); return NULL;
    }

    loc_addr.rc_family = AF_BLUETOOTH;
    bacpy(&loc_addr.rc_bdaddr, BDADDR_ANY);
    loc_addr.rc_channel = CHANNEL;

    if (bind(s, (struct sockaddr *)&loc_addr, sizeof(loc_addr)) < 0) {
        perror("bind"); close(s); goto out;
    }
    if (listen(s, 1) < 0) {
        perror("listen"); close(s); goto out;
    }

    for ( ; ; ) {
        XP_LOGFF("Server listening on RFCOMM channel %d...\n", CHANNEL);
        struct sockaddr_rc rem_addr = {0};
        socklen_t opt = sizeof(rem_addr);
        int client = accept(s, (struct sockaddr *)&rem_addr, &opt);
        if (client < 0) {
            perror("accept");
            close(s);
            break;
        }

        char addr_str[18] = {0};
        ba2str(&rem_addr.rc_bdaddr, addr_str);
        printf("Accepted connection from %s\n", addr_str);

        short hlen;
        read(client, &hlen, sizeof(hlen));
        XP_U16 len = ntohs(hlen);
        if ( 1024 < len ) {
            XP_LOGFF( "packet too large, at %d", len );
        } else {
            XP_U8 buf[len];
            ssize_t bytes = read( client, buf, len );
            if (bytes == len ) {
                XP_LOGFF("Received: %d bytes", len);
                dvc_parseBTPacket( params->dutil, NULL_XWE,
                                   buf, len, NULL, addr_str );
            } else {
                XP_LOGFF( "too few bytes? got %ld, expected %d", bytes, len );
            }
        }

        close(client);
    }
 out:
    close(s);
    return NULL;
}

static void
startListener( LaunchParams* params )
{
    pthread_t thread;
    (void)pthread_create( &thread, NULL, serverProc, params );
}

/* Looks like ChatGPT stole this from some MIT course notes */
static void
figureSelfAddress( LinuxBTState* btState )
{
    int hciSocket = hci_open_dev(hci_get_route(NULL));
    XP_LOGFF( "hciSocket: %d", hciSocket );

    // Create int and pointers to hold results for later
    if ( 0 < hciSocket ) {
        struct hci_dev_list_req *devList;

        // Allocate memory for devList pointer. Based on HCI_MAX_DEV (maximum
        // number of HCI devices) multiplied by the size of struct hci_dev_req,
        // plus the size of uint16_t (to store the device number)
        devList = (struct hci_dev_list_req *)
            malloc(HCI_MAX_DEV * sizeof(*devList) + sizeof(uint16_t));
        if (!devList) {
            XP_LOGFF("Failed to allocate HCI device request memory");
            goto exit;
        }

        devList->dev_num = HCI_MAX_DEV;

        // Send the HCIGETDEVLIST command to get the device list.
        if (ioctl(hciSocket, HCIGETDEVLIST, (void *)devList) < 0) {
            XP_LOGFF("Failed to get HCI device list");
            free(devList);
            goto exit;
        }
        int devCount = devList->dev_num;
        XP_LOGFF( "ioctl worked: got %d", devCount );

        for (int ii = 0; ii < devCount; ii++) {
            // Set the device ID for device info retrieval
            struct hci_dev_info devInfo = {
                .dev_id = devList->dev_req[ii].dev_id,
                    };
            if (ioctl(hciSocket, HCIGETDEVINFO, (void *)&devInfo) < 0) {
                XP_LOGFF("Failed to get HCI device info");
                continue;
            }

            ba2str(&devInfo.bdaddr, btState->myBtAddr.chars );
            XP_LOGFF( "got addr: %s", btState->myBtAddr.chars );
        }
    exit:
        close(hciSocket);
    }
}

static LinuxBTState*
lbt_make( LaunchParams* params )
{
    LinuxBTState* btState = XP_CALLOC( params->dutil->mpool,
                                       sizeof(*btState) );

    GError *error = NULL;
    btState->conn = g_bus_get_sync( G_BUS_TYPE_SYSTEM, NULL, &error );
    if (!btState->conn) {
        XP_LOGFF("Failed to connect to system bus: %s\n", error->message);
        XP_ASSERT(0);
    }
    g_signal_connect(btState->conn, "closed", G_CALLBACK(on_dbus_closed),
                     btState);
    XP_LOGFF( "connected using dbus conn" );

    figureSelfAddress(btState);

    startListener( params );

    return btState;
} /* lbt_make */

void
lbt_init( LaunchParams* params )
{
    LinuxBTState* btState = params->btState;
    if ( !btState ) {
        btState = params->btState = lbt_make( params );

        /* if ( amMaster ) { */
        /*     lbt_listenerSetup( cGlobals ); */
        /* } else { */
        /*     if ( btStuff->socket < 0 ) { */
        /*         XP_ASSERT(0);   /\* don't know if this works *\/ */
        /*         CommsAddrRec addr; */
        /*         XW_DUtilCtxt* dutil = cGlobals->params->dutil; */
        /*         gr_getSelfAddr( dutil, cGlobals->gr, NULL_XWE, &addr ); */
        /*         lbt_connectSocket( btStuff, &addr ); */
        /*     } */
        /* } */
    }
} /* lbt_open */

void
lbt_reset( LaunchParams* params )
{
    LOG_FUNC();
    lbt_destroy( params );
    lbt_init( params );
    LOG_RETURN_VOID();
}

void
lbt_destroy( LaunchParams* params )
{
    LinuxBTState* btState = params->btState;

    if ( !!btState ) {
        /* if ( btState->amMaster ) { */
            /* XP_LOGFF( "closing listener socket %d", btStuff->u.master.listener ); */
            /* /\* Remove from main event loop *\/ */
            /* (*cGlobals->addAcceptor)( -1, NULL, cGlobals, */
            /*                          &btStuff->u.master.listenerStorage ); */
            /* close( btStuff->u.master.listener ); */
            /* btStuff->u.master.listener = -1; */

            /* sdp_close( btStuff->u.master.session ); */
            /* XP_LOGFF( "sleeping for Palm's sake..." ); */
            /* sleep( 2 );         /\* see if this gives palm a chance to not hang *\/ */
        /* } */
        g_object_unref(btState->conn);
        XP_FREEP( params->dutil->mpool, &params->btState );
    }
} /* lbt_close */

/* static XP_S16 */
/* lbt_send_impl( const XP_U8* buf, XP_U16 buflen, */
/*                     const CommsAddrRec* addrP, */
/*                     CommonGlobals* cGlobals ) */
/* { */
/*     XP_S16 nSent = -1; */
/*     LinBtStuff* btStuff; */

/*     XP_LOGFF( "(len=%d)", buflen ); */
/*     LOG_HEX( buf, buflen, __func__ ); */

/*     btStuff = cGlobals->btStuff; */
/*     if ( !!btStuff ) { */
/*         CommsAddrRec addr; */
/*         if ( !addrP ) { */
/*             XW_DUtilCtxt* dutil = cGlobals->params->dutil; */
/*             gr_getSelfAddr( dutil, cGlobals->gr, NULL_XWE, &addr ); */
/*             addrP = &addr; */
/*         } */

/*         if ( btStuff->socket < 0  && !btStuff->amMaster ) { */
/*             lbt_connectSocket( btStuff, addrP ); */
/*         } */

/*         if ( btStuff->socket >= 0 ) { */
/* #if defined BT_USE_RFCOMM */
/*             unsigned short len = htons(buflen); */
/*             nSent = write( btStuff->socket, &len, sizeof(len) ); */
/*             assert( nSent == sizeof(len) ); */
/* #endif */
/*             nSent = write( btStuff->socket, buf, buflen ); */
/*             if ( nSent < 0 ) { */
/*                 XP_LOGFF( "send->%s", strerror(errno) ); */
/*             } else if ( nSent < buflen ) { */
/*                 XP_LOGFF( "sent only %d bytes of %d", nSent, buflen ); */
/*                 /\* Need to loop until sent if this is happening *\/ */
/*                 XP_ASSERT( 0 ); */
/*             } */
/*         } else { */
/*             XP_LOGFF( "socket still not set" ); */
/*         } */
/*     } */
/*     LOG_RETURNF( "%d", nSent ); */
/*     return nSent; */
/* } /\* lbt_send_impl *\/ */

typedef struct _BTSendData {
    LaunchParams* params;
    XP_U16 len;
    XP_U8* buf;
    XP_UCHAR hostName[64];
    XP_BtAddrStr btAddr;
} BTSendData;

static void
freeSendData( BTSendData* dp )
{
    g_free( dp->buf );
    g_free( dp );
}

static void*
sendProc( void* closure )
{
    BTSendData* dp = (BTSendData*)closure;
    XP_LOGFF( "(len: %d, hostName: %s, addr: %s)", dp->len, dp->hostName, dp->btAddr.chars );

    int s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (s < 0) {
        perror("socket");
        goto err;
    }

    struct sockaddr_rc addr = {
        .rc_family = AF_BLUETOOTH,
        .rc_channel = CHANNEL,
    };
    const char* addr_str = dp->btAddr.chars;
    if (str2ba(addr_str, &addr.rc_bdaddr) < 0) {
        XP_LOGFF( "Invalid Bluetooth address %s", addr_str);
        goto err;
    }

    XP_LOGFF("Connecting to %s on RFCOMM channel %d...", addr_str, CHANNEL);
    int status = connect(s, (struct sockaddr *)&addr, sizeof(addr));
    if (status == 0) {
        printf("Connected. Sending len plus %d bytes...", dp->len);
        short hlen = htons(dp->len);
        write( s, &hlen, sizeof(hlen) );
        write( s, dp->buf, dp->len );
        printf("Message sent.\n");
    } else {
        XP_LOGFF( "error connecting" );
        goto err;
    }

 err:
    close(s);
    freeSendData(dp);
    return NULL;
}

static int
sendToSelfIdle( void* closure )
{
    BTSendData* dp = (BTSendData*)closure;
    dvc_parseBTPacket( dp->params->dutil, NULL_XWE,
                       dp->buf, dp->len,
                       dp->hostName, dp->btAddr.chars );
    freeSendData( dp );
    return 0;                   /* don't run again */
}

XP_S16
lbt_send( LaunchParams* params, const XP_U8* buf, XP_U16 len,
          const XP_UCHAR* hostName, const XP_BtAddrStr* btAddr )
{
    LinuxBTState* btState = params->btState;
    if ( !!btState ) {
        XP_LOGFF( "(len: %d, hostName: %s, addr: %s)", len, hostName, btAddr->chars );
        BTSendData* dp = g_malloc0( sizeof(*dp) );
        dp->len = len;
        dp->buf = g_malloc(len);
        memcpy( dp->buf, buf, len );
        strcat( dp->hostName, hostName );
        dp->btAddr = *btAddr;
        dp->params = params;

        if ( isLocalSend(params, btAddr) ) {
            g_idle_add( sendToSelfIdle, dp );
        } else {
            pthread_t thread;
            (void)pthread_create( &thread, NULL, sendProc, dp );
        }
    } else {
        XP_LOGFF( "no btState; shutting down?" );
    }
    return -1;
}

#if defined BT_USE_RFCOMM
static void
read_all( int sock, unsigned char* buf, const int len )
{
    int totalRead = 0;
    while ( totalRead < len ) {
        int nRead = read( sock, buf+totalRead, len-totalRead );
        if ( nRead < 0 ) {
            XP_LOGFF( "read->%s", strerror(errno) );
            break;
        }
        totalRead += nRead;
        XP_ASSERT( totalRead <= len );
    }
}
#endif

#define RESPMAX 20

typedef gboolean (*bt_proc)( const bdaddr_t* bdaddr, int socket, void* closure );

static void
btDevsIterate( bt_proc proc, void* closure )
{
    LOG_FUNC();
    int id = hci_get_route( NULL );
    int socket = hci_open_dev( id );
    XP_LOGFF( "id: %d; socket: %d", id, socket );
    if ( id >= 0 && socket >= 0 ) {
        int flags = IREQ_CACHE_FLUSH;
        inquiry_info* iinfo = (inquiry_info*)
            malloc( RESPMAX * sizeof(inquiry_info) );
        int count = hci_inquiry( id, 8/*wait seconds*/,
                                 RESPMAX, NULL, &iinfo, flags );
        XP_LOGFF( "hci_inquiry=>%d", count );
        int ii;
        if ( 0 < count ) {
            for ( ii = 0; ii < count; ++ii ) {
                const bdaddr_t* bdaddr = &iinfo[ii].bdaddr;
                if ( !(*proc)( bdaddr, socket, closure ) ) {
                    break;
                }
            }
        } else {
            close( socket );
        }
    }
}

typedef struct _get_ba_data {
    const char* name;
    bdaddr_t* ba;
    gboolean success;
} get_ba_data;

static gboolean
get_ba_proc( const bdaddr_t* bdaddr, int socket, void* closure )
{
    get_ba_data* data = (get_ba_data*)closure;
    char buf[64];
    if ( 0 >= hci_read_remote_name( socket, bdaddr, sizeof(buf), buf, 0 ) ) {
        if ( 0 == strcmp( buf, data->name ) ) {
            XP_MEMCPY( data->ba, bdaddr, sizeof(*data->ba) );
            data->success = XP_TRUE;
            XP_LOGFF( "matched %s", data->name );
            char addrStr[32];
            ba2str( data->ba, addrStr );
            XP_LOGFF( "bt_addr is %s", addrStr );
        }
    }
    return !data->success;
}

XP_Bool
nameToBtAddr( const char* name, bdaddr_t* ba )
{
    LOG_FUNC();
    get_ba_data data = { .name = name, .ba = ba, .success = FALSE };
    btDevsIterate( get_ba_proc, &data );
    return data.success;
} /* nameToBtAddr */

/* static gboolean */
/* append_name_proc( const bdaddr_t* bdaddr, int socket, void* closure ) */
/* { */
/*     GSList** list = (GSList**)closure; */
/*     char buf[64]; */
/*     char addr[19] = { 0 }; */
/*     ba2str( bdaddr, addr ); */
/*     XP_LOGFF( "adding %s", addr ); */
/*     if ( 0 >= hci_read_remote_name( socket, bdaddr, sizeof(buf), buf, 0 ) ) { */
/*         gchar* name = g_strdup( buf ); */
/*         *list = g_slist_append( *list, name ); */
/*     } */
/*     return TRUE; */
/* } */

/* static void */
/* on_interfaces_added( GDBusObjectManager* XP_UNUSED(manager), */
/*                      GDBusObject *object, */
/*                      GDBusInterface *interface, */
/*                      gpointer user_data ) */
/* { */
/*     LinuxBTState* btState = (LinuxBTState*)user_data; */
/*     XP_USE(btState); */
/*     const gchar *path = g_dbus_object_get_object_path(object); */
/*     if (g_str_has_prefix(path, "/org/bluez/hci0/dev_")) { */
/*         GVariant *props = g_dbus_proxy_get_cached_property( */
/*             G_DBUS_PROXY(interface), "Address"); */
/*         if (props) { */
/*             const gchar *addr = g_variant_get_string(props, NULL); */
/*             XP_LOGFF("Discovered device: %s (%s)\n", addr, path); */
/*             g_variant_unref(props); */
/*         } */
/*     } */
/* } */

GSList*
lbt_scan( LaunchParams* params )
{
    GSList* result = NULL;
    LinuxBTState* btState = params->btState;
    GError *error = NULL;
    GDBusObjectManager* manager =
        g_dbus_object_manager_client_new_sync
        ( btState->conn,
          G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
          "org.bluez",      // service
          "/",               // root path
          NULL, NULL, NULL,
          NULL, &error);
    XP_ASSERT( !!manager );

    GList *objects = g_dbus_object_manager_get_objects(manager);
    for (GList *l = objects; l != NULL; l = l->next) {
        GDBusObject *obj = l->data;

        GDBusInterface *iface = g_dbus_object_get_interface(obj, "org.bluez.Device1");
        if (!iface) continue;

        GVariant *paired_var = g_dbus_proxy_get_cached_property(G_DBUS_PROXY(iface), "Paired");
        if (!paired_var || !g_variant_get_boolean(paired_var)) {
            g_variant_unref(paired_var);
            continue; // skip non-paired
        }

        GVariant *addr_var = g_dbus_proxy_get_cached_property(G_DBUS_PROXY(iface), "Address");
        GVariant *name_var = g_dbus_proxy_get_cached_property(G_DBUS_PROXY(iface), "Name");

        const gchar* name = g_variant_get_string(name_var, NULL);
        const gchar* addr = g_variant_get_string(addr_var, NULL);
        XP_LOGFF("Paired device: %s (%s)", name, addr );

        BTHostPair* hp = g_malloc0( sizeof(*hp) );
        strcat( hp->hostName, name );
        strcat( hp->btAddr.chars, addr );
        result = g_slist_append( result, hp );

        g_variant_unref(paired_var);
        g_variant_unref(addr_var);
        g_variant_unref(name_var);
    }
    g_list_free_full(objects, g_object_unref);
    return result;
}

void
lbt_freeScan( LaunchParams* params, GSList* list )
{
    XP_USE(params);
    XP_USE(list);
}

void
lbt_setToSelf( LaunchParams* params, BTHostPair* hp )
{
    strcpy( hp->hostName, SELF_NAME );
    LinuxBTState* btState = params->btState;
    strcpy( hp->btAddr.chars, btState->myBtAddr.chars );
}

static XP_Bool
isLocalSend( const LaunchParams* params, const XP_BtAddrStr* btAddr )
{
    LinuxBTState* btState = params->btState;
    return 0 == strcmp( btState->myBtAddr.chars, btAddr->chars );
}

/* static gboolean */
/* bt_socket_proc( GIOChannel* source, GIOCondition condition, gpointer data ) */
/* { */
/*     XP_USE( data ); */
/*     int fd = g_io_channel_unix_get_fd( source ); */
/*     if ( 0 != (G_IO_IN & condition) ) { */
/*         unsigned char buf[1024]; */
/* #ifdef DEBUG */
/*         XP_S16 nBytes =  */
/* #endif */
/*             lbt_receive( fd, buf, sizeof(buf) ); */
/*         XP_ASSERT(nBytes==2 || XP_TRUE); */

/*         XP_ASSERT(0);           /\* not implemented beyond this point *\/ */
/*     } else { */
/*         XP_ASSERT(0);           /\* not implemented beyond this point *\/ */
/*     } */
/*     return FALSE; */
/* } */

#endif /* XWFEATURE_BLUETOOTH */
