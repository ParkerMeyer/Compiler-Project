/*
 * Parker Meyer
 * CSC-4510 - Compiler Project
 * parser.cpp
 *
 * Implements recursive-descent parsing for Lille.
 * Initial implementation handles program structure,
 * blocks, and NULL statements.
 */

#include "parser.h"

namespace {
    struct parse_failed {};
}

parser::parser(scanner* s, error_handler* e)
    : scan(s), errors(e) {}

// Require a token and stop on the first syntax error.
void parser::expect(symbol::symbol_type expected) {
    int before = errors->error_count();

    scan->must_be(expected);

    if (errors->error_count() > before)
        throw parse_failed{};
}

// Begin parsing at the first token.
void parser::parse() {
    scan->get_token();

    try {
        program();
        expect(symbol::end_of_program);
    }
    catch (const parse_failed&) {
        // Error was already recorded by the error handler.
    }
}

// <prog> ::= program <ident> is <block> ;
void parser::program() {
    expect(symbol::program_sym);
    expect(symbol::identifier);
    expect(symbol::is_sym);

    block();

    expect(symbol::semicolon_sym);
}

// Initial block implementation.
// Declaration parsing will be added next.
void parser::block() {
    expect(symbol::begin_sym);

    statement_list();

    expect(symbol::end_sym);

    // The identifier following END is optional.
    if (scan->have(symbol::identifier))
        expect(symbol::identifier);
}

// <statement_list> ::= <statement> ;
//                      { <statement> ; }*
void parser::statement_list() {
    statement();
    expect(symbol::semicolon_sym);

    // Initially support repeated NULL statements.
    while (scan->have(symbol::null_sym)) {
        statement();
        expect(symbol::semicolon_sym);
    }
}

// Initial statement implementation.
// Additional statement types will be added next.
void parser::statement() {
    if (scan->have(symbol::null_sym)) {
        expect(symbol::null_sym);
    }
    else {
        errors->flag(scan->this_token(), 80);
        throw parse_failed{};
    }
}