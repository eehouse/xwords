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

#define MAX_CLIENTS 1

#if defined BT_USE_L2CAP
# define L2_RF_ADDR struct sockaddr_l2
#elif defined BT_USE_RFCOMM
# define L2_RF_ADDR struct sockaddr_rc
#endif

#define PROFILE_PATH "/org/bluez/my/profile"
#define UUID "00001101-0000-1000-8000-00805f9b34fb"

/* LinuxBTState* */
/* lbt_init( LaunchParams* params ) */
/* { */
/* } */

/* void */
/* lbt_destroy( LinuxBTState** state ) */
/* { */
/* } */

struct LinuxBTState {
    union {
        struct {
            int listener;               /* socket */
            void* listenerStorage;
            XP_Bool threadDie;
            sdp_session_t* session;
        } master;
    } u;

    /* A single socket's fine as long as there's only one client allowed. */
    // int socket;
    // XP_Bool amMaster;

    GDBusConnection* conn;
} LinBtStuff;

/* static gboolean bt_socket_proc( GIOChannel* source, GIOCondition condition,  */
/*                                 gpointer data ); */


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

static void
on_method_call( GDBusConnection *conn,
                const gchar *sender,
                const gchar *XP_UNUSED(object_path),
                const gchar *XP_UNUSED(interface_name),
                const gchar *method_name,
                GVariant *parameters,
                GDBusMethodInvocation *invocation,
                gpointer user_data )
{
    LinuxBTState* btState = (LinuxBTState*)user_data;
    XP_ASSERT( conn == btState->conn );
    XP_LOGFF( "(method_name: %s, sender: %s)", method_name, sender );
    if (g_strcmp0(method_name, "NewConnection") == 0) {
        const gchar *device_path;
        int fd;
        GVariant *fd_props;

        g_variant_get(parameters, "(&oh@a{sv})", &device_path, &fd, &fd_props);
        g_print("NewConnection from %s, fd=%d\n", device_path, fd);

        // make fd non-blocking (optional)
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        // Example: just read 1 byte (for demonstration)
        char buf[128];
        ssize_t n = read(fd, buf, sizeof(buf)-1);
        if (n > 0) {
            buf[n] = 0;
            g_print("Received: %s\n", buf);
        }

        g_variant_unref(fd_props);

        // return to BlueZ
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else if (g_strcmp0(method_name, "Release") == 0) {
        g_print("Profile released\n");
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
}

static const GDBusInterfaceVTable vtable = {
    .method_call = on_method_call,
    .get_property = NULL,
    .set_property = NULL
};

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

    // Introspect XML to create interface skeleton
    const gchar profile_xml[] =
        "<node>"
        "  <interface name='org.bluez.Profile1'>"
        "    <method name='NewConnection'>"
        "      <arg type='o' name='device' direction='in'/>"
        "      <arg type='h' name='fd' direction='in'/>"
        "      <arg type='a{sv}' name='fd_properties' direction='in'/>"
        "    </method>"
        "    <method name='RequestDisconnection'>"
        "      <arg type='o' name='device' direction='in'/>"
        "    </method>"
        "    <method name='Release'/>"
        "  </interface>"
        "</node>";

    GDBusNodeInfo *introspection =
        g_dbus_node_info_new_for_xml( profile_xml, &error );
    if (!introspection) {
        XP_LOGFF("Failed to parse introspection XML: %s", error->message);
        XP_ASSERT(0);
    }

    // Export the object path first
    guint reg_id = g_dbus_connection_register_object
        ( btState->conn,
          PROFILE_PATH,
          introspection->interfaces[0],
          &vtable,
          btState,  // user_data
          NULL,  // user_data_free_func
          &error );

    if (!reg_id) {
        XP_LOGFF("Failed to register object path: %s", error->message);
        XP_ASSERT(0);
    }

    // Register profile
    // Build the dictionary inline
    GVariantBuilder dict_builder;
    g_variant_builder_init(&dict_builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&dict_builder, "{sv}", "AutoConnect", g_variant_new_boolean(TRUE));
    g_variant_builder_add(&dict_builder, "{sv}", "Role", g_variant_new_string("server"));
    g_variant_builder_add(&dict_builder, "{sv}", "Channel", g_variant_new_uint16(1));
    g_variant_builder_add(&dict_builder, "{sv}", "Service", g_variant_new_string(UUID));
    g_variant_builder_add(&dict_builder, "{sv}", "Name", g_variant_new_string("EricsServer"));

    // Build the tuple with object path, UUID, and dictionary
    GVariantBuilder tuple_builder;
    g_variant_builder_init(&tuple_builder, G_VARIANT_TYPE("(osa{sv})"));
    g_variant_builder_add(&tuple_builder, "o", PROFILE_PATH);
    g_variant_builder_add(&tuple_builder, "s", UUID);
    g_variant_builder_add(&tuple_builder, "a{sv}", &dict_builder);
    GVariant *profile_opts = g_variant_builder_end(&tuple_builder);
    XP_ASSERT( !!profile_opts );

    GVariant* result = g_dbus_connection_call_sync
        (btState->conn, "org.bluez", "/org/bluez",
         "org.bluez.ProfileManager1", "RegisterProfile",
         profile_opts, NULL,
         G_DBUS_CALL_FLAGS_NONE,
         -1, NULL, &error);

    if (!result) {
        XP_LOGFF("RegisterProfile failed: %s", error->message);
        XP_ASSERT(0);
    }

    XP_LOGFF("Profile registered, waiting for connections...");

    // Listen for new connections
    // g_signal_connect( btState->conn, "signal", G_CALLBACK(on_new_connection), btState );
    
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

XP_S16
lbt_send( const SendMsgsPacket* const XP_UNUSED(msgs),
          const CommsAddrRec* XP_UNUSED(addrRec),
          LaunchParams* XP_UNUSED(params) )
{
    XP_ASSERT(0);               /* look this stuff over; I doubt it works,
                                   starting with lbt_open() never being
                                   called. */
    /* XP_S16 result = 0; */
    /* for ( SendMsgsPacket* packet = (SendMsgsPacket*)msgs; */
    /*       !!packet; packet = (SendMsgsPacket* const)packet->next ) { */
    /*     XP_S16 tmp = lbt_send_impl( packet->buf, packet->len, */
    /*                                      addrRec, cGlobals ); */
    /*     if ( tmp > 0 ) { */
    /*         result += tmp; */
    /*     } else { */
    /*         result = -1; */
    /*         break; */
    /*     } */
    /* } */
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

XP_S16
lbt_receive( int sock, XP_U8* buf, XP_U16 buflen )
{
    XP_S16 nRead = 0;
    LOG_FUNC();
    XP_ASSERT( sock >= 0 );

#if defined BT_USE_RFCOMM
    read_all( sock, (unsigned char*)&nRead, sizeof(nRead) );
    nRead = ntohs(nRead);
    XP_LOGFF( "nRead=%d", nRead );
    XP_ASSERT( nRead < buflen );

    read_all( sock, buf, nRead );
    LOG_HEX( buf, nRead, __func__ );
#else        
    nRead = read( sock, buf, buflen );
    if ( nRead < 0 ) {
        XP_LOGFF( "read->%s", strerror(errno) );
    }
#endif

    LOG_RETURNF( "%d", nRead );
    return nRead;
}

/* void */
/* lbt_socketclosed( CommonGlobals* cGlobals, int XP_UNUSED_DBG(sock) ) */
/* { */
/*     LinBtStuff* btStuff = cGlobals->btStuff; */
/*     LOG_FUNC(); */
/*     XP_ASSERT( sock == btStuff->socket ); */
/*     btStuff->socket = -1; */
/* } */

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
        XP_LOGFF("Paired device: %s (%s)", name,
                 g_variant_get_string(addr_var, NULL));

        result = g_slist_append( result, g_strdup(name) );

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
