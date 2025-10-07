/*
 * Copyright 2025 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "blecon.h"

struct BLEConState {
    XW_DUtilCtxt* dutil;

    GDBusNodeInfo* introspection_data;
    GDBusConnection* connection;
    guint registration_id;

    gchar* value;
};

/* Most of this file is from ChatGPT, when asked to generate linux C code for
   a BLE server. Unlike what ChatGPT generated, this compiles and links, but I
   haven't seen anything show up in a BLE scanner so have no evidence it
   actually works.
*/

static void
handle_method_call(GDBusConnection* connection,
                   const gchar* sender,
                   const gchar* object_path,
                   const gchar* interface_name,
                   const gchar* method_name,
                   GVariant* parameters,
                   GDBusMethodInvocation* invocation,
                   gpointer user_data)
{
    XP_LOGFF( "(sender: %s, object_path: %s)", sender, object_path );
    XP_LOGFF( "(interface_name: %s, method_name: %s)", interface_name, method_name );
    BLEConState* state = (BLEConState*)user_data;
    XP_ASSERT( connection == state->connection );
    if (g_strcmp0(method_name, "ReadValue") == 0) {
        GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("ay"));
        const gchar *v = state->value ? state->value : "default";
        for (const gchar *p = v; *p; p++)
            g_variant_builder_add(builder, "y", *p);

        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(ay)", builder));
        g_variant_builder_unref(builder);
        printf("Read request: returning \"%s\"\n", v);
    } else if (g_strcmp0(method_name, "WriteValue") == 0) {
        GVariantIter *iter;
        guchar byte;
        g_variant_get(parameters, "(aya{sv})", &iter, NULL);
        GString *s = g_string_new("");
        while (g_variant_iter_loop(iter, "y", &byte))
            g_string_append_c(s, byte);
        state->value = g_strdup(s->str);
        g_string_free(s, TRUE);
        g_variant_iter_free(iter);
        g_dbus_method_invocation_return_value(invocation, NULL);
        printf("Write request: new value \"%s\"\n", state->value);
    }
}

static const GDBusInterfaceVTable interface_vtable = {
    handle_method_call,
    NULL,
    NULL
};

void
ble_init( LaunchParams* params )
{
    LOG_FUNC();
    XW_DUtilCtxt* dutil = params->dutil;
    BLEConState* state = XP_CALLOC( dutil->mpool, sizeof(*state) );
    params->bleConState = state;

    const gchar introspection_xml[] =
        "<node>"
        "  <interface name='org.bluez.GattService1'>"
        "    <property name='UUID' type='s' access='read'/>"
        "    <property name='Primary' type='b' access='read'/>"
        "  </interface>"
        "  <interface name='org.bluez.GattCharacteristic1'>"
        "    <property name='UUID' type='s' access='read'/>"
        "    <property name='Flags' type='as' access='read'/>"
        "    <method name='ReadValue'>"
        "      <arg name='options' type='a{sv}' direction='in'/>"
        "      <arg name='value' type='ay' direction='out'/>"
        "    </method>"
        "    <method name='WriteValue'>"
        "      <arg name='value' type='ay' direction='in'/>"
        "      <arg name='options' type='a{sv}' direction='in'/>"
        "    </method>"
        "  </interface>"
        "</node>";

    GError *error = NULL;
    state->introspection_data =
        g_dbus_node_info_new_for_xml(introspection_xml, &error);
    if (!state->introspection_data) {
        XP_LOGFF("Failed to parse introspection XML: %s\n",
                 error->message);
        g_error_free(error);
        return;
    }

    state->connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!state->connection) {
        XP_LOGFF( "Failed to connect to system bus: %s\n", error->message);
        g_error_free(error);
        return;
    }

    state->registration_id = g_dbus_connection_register_object
        (state->connection,
         "/org/bluez/example/service0/char0",
         state->introspection_data->interfaces[1],
         &interface_vtable,
         state, NULL, &error);
    if (state->registration_id == 0) {
        XP_LOGFF( "Failed to register object: %s\n", error->message);
        g_error_free(error);
        return;
    }

    XP_LOGFF("GATT server running.\n");
    XP_LOGFF("Use a BLE client to connect and read/write"
             " the characteristic.\n");

    /* loop = g_main_loop_new(NULL, FALSE); */
    /* g_main_loop_run(loop); */
}

void
ble_destroy( LaunchParams* params )
{
    XW_DUtilCtxt* dutil = params->dutil;
    BLEConState* state = params->bleConState;
    g_dbus_connection_unregister_object(state->connection,
                                        state->registration_id);
    g_object_unref(state->connection);
    g_dbus_node_info_unref(state->introspection_data);
    
    XP_FREEP( dutil->mpool, &params->bleConState );
}
