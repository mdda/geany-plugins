#ifndef __PROJECT_TREE_H__
#define __PROJECT_TREE_H__

#include <geanyplugin.h>
#include "project.h" // For ProjectTreeNode definition

// Global plugin variables
extern GeanyPlugin *geany_plugin;
extern GeanyData *geany_data;

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