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
GeanyPlugin       *geany_plugin;
GeanyData         *geany_data;
ProjectTreeConfig *pt_config = NULL;

static gchar *get_plugin_config_path(void)
{
    return g_build_filename(geany_data->app->configdir, "plugins", "project-tree.conf", NULL);
}

static void load_config(void)
{
    GKeyFile *config = g_key_file_new();
    gchar *path = get_plugin_config_path();

    if (!pt_config)
        pt_config = g_new0(ProjectTreeConfig, 1);

    if (g_key_file_load_from_file(config, path, G_KEY_FILE_NONE, NULL))
    {
        pt_config->display_sidebar = g_key_file_get_boolean(config, "general", "display_sidebar", NULL);
        pt_config->sync_editor_colors = g_key_file_get_boolean(config, "general", "sync_editor_colors", NULL);
    }
    else
    {
        // Set defaults if it's the first run
        pt_config->display_sidebar = TRUE;
        pt_config->sync_editor_colors = TRUE;
    }
    
    g_free(path);
    g_key_file_free(config);
}

static void save_config(void)
{
    GKeyFile *config = g_key_file_new();
    gchar *path = get_plugin_config_path();

    g_key_file_set_boolean(config, "general", "display_sidebar", pt_config->display_sidebar);
    g_key_file_set_boolean(config, "general", "sync_editor_colors", pt_config->sync_editor_colors);

    gchar *data = g_key_file_to_data(config, NULL, NULL);
    if (data)
    {
        gchar *dir = g_path_get_dirname(path);
        if (!g_file_test(dir, G_FILE_TEST_IS_DIR))
            g_mkdir_with_parents(dir, 0755);
        
        g_file_set_contents(path, data, -1, NULL);
        g_free(data);
        g_free(dir);
    }
    
    g_free(path);
    g_key_file_free(config);
}



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

// Callback for "Add Current File to Project" menu item
void on_add_current_file_to_project(GtkMenuItem *menuitem, gpointer user_data)
{
    GeanyDocument *doc;
    if (!current_project_tree) return;

    doc = document_get_current();
    if (doc && doc->file_name)
    {
        GtkTreeView *tree_view = get_project_tree_view();
        GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);
        GtkTreeModel *model;
        GtkTreeIter iter;

        if (gtk_tree_selection_get_selected(selection, &model, &iter))
        {
            ProjectTreeNode *selected_node = NULL;
            gtk_tree_model_get(model, &iter, PROJECT_TREE_COLUMN_NODE_PTR, &selected_node, -1);
            if (selected_node)
            {
                if (selected_node->is_group)
                    add_file_to_project(doc->file_name, selected_node);
                else
                    insert_file_to_project_after(doc->file_name, selected_node);
            }
        }
        else
        {
            add_file_to_project(doc->file_name, NULL);
        }
    }
    else
    {
        dialogs_show_msgbox(GTK_MESSAGE_INFO, _("No current file to add to project."));
    }
}

// Callback for "Add All Open Files to Project" menu item
void on_add_all_open_files_to_project(GtkMenuItem *menuitem, gpointer user_data)
{
    guint page_num = 0;
    GtkTreeView *tree_view = get_project_tree_view();
    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);
    GtkTreeModel *model;
    GtkTreeIter iter;
    ProjectTreeNode *selected_node = NULL;
    GeanyDocument *doc = NULL;
    ProjectTreeNode *last_added;

    if (!current_project_tree) return;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter))
        gtk_tree_model_get(model, &iter, PROJECT_TREE_COLUMN_NODE_PTR, &selected_node, -1);

    last_added = selected_node;

    while ((doc = document_get_from_page(page_num)) != NULL)
    {
        if (doc && doc->file_name)
        {
            if (last_added)
            {
                if (last_added->is_group)
                    last_added = add_file_to_project(doc->file_name, last_added);
                else
                    last_added = insert_file_to_project_after(doc->file_name, last_added);
            }
            else
            {
                last_added = add_file_to_project(doc->file_name, NULL);
            }
        }
        page_num++;
    }
}



static gboolean on_idle_refresh(gpointer user_data)

{

    sidebar_refresh();

    return FALSE; // Run once

}



static void on_editor_notify(GObject *obj, GeanyEditor *editor, SCNotification *nt, gpointer user_data)

{

    // Update colors only to avoid destroying tree state on every notification

    if (pt_config && pt_config->sync_editor_colors)

    {

        sidebar_update_colors();

    }

}



static gboolean pt_init(GeanyPlugin *p, gpointer data)
{
    geany_plugin = p;
    geany_data = p->geany_data;

    load_config();



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



    if (pt_config->display_sidebar)

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



    



        return TRUE;



    }



    



static void pt_cleanup(GeanyPlugin *p, gpointer data)

{

    save_config();



    if (current_project_tree)

    {

        project_tree_free(current_project_tree);

        current_project_tree = NULL;

    }

    destroy_sidebar();

    g_free(pt_config);

}



static void on_display_sidebar_toggled(GtkToggleButton *button, gpointer user_data)

{

    pt_config->display_sidebar = gtk_toggle_button_get_active(button);

}



static void on_sync_colors_toggled(GtkToggleButton *button, gpointer user_data)

{

    pt_config->sync_editor_colors = gtk_toggle_button_get_active(button);

    sidebar_refresh();

}



static GtkWidget *pt_configure(GeanyPlugin *p, GtkDialog *dialog, gpointer data)

{

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

    GtkWidget *checkbox_sidebar = gtk_check_button_new_with_label(_("Display project tree sidebar"));

    GtkWidget *checkbox_sync = gtk_check_button_new_with_label(_("Sync sidebar colors with editor scheme"));



    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox_sidebar), pt_config->display_sidebar);

    g_signal_connect(checkbox_sidebar, "toggled", G_CALLBACK(on_display_sidebar_toggled), NULL);

    gtk_box_pack_start(GTK_BOX(vbox), checkbox_sidebar, FALSE, FALSE, 0);



    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox_sync), pt_config->sync_editor_colors);

    g_signal_connect(checkbox_sync, "toggled", G_CALLBACK(on_sync_colors_toggled), NULL);

    gtk_box_pack_start(GTK_BOX(vbox), checkbox_sync, FALSE, FALSE, 0);



    gtk_widget_show_all(vbox);

    return vbox;

}



G_MODULE_EXPORT

void geany_load_module(GeanyPlugin *p)

{

    p->info->name = _("ProjectTree");

    p->info->description = _("A custom project tree view.");

    p->info->version = "0.1";

    p->info->author = "Your Name <your.email@example.com>";



    p->funcs->init = pt_init;

    p->funcs->cleanup = pt_cleanup;

    p->funcs->configure = pt_configure;



    GEANY_PLUGIN_REGISTER(p, 247);

}


