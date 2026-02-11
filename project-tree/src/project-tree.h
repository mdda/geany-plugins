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

#ifndef __PROJECT_TREE_H__
#define __PROJECT_TREE_H__

#include <geanyplugin.h>
#include "project.h" // For ProjectTreeNode definition

// Global plugin variables
extern GeanyPlugin *geany_plugin;
extern GeanyData *geany_data;
extern gboolean sync_editor_colors;

// Keybinding enum for new actions
enum
{
    PT_KEYBIND_ADD_CURRENT_FILE,
    PT_KEYBIND_ADD_ALL_OPEN_FILES,
    PT_KEYBIND_COUNT
};

// Function declarations for plugin lifecycle
void plugin_init(GeanyData *data);
void plugin_cleanup(void);
GtkWidget *plugin_configure(GtkDialog *dialog);

// Forward declarations for functions used across files
ProjectTreeNode *add_file_to_project(const gchar *file_path, ProjectTreeNode *parent_node);
ProjectTreeNode *insert_file_to_project_after(const gchar *file_path, ProjectTreeNode *after_node);
void on_add_current_file_to_project(GtkMenuItem *menuitem, gpointer user_data);
void on_add_all_open_files_to_project(GtkMenuItem *menuitem, gpointer user_data);


#endif /* __PROJECT_TREE_H__ */