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

#ifndef __PROJECT_TREE_PROJECT_H__
#define __PROJECT_TREE_PROJECT_H__

#include <geanyplugin.h>

// Data structure to represent a node in our project tree (file or group)
typedef struct _ProjectTreeNode
{
    gchar *name;         // Display name
    gchar *path;         // Full path for files, NULL for groups
    gboolean is_group;   // TRUE for a group, FALSE for a file
    gint line_number;    // Line number for session data, -1 if not applicable
    gboolean read_only;  // Read-only status for session data, FALSE if not applicable

    GSList *children;    // List of _ProjectTreeNode for groups
    struct _ProjectTreeNode *parent; // Pointer to parent node for easy traversal
} ProjectTreeNode;

// Data structure to hold the entire project tree
typedef struct _ProjectTree
{
    gchar *project_file_path; // Path to the .editor/project.ini file
    gchar *session_file_path; // Path to the .editor/session.ini file
    GSList *root_nodes;       // List of top-level ProjectTreeNodes
    gchar *current_active_file_path; // Path of the currently active file from session
    GSList *open_groups;      // List of paths (strings) of open groups for session restoration
    // TODO: Add other project-specific metadata if needed
} ProjectTree;

extern ProjectTree *current_project_tree; // Global pointer to the currently loaded project

// Function declarations
ProjectTree *project_tree_new(void);
void project_tree_free(ProjectTree *tree);

// INI file parsing and writing
ProjectTree *project_tree_load(const gchar *project_ini_path, const gchar *session_ini_path);

typedef enum {
    SAVE_RESULT_FAILED = 0,
    SAVE_RESULT_SAVED,
    SAVE_RESULT_CREATED
} SaveResult;

gint project_tree_save(ProjectTree *tree);
gint project_tree_save_session(ProjectTree *tree);


// Node management
ProjectTreeNode *project_tree_node_new(const gchar *name, const gchar *path, gboolean is_group);
void project_tree_node_free(ProjectTreeNode *node);
gchar *get_node_tree_path(ProjectTreeNode *node);
ProjectTreeNode *project_tree_find_node_by_path(ProjectTree *tree, const gchar *path);
ProjectTreeNode *project_tree_add_node(ProjectTree *tree, ProjectTreeNode *parent, ProjectTreeNode *new_node);
ProjectTreeNode *project_tree_insert_node_after(ProjectTree *tree, ProjectTreeNode *after_this, ProjectTreeNode *new_node);
gboolean project_tree_remove_node(ProjectTree *tree, ProjectTreeNode *node_to_remove);
gboolean project_tree_unlink_node(ProjectTree *tree, ProjectTreeNode *node_to_unlink);

#endif /* __PROJECT_TREE_PROJECT_H__ */
