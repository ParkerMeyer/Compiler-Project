
/*
 * Parker Meyer - CSC-4510
 * Test for BST insertion, lookup, and scope management.
 */

#include <iostream>
#include "id_table.h"

using namespace std;

int main()
{
    id_table table(nullptr);

    lille_type int_type(lille_type::type_integer);
    lille_kind var_kind(lille_kind::variable);

    // Add an identifier to global scope.
    table.insert_identifier("M", int_type, var_kind, 1, 1);

    // Enter scope 1 and add two identifiers.
    table.enter_scope();

    table.insert_identifier("X", int_type, var_kind, 2, 1);
    table.insert_identifier("C", int_type, var_kind, 3, 1);

    // Test duplicate detection.
    cout << "Duplicate rejected (expected 1): "
         << !table.insert_identifier("C", int_type, var_kind, 4, 1)
         << endl;

    // Test lookup from an outer scope.
    lille_type found_type;
    lille_kind found_kind;
    int found_scope = -1;

    bool found = table.lookup_identifier(
        "M", found_type, found_kind, found_scope);

    cout << "Found M (expected 1): " << found << endl;
    cout << "M scope (expected 0): " << found_scope << endl;

    // Print all active scopes, then exit scope 1.
    table.exit_scope();

    // C should no longer be visible.
    found = table.lookup_identifier(
        "C", found_type, found_kind, found_scope);

    cout << "Found C after exit (expected 0): "
         << found << endl;

    return 0;
}
