#ifndef __PROJECT_TREE_SIDEBAR_H__
#define __PROJECT_TREE_SIDEBAR_H__

#include <geanyplugin.h>

// Enum for columns in the GtkTreeStore
enum
{
    PROJECT_TREE_COLUMN_NAME = 0, // Display name (e.g., Group name, file name)
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

// Function to refresh/update the sidebar content
void sidebar_refresh(void);

// Getter for the GtkTreeView widget
GtkTreeView *get_project_tree_view(void);

// Prototypes for functions used in sidebar.c (Geany API functions)
void document_goto_line(GeanyDocument *doc, gint line, gboolean center_line);
gchar* dialogs_show_input (const gchar *title, GtkWindow *parent, const gchar *message, const gchar *default_value);
gboolean dialogs_show_question (const gchar *message, ...) G_GNUC_PRINTF (1, 2);


#endif /* __PROJECT_TREE_SIDEBAR_H__ */