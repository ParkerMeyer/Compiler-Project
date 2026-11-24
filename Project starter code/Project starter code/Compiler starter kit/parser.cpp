
/*
 * Parker Meyer
 * CSC-4510 - Compiler Project
 * parser.cpp
 *
 * Implements recursive-descent parsing for Lille.
 * Implements program structure, declarations, simple and compound
 * statements, and precedence-aware expression parsing.
 *
 * Semantic Analysis:
 * Uses the symbol table to track identifiers and scopes,
 * detects undeclared and duplicate identifiers, checks
 * assignment targets, and begins expression type checking.
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

    // Save the program identifier before advancing the scanner.
    token* program_token = scan->this_token();
    string program_name = program_token->get_identifier_value();
    int program_line = program_token->get_line_number();
    int program_position = program_token->get_pos_on_line();

    expect(symbol::identifier);
    expect(symbol::is_sym);

    // Register Lille's built-in conversion functions.
    // These are available without being declared by the user.
    lille_type builtin_type(lille_type::type_func);
    lille_kind builtin_kind(lille_kind::unknown);

    id_tab->insert_identifier(
        "INT2REAL", builtin_type, builtin_kind, 0, 0);

    id_tab->insert_identifier(
        "REAL2INT", builtin_type, builtin_kind, 0, 0);

    id_tab->insert_identifier(
        "INT2STRING", builtin_type, builtin_kind, 0, 0);

    id_tab->insert_identifier(
        "REAL2STRING", builtin_type, builtin_kind, 0, 0);

    // Store the program name in global scope 0.
    id_tab->insert_identifier(
        program_name,
        lille_type(lille_type::type_prog),
        lille_kind(lille_kind::unknown),
        program_line,
        program_position
    );


    // Enter the program's scope.
    id_tab->enter_scope();

    // Register the parameter signatures of Lille's
    // four built-in conversion functions.

    // INT2REAL accepts one INTEGER by value.
    id_tab->add_routine_parameter(
        "INT2REAL",
        lille_type(lille_type::type_integer),
        lille_kind(lille_kind::value_param)
    );

    // REAL2INT accepts one REAL by value.
    id_tab->add_routine_parameter(
        "REAL2INT",
        lille_type(lille_type::type_real),
        lille_kind(lille_kind::value_param)
    );

    // INT2STRING accepts one INTEGER by value.
    id_tab->add_routine_parameter(
        "INT2STRING",
        lille_type(lille_type::type_integer),
        lille_kind(lille_kind::value_param)
    );

    // REAL2STRING accepts one REAL by value.
    id_tab->add_routine_parameter(
        "REAL2STRING",
        lille_type(lille_type::type_real),
        lille_kind(lille_kind::value_param)
    );


    // Set built-in function return types.
    id_tab->set_routine_return_type(
        "INT2REAL",
        lille_type(lille_type::type_real)
    );

    id_tab->set_routine_return_type(
        "REAL2INT",
        lille_type(lille_type::type_integer)
    );

    id_tab->set_routine_return_type(
        "INT2STRING",
        lille_type(lille_type::type_string)
    );

    id_tab->set_routine_return_type(
        "REAL2STRING",
        lille_type(lille_type::type_string)
    );


    block();


    // Print the symbol table and exit the program's scope.
    id_tab->exit_scope();

    expect(symbol::semicolon_sym);
}

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

        // Save the CONSTANT modifier before consuming it.
        bool is_constant = scan->have(symbol::constant_sym);
        if (is_constant)
            expect(symbol::constant_sym);

        // Determine the declared type.
        lille_type declared_type(lille_type::type_unknown);

        if (scan->have(symbol::integer_sym))
            declared_type = lille_type(lille_type::type_integer);
        else if (scan->have(symbol::real_sym))
            declared_type = lille_type(lille_type::type_real);
        else if (scan->have(symbol::string_sym))
            declared_type = lille_type(lille_type::type_string);
        else if (scan->have(symbol::boolean_sym))
            declared_type = lille_type(lille_type::type_boolean);

        // Parse the type normally.
        type_name();

        // Determine whether this is a variable or constant.
        lille_kind declared_kind(
            is_constant ? lille_kind::constant : lille_kind::variable
        );

        // Insert every declared identifier into the current BST.
        for (token& name_token : declaration_names)
        {
            bool inserted = id_tab->insert_identifier(
                name_token.get_identifier_value(),
                declared_type,
                declared_kind,
                name_token.get_line_number(),
                name_token.get_pos_on_line()
            );

            // Error 82: Duplicate declaration in the same scope.
            if (!inserted)
            {
                errors->flag(&name_token, 82);
                throw parse_failed{};
            }
        }

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

        // Remember the procedure or function name.
        token routine_token = *scan->this_token();
        expect(symbol::identifier);

        // Determine whether this is a function or procedure.
        lille_type routine_type(
            is_function ? lille_type::type_func
                        : lille_type::type_proc
        );

        // Register the name in its enclosing scope.
        bool inserted = id_tab->insert_identifier(
            routine_token.get_identifier_value(),
            routine_type,
            lille_kind(lille_kind::unknown),
            routine_token.get_line_number(),
            routine_token.get_pos_on_line()
        );

        // Error 82: Duplicate declaration.
        if (!inserted) {
            errors->flag(&routine_token, 82);
            throw parse_failed{};
        }

        // Enter a separate scope for parameters and local variables.
        id_tab->enter_scope();


        // Formal parameters are optional.
        if (scan->have(symbol::left_paren_sym)) {
            expect(symbol::left_paren_sym);
            parameter_list(routine_token.get_identifier_value());
            expect(symbol::right_paren_sym);
        }


        // Functions require a return type in their declaration.
        if (is_function) {
            expect(symbol::return_sym);

            // Determine the function's declared return type.
            lille_type declared_return_type(lille_type::type_unknown);

            if (scan->have(symbol::integer_sym))
                declared_return_type = lille_type(lille_type::type_integer);
            else if (scan->have(symbol::real_sym))
                declared_return_type = lille_type(lille_type::type_real);
            else if (scan->have(symbol::string_sym))
                declared_return_type = lille_type(lille_type::type_string);
            else if (scan->have(symbol::boolean_sym))
                declared_return_type = lille_type(lille_type::type_boolean);

            // Consume the declared type.
            type_name();

            // Store the return type in the function's BST entry.
            id_tab->set_routine_return_type(
                routine_token.get_identifier_value(),
                declared_return_type
            );
        }



        expect(symbol::is_sym);

        // Retrieve the declared return type.
        std::vector<lille_type> formal_types;
        std::vector<lille_kind> formal_modes;
        lille_type expected_return_type(lille_type::type_unknown);

        id_tab->get_routine_signature(
            routine_token.get_identifier_value(),
            formal_types,
            formal_modes,
            expected_return_type
        );

        // Track this routine while parsing its body.
        routine_return_types.push_back(expected_return_type);
        routine_has_return.push_back(false);

        // Parse the procedure or function body.
        block();

        // Error 109: Functions require at least one RETURN.
        if (is_function && !routine_has_return.back()) {
            errors->flag(&routine_token, 109);
            throw parse_failed{};
        }

        // Restore the enclosing routine context.
        routine_return_types.pop_back();
        routine_has_return.pop_back();

        // Print and exit this procedure/function scope.
        id_tab->exit_scope();


        expect(symbol::semicolon_sym);
    }
    else {
        errors->flag(scan->this_token(), 106);
        throw parse_failed{};
    }
}

// Parse and temporarily save a list of identifiers.
void parser::identifier_list() {
    // Clear identifiers from the previous declaration.
    declaration_names.clear();

    // Save the identifier before consuming the token.
    declaration_names.push_back(*scan->this_token());
    expect(symbol::identifier);

    while (scan->have(symbol::comma_sym)) {
        expect(symbol::comma_sym);

        // Save each additional identifier.
        declaration_names.push_back(*scan->this_token());
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

/*
 * Parse the parameter list for a procedure or function.
 * Pass the routine name so parameters can be recorded
 * in the routine's symbol-table entry.
 */
void parser::parameter_list(const std::string& routine_name) {
    parameter(routine_name);

    while (scan->have(symbol::semicolon_sym)) {
        expect(symbol::semicolon_sym);
        parameter(routine_name);
    }
}


// Parse parameters and insert them into their procedure/function scope.
void parser::parameter(const std::string& routine_name) {
    identifier_list();
    expect(symbol::colon_sym);

    // Determine whether parameters are VALUE or REF.
    lille_kind param_kind(
        scan->have(symbol::value_sym)
            ? lille_kind::value_param
            : lille_kind::ref_param
    );

    if (scan->have(symbol::value_sym))
        expect(symbol::value_sym);
    else if (scan->have(symbol::ref_sym))
        expect(symbol::ref_sym);
    else {
        errors->flag(scan->this_token(), 94);
        throw parse_failed{};
    }

    // Determine the parameter's type.
    lille_type param_type(lille_type::type_unknown);

    if (scan->have(symbol::integer_sym))
        param_type = lille_type(lille_type::type_integer);
    else if (scan->have(symbol::real_sym))
        param_type = lille_type(lille_type::type_real);
    else if (scan->have(symbol::string_sym))
        param_type = lille_type(lille_type::type_string);
    else if (scan->have(symbol::boolean_sym))
        param_type = lille_type(lille_type::type_boolean);

    type_name();


    // Insert parameters into the current scope's BST.
    for (token& name_token : declaration_names) {
        bool inserted = id_tab->insert_identifier(
            name_token.get_identifier_value(),
            param_type,
            param_kind,
            name_token.get_line_number(),
            name_token.get_pos_on_line()
        );

        // Reject duplicate parameter declarations.
        if (!inserted) {
            errors->flag(&name_token, 82);
            throw parse_failed{};
        }

        // Record this parameter in the enclosing
        // procedure or function's symbol-table entry.
        id_tab->add_routine_parameter(
            routine_name,
            param_type,
            param_kind
        );
    }
}



// FIRST(statement) tells us which tokens can begin a statement.
// END, ELSE, and ELSIF signal that the statement list should finish.
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

// <expr> ::= <simple_expr> [ <relop> <simple_expr> ]
//          | <simple_expr> IN <range>

/*
 * Parse a complete expression and return its type.
 * Relational comparisons and IN expressions produce BOOLEAN.
 */

/*
 * Parse complete expressions and check relational operands.
 * Comparisons must use compatible types and return BOOLEAN.
 */
lille_type parser::expression() {

    lille_type result_type = simple_expression();

    if (scan->have(symbol::equals_sym) ||
        scan->have(symbol::not_equals_sym) ||
        scan->have(symbol::less_than_sym) ||
        scan->have(symbol::greater_than_sym) ||
        scan->have(symbol::less_or_equal_sym) ||
        scan->have(symbol::greater_or_equal_sym)) {

        // Save the comparison operator for error reporting.
        token operator_token = *scan->this_token();

        if (scan->have(symbol::equals_sym))
            expect(symbol::equals_sym);
        else if (scan->have(symbol::not_equals_sym))
            expect(symbol::not_equals_sym);
        else if (scan->have(symbol::less_than_sym))
            expect(symbol::less_than_sym);
        else if (scan->have(symbol::greater_than_sym))
            expect(symbol::greater_than_sym);
        else if (scan->have(symbol::less_or_equal_sym))
            expect(symbol::less_or_equal_sym);
        else
            expect(symbol::greater_or_equal_sym);

        // Determine the right operand's type.
        lille_type right_type = simple_expression();

        // Initially require matching known operand types.
        if (!result_type.is_type(lille_type::type_unknown) &&
            !right_type.is_type(lille_type::type_unknown) &&
            !result_type.is_type(right_type)) {

            // Error 114: Types of expressions must match.
            errors->flag(&operator_token, 114);
            throw parse_failed{};
        }

        // Relational comparisons produce BOOLEAN.
        return lille_type(lille_type::type_boolean);
    }
    else if (scan->have(symbol::in_sym)) {
        expect(symbol::in_sym);
        range();

        return lille_type(lille_type::type_boolean);
    }

    return result_type;
}

// <simple_expr> ::= <expr2> { & <expr2> }*

/*
 * Parse a simple expression and return its type.
 * Handles string concatenation using the & operator.
 */

/*
 * Parse string concatenation expressions.
 * The & operator requires STRING operands.
 */
lille_type parser::simple_expression() {

    // Get the type of the first expression.
    lille_type result_type = expression2();

    while (scan->have(symbol::ampersand_sym)) {

        // Save the operator for error reporting.
        token operator_token = *scan->this_token();

        expect(symbol::ampersand_sym);

        // Get the type of the next expression.
        lille_type right_type = expression2();

        // Check both operand types when known.
        if (!result_type.is_type(lille_type::type_unknown) &&
            !right_type.is_type(lille_type::type_unknown)) {

            if (!result_type.is_type(lille_type::type_string) ||
                !right_type.is_type(lille_type::type_string)) {

                // Error 115: Both expressions must be strings.
                errors->flag(&operator_token, 115);
                throw parse_failed{};
            }

            result_type = lille_type(lille_type::type_string);
        }
        else {
            // Some expression types are not implemented yet.
            result_type = lille_type(lille_type::type_unknown);
        }
    }

    return result_type;
}



// <expr2> ::= <term> { (+ | - | OR) <term> }*

/*
 * Parse addition, subtraction, and Boolean OR expressions.
 * Return the type of the resulting expression.
 */

/*
 * Parse addition, subtraction, and Boolean OR.
 * Check operand types and return the resulting type.
 */
lille_type parser::expression2() {

    // Get the type of the first term.
    lille_type result_type = term();

    while (scan->have(symbol::plus_sym) ||
           scan->have(symbol::minus_sym) ||
           scan->have(symbol::or_sym)) {

        // Remember the operator for error reporting.
        token operator_token = *scan->this_token();

        bool is_or = scan->have(symbol::or_sym);

        if (scan->have(symbol::plus_sym))
            expect(symbol::plus_sym);
        else if (scan->have(symbol::minus_sym))
            expect(symbol::minus_sym);
        else

            expect(symbol::or_sym);

        // Get the type of the right operand.
        lille_type right_type = term();

        // Some expression types are still unfinished.
        if (result_type.is_type(lille_type::type_unknown) ||
            right_type.is_type(lille_type::type_unknown)) {
            result_type = lille_type(lille_type::type_unknown);
            continue;
        }

        if (is_or) {
            // OR requires two Boolean expressions.
            if (!result_type.is_type(lille_type::type_boolean) ||
                !right_type.is_type(lille_type::type_boolean)) {
                errors->flag(&operator_token, 117);
                throw parse_failed{};
            }

            result_type = lille_type(lille_type::type_boolean);
        }
        else {
            // Addition and subtraction require numeric operands.
            bool left_numeric =
                result_type.is_type(lille_type::type_integer) ||
                result_type.is_type(lille_type::type_real);

            bool right_numeric =
                right_type.is_type(lille_type::type_integer) ||
                right_type.is_type(lille_type::type_real);

            if (!left_numeric || !right_numeric) {
                errors->flag(&operator_token, 116);
                throw parse_failed{};
            }

            // Require matching numeric types.
            if (!result_type.is_type(right_type)) {
                errors->flag(&operator_token, 114);
                throw parse_failed{};
            }

            // The result keeps its numeric type.
        }
    }

    return result_type;
}


// <term> ::= <factor> { (* | / | AND) <factor> }*

/*
 * Parse a term and return its resulting type.
 * Handles multiplication, division, and Boolean AND.
 */

/*
 * Parse multiplication, division, and Boolean AND.
 * Check operand types and return the resulting type.
 */
lille_type parser::term() {

    // Get the type of the first factor.
    lille_type result_type = factor();

    while (scan->have(symbol::asterisk_sym) ||
           scan->have(symbol::slash_sym) ||
           scan->have(symbol::and_sym)) {

        // Remember the operator for error reporting.
        token operator_token = *scan->this_token();

        bool is_and = scan->have(symbol::and_sym);

        if (scan->have(symbol::asterisk_sym))
            expect(symbol::asterisk_sym);
        else if (scan->have(symbol::slash_sym))
            expect(symbol::slash_sym);
        else
            expect(symbol::and_sym);

        // Get the right operand's type.
        lille_type right_type = factor();

        // Skip unfinished expression types for now.
        if (result_type.is_type(lille_type::type_unknown) ||
            right_type.is_type(lille_type::type_unknown)) {
            result_type = lille_type(lille_type::type_unknown);
            continue;
        }

        if (is_and) {
            // AND requires two Boolean operands.
            if (!result_type.is_type(lille_type::type_boolean) ||
                !right_type.is_type(lille_type::type_boolean)) {
                errors->flag(&operator_token, 117);
                throw parse_failed{};
            }

            result_type = lille_type(lille_type::type_boolean);
        }
        else {
            // Multiplication and division require numbers.
            bool left_numeric =
                result_type.is_type(lille_type::type_integer) ||
                result_type.is_type(lille_type::type_real);

            bool right_numeric =
                right_type.is_type(lille_type::type_integer) ||
                right_type.is_type(lille_type::type_real);

            if (!left_numeric || !right_numeric) {
                errors->flag(&operator_token, 116);
                throw parse_failed{};
            }

            // Initial rule: require matching numeric types.
            if (!result_type.is_type(right_type)) {
                errors->flag(&operator_token, 114);
                throw parse_failed{};
            }
        }
    }

    return result_type;
}



// <factor> ::= <primary> [ ** <primary> ]
//            | [ + | - ] <primary>

/*
 * Parse a factor and return its type.
 * Handles unary signs and optional exponentiation.
 */

/*
 * Parse a factor and determine its resulting type.
 * Unary signs and exponentiation require numeric operands.
 */
lille_type parser::factor() {

    // Check for unary plus or minus.
    if (scan->have(symbol::plus_sym) ||
        scan->have(symbol::minus_sym)) {

        // Save the operator for error reporting.
        token operator_token = *scan->this_token();

        if (scan->have(symbol::plus_sym))
            expect(symbol::plus_sym);
        else
            expect(symbol::minus_sym);

        lille_type operand_type = primary();

        // Unary signs require INTEGER or REAL.
        if (!operand_type.is_type(lille_type::type_unknown) &&

            !operand_type.is_type(lille_type::type_integer) &&
            !operand_type.is_type(lille_type::type_real)) {

            errors->flag(&operator_token, 118);
            throw parse_failed{};
        }

        return operand_type;
    }

    // Get the type of the primary expression.
    lille_type result_type = primary();

    // Check for exponentiation.
    if (scan->have(symbol::power_sym)) {

        token operator_token = *scan->this_token();
        expect(symbol::power_sym);

        lille_type exponent_type = primary();

        // Both operands must be numeric when known.
        bool left_numeric =
            result_type.is_type(lille_type::type_integer) ||
            result_type.is_type(lille_type::type_real);

        bool right_numeric =
            exponent_type.is_type(lille_type::type_integer) ||
            exponent_type.is_type(lille_type::type_real);

        if ((!left_numeric &&
             !result_type.is_type(lille_type::type_unknown)) ||
            (!right_numeric &&
             !exponent_type.is_type(lille_type::type_unknown))) {

            errors->flag(&operator_token, 118);
            throw parse_failed{};
        }

        // Preserve the type when both numeric types match.
        if (left_numeric && right_numeric &&
            result_type.is_type(exponent_type)) {
            return result_type;
        }

        // Mixed or unknown types need further semantic rules.
        return lille_type(lille_type::type_unknown);
    }

    return result_type;
}


/*
 * Parse a primary expression and return its type.
 * This is the foundation of expression type checking.
 */
lille_type parser::primary() {

    if (scan->have(symbol::not_sym)) {
        expect(symbol::not_sym);
        expression();

        return lille_type(lille_type::type_boolean);
    }
    else if (scan->have(symbol::odd_sym)) {
        expect(symbol::odd_sym);
        expression();

        return lille_type(lille_type::type_boolean);
    }

    else if (scan->have(symbol::left_paren_sym)) {
        expect(symbol::left_paren_sym);

        // Get the type of the expression inside parentheses.
        lille_type inner_type = simple_expression();

        expect(symbol::right_paren_sym);

        // Parentheses preserve the expression's type.
        return inner_type;
    }

    else if (scan->have(symbol::identifier)) {

        token name_token = *scan->this_token();

        lille_type found_type;
        lille_kind found_kind;
        int found_scope = -1;

        bool found = id_tab->lookup_identifier(
            name_token.get_identifier_value(),
            found_type,
            found_kind,
            found_scope
        );

        if (!found) {
            errors->flag(&name_token, 81);
            throw parse_failed{};
        }

        expect(symbol::identifier);


        // Validate function calls and their arguments.
        if (scan->have(symbol::left_paren_sym)) {

            // Only functions can be called in expressions.
            if (!found_type.is_type(lille_type::type_func)) {
                errors->flag(&name_token, 121);
                throw parse_failed{};
            }

            // Retrieve the function's declared signature.
            std::vector<lille_type> parameter_types;
            std::vector<lille_kind> parameter_modes;
            lille_type return_type(lille_type::type_unknown);

            bool signature_found = id_tab->get_routine_signature(
                name_token.get_identifier_value(),
                parameter_types,
                parameter_modes,
                return_type
            );

            if (!signature_found) {
                errors->flag(&name_token, 121);
                throw parse_failed{};
            }


            // Collect function arguments and validate REF parameters.
            std::vector<lille_type> actual_types;

            expect(symbol::left_paren_sym);

            if (!scan->have(symbol::right_paren_sym)) {
                while (true) {
                    size_t index = actual_types.size();
                    token argument_token = *scan->this_token();

                    bool needs_reference =
                        index < parameter_modes.size() &&
                        parameter_modes[index].is_kind(
                            lille_kind::ref_param);

                    if (needs_reference) {
                        // REF arguments must be identifiers.
                        if (!scan->have(symbol::identifier)) {
                            errors->flag(&argument_token, 99);
                            throw parse_failed{};
                        }

                        lille_type argument_type;
                        lille_kind argument_kind;
                        int argument_scope = -1;

                        bool found = id_tab->lookup_identifier(
                            argument_token.get_identifier_value(),
                            argument_type,
                            argument_kind,
                            argument_scope
                        );

                        if (!found) {
                            errors->flag(&argument_token, 81);
                            throw parse_failed{};
                        }

                        expect(symbol::identifier);

                        // REF cannot receive an expression.
                        if (!scan->have(symbol::comma_sym) &&
                            !scan->have(symbol::right_paren_sym)) {
                            errors->flag(&argument_token, 99);
                            throw parse_failed{};
                        }

                        // Only assignable identifiers qualify.
                        if (!argument_kind.is_kind(lille_kind::variable) &&
                            !argument_kind.is_kind(lille_kind::ref_param)) {
                            errors->flag(&argument_token, 99);
                            throw parse_failed{};
                        }

                        actual_types.push_back(argument_type);
                    }
                    else {
                        // VALUE parameters accept expressions.
                        actual_types.push_back(expression());
                    }

                    if (!scan->have(symbol::comma_sym))
                        break;

                    expect(symbol::comma_sym);
                }
            }

            expect(symbol::right_paren_sym);


            // Error 97: Incorrect number of arguments.
            if (actual_types.size() != parameter_types.size()) {
                errors->flag(&name_token, 97);
                throw parse_failed{};
            }

            // Error 98: Incompatible argument types.
            for (size_t i = 0; i < actual_types.size(); i++) {
                if (!actual_types[i].is_type(lille_type::type_unknown) &&
                    !parameter_types[i].is_type(lille_type::type_unknown) &&
                    !actual_types[i].is_type(parameter_types[i])) {

                    errors->flag(&name_token, 98);
                    throw parse_failed{};
                }
            }

            // Return the function's declared result type.
            return return_type;
        }


        // Return the declared identifier type.
        return found_type;
    }
    else if (scan->have(symbol::integer)) {
        expect(symbol::integer);
        return lille_type(lille_type::type_integer);
    }
    else if (scan->have(symbol::real_num)) {
        expect(symbol::real_num);
        return lille_type(lille_type::type_real);
    }
    else if (scan->have(symbol::strng)) {
        expect(symbol::strng);
        return lille_type(lille_type::type_string);
    }
    else if (scan->have(symbol::true_sym)) {
        expect(symbol::true_sym);
        return lille_type(lille_type::type_boolean);
    }
    else if (scan->have(symbol::false_sym)) {
        expect(symbol::false_sym);
        return lille_type(lille_type::type_boolean);
    }
    else {
        errors->flag(scan->this_token(), 92);
        throw parse_failed{};
    }
}

// Parse a simple Lille statement based on its starting token.
void parser::statement() {

    if (scan->have(symbol::identifier)) {
        // Save the identifier before consuming its token.
        token name_token = *scan->this_token();
        expect(symbol::identifier);

        if (scan->have(symbol::becomes_sym)) {
            // Assignment: identifier := expression.
            lille_type found_type;
            lille_kind found_kind;
            int found_scope = -1;

            // Look up the assignment target, searching outward through scopes.
            bool found = id_tab->lookup_identifier(
                name_token.get_identifier_value(),
                found_type,
                found_kind,
                found_scope
            );

            // Error 81: assignment to an undeclared identifier.
            if (!found) {
                errors->flag(&name_token, 81);
                throw parse_failed{};
            }

            // Error 85: Only variables and reference
            // parameters can be assignment targets.
            if (!found_kind.is_kind(lille_kind::variable) &&
                !found_kind.is_kind(lille_kind::ref_param))
            {
                errors->flag(&name_token, 85);
                throw parse_failed{};
            }


            expect(symbol::becomes_sym);

            // Remember where the expression begins for error reporting.
            token expression_token = *scan->this_token();

            // Parse the expression and determine its type.
            lille_type expression_type = expression();

            // Check compatibility when both types are known.
            if (!expression_type.is_type(lille_type::type_unknown) &&
                !found_type.is_type(lille_type::type_unknown) &&
                !found_type.is_type(expression_type))
            {
                // Error 93: Assignment types are incompatible.
                errors->flag(&expression_token, 93);
                throw parse_failed{};
            }

        }

        else {
            // This identifier must be a declared procedure.
            lille_type found_type;
            lille_kind found_kind;
            int found_scope = -1;

            bool found = id_tab->lookup_identifier(
                name_token.get_identifier_value(),
                found_type,
                found_kind,
                found_scope
            );

            // Error 81: Undeclared procedure.
            if (!found) {
                errors->flag(&name_token, 81);
                throw parse_failed{};
            }

            // Error 90: Identifier must be a procedure.
            if (!found_type.is_type(lille_type::type_proc)) {
                errors->flag(&name_token, 90);
                throw parse_failed{};
            }

            // Retrieve the procedure's formal parameters.
            std::vector<lille_type> parameter_types;
            std::vector<lille_kind> parameter_modes;
            lille_type return_type(lille_type::type_unknown);

            bool signature_found = id_tab->get_routine_signature(
                name_token.get_identifier_value(),
                parameter_types,
                parameter_modes,
                return_type
            );

            if (!signature_found) {
                errors->flag(&name_token, 90);
                throw parse_failed{};
            }

           
            // Collect and validate actual arguments.
            std::vector<lille_type> actual_types;

            if (scan->have(symbol::left_paren_sym)) {
                expect(symbol::left_paren_sym);

                if (!scan->have(symbol::right_paren_sym)) {
                    while (true) {
                        size_t index = actual_types.size();
                        token argument_token = *scan->this_token();

                        bool needs_reference =
                            index < parameter_modes.size() &&
                            parameter_modes[index].is_kind(
                                lille_kind::ref_param);

                        if (needs_reference) {
                            // REF arguments must be identifiers.
                            if (!scan->have(symbol::identifier)) {
                                errors->flag(&argument_token, 99);
                                throw parse_failed{};
                            }

                            lille_type argument_type;
                            lille_kind argument_kind;
                            int argument_scope = -1;

                            bool found = id_tab->lookup_identifier(
                                argument_token.get_identifier_value(),
                                argument_type,
                                argument_kind,
                                argument_scope
                            );

                            if (!found) {
                                errors->flag(&argument_token, 81);
                                throw parse_failed{};
                            }

                            expect(symbol::identifier);

                            // Reject expressions such as X + 1.
                            if (!scan->have(symbol::comma_sym) &&
                                !scan->have(symbol::right_paren_sym)) {
                                errors->flag(&argument_token, 99);
                                throw parse_failed{};
                            }

                            // REF requires an assignable identifier.
                            if (!argument_kind.is_kind(lille_kind::variable) &&
                                !argument_kind.is_kind(lille_kind::ref_param)) {
                                errors->flag(&argument_token, 99);
                                throw parse_failed{};
                            }

                            actual_types.push_back(argument_type);
                        }
                        else {
                            // VALUE parameters accept expressions.
                            actual_types.push_back(expression());
                        }

                        if (!scan->have(symbol::comma_sym))
                            break;

                        expect(symbol::comma_sym);
                    }
                }

                expect(symbol::right_paren_sym);
            }

            // Error 97: Parameter count mismatch.
            if (actual_types.size() != parameter_types.size()) {
                errors->flag(&name_token, 97);
                throw parse_failed{};
            }

            // Error 98: Parameter type mismatch.
            for (size_t i = 0; i < actual_types.size(); i++) {
                if (!actual_types[i].is_type(lille_type::type_unknown) &&
                    !parameter_types[i].is_type(lille_type::type_unknown) &&
                    !actual_types[i].is_type(parameter_types[i])) {
                    errors->flag(&name_token, 98);
                    throw parse_failed{};
                }
            }


        }

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
        token return_token = *scan->this_token();
        expect(symbol::return_sym);

        // Error 88: RETURN cannot appear in the main program.
        if (routine_return_types.empty()) {
            errors->flag(&return_token, 88);
            throw parse_failed{};
        }

        // Determine whether RETURN includes an expression.
        bool has_expression = starts_expression();
        lille_type actual_type(lille_type::type_unknown);

        if (has_expression) {
            actual_type = expression();
        }

        // Get the expected type of the active routine.
        lille_type expected_type = routine_return_types.back();

        // Functions must return the declared type.
        if (!expected_type.is_type(lille_type::type_unknown)) {
            if (!has_expression ||
                (!actual_type.is_type(lille_type::type_unknown) &&
                 !actual_type.is_type(expected_type))) {

                // Error 87: Incorrect function return type.
                errors->flag(&return_token, 87);
                throw parse_failed{};
            }

            // This function contains a valid RETURN statement.
            routine_has_return.back() = true;
        }
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
    else if (scan->have(symbol::if_sym)) {
        if_statement();
    }
    else if (scan->have(symbol::loop_sym)) {
        loop_statement();
    }
    else if (scan->have(symbol::for_sym)) {
        for_statement();
    }
    else if (scan->have(symbol::while_sym)) {
        while_statement();
    }
    else {
        errors->flag(scan->this_token(), 80);
        throw parse_failed{};
    }
} // End of statement()

// IF <expr> THEN <statement_list>
// {ELSIF <expr> THEN <statement_list>}*
// [ELSE <statement_list>] END IF
void parser::if_statement() {
    expect(symbol::if_sym);
    expression();
    expect(symbol::then_sym);
    statement_list();

    // ELSIF may appear zero or more times.
    while (scan->have(symbol::elsif_sym)) {
        expect(symbol::elsif_sym);
        expression();
        expect(symbol::then_sym);
        statement_list();
    }

    // ELSE is optional.
    if (scan->have(symbol::else_sym)) {
        expect(symbol::else_sym);
        statement_list();
    }

    expect(symbol::end_sym);
    expect(symbol::if_sym);
}

// LOOP <statement_list> END LOOP
void parser::loop_statement() {
    expect(symbol::loop_sym);
    statement_list();
    expect(symbol::end_sym);
    expect(symbol::loop_sym);
}

// WHILE <expr> <loop_statement>
void parser::while_statement() {
    expect(symbol::while_sym);
    expression();

    // Reuse the existing loop grammar.
    loop_statement();
}

/*
 * Parse a FOR loop and register its loop identifier.
 * Each FOR loop has its own scope.
 */
void parser::for_statement() {
    expect(symbol::for_sym);

    // Remember the loop identifier before consuming it.
    token loop_token = *scan->this_token();
    expect(symbol::identifier);

    // Create a new scope for this FOR loop.
    id_tab->enter_scope();

    // Insert the loop identifier as an INTEGER.
    bool inserted = id_tab->insert_identifier(
        loop_token.get_identifier_value(),
        lille_type(lille_type::type_integer),
        lille_kind(lille_kind::for_ident),
        loop_token.get_line_number(),
        loop_token.get_pos_on_line()
    );

    // Reject duplicate declarations in this scope.
    if (!inserted) {
        errors->flag(&loop_token, 82);
        throw parse_failed{};
    }

    expect(symbol::in_sym);

    // REVERSE is optional.
    if (scan->have(symbol::reverse_sym))
        expect(symbol::reverse_sym);

    range();
    loop_statement();

    // Print the active symbol tables before exiting.
    id_tab->exit_scope();
}

// <range> ::= <simple_expr> .. <simple_expr>
void parser::range() {
    simple_expression();
    expect(symbol::range_sym);
    simple_expression();
}
