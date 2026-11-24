/*
 * Parker Meyer
 * CSC-4510 - Compiler Project
 * parser.h
 *
 * Declares the recursive-descent parser and its
 * functions for recognizing Lille syntax.
 */

#ifndef PARSER_H_
#define PARSER_H_

#include "scanner.h"
#include "error_handler.h"
#include "id_table.h"
#include <vector>

class parser {
private:
    scanner* scan;
    error_handler* errors;
        // Pointer to the compiler's shared symbol table.
    id_table* id_tab = nullptr;
    // Temporarily save identifiers from a declaration.
    std::vector<token> declaration_names;

    void expect(symbol::symbol_type expected);
    void program();
    void block();
    void declaration();
    void identifier_list();
    void type_name();
    void parameter_list();
    void parameter();
    void statement_list();
    void statement();
    void if_statement();
    void loop_statement();
    void for_statement();
    void while_statement();
    void range();
    bool starts_expression();
    void expression();
    void simple_expression();
    void expression2();
    void term();
    void factor();
    void primary();
public:
    parser(scanner* s, error_handler* e);
    void parse();
        // Connect the parser to the existing symbol table.
    void set_id_table(id_table* table) {
        id_tab = table;
    }
};


#endif
