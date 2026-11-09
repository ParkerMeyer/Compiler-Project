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

class parser {
private:
    scanner* scan;
    error_handler* errors;

    void expect(symbol::symbol_type expected);
    void program();
    void block();
    void statement_list();
    void statement();

public:
    parser(scanner* s, error_handler* e);
    void parse();
};

#endif