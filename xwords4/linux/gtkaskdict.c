/* 
 * Copyright 2000-2009 by Eric House (xwords@eehouse.org).  All rights
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
#ifdef PLATFORM_GTK

#include "gtkaskdict.h"

// adapted from code at: http://zetcode.com/tutorials/gtktutorial/gtktreeview/

// This thing really needs a cancel button

enum { LIST_ITEM, N_ITEMS };

static void
init_list( GtkWidget* list )
{
    GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn* column = 
        gtk_tree_view_column_new_with_attributes( "Dict files", renderer, 
                                                  "text", LIST_ITEM, NULL );
    gtk_tree_view_append_column( GTK_TREE_VIEW(list), column );
    GtkListStore* store = gtk_list_store_new( N_ITEMS, G_TYPE_STRING );
    gtk_tree_view_set_model( GTK_TREE_VIEW(list), GTK_TREE_MODEL(store) );
    g_object_unref( store );
}

static void
add_to_list( GtkWidget* list, const gchar* str )
{
    GtkListStore* store = 
        GTK_LIST_STORE( gtk_tree_view_get_model(GTK_TREE_VIEW(list)));
    GtkTreeIter iter;
    gtk_list_store_append( store, &iter );
    gtk_list_store_set( store, &iter, LIST_ITEM, str, -1 );
}

static void
on_changed( GtkWidget *widget, gpointer buf )
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    if ( gtk_tree_selection_get_selected( GTK_TREE_SELECTION(widget), 
                                          &model, &iter ) ) {
        gchar* value;
        gtk_tree_model_get( model, &iter, LIST_ITEM, &value,  -1 );
        strcat( buf, value );
        g_free( value );
    }
}

static void
on_clicked( GtkWidget* XP_UNUSED(widget), void* XP_UNUSED(closure) )
{
    gtk_main_quit();
} /* on_clicked */

gchar* 
gtkaskdict( GSList* dicts, gchar* buf, gint buflen )
{
    gchar* result = NULL;
    buf[0] = '\0';
    XP_USE(buflen);

    GtkWidget* dialog = gtk_dialog_new();
    gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );
    GtkWidget* list = gtk_tree_view_new();
    gtk_box_pack_start( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                        list, TRUE, TRUE, 5);

    init_list( list );

    /* GtkTreeSelection* selection =  */
    g_signal_connect( gtk_tree_view_get_selection( GTK_TREE_VIEW(list) ),
                      "changed", G_CALLBACK(on_changed), buf );

    GSList* iter;
    for ( iter = dicts; !!iter; iter = iter->next ) {
        add_to_list( list, (gchar*)iter->data );
    }

    GtkWidget* button = gtk_button_new_with_label( "Ok" );
    g_signal_connect( button, "clicked", 
                      G_CALLBACK(on_clicked), NULL );
    gtk_box_pack_start( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                        button, FALSE, TRUE, 0 );

    gtk_widget_show_all( dialog );
    gtk_main();
    gtk_widget_destroy( dialog );

    if ( '\0' != buf[0] ) {
        XP_ASSERT( buflen > XP_STRLEN(buf) );
        result = buf;
    }

    LOG_RETURNF( "%s", result );
    return result;
}

#endif /* PLATFORM_GTK */
