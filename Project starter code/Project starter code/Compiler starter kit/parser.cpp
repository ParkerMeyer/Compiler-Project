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
// <block> ::= { <declaration> }* begin <statement_list> end [ <ident> ]
void parser::block() {

    // FIRST(declaration) contains identifiers, PROCEDURE, and FUNCTION.
    // BEGIN marks the end of the optional declaration section.
    while (scan->have(symbol::identifier) ||
           scan->have(symbol::procedure_sym) ||
           scan->have(symbol::function_sym)) {
        declaration();
    }

    expect(symbol::begin_sym);
    statement_list();
    expect(symbol::end_sym);

    // A block can optionally end with an identifier.
    if (scan->have(symbol::identifier))
        expect(symbol::identifier);
}


// Handles variable, procedure, and function declarations.
void parser::declaration() {

    if (scan->have(symbol::identifier)) {
        identifier_list();
        expect(symbol::colon_sym);

        // CONSTANT is optional in the grammar.
        if (scan->have(symbol::constant_sym))
            expect(symbol::constant_sym);

        type_name();

        // A declaration can optionally have a literal initializer.
        // Type and constant restrictions belong to semantic analysis.
        if (scan->have(symbol::becomes_sym)) {
            expect(symbol::becomes_sym);

            if (scan->have(symbol::integer))
                expect(symbol::integer);
            else if (scan->have(symbol::real_num))
                expect(symbol::real_num);
            else if (scan->have(symbol::strng))
                expect(symbol::strng);
            else if (scan->have(symbol::true_sym))
                expect(symbol::true_sym);
            else if (scan->have(symbol::false_sym))
                expect(symbol::false_sym);
            else {
                errors->flag(scan->this_token(), 112);
                throw parse_failed{};
            }
        }

        expect(symbol::semicolon_sym);
    }
    else if (scan->have(symbol::procedure_sym) ||
             scan->have(symbol::function_sym)) {

        bool is_function = scan->have(symbol::function_sym);

        expect(is_function ? symbol::function_sym
                           : symbol::procedure_sym);

        expect(symbol::identifier);

        // Formal parameters are optional.
        if (scan->have(symbol::left_paren_sym)) {
            expect(symbol::left_paren_sym);
            parameter_list();
            expect(symbol::right_paren_sym);
        }

        // Functions require a return type in their declaration.
        if (is_function) {
            expect(symbol::return_sym);
            type_name();
        }

        expect(symbol::is_sym);

        // Procedures and functions contain their own blocks.
        block();

        expect(symbol::semicolon_sym);
    }
    else {
        errors->flag(scan->this_token(), 106);
        throw parse_failed{};
    }
}


// <ident_list> ::= <ident> { , <ident> }*
void parser::identifier_list() {
    expect(symbol::identifier);

    while (scan->have(symbol::comma_sym)) {
        expect(symbol::comma_sym);
        expect(symbol::identifier);
    }
}


// <type> ::= integer | real | string | boolean
void parser::type_name() {
    if (scan->have(symbol::integer_sym))
        expect(symbol::integer_sym);
    else if (scan->have(symbol::real_sym))
        expect(symbol::real_sym);
    else if (scan->have(symbol::string_sym))
        expect(symbol::string_sym);
    else if (scan->have(symbol::boolean_sym))
        expect(symbol::boolean_sym);
    else {
        errors->flag(scan->this_token(), 96);
        throw parse_failed{};
    }
}


// <param_list> ::= <param> { ; <param> }*
void parser::parameter_list() {
    parameter();

    while (scan->have(symbol::semicolon_sym)) {
        expect(symbol::semicolon_sym);
        parameter();
    }
}


// <param> ::= <ident_list> : <param_kind> <type>
void parser::parameter() {
    identifier_list();
    expect(symbol::colon_sym);

    if (scan->have(symbol::value_sym))
        expect(symbol::value_sym);
    else if (scan->have(symbol::ref_sym))
        expect(symbol::ref_sym);
    else {
        errors->flag(scan->this_token(), 94);
        throw parse_failed{};
    }

    type_name();
}



// FIRST(statement) tells us which tokens can begin a statement.
// END, ELSE, and ELSIF are not included because they signal
// that the current statement list should finish.
static bool starts_statement(scanner* scan) {
    return scan->have(symbol::identifier) ||
           scan->have(symbol::exit_sym) ||
           scan->have(symbol::return_sym) ||
           scan->have(symbol::read_sym) ||
           scan->have(symbol::write_sym) ||
           scan->have(symbol::writeln_sym) ||
           scan->have(symbol::null_sym) ||
           scan->have(symbol::if_sym) ||
           scan->have(symbol::loop_sym) ||
           scan->have(symbol::for_sym) ||
           scan->have(symbol::while_sym);
}

// <statement_list> ::= <statement> ; { <statement> ; }*
void parser::statement_list() {
    statement();
    expect(symbol::semicolon_sym);

    while (starts_statement(scan)) {
        statement();
        expect(symbol::semicolon_sym);
    }
}

// Check whether the current token can begin an expression.
// This will also be used for optional RETURN expressions.
bool parser::starts_expression() {
    return scan->have(symbol::identifier) ||
           scan->have(symbol::integer) ||
           scan->have(symbol::real_num) ||
           scan->have(symbol::strng) ||
           scan->have(symbol::true_sym) ||
           scan->have(symbol::false_sym) ||
           scan->have(symbol::left_paren_sym) ||
           scan->have(symbol::plus_sym) ||
           scan->have(symbol::minus_sym) ||
           scan->have(symbol::not_sym) ||
           scan->have(symbol::odd_sym);
}

// Initial expression parser.
// Currently handles literals, identifiers, function calls,
// parentheses, and prefix operators.
// Binary operators and precedence will be added next.
void parser::expression() {
    if (scan->have(symbol::plus_sym) ||
        scan->have(symbol::minus_sym) ||
        scan->have(symbol::not_sym) ||
        scan->have(symbol::odd_sym)) {

        if (scan->have(symbol::plus_sym))
            expect(symbol::plus_sym);
        else if (scan->have(symbol::minus_sym))
            expect(symbol::minus_sym);
        else if (scan->have(symbol::not_sym))
            expect(symbol::not_sym);
        else
            expect(symbol::odd_sym);

        expression();
    }
    else if (scan->have(symbol::left_paren_sym)) {
        expect(symbol::left_paren_sym);
        expression();
        expect(symbol::right_paren_sym);
    }
    else if (scan->have(symbol::identifier)) {
        expect(symbol::identifier);

        // An identifier can be followed by function arguments.
        if (scan->have(symbol::left_paren_sym)) {
            expect(symbol::left_paren_sym);
            expression();

            while (scan->have(symbol::comma_sym)) {
                expect(symbol::comma_sym);
                expression();
            }

            expect(symbol::right_paren_sym);
        }
    }
    else if (scan->have(symbol::integer))
        expect(symbol::integer);
    else if (scan->have(symbol::real_num))
        expect(symbol::real_num);
    else if (scan->have(symbol::strng))
        expect(symbol::strng);
    else if (scan->have(symbol::true_sym))
        expect(symbol::true_sym);
    else if (scan->have(symbol::false_sym))
        expect(symbol::false_sym);
    else {
        errors->flag(scan->this_token(), 92);
        throw parse_failed{};
    }
}

// Parse a simple Lille statement based on its starting token.
void parser::statement() {

    if (scan->have(symbol::identifier)) {
        expect(symbol::identifier);

        if (scan->have(symbol::becomes_sym)) {
            // Assignment: identifier := expression
            expect(symbol::becomes_sym);
            expression();
        }
        else if (scan->have(symbol::left_paren_sym)) {
            // Procedure call with arguments.
            expect(symbol::left_paren_sym);
            expression();

            while (scan->have(symbol::comma_sym)) {
                expect(symbol::comma_sym);
                expression();
            }

            expect(symbol::right_paren_sym);
        }

        // Otherwise, the identifier alone is a procedure call.
    }
    else if (scan->have(symbol::exit_sym)) {
        expect(symbol::exit_sym);

        // EXIT optionally includes WHEN followed by an expression.
        if (scan->have(symbol::when_sym)) {
            expect(symbol::when_sym);
            expression();
        }
    }
    else if (scan->have(symbol::return_sym)) {
        expect(symbol::return_sym);

        // RETURN can have an optional expression.
        if (starts_expression())
            expression();
    }
    else if (scan->have(symbol::read_sym)) {
        expect(symbol::read_sym);

        // READ accepts an identifier list with optional parentheses.
        bool wrapped = scan->have(symbol::left_paren_sym);

        if (wrapped)
            expect(symbol::left_paren_sym);

        identifier_list();

        if (wrapped)
            expect(symbol::right_paren_sym);
    }
    else if (scan->have(symbol::write_sym) ||
             scan->have(symbol::writeln_sym)) {

        bool is_writeln = scan->have(symbol::writeln_sym);

        if (is_writeln)
            expect(symbol::writeln_sym);
        else
            expect(symbol::write_sym);

        bool wrapped = scan->have(symbol::left_paren_sym);

        if (wrapped)
            expect(symbol::left_paren_sym);

        // WRITE requires an expression.
        // WRITELN can have an empty expression list.
        if (!is_writeln || starts_expression()) {
            expression();

            while (scan->have(symbol::comma_sym)) {
                expect(symbol::comma_sym);
                expression();
            }
        }

        if (wrapped)
            expect(symbol::right_paren_sym);
    }
    else if (scan->have(symbol::null_sym)) {
        expect(symbol::null_sym);
    }
    else {
        // Compound statements will be implemented next.
        errors->flag(scan->this_token(), 80);
        throw parse_failed{};
    }
}
