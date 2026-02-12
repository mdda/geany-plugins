/**
 *   Copyright (C) 2010-2026  Martin Andrews
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "project-tree.h"
#include "sidebar.h"
#include "project.h"
#include <geany/document.h> // Explicitly include for document_get_from_page() and document_get_count()

// Global plugin variables
GeanyPlugin *geany_plugin;
GeanyData *geany_data;
gboolean sync_editor_colors = TRUE;

PLUGIN_VERSION_CHECK(247)
PLUGIN_SET_TRANSLATABLE_INFO(LOCALEDIR, GETTEXT_PACKAGE,
                             "ProjectTree", _("A custom project tree view."),
                             "0.1", "Your Name <your.email@example.com>")

static gboolean display_sidebar = TRUE;

// Forward declarations for static functions
static gchar* find_project_root_path(void);
static gchar* get_config_file_path(const gchar *project_root_path, const gchar *filename);

// Placeholder utility to find the project root path
static gchar* find_project_root_path(void)
{
    return g_strdup(g_get_current_dir());
}

// Placeholder utility to get config file path
static gchar* get_config_file_path(const gchar *project_root_path, const gchar *filename)
{
    gchar *editor_dir = g_build_filename(project_root_path, ".editor", NULL);
    gchar *file_path = g_build_filename(editor_dir, filename, NULL);
    g_free(editor_dir);
    return file_path;
}

// Helper to add a file to the project tree as a child
ProjectTreeNode *add_file_to_project(const gchar *file_path, ProjectTreeNode *parent_node)
{
    if (!current_project_tree || !file_path) return NULL;

    // Deduplication removed per user request.
    // if (project_tree_find_node_by_path(current_project_tree, file_path)) ...

    ProjectTreeNode *new_file_node = project_tree_node_new(g_path_get_basename(file_path), file_path, FALSE);
    ProjectTreeNode *ret = project_tree_add_node(current_project_tree, parent_node, new_file_node);
    
    sidebar_sync_and_refresh();
    return ret;
}

// Helper to add a file to the project tree after a specific node
ProjectTreeNode *insert_file_to_project_after(const gchar *file_path, ProjectTreeNode *after_node)
{
    if (!current_project_tree || !file_path) return NULL;

    ProjectTreeNode *new_file_node = project_tree_node_new(g_path_get_basename(file_path), file_path, FALSE);
    ProjectTreeNode *ret = project_tree_insert_node_after(current_project_tree, after_node, new_file_node);
    
    sidebar_sync_and_refresh();
    return ret;
}

static gboolean on_idle_refresh(gpointer user_data)
{
    sidebar_refresh();
    return FALSE; // Run once
}

static void on_editor_notify(GObject *obj, GeanyEditor *editor, SCNotification *nt, gpointer user_data)
{
    // Update colors only to avoid destroying tree state on every notification
    if (sync_editor_colors)
    {
        sidebar_update_colors();
    }
}

// Main plugin entry point (old API)
void plugin_init(GeanyData *data)
{
    geany_data = data; // Assign geany_data here

    gchar *project_root = find_project_root_path();
    if (project_root)
    {
        gchar *project_ini_path = get_config_file_path(project_root, "project-tree-layout.ini");
        gchar *session_ini_path = get_config_file_path(project_root, "session.ini");

        current_project_tree = project_tree_load(project_ini_path, session_ini_path);

        g_free(project_ini_path);
        g_free(session_ini_path);
        g_free(project_root);
    }

    if (display_sidebar)
    {
        create_sidebar();
        // Add idle refresh to ensure expansion works after UI is ready
        g_idle_add(on_idle_refresh, NULL);
    }

    // Add items to Geany's Project menu
    GtkWidget *project_menu = geany_data->main_widgets->project_menu;
    if (project_menu)
    {
        GtkWidget *separator = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(project_menu), separator);
        gtk_widget_show(separator);

        GtkWidget *item_add_curr = gtk_menu_item_new_with_label(_("Add Current File to Project Tree"));
        g_signal_connect(item_add_curr, "activate", G_CALLBACK(on_add_current_file_to_project), NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(project_menu), item_add_curr);
        gtk_widget_show(item_add_curr);

        GtkWidget *item_add_all = gtk_menu_item_new_with_label(_("Add All Open Files to Project Tree"));
        g_signal_connect(item_add_all, "activate", G_CALLBACK(on_add_all_open_files_to_project), NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(project_menu), item_add_all);
        gtk_widget_show(item_add_all);
    }

    // Connect to editor-notify to catch color scheme changes immediately
    plugin_signal_connect(geany_plugin, NULL, "editor-notify", TRUE, (GCallback)on_editor_notify, NULL);
}

void plugin_cleanup(void)
{
    if (current_project_tree)
    {
        project_tree_free(current_project_tree);
        current_project_tree = NULL;
    }
    destroy_sidebar();
}

static void on_sync_colors_toggled(GtkToggleButton *button, gpointer user_data)
{
    sync_editor_colors = gtk_toggle_button_get_active(button);
    sidebar_refresh();
}

GtkWidget *plugin_configure(GtkDialog *dialog)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *checkbox_sidebar = gtk_check_button_new_with_label(_("Display project tree sidebar"));
    GtkWidget *checkbox_sync = gtk_check_button_new_with_label(_("Sync sidebar colors with editor scheme"));

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox_sidebar), display_sidebar);
    gtk_box_pack_start(GTK_BOX(vbox), checkbox_sidebar, FALSE, FALSE, 0);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox_sync), sync_editor_colors);
    g_signal_connect(checkbox_sync, "toggled", G_CALLBACK(on_sync_colors_toggled), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), checkbox_sync, FALSE, FALSE, 0);

    gtk_widget_show_all(vbox);
    return vbox;
}

// Callback for "Add Current File to Project" menu item (legacy, if still used)
void on_add_current_file_to_project(GtkMenuItem *menuitem, gpointer user_data)
{
    GeanyDocument *doc = document_get_current();
    if (doc && doc->file_name)
        add_file_to_project(doc->file_name, NULL);
}

// Callback for "Add All Open Files to Project" menu item (legacy, if still used)
void on_add_all_open_files_to_project(GtkMenuItem *menuitem, gpointer user_data)
{
    guint page_num = 0;
    GeanyDocument *doc = NULL;
    while ((doc = document_get_from_page(page_num)) != NULL)
    {
        if (doc && doc->file_name)
            add_file_to_project(doc->file_name, NULL);
        page_num++;
    }
}