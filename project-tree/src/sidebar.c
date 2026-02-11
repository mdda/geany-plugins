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

#include "sidebar.h"
#include "project-tree.h" // For geany_data and geany_plugin
#include "project.h"      // For project data and functions
#include <gtk/gtktreeview.h> // Explicitly include for GtkTreeViewDropPosition enum values
#include <geany/navqueue.h> // For navqueue_goto_line

#include <geany/scintilla/Scintilla.h>
#include <geany/scintilla/ScintillaWidget.h>
#include <geany/sciwrappers.h>

GtkWidget *project_tree_view;       // The GtkTreeView (removed static)

static GtkWidget *project_tree_view_vbox; // Main container for the sidebar
static GtkTreeStore *project_tree_store;   // The model for the GtkTreeStore

// Drag-and-drop targets
// We only support our custom target to avoid conflicts with GtkTreeView's default reordering
static GtkTargetEntry dnd_targets[] = {
    { (gchar*)"PROJECT_TREE_NODE_PTR", 0, 0 } // Info ID 0
};
static guint n_dnd_targets = G_N_ELEMENTS(dnd_targets);

// Forward declarations for drag-and-drop callbacks
static void on_tree_view_drag_begin(GtkWidget *widget, GdkDragContext *context, gpointer user_data);
static void on_tree_view_drag_data_get(GtkWidget *widget, GdkDragContext *context,
                                        GtkSelectionData *selection_data, guint info, guint time, gpointer user_data);
static void on_tree_view_drag_data_received(GtkWidget *widget, GdkDragContext *context, gint x, gint y,
                                             GtkSelectionData *selection_data, guint info, guint time, gpointer user_data);

// Forward declarations for context menu callbacks
static void on_new_group_activated(GtkMenuItem *menuitem, gpointer user_data);
static void on_remove_activated(GtkMenuItem *menuitem, gpointer user_data);
static void on_add_current_file_to_project_activated(GtkMenuItem *menuitem, gpointer user_data);
static void on_add_all_open_files_to_project_activated(GtkMenuItem *menuitem, gpointer user_data);
static void on_save_project_tree_activated(GtkMenuItem *menuitem, gpointer user_data);
static void on_save_session_activated(GtkMenuItem *menuitem, gpointer user_data);


// Function to create the context menu
static GtkWidget *create_context_menu(void)
{
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *item;

    // Add/Remove Group
    item = gtk_menu_item_new_with_label(_("New Group..."));
    g_signal_connect(item, "activate", G_CALLBACK(on_new_group_activated), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label(_("Remove Highlighted Entry"));
    g_signal_connect(item, "activate", G_CALLBACK(on_remove_activated), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    // Add Files
    item = gtk_menu_item_new_with_label(_("Add Current File to Project Tree"));
    g_signal_connect(item, "activate", G_CALLBACK(on_add_current_file_to_project_activated), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label(_("Add All Open Files to Project Tree"));
    g_signal_connect(item, "activate", G_CALLBACK(on_add_all_open_files_to_project_activated), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    // Save Actions
    item = gtk_menu_item_new_with_label(_("Save Project Tree"));
    g_signal_connect(item, "activate", G_CALLBACK(on_save_project_tree_activated), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label(_("Save Session"));
    g_signal_connect(item, "activate", G_CALLBACK(on_save_session_activated), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gtk_widget_show_all(menu);
    return menu;
}


// Callback for when a row in the tree view is activated (e.g., double-click)
static void on_row_activated(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *file_path;
    gboolean is_group;
    gint line_number;

    model = gtk_tree_view_get_model(tree_view);
    if (!gtk_tree_model_get_iter(model, &iter, path))
        return;

    gtk_tree_model_get(model, &iter,
                       PROJECT_TREE_COLUMN_PATH, &file_path,
                       PROJECT_TREE_COLUMN_IS_GROUP, &is_group,
                       PROJECT_TREE_COLUMN_LINE, &line_number,
                       -1);

    if (!is_group && file_path)
    {
        // Corrected arguments for document_open_file
        GeanyDocument *doc = document_open_file(file_path, FALSE, FALSE, NULL);
        if (doc && line_number != -1)
        {
            navqueue_goto_line(document_get_current(), doc, line_number);
        }
    }
    g_free(file_path);
}

// Callback for button press events on the tree view (for context menu)
static gboolean on_tree_view_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) // Right-click
    {
        GtkTreePath *path;
        gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(project_tree_view), (gint)event->x, (gint)event->y, &path, NULL, NULL, NULL);

        if (path)
        {
            GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(project_tree_view));
            gtk_tree_selection_select_path(selection, path);
            gtk_tree_path_free(path);
        }

        GtkMenu *menu = GTK_MENU(create_context_menu());
        gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event->button, event->time);
        return TRUE;
    }
    return FALSE;
}

// DND Callback: Called when a drag operation begins
static void on_tree_view_drag_begin(GtkWidget *widget, GdkDragContext *context, gpointer user_data)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;
    ProjectTreeNode *node_to_drag = NULL;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
    if (gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        gtk_tree_model_get(model, &iter, PROJECT_TREE_COLUMN_NODE_PTR, &node_to_drag, -1);
        if (node_to_drag)
        {
            g_object_set_data(G_OBJECT(context), "dragged-node", node_to_drag);
        }
    }
}

// DND Callback: Called when the drop target requests the data being dragged
static void on_tree_view_drag_data_get(GtkWidget *widget, GdkDragContext *context,
                                        GtkSelectionData *selection_data, guint info, guint time, gpointer user_data)
{
    // We only support info 0 (PROJECT_TREE_NODE_PTR) now
    if (info == 0)
    {
        ProjectTreeNode *node_to_drag = g_object_get_data(G_OBJECT(context), "dragged-node");
        if (node_to_drag)
        {
            gtk_selection_data_set(selection_data, gdk_atom_intern("PROJECT_TREE_NODE_PTR", FALSE),
                                   8, (guchar *)&node_to_drag, sizeof(ProjectTreeNode *));
        }
    }
}

// DND Callback: Called when data is dropped onto the target
static void on_tree_view_drag_data_received(GtkWidget *widget, GdkDragContext *context, gint x, gint y,
                                             GtkSelectionData *selection_data, guint info, guint time, gpointer user_data)
{
    // We expect info 0
    if (info != 0)
    {
        g_warning("  Unexpected info %d (expected 0)", info);
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }

    GtkTreeViewDropPosition pos;
    GtkTreePath *dest_path;

    if (!gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget), x, y, &dest_path, &pos))
    {
        // Invalid drop position (e.g. empty space)
        if (dest_path) gtk_tree_path_free(dest_path);
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }

    ProjectTreeNode *dragged_node = NULL;
    if (gtk_selection_data_get_length(selection_data) == sizeof(ProjectTreeNode *))
    {
        dragged_node = *(ProjectTreeNode **)gtk_selection_data_get_data(selection_data);
    }
    
    if (!dragged_node)
    {
        dragged_node = g_object_get_data(G_OBJECT(context), "dragged-node");
    }
    
    if (!dragged_node)
    {
        if (dest_path) gtk_tree_path_free(dest_path);
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }

    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
    GtkTreeIter dest_iter;
    ProjectTreeNode *dest_node = NULL;

    if (gtk_tree_model_get_iter(model, &dest_iter, dest_path))
    {
        gtk_tree_model_get(model, &dest_iter, PROJECT_TREE_COLUMN_NODE_PTR, &dest_node, -1);
    }
    gtk_tree_path_free(dest_path);

    if (!dest_node) 
    {
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }

    // Logic to update the tree structure
    ProjectTreeNode *new_parent = NULL;
    int new_position = -1;

    // Check against literal value for GTK_TREE_VIEW_DROP_INTO_ROW if macro is missing
    // 0 = BEFORE, 1 = AFTER, 2 = INTO_ROW_BEFORE, 3 = INTO_ROW_AFTER
    if (pos == 2 || pos == 3)
    {
        if (dest_node->is_group)
            new_parent = dest_node;
        else
        {
            new_parent = dest_node->parent;
            if (new_parent)
                new_position = g_slist_index(new_parent->children, dest_node) + 1;
            else
                new_position = g_slist_index(current_project_tree->root_nodes, dest_node) + 1;
        }
    }
    else // GTK_TREE_VIEW_DROP_BEFORE (0) or GTK_TREE_VIEW_DROP_AFTER (1)
    {
        new_parent = dest_node->parent;
        if (new_parent)
        {
            new_position = g_slist_index(new_parent->children, dest_node);
            if (pos == 1) // AFTER
                new_position++;
        }
        else
        {
            new_position = g_slist_index(current_project_tree->root_nodes, dest_node);
            if (pos == 1) // AFTER
                new_position++;
        }
    }
    
    if (current_project_tree)
    {
        project_tree_unlink_node(current_project_tree, dragged_node);

        if (new_parent)
        {
            if (new_position == -1 || new_position >= g_slist_length(new_parent->children))
                new_parent->children = g_slist_append(new_parent->children, dragged_node);
            else
                new_parent->children = g_slist_insert(new_parent->children, dragged_node, new_position);
            dragged_node->parent = new_parent;
        }
        else
        {
            if (new_position == -1 || new_position >= g_slist_length(current_project_tree->root_nodes))
                current_project_tree->root_nodes = g_slist_append(current_project_tree->root_nodes, dragged_node);
            else
                current_project_tree->root_nodes = g_slist_insert(current_project_tree->root_nodes, dragged_node, new_position);
            dragged_node->parent = NULL;
        }
        
        project_tree_save(current_project_tree);
        sidebar_refresh();
    }
    gtk_drag_finish(context, TRUE, TRUE, time);
}

// Connect selection signals (single-click, double-click for opening files/groups)
static void prepare_project_tree_view(void)
{
    GtkCellRenderer *text_renderer;
    GtkCellRenderer *icon_renderer;
    GtkTreeViewColumn *column;

    project_tree_store = gtk_tree_store_new(PROJECT_TREE_N_COLUMNS,
                                            G_TYPE_STRING,    // Name
                                            G_TYPE_STRING,    // Icon
                                            G_TYPE_STRING,    // Path
                                            G_TYPE_BOOLEAN,   // Is Group
                                            G_TYPE_INT,       // Line Number
                                            G_TYPE_BOOLEAN,   // Readonly Status
                                            G_TYPE_POINTER);  // ProjectTreeNode*

    gtk_tree_view_set_model(GTK_TREE_VIEW(project_tree_view), GTK_TREE_MODEL(project_tree_store));

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, _("Project Tree"));

    icon_renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(column, icon_renderer, FALSE);
    gtk_tree_view_column_set_attributes(column, icon_renderer,
                                        "icon-name", PROJECT_TREE_COLUMN_ICON,
                                        NULL);

    text_renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
    gtk_tree_view_column_set_attributes(column, text_renderer,
                                        "text", PROJECT_TREE_COLUMN_NAME,
                                        NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(project_tree_view), column);

    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(project_tree_view), FALSE);

    g_signal_connect(project_tree_view, "row-activated", G_CALLBACK(on_row_activated), NULL);
    g_signal_connect(project_tree_view, "button-press-event", G_CALLBACK(on_tree_view_button_press), NULL);

    gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(project_tree_view),
                                           GDK_BUTTON1_MASK,
                                           dnd_targets, n_dnd_targets,
                                           GDK_ACTION_MOVE);

    gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(project_tree_view),
                                         dnd_targets, n_dnd_targets,
                                         GDK_ACTION_MOVE);

    g_signal_connect(project_tree_view, "drag-begin", G_CALLBACK(on_tree_view_drag_begin), NULL);
    g_signal_connect(project_tree_view, "drag-data-get", G_CALLBACK(on_tree_view_drag_data_get), NULL);
    g_signal_connect(project_tree_view, "drag-data-received", G_CALLBACK(on_tree_view_drag_data_received), NULL);
}

// Recursive helper function to add ProjectTreeNodes to the GtkTreeStore
static void add_nodes_to_tree_store(GtkTreeStore *store, ProjectTreeNode *node, GtkTreeIter *parent_iter)
{
    GtkTreeIter iter;
    GSList *l;
    const gchar *icon_name = node->is_group ? "folder" : "text-x-generic";

    gtk_tree_store_append(store, &iter, parent_iter);
    gtk_tree_store_set(store, &iter,
                       PROJECT_TREE_COLUMN_NAME, node->name,
                       PROJECT_TREE_COLUMN_ICON, icon_name,
                       PROJECT_TREE_COLUMN_PATH, node->path,
                       PROJECT_TREE_COLUMN_IS_GROUP, node->is_group,
                       PROJECT_TREE_COLUMN_LINE, node->line_number,
                       PROJECT_TREE_COLUMN_READONLY, node->read_only,
                       PROJECT_TREE_COLUMN_NODE_PTR, node,
                       -1);

    for (l = node->children; l != NULL; l = g_slist_next(l))
    {
        ProjectTreeNode *child_node = (ProjectTreeNode *)l->data;
        add_nodes_to_tree_store(store, child_node, &iter);
    }
}

// Helper to recursively restore expansion state
static void restore_expansion_state_recursive(GtkTreeModel *model, GtkTreeIter *parent_iter)
{
    GtkTreeIter iter;
    gboolean valid;

    valid = gtk_tree_model_iter_children(model, &iter, parent_iter);
    while (valid)
    {
        ProjectTreeNode *node = NULL;
        gtk_tree_model_get(model, &iter, PROJECT_TREE_COLUMN_NODE_PTR, &node, -1);

        if (node && node->is_group && current_project_tree && current_project_tree->open_groups)
        {
            gchar *full_path = get_node_tree_path(node);
            GSList *l_open;
            gboolean should_expand = FALSE;

            for (l_open = current_project_tree->open_groups; l_open != NULL; l_open = g_slist_next(l_open))
            {
                if (strcmp((gchar *)l_open->data, full_path) == 0)
                {
                    should_expand = TRUE;
                    break;
                }
            }
            g_free(full_path);

            if (should_expand)
            {
                GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
                gtk_tree_view_expand_row(GTK_TREE_VIEW(project_tree_view), path, FALSE);
                gtk_tree_path_free(path);
                
                // Recurse ONLY if expanded (though here we want to recurse anyway to check children)
            }
        }

        // Always recurse to check children, because even if this node isn't in open_groups, 
        // it might be expanded manually or implicitly? 
        // Actually, if we just expanded it, we should check its children.
        // If we didn't expand it, its children won't be visible, so expanding them technically doesn't hurt 
        // but might fail or be useless. 
        // However, standard behavior is top-down.
        restore_expansion_state_recursive(model, &iter);

        valid = gtk_tree_model_iter_next(model, &iter);
    }
}

void sidebar_refresh(void)
{
    if (!project_tree_view_vbox || !project_tree_store)
        return;

    // Apply or remove dynamic Color Sync styling
    static GtkCssProvider *provider = NULL;
    static gboolean provider_added = FALSE;
    GtkStyleContext *context = gtk_widget_get_style_context(project_tree_view);
    
    if (sync_editor_colors)
    {
        GeanyDocument *doc = document_get_current();
        if (!doc) doc = document_get_from_page(0); // Fallback to first tab if none active

        if (doc && doc->editor && doc->editor->sci)
        {
            // Scintilla colors are 0xBBGGRR
            long bg = scintilla_send_message(doc->editor->sci, SCI_STYLEGETBACK, STYLE_DEFAULT, 0);
            long fg = scintilla_send_message(doc->editor->sci, SCI_STYLEGETFORE, STYLE_DEFAULT, 0);
            
            gchar *css = g_strdup_printf(
                "#project_tree_view { background-color: #%02x%02x%02x; color: #%02x%02x%02x; }\n"
                "#project_tree_view:selected { background-color: #4a90d9; color: #ffffff; }",
                (int)(bg & 0xff), (int)((bg >> 8) & 0xff), (int)((bg >> 16) & 0xff),
                (int)(fg & 0xff), (int)((fg >> 8) & 0xff), (int)((fg >> 16) & 0xff));

            // g_message("Syncing colors: BG=#%02x%02x%02x, FG=#%02x%02x%02x", 
            //           (int)(bg & 0xff), (int)((bg >> 8) & 0xff), (int)((bg >> 16) & 0xff),
            //           (int)(fg & 0xff), (int)((fg >> 8) & 0xff), (int)((fg >> 16) & 0xff));

            if (!provider)
                provider = gtk_css_provider_new();
            
            gtk_css_provider_load_from_data(provider, css, -1, NULL);
            g_free(css);

            if (!provider_added)
            {
                gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
                gtk_widget_set_name(project_tree_view, "project_tree_view");
                provider_added = TRUE;
            }
        }
    }
    else if (provider_added)
    {
        gtk_style_context_remove_provider(context, GTK_STYLE_PROVIDER(provider));
        provider_added = FALSE;
    }

    gtk_tree_store_clear(project_tree_store);

    if (!current_project_tree)
        return;

    GSList *l;
    for (l = current_project_tree->root_nodes; l != NULL; l = g_slist_next(l))
    {
        ProjectTreeNode *node = (ProjectTreeNode *)l->data;
        add_nodes_to_tree_store(project_tree_store, node, NULL);
    }

    // Apply expansion state top-down
    restore_expansion_state_recursive(GTK_TREE_MODEL(project_tree_store), NULL);
}

void create_sidebar(void)
{
    GtkWidget *scrollwin;

    project_tree_view_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    project_tree_view = gtk_tree_view_new();
    prepare_project_tree_view();

    scrollwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrollwin), project_tree_view);
    gtk_box_pack_start(GTK_BOX(project_tree_view_vbox), scrollwin, TRUE, TRUE, 0);

    gtk_widget_show_all(project_tree_view_vbox);
    gtk_notebook_append_page(GTK_NOTEBOOK(geany_data->main_widgets->sidebar_notebook),
                             project_tree_view_vbox, gtk_label_new(_("Project Tree")));
}

void destroy_sidebar(void)
{
    if (project_tree_view_vbox)
    {
        gtk_widget_destroy(project_tree_view_vbox);
        project_tree_view_vbox = NULL;
    }
}

GtkTreeView *get_project_tree_view(void)
{
    return GTK_TREE_VIEW(project_tree_view);
}

// Helper to recursively get expanded group paths
static void get_expanded_paths_recursive(GtkTreeView *tree_view, GtkTreeModel *model, GtkTreeIter *iter, GSList **list)
{
    GtkTreeIter child_iter;
    if (gtk_tree_model_iter_children(model, &child_iter, iter))
    {
        do
        {
            GtkTreePath *path = gtk_tree_model_get_path(model, &child_iter);
            if (gtk_tree_view_row_expanded(tree_view, path))
            {
                ProjectTreeNode *node;
                gtk_tree_model_get(model, &child_iter, PROJECT_TREE_COLUMN_NODE_PTR, &node, -1);
                if (node && node->is_group)
                {
                    gchar *full_path = get_node_tree_path(node);
                    *list = g_slist_prepend(*list, full_path);
                    
                    // Recurse into children
                    get_expanded_paths_recursive(tree_view, model, &child_iter, list);
                }
            }
            gtk_tree_path_free(path);
        } while (gtk_tree_model_iter_next(model, &child_iter));
    }
}

GSList *sidebar_get_open_groups(void)
{
    GSList *list = NULL;
    if (!project_tree_view || !project_tree_store) return NULL;

    get_expanded_paths_recursive(GTK_TREE_VIEW(project_tree_view), GTK_TREE_MODEL(project_tree_store), NULL, &list);
    return list;
}

// Callback for "New Group..." menu item
static void on_new_group_activated(GtkMenuItem *menuitem, gpointer user_data)
{
    gchar *group_name;
    if (!current_project_tree) return;

    group_name = dialogs_show_input(_("New Group"), GTK_WINDOW(geany->main_widgets->window), _("Enter new group name:"), "");
    if (group_name && group_name[0] != '\0')
    {
        ProjectTreeNode *new_group_node = project_tree_node_new(group_name, NULL, TRUE);

        GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(project_tree_view));
        GtkTreeModel *model;
        GtkTreeIter iter;
        
        if (gtk_tree_selection_get_selected(selection, &model, &iter))
        {
            ProjectTreeNode *selected_node = NULL;
            gtk_tree_model_get(model, &iter, PROJECT_TREE_COLUMN_NODE_PTR, &selected_node, -1);
            if (selected_node)
            {
                if (selected_node->is_group)
                    project_tree_add_node(current_project_tree, selected_node, new_group_node);
                else
                    project_tree_insert_node_after(current_project_tree, selected_node, new_group_node);
            }
        }
        else
        {
            project_tree_add_node(current_project_tree, NULL, new_group_node);
        }
        sidebar_refresh();
    }
    g_free(group_name);
}

// Callback for "Remove" menu item
static void on_remove_activated(GtkMenuItem *menuitem, gpointer user_data)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GList *selected_rows;

    if (!current_project_tree) return;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(project_tree_view));
    selected_rows = gtk_tree_selection_get_selected_rows(selection, &model);

    if (selected_rows)
    {
        GtkTreePath *selected_path = (GtkTreePath *)selected_rows->data;
        GtkTreeIter iter;
        if (gtk_tree_model_get_iter(model, &iter, selected_path))
        {
            ProjectTreeNode *node_to_remove = NULL;
            gchar *node_name;
            gtk_tree_model_get(model, &iter,
                               PROJECT_TREE_COLUMN_NODE_PTR, &node_to_remove,
                               PROJECT_TREE_COLUMN_NAME, &node_name,
                               -1);

            if (node_to_remove)
            {
                gchar *confirm_msg = g_strdup_printf(_("Are you sure you want to remove '%s'%s?"),
                                                      node_name, node_to_remove->is_group ? _(" and its contents") : "");
                if (dialogs_show_question(confirm_msg))
                {
                    project_tree_remove_node(current_project_tree, node_to_remove);
                    project_tree_save(current_project_tree);
                    sidebar_refresh();
                }
                g_free(confirm_msg);
            }
            g_free(node_name);
        }
        g_list_foreach(selected_rows, (GFunc)gtk_tree_path_free, NULL);
        g_list_free(selected_rows);
    }
}

// Callback for "Add Current File to Project Tree" menu item
static void on_add_current_file_to_project_activated(GtkMenuItem *menuitem, gpointer user_data)
{
    GeanyDocument *doc;
    if (!current_project_tree) return;

    doc = document_get_current();
    if (doc && doc->file_name)
    {
        GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(project_tree_view));
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

// Callback for "Add All Open Files to Project Tree" menu item
static void on_add_all_open_files_to_project_activated(GtkMenuItem *menuitem, gpointer user_data)
{
    guint page_num = 0;
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;
    ProjectTreeNode *selected_node = NULL;
    GeanyDocument *doc = NULL;
    ProjectTreeNode *last_added;

    if (!current_project_tree) return;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(project_tree_view));
    
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
                {
                    last_added = add_file_to_project(doc->file_name, last_added);
                }
                else
                {
                    last_added = insert_file_to_project_after(doc->file_name, last_added);
                }
            }
            else
            {
                last_added = add_file_to_project(doc->file_name, NULL);
            }
        }
        page_num++;
    }
}

// Callback for "Save Project Tree" menu item
static void on_save_project_tree_activated(GtkMenuItem *menuitem, gpointer user_data)
{
    gchar *message;
    if (!current_project_tree) return;

    project_tree_save(current_project_tree);
    message = g_strdup_printf(_("Project tree saved to '%s'"), current_project_tree->project_file_path);
    dialogs_show_msgbox(GTK_MESSAGE_INFO, message);
    g_free(message);
}

// Callback for "Save Session" menu item

static void on_save_session_activated(GtkMenuItem *menuitem, gpointer user_data)

{

    gchar *message;

    if (!current_project_tree) return;



    // Update open groups list from UI state

    g_slist_foreach(current_project_tree->open_groups, (GFunc)g_free, NULL);

    g_slist_free(current_project_tree->open_groups);

    current_project_tree->open_groups = sidebar_get_open_groups();



    project_tree_save_session(current_project_tree);

    message = g_strdup_printf(_("Session saved to '%s'"), current_project_tree->session_file_path);

    dialogs_show_msgbox(GTK_MESSAGE_INFO, message);

    g_free(message);

}
