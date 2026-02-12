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

#ifndef __PROJECT_TREE_SIDEBAR_H__
#define __PROJECT_TREE_SIDEBAR_H__

#include <geanyplugin.h>

// Enum for columns in the GtkTreeStore
enum
{
    PROJECT_TREE_COLUMN_NAME = 0, // Display name (e.g., Group name, file name)
    PROJECT_TREE_COLUMN_ICON,     // Icon name
    PROJECT_TREE_COLUMN_PATH,     // Full path to the file (for files)
    PROJECT_TREE_COLUMN_IS_GROUP, // Boolean: TRUE if it's a group, FALSE if it's a file
    PROJECT_TREE_COLUMN_LINE,     // Line number (for session data, if applicable)
    PROJECT_TREE_COLUMN_READONLY, // Read-only status (for session data, if applicable)
    PROJECT_TREE_COLUMN_NODE_PTR, // Pointer to the ProjectTreeNode structure
    PROJECT_TREE_N_COLUMNS
};


// Function to create the sidebar widget
void create_sidebar(void);

// Function to destroy the sidebar widget
void destroy_sidebar(void);

// Capture current UI state (expansion/selection) into the project model
void sidebar_sync_ui_state(void);

// Rebuild the UI from the project model (no state capture)
void sidebar_refresh(void);

// Helper for one-step sync and refresh
void sidebar_sync_and_refresh(void);

// Update colors only
void sidebar_update_colors(void);

// Getter for the GtkTreeView widget
GtkTreeView *get_project_tree_view(void);

// Get list of open (expanded) groups
GSList *sidebar_get_open_groups(void);

// Prototypes for functions used in sidebar.c (Geany API functions)
void document_goto_line(GeanyDocument *doc, gint line, gboolean center_line);
gchar* dialogs_show_input (const gchar *title, GtkWindow *parent, const gchar *message, const gchar *default_value);
gboolean dialogs_show_question (const gchar *message, ...) G_GNUC_PRINTF (1, 2);


#endif /* __PROJECT_TREE_SIDEBAR_H__ */