#include "project.h"
#include "project-tree.h" // For geany_data
#include <glib.h> // For GKeyFile
#include <string.h> // For strcmp
#include <stdlib.h> // For atoi
#include <geany/document.h> // For session saving
#include <geany/sciwrappers.h> // For sci_goto_line
#include <geany/scintilla/Scintilla.h> // For SCI_SETREADONLY
#include <geany/scintilla/ScintillaWidget.h> // For scintilla_send_message

// Global pointer to the currently loaded project tree
ProjectTree *current_project_tree = NULL;

ProjectTreeNode *project_tree_node_new(const gchar *name, const gchar *path, gboolean is_group)
{
    ProjectTreeNode *node = g_new0(ProjectTreeNode, 1);
    node->name = g_strdup(name);
    node->path = path ? g_strdup(path) : NULL;
    node->is_group = is_group;
    node->line_number = -1; // Default to -1 (not applicable)
    node->read_only = FALSE; // Default to FALSE
    node->children = NULL;
    node->parent = NULL;
    return node;
}

void project_tree_node_free(ProjectTreeNode *node)
{
    if (!node) return;

    g_free(node->name);
    g_free(node->path);
    g_slist_foreach(node->children, (GFunc)project_tree_node_free, NULL);
    g_slist_free(node->children);
    g_free(node);
}

ProjectTree *project_tree_new(void)
{
    ProjectTree *tree = g_new0(ProjectTree, 1);
    tree->root_nodes = NULL;
    tree->project_file_path = NULL;
    tree->session_file_path = NULL;
    tree->current_active_file_path = NULL;
    return tree;
}

void project_tree_free(ProjectTree *tree)
{
    if (!tree) return;

    g_free(tree->project_file_path);
    g_free(tree->session_file_path);
    g_free(tree->current_active_file_path);
    g_slist_foreach(tree->open_groups, (GFunc)g_free, NULL);
    g_slist_free(tree->open_groups);
    g_slist_foreach(tree->root_nodes, (GFunc)project_tree_node_free, NULL);
    g_slist_free(tree->root_nodes);
    g_free(tree);
}

// Helper to get the "tree path" of a node (e.g. "./Group/SubGroup")
gchar *get_node_tree_path(ProjectTreeNode *node)
{
    if (!node) return NULL;
    if (!node->parent) return g_build_filename(".", node->name, NULL);

    gchar *parent_path = get_node_tree_path(node->parent);
    gchar *full_path = g_build_filename(parent_path, node->name, NULL);
    g_free(parent_path);
    return full_path;
}

ProjectTreeNode *project_tree_add_node(ProjectTree *tree, ProjectTreeNode *parent, ProjectTreeNode *new_node)
{
    if (!tree || !new_node) return NULL;

    if (parent)
    {
        if (!parent->is_group) return NULL; // Can only add children to groups
        new_node->parent = parent;
        parent->children = g_slist_append(parent->children, new_node);
    }
    else
    {
        new_node->parent = NULL;
        tree->root_nodes = g_slist_append(tree->root_nodes, new_node);
    }
    return new_node;
}

ProjectTreeNode *project_tree_insert_node_after(ProjectTree *tree, ProjectTreeNode *after_this, ProjectTreeNode *new_node)
{
    if (!tree || !new_node) return NULL;

    if (!after_this)
    {
        // Add as the first root node
        new_node->parent = NULL;
        tree->root_nodes = g_slist_prepend(tree->root_nodes, new_node);
        return new_node;
    }

    ProjectTreeNode *parent = after_this->parent;
    new_node->parent = parent;

    GSList *list = parent ? parent->children : tree->root_nodes;
    gint idx = g_slist_index(list, after_this);
    
    if (idx == -1) return NULL;

    if (parent)
        parent->children = g_slist_insert(parent->children, new_node, idx + 1);
    else
        tree->root_nodes = g_slist_insert(tree->root_nodes, new_node, idx + 1);

    return new_node;
}

gboolean project_tree_remove_node(ProjectTree *tree, ProjectTreeNode *node_to_remove)
{
    if (!tree || !node_to_remove) return FALSE;

    if (node_to_remove->parent)
    {
        node_to_remove->parent->children = g_slist_remove(node_to_remove->parent->children, node_to_remove);
    }
    else
    {
        tree->root_nodes = g_slist_remove(tree->root_nodes, node_to_remove);
    }
    project_tree_node_free(node_to_remove); // Free the node and its children
    return TRUE;
}

gboolean project_tree_unlink_node(ProjectTree *tree, ProjectTreeNode *node_to_unlink)
{
    if (!tree || !node_to_unlink) return FALSE;

    if (node_to_unlink->parent)
    {
        node_to_unlink->parent->children = g_slist_remove(node_to_unlink->parent->children, node_to_unlink);
    }
    else
    {
        tree->root_nodes = g_slist_remove(tree->root_nodes, node_to_unlink);
    }
    // Do NOT free the node
    node_to_unlink->parent = NULL; // Clear parent pointer
    return TRUE;
}

// Helper to recursively find a node by path
static ProjectTreeNode *find_node_recursive(GSList *nodes, const gchar *path)
{
    GSList *l;
    for (l = nodes; l != NULL; l = g_slist_next(l))
    {
        ProjectTreeNode *node = (ProjectTreeNode *)l->data;
        if (!node->is_group && node->path && strcmp(node->path, path) == 0)
        {
            return node;
        }
        if (node->is_group && node->children)
        {
            ProjectTreeNode *found = find_node_recursive(node->children, path);
            if (found) return found;
        }
    }
    return NULL;
}

ProjectTreeNode *project_tree_find_node_by_path(ProjectTree *tree, const gchar *path)
{
    if (!tree || !path) return NULL;
    return find_node_recursive(tree->root_nodes, path);
}

// Comparison function for sorting keys numerically
static gint compare_numeric_keys(const gchar *a, const gchar *b)
{
    return atoi(a) - atoi(b);
}

// Recursive loader for sections in the new format
static void load_section_recursive(GKeyFile *key_file, ProjectTree *tree, ProjectTreeNode *parent_group, const gchar *section_name)
{
    GError *error = NULL;
    gchar **keys = g_key_file_get_keys(key_file, section_name, NULL, &error);
    if (error) { g_error_free(error); return; }

    // Sort keys numerically
    g_qsort_with_data(keys, g_strv_length(keys), sizeof(gchar *), (GCompareDataFunc)compare_numeric_keys, NULL);

    gchar *repo_root = NULL;
    if (tree->project_file_path)
    {
        gchar *dot_editor = g_path_get_dirname(tree->project_file_path);
        repo_root = g_path_get_dirname(dot_editor);
        g_free(dot_editor);
    }

    for (gint i = 0; keys[i]; i++)
    {
        gchar *key = keys[i];
        gchar *value = g_key_file_get_string(key_file, section_name, key, NULL);
        if (!value) continue;

        if (g_str_has_suffix(key, "-group"))
        {
            // It's a group
            ProjectTreeNode *group_node = project_tree_node_new(value, NULL, TRUE);
            project_tree_add_node(tree, parent_group, group_node);

            // Construct subgroup section name: parent_section/GroupName
            gchar *sub_section = g_build_filename(section_name, value, NULL);
            load_section_recursive(key_file, tree, group_node, sub_section);
            g_free(sub_section);
        }
        else
        {
            // It's a file - resolve to absolute path
            gchar *abs_path;
            if (!g_path_is_absolute(value) && repo_root)
            {
                abs_path = g_build_filename(repo_root, value, NULL);
            }
            else
            {
                abs_path = g_strdup(value);
            }

            ProjectTreeNode *file_node = project_tree_node_new(g_path_get_basename(abs_path), abs_path, FALSE);
            project_tree_add_node(tree, parent_group, file_node);
            g_free(abs_path);
        }
        g_free(value);
    }
    g_free(repo_root);
    g_strfreev(keys);
}

static void load_project_layout(ProjectTree *tree, const gchar *project_ini_path)
{
    GKeyFile *key_file = g_key_file_new();
    GError *error = NULL;

    tree->project_file_path = g_strdup(project_ini_path);

    if (!g_key_file_load_from_file(key_file, project_ini_path, G_KEY_FILE_NONE, &error))
    {
        // Don't warn on missing file, it's expected for new projects
        g_error_free(error);
        g_key_file_free(key_file);
        return;
    }

    // Start recursive loading from the root section [.]
    if (g_key_file_has_group(key_file, "."))
    {
        load_section_recursive(key_file, tree, NULL, ".");
    }

    g_key_file_free(key_file);
}

// Loads the session.ini
static void load_session_data(ProjectTree *tree, const gchar *session_ini_path)
{
    GKeyFile *key_file = g_key_file_new();
    GError *error = NULL;

    tree->session_file_path = g_strdup(session_ini_path);

    if (!g_key_file_load_from_file(key_file, session_ini_path, G_KEY_FILE_NONE, &error))
    {
        g_error_free(error);
        g_key_file_free(key_file);
        return;
    }

    const gchar *group_name = NULL;
    if (g_key_file_has_group(key_file, "open-files"))
        group_name = "open-files";
    else if (g_key_file_has_group(key_file, "Session"))
        group_name = "Session";

    if (group_name)
    {
        gchar **session_keys = g_key_file_get_keys(key_file, group_name, NULL, &error);
        if (error) { g_error_free(error); error = NULL; }

        g_qsort_with_data(session_keys, g_strv_length(session_keys), sizeof(gchar *), (GCompareDataFunc)compare_numeric_keys, NULL);

        for (gint i = 0; session_keys && session_keys[i]; i++)
        {
            gchar *key = session_keys[i];
            gchar *value = g_key_file_get_string(key_file, group_name, key, &error);
            if (error) { g_error_free(error); error = NULL; continue; }

            char *endptr;
            strtol(key, &endptr, 10);
            if (*endptr == '\0') // It's a number key
            {
                gchar **parts = g_strsplit(value, ":", -1);
                gint num_parts = g_strv_length(parts);
                if (num_parts > 0 && parts[0])
                {
                    gchar *file_path = NULL;
                    if (!g_path_is_absolute(parts[0]))
                    {
                        gchar *repo_root = g_path_get_dirname(g_path_get_dirname(tree->session_file_path));
                        file_path = g_build_filename(repo_root, parts[0], NULL);
                        g_free(repo_root);
                    }
                    else
                    {
                        file_path = g_strdup(parts[0]);
                    }

                    GeanyDocument *doc = document_open_file(file_path, FALSE, FALSE, NULL);
                    
                    if (doc)
                    {
                        gint line = -1;
                        gint j;

                        if (num_parts > 1) line = atoi(parts[1]);
                        if (line > 0 && doc->editor && doc->editor->sci)
                        {
                            sci_goto_line(doc->editor->sci, line - 1, TRUE);
                        }

                        // Parse extensible flags (from index 2 onwards)
                        for (j = 2; j < num_parts; j++)
                        {
                            if (strcmp(parts[j], "readonly") == 0)
                            {
                                doc->readonly = TRUE;
                                if (doc->editor && doc->editor->sci)
                                {
                                    scintilla_send_message(doc->editor->sci, SCI_SETREADONLY, TRUE, 0);
                                    document_set_text_changed(doc, doc->changed); /* Trigger UI refresh */
                                }
                            }
                            // Add future flags here
                        }
                    }
                    g_free(file_path);
                }
                g_strfreev(parts);
            }
            else if (strcmp(key, "current_file") == 0)
            {
                if (tree->current_active_file_path) g_free(tree->current_active_file_path);
                tree->current_active_file_path = g_strdup(value);
                
                gchar *abs_current = NULL;
                if (!g_path_is_absolute(value))
                {
                    gchar *repo_root = g_path_get_dirname(g_path_get_dirname(tree->session_file_path));
                    abs_current = g_build_filename(repo_root, value, NULL);
                    g_free(repo_root);
                }
                else
                {
                    abs_current = g_strdup(value);
                }

                GeanyDocument *doc = document_find_by_filename(abs_current);
                if (doc)
                {
                    gint page = document_get_notebook_page(doc);
                    gtk_notebook_set_current_page(GTK_NOTEBOOK(geany_data->main_widgets->notebook), page);
                }
                g_free(abs_current);
            }
            g_free(value);
        }
        g_strfreev(session_keys);
    }

    // Load Open Tree (Expanded Groups)
    if (g_key_file_has_group(key_file, "open-tree"))
    {
        gchar **tree_keys = g_key_file_get_keys(key_file, "open-tree", NULL, &error);
        if (tree_keys)
        {
            g_qsort_with_data(tree_keys, g_strv_length(tree_keys), sizeof(gchar *), (GCompareDataFunc)compare_numeric_keys, NULL);
            for (gint i = 0; tree_keys[i]; i++)
            {
                gchar *val = g_key_file_get_string(key_file, "open-tree", tree_keys[i], NULL);
                if (val)
                    tree->open_groups = g_slist_append(tree->open_groups, val);
            }
            g_strfreev(tree_keys);
        }
    }

    g_key_file_free(key_file);
}

ProjectTree *project_tree_load(const gchar *project_ini_path, const gchar *session_ini_path)
{
    ProjectTree *tree = project_tree_new();
    load_project_layout(tree, project_ini_path);
    load_session_data(tree, session_ini_path);
    return tree;
}

// Helper to get relative path
static gchar *get_relative_path_for_save(const gchar *file_path, const gchar *repo_root)
{
    if (!file_path || !repo_root) return NULL;

    // Use canonical paths for reliable prefix matching
    gchar *abs_file = g_canonicalize_filename(file_path, NULL);
    gchar *abs_root = g_canonicalize_filename(repo_root, NULL);

    gchar *result = NULL;
    if (g_str_has_prefix(abs_file, abs_root))
    {
        const gchar *rel = abs_file + strlen(abs_root);
        while (*rel == G_DIR_SEPARATOR) rel++;
        result = g_strdup(rel);
    }
    else
    {
        GFile *f_file = g_file_new_for_path(abs_file);
        GFile *f_root = g_file_new_for_path(abs_root);
        gchar *rel = g_file_get_relative_path(f_root, f_file);
        
        if (rel)
        {
            if (g_str_has_prefix(rel, "../../.."))
            {
                result = g_strdup(abs_file);
            }
            else
            {
                result = g_strdup(rel);
            }
            g_free(rel);
        }
        else
        {
            result = g_strdup(abs_file);
        }

        g_object_unref(f_file);
        g_object_unref(f_root);
    }

    g_free(abs_file);
    g_free(abs_root);
    return result;
}

// Recursive saver for sections in the new format
static void save_section_recursive(GKeyFile *key_file, GSList *nodes, const gchar *section_name, const gchar *repo_root)
{
    gint key_counter = 10;
    GSList *l;
    for (l = nodes; l != NULL; l = g_slist_next(l))
    {
        ProjectTreeNode *node = (ProjectTreeNode *)l->data;
        if (node->is_group)
        {
            gchar *key = g_strdup_printf("%d-group", key_counter);
            g_key_file_set_string(key_file, section_name, key, node->name);
            g_free(key);

            gchar *sub_section = g_build_filename(section_name, node->name, NULL);
            save_section_recursive(key_file, node->children, sub_section, repo_root);
            g_free(sub_section);
        }
        else
        {
            gchar *key = g_strdup_printf("%d", key_counter);
            gchar *rel_path = get_relative_path_for_save(node->path, repo_root);
            g_key_file_set_string(key_file, section_name, key, rel_path);
            g_free(rel_path);
            g_free(key);
        }
        key_counter += 10;
    }
}

void project_tree_save(ProjectTree *tree)
{
    if (!tree || !tree->project_file_path) return;

    // repo root is parent of .editor (where project_file_path is)
    gchar *dot_editor_dir = g_path_get_dirname(tree->project_file_path);
    gchar *repo_root = g_path_get_dirname(dot_editor_dir);

    GKeyFile *key_file = g_key_file_new();
    save_section_recursive(key_file, tree->root_nodes, ".", repo_root);

    gsize data_len;
    GError *error = NULL;
    gchar *data = g_key_file_to_data(key_file, &data_len, &error);
    
    if (!error)
    {
        if (!g_file_test(dot_editor_dir, G_FILE_TEST_IS_DIR))
        {
            gchar *confirm_msg = g_strdup_printf(_("The directory '%s' does not exist. Create it?"), dot_editor_dir);
            gboolean create_dir = dialogs_show_question(confirm_msg);
            g_free(confirm_msg);

            if (create_dir)
            {
                if (g_mkdir_with_parents(dot_editor_dir, 0755) != 0)
                {
                    g_warning("Failed to create directory '%s'", dot_editor_dir);
                    g_free(dot_editor_dir); g_free(repo_root); g_free(data); g_key_file_free(key_file); return;
                }
            }
            else
            {
                g_free(dot_editor_dir); g_free(repo_root); g_free(data); g_key_file_free(key_file); return;
            }
        }

        if (!g_file_set_contents(tree->project_file_path, data, data_len, &error))
        {
            if (error) { g_warning("Save failed: %s", error->message); g_error_free(error); }
        }
    }
    else { g_error_free(error); }

    g_free(dot_editor_dir);
    g_free(repo_root);
    g_free(data);
    g_key_file_free(key_file);
}

void project_tree_save_session(ProjectTree *tree)
{
    if (!tree || !tree->session_file_path) return;

    gchar *dot_editor_dir = g_path_get_dirname(tree->session_file_path);
    gchar *repo_root = g_path_get_dirname(dot_editor_dir);

    GKeyFile *key_file = g_key_file_new();
    
    // Save Open Files
    gint open_file_idx = 10;
    guint page_num = 0;
    GeanyDocument *doc = NULL;
    while ((doc = document_get_from_page(page_num)) != NULL)
    {
        if (doc && doc->file_name)
        {
            gchar *key = g_strdup_printf("%d", open_file_idx);
            gchar *rel_path = get_relative_path_for_save(doc->file_name, repo_root);
            
            gint line = 1;
            if (doc->editor && doc->editor->sci)
                line = sci_get_current_line(doc->editor->sci) + 1;

            GString *val = g_string_new(rel_path);
            g_string_append_printf(val, ":%d", line);
            
            if (doc->readonly)
                g_string_append(val, ":readonly");

            g_key_file_set_string(key_file, "open-files", key, val->str);
            
            g_free(key);
            g_string_free(val, TRUE);
            g_free(rel_path);
            open_file_idx += 10;
        }
        page_num++;
    }

    GeanyDocument *curr_doc = document_get_current();
    if (curr_doc && curr_doc->file_name)
    {
        gchar *rel_path = get_relative_path_for_save(curr_doc->file_name, repo_root);
        g_key_file_set_string(key_file, "open-files", "current_file", rel_path);
        g_free(rel_path);
    }

    // Save Open Groups (Tree State)
    if (tree->open_groups)
    {
        gint group_idx = 10;
        GSList *l;
        for (l = tree->open_groups; l != NULL; l = g_slist_next(l))
        {
            gchar *key = g_strdup_printf("%d", group_idx);
            g_key_file_set_string(key_file, "open-tree", key, (gchar *)l->data);
            g_free(key);
            group_idx += 10;
        }
    }

    gsize data_len;
    GError *error = NULL;
    gchar *data = g_key_file_to_data(key_file, &data_len, &error);

    if (!error)
    {
        if (!g_file_test(dot_editor_dir, G_FILE_TEST_IS_DIR))
        {
             if (g_mkdir_with_parents(dot_editor_dir, 0755) != 0)
             {
                 g_warning("Failed to create directory '%s'", dot_editor_dir);
             }
        }

        if (!g_file_set_contents(tree->session_file_path, data, data_len, &error))
        {
            if (error) { g_warning("Save failed: %s", error->message); g_error_free(error); }
        }
    }
    else { g_error_free(error); }

    g_free(dot_editor_dir);
    g_free(repo_root);
    g_free(data);
    g_key_file_free(key_file);
}