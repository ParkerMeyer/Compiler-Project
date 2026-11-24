/*
 * idtable.cpp
 *
 * Written:            1 June 2020
 * Last modified:      23 Nov 2026
 *      Author: Michael Oudshoorn
 * Modified by: Parker Meyer
 * Course: CSC-4510
 * Description: Added scope entry functionality
 * for the semantic analysis symbol table.
 */



#include <iostream>
#include <string>

#include "token.h"
#include "error_handler.h"
#include "id_table.h"

using namespace std;


id_table::id_table(error_handler* err)
{
    error = err;
    // INSERT CODE HERE
}


/*
 * Enter a new scope by increasing the scope level.
 * Each scope has its own BST in the scope_roots array.
 */
void id_table::enter_scope()
{
    // Prevent exceeding our maximum number of scopes.
    if (current_scope >= MAX_SCOPES - 1)
    {
        throw runtime_error("Maximum scope level exceeded");
    }

    current_scope++;
}


/*
 * Exit the current scope.
 * Print all active scopes before removing identifiers.
 */
void id_table::exit_scope()
{
    // Prevent exiting the global scope.
    if (current_scope == 0)
    {
        throw runtime_error("Cannot exit global scope");
    }

    // Print all active scopes before removing anything.
    dump_id_table(true);

    // Delete identifiers from the scope being exited.
    delete_tree(scope_roots[current_scope]);
    scope_roots[current_scope] = nullptr;

    // Return to the enclosing scope.
    current_scope--;
}


/*
 * Insert an identifier into the current scope's BST.
 * Returns false if the identifier already exists.
 */
bool id_table::insert_identifier(const string& name,
                                 lille_type type,
                                 lille_kind kind,
                                 int line_number,
                                 int position)
{
    // Start at the root of the current scope's BST.
    bst_node** current = &scope_roots[current_scope];

    // Find the correct location alphabetically.
    while (*current != nullptr)
    {
        if (name == (*current)->name)
        {
            // Duplicate identifier in this scope.
            return false;
        }
        else if (name < (*current)->name)
        {
            current = &((*current)->left);
        }
        else
        {
            current = &((*current)->right);
        }
    }

    // Create and populate a new identifier node.
    bst_node* new_node = new bst_node;

    new_node->name = name;
    new_node->type = type;
    new_node->kind = kind;
    new_node->scope_level = current_scope;
    new_node->line_number = line_number;
    new_node->position = position;

    // Insert the new node into the BST.
    *current = new_node;

    return true;
}


/*
 * Search for an identifier in the symbol table.
 * Begin in the current scope and search outward.
 * Returns true when the identifier is found.
 */
bool id_table::lookup_identifier(const string& name,
                                 lille_type& found_type,
                                 lille_kind& found_kind,
                                 int& found_scope)
{
    // Search from the innermost scope outward.
    for (int level = current_scope; level >= 0; level--)
    {
        bst_node* current = scope_roots[level];

        // Search this scope's BST alphabetically.
        while (current != nullptr)
        {
            if (name == current->name)
            {
                found_type = current->type;
                found_kind = current->kind;
                found_scope = level;

                return true;
            }
            else if (name < current->name)
            {
                current = current->left;
            }
            else
            {
                current = current->right;
            }
        }
    }

    // Identifier does not exist in any active scope.
    return false;
}

/*
 * Add parameter information to a procedure or function.
 * The routine is declared in the enclosing scope.
 */
bool id_table::add_routine_parameter(const string& routine_name,
                                     lille_type parameter_type,
                                     lille_kind parameter_mode)
{
    // Parameters belong to a routine in an enclosing scope.
    if (current_scope == 0)
        return false;

    // Search the enclosing scope's BST.
    bst_node* current = scope_roots[current_scope - 1];

    while (current != nullptr)
    {
        if (routine_name == current->name)
        {
            // Make sure the identifier is a routine.
            if (!current->type.is_type(lille_type::type_proc) &&
                !current->type.is_type(lille_type::type_func))
                return false;

            // Store the parameter's type and passing mode.
            current->parameter_types.push_back(parameter_type);
            current->parameter_modes.push_back(parameter_mode);

            return true;
        }
        else if (routine_name < current->name)
        {
            current = current->left;
        }
        else
        {
            current = current->right;
        }
    }

    return false;

	
}

/*
 * Retrieve the signature of a procedure or function.
 * Search from the current scope outward.
 */
bool id_table::get_routine_signature(
    const string& routine_name,
    vector<lille_type>& parameter_types,
    vector<lille_kind>& parameter_modes,
    lille_type& return_type)
{
    for (int level = current_scope; level >= 0; level--)
    {
        bst_node* current = scope_roots[level];

        while (current != nullptr)
        {
            if (routine_name == current->name)
            {
                // Only procedures and functions have signatures.
                if (!current->type.is_type(lille_type::type_proc) &&
                    !current->type.is_type(lille_type::type_func))
                    return false;

                parameter_types = current->parameter_types;
                parameter_modes = current->parameter_modes;
                return_type = current->return_type;

                return true;
            }
            else if (routine_name < current->name)
            {
                current = current->left;
            }
            else
            {
                current = current->right;
            }
        }
    }

    return false;
}


/*
 * Set a function's return type.
 * Search active scopes from innermost to outermost.
 */
bool id_table::set_routine_return_type(
    const string& routine_name,
    lille_type return_type)
{
    for (int level = current_scope; level >= 0; level--)
    {
        bst_node* current = scope_roots[level];

        while (current != nullptr)
        {
            if (routine_name == current->name)
            {
                // Only functions have return types.
                if (!current->type.is_type(lille_type::type_func))
                    return false;

                current->return_type = return_type;
                return true;
            }
            else if (routine_name < current->name)
            {
                current = current->left;
            }
            else
            {
                current = current->right;
            }
        }
    }

    return false;
}


/*
 * Print a BST using inorder traversal.
 * Left subtree, current identifier, then right subtree.
 */
void id_table::print_tree_inorder(bst_node* node)
{
    if (node == nullptr)
    {
        return;
    }

    // Visit identifiers alphabetically.
    print_tree_inorder(node->left);

    cout << "Name: " << node->name
         << " | Type: " << node->type.to_string()
         << " | Kind: " << node->kind.to_string()
         << " | Scope: " << node->scope_level
         << " | Line: " << node->line_number
         << " | Position: " << node->position
         << endl;


    // Display parameter information for procedures and functions.
    if (node->type.is_type(lille_type::type_proc) ||
        node->type.is_type(lille_type::type_func)) {

        std::cout << "  Parameter Count: "
                  << node->parameter_types.size() << std::endl;

        for (size_t i = 0; i < node->parameter_types.size(); i++) {
            std::cout << "    Parameter " << (i + 1)
                      << " | Type: "
                      << node->parameter_types[i].to_string()
                      << " | Mode: "
                      << node->parameter_modes[i].to_string()
                      << std::endl;
        }

        if (node->type.is_type(lille_type::type_func)) {
            std::cout << "  Return Type: "
                      << node->return_type.to_string()
                      << std::endl;
        }
    }


    print_tree_inorder(node->right);
}


/*
 * Recursively delete a BST.
 * Delete both children before deleting their parent.
 */
void id_table::delete_tree(bst_node* node)
{
    if (node == nullptr)
    {
        return;
    }

    delete_tree(node->left);
    delete_tree(node->right);

    delete node;
}


/*
 * Print the symbol table.
 * dump_all = true prints every active scope.
 * dump_all = false prints only the current scope.
 */
void id_table::dump_id_table(bool dump_all)
{
    if (!dump_all)
    {
        cout << "Symbol Table - Scope Level "
             << current_scope << endl;

        print_tree_inorder(scope_roots[current_scope]);
    }
    else
    {
        cout << "\n===== FULL SYMBOL TABLE =====" << endl;

        // Print every active scope, including the current one.
        for (int level = 0; level <= current_scope; level++)
        {
            cout << "\nScope Level " << level << endl;
            cout << "------------------------" << endl;

            if (scope_roots[level] == nullptr)
            {
                cout << "(No identifiers)" << endl;
            }
            else
            {
                print_tree_inorder(scope_roots[level]);
            }
        }

        cout << "=============================\n" << endl;
    }
}

