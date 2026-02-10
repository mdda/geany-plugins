#include "project-tree.h"
#include "sidebar.h"
#include "project.h"
#include <geany/document.h> // Explicitly include for document_get_from_page() and document_get_count()

// Global plugin variables
GeanyPlugin *geany_plugin;
GeanyData *geany_data;

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
    
    sidebar_refresh();
    return ret;
}

// Helper to add a file to the project tree after a specific node
ProjectTreeNode *insert_file_to_project_after(const gchar *file_path, ProjectTreeNode *after_node)
{
    if (!current_project_tree || !file_path) return NULL;

    ProjectTreeNode *new_file_node = project_tree_node_new(g_path_get_basename(file_path), file_path, FALSE);
    ProjectTreeNode *ret = project_tree_insert_node_after(current_project_tree, after_node, new_file_node);
    
    sidebar_refresh();
    return ret;
}

static gboolean on_idle_refresh(gpointer user_data)
{
    sidebar_refresh();
    return FALSE; // Run once
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

GtkWidget *plugin_configure(GtkDialog *dialog)
{
    GtkWidget *vbox = gtk_vbox_new(FALSE, 6);
    GtkWidget *checkbox = gtk_check_button_new_with_label(_("Display project tree sidebar"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), display_sidebar);
    gtk_box_pack_start(GTK_BOX(vbox), checkbox, FALSE, FALSE, 0);
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