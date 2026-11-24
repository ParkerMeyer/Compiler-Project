/*
 * id_table.h
 *
 * Written:            1 June 2020
 * Last modified:      23 Nov 2026
 * Author: Michael Oudshoorn
 *
 * Modified by: Parker Meyer
 * Course: CSC-4510
 * Description: Added a BST node structure for
 * storing identifiers during semantic analysis.
 */

#ifndef ID_TABLE_H_
#define ID_TABLE_H_

#include <iostream>
#include <string>
#include <stdexcept>

#include "token.h"
#include "error_handler.h"
#include "lille_type.h"
#include "lille_kind.h"

using namespace std;

class id_table {
private:
	error_handler* error;
        // Each node represents one identifier in the symbol table.
    struct bst_node {
        string name;
        lille_type type;
        lille_kind kind;

        int scope_level;
        int line_number;
        int position;

        bst_node* left = nullptr;
        bst_node* right = nullptr;
    };

    // Print identifiers in alphabetical order.
    void print_tree_inorder(bst_node* node);
    
    // Free all nodes belonging to a BST.
    void delete_tree(bst_node* node);

    // Maximum number of scope levels supported.
    static const int MAX_SCOPES = 100;

    // Each array position stores the root of a separate BST.
    bst_node* scope_roots[MAX_SCOPES] = {};

    // Tracks which scope we are currently working in.
    int current_scope = 0;



public:
    id_table(error_handler* err);

    void enter_scope();
    void exit_scope();

    // Insert an identifier into the current scope's BST.
    bool insert_identifier(const string& name, lille_type type,
                           lille_kind kind, int line_number, int position);

    // Search for an identifier, starting in the current scope.
    bool lookup_identifier(const string& name,
                           lille_type& found_type,
                           lille_kind& found_kind,
                           int& found_scope);

    void dump_id_table(bool dump_all = true);
};

#endif /* ID_TABLE_H_ */
