//
// Created by Clarkok on 16/4/29.
//

#include <iostream>
#include <ctype.h>
#include <cassert>

#include "parse.hpp"

using namespace cyan;

int
Parser::_peak()
{
    if (peaking_token == T_UNPEAKED) { return _next(); }
    return peaking_token;
}

int
Parser::_next()
{
    if (_endOfInput()) {
        return (peaking_token = T_EOF);
    }

    while (!_endOfInput() && isspace(_current())) {
        _forward();
    }

    token_start = current;

    while (!_endOfInput()) {
        if (isalpha(_current()) || _current() == '_') {
            return _parseId();
        }
        else if (isdigit(_current())) {
            return _parseNumber();
        }
        else if (_current() == '\"') {
            return _parseString();
        }
        else if (_current() == '/') {
            if (!_endOfInput(1)) {
                if (_current(1) == '*') {
                    _parseBlockComment();
                    continue;
                }
                else if (_current(1) == '/') {
                    _parseLineComment();
                    continue;
                }
            }
            peaking_token = _current();
            _forward();
            break;
        }
        else {
            peaking_token = *current++;
            break;
        }
    }

    return peaking_token;
}

Parser::Token
Parser::_parseId()
{
    assert(isalpha(_current()) || _current() == '_');

    peaking_string = "";
    while (!_endOfInput() && (isalnum(_current()) || _current() == '_')) {
        peaking_string.push_back(_current());
        _forward();
    }

    peaking_token = static_cast<int>(T_ID);
    return T_ID;
}

Parser::Token
Parser::_parseNumber()
{
    assert(isdigit(_current()));

    peaking_int = 0;
    while (!_endOfInput() && (isdigit(_current()))) {
        peaking_int = peaking_int * 10 + _current() - '0';
        _forward();
    }

    peaking_token = static_cast<int>(T_INTEGER);
    return T_INTEGER;
}

Parser::Token
Parser::_parseString()
{
    assert(_current() == '\"');
    _forward();

    peaking_string = "";
    while (!_endOfInput() && (_current() != '\"')) {
        if (_current() == '\\') {
            _forward();

#define escape_seq(escaped, original)           \
    case escaped:                               \
        peaking_string.push_back(original);    \
        break
            switch (_current()) {
                escape_seq('a', '\a');
                escape_seq('b', '\b');
                escape_seq('f', '\f');
                escape_seq('n', '\n');
                escape_seq('r', '\r');
                escape_seq('t', '\t');
                escape_seq('v', '\v');
                escape_seq('0', '\0');
                default:
                    peaking_string.push_back(_current());
                    break;
            }
#undef escape_seq
        }
        else {
            peaking_string.push_back(_current());
        }
        _forward();
    }

    if (_endOfInput()) {
        throw ParseErrorException(location, "Unexpected EOF when parsing string");
    }
    else {
        _forward();
    }

    peaking_token = static_cast<int>(T_STRING);
    return T_STRING;
}

void
Parser::_parseBlockComment()
{
    assert(_current() == '/');
    assert(_forward() == '*');
    _forward();

    while (!_endOfInput()) {
        if (_current() == '*') {
            if (_endOfInput(1)) { return; }
            else if (_current(1) == '/') {
                _forward();
                _forward();
                return;
            }
        }
        else {
            _forward();
        }
    }
}

void
Parser::_parseLineComment()
{
    assert(_current() == '/');
    assert(_forward() == '/');
    _forward();

    while (!_endOfInput() && _current() != '\n') {
        _forward();
    }
}

bool
Parser::parse()
{
    _registerReserved();

    while (_peak() != T_EOF && error_nr < ERROR_NR_LIMIT) {
        try {
            if (_peak() != T_ID) {
                throw ParseErrorException(
                    location,
                    "Only concept / define / let / struct allowed to appear here"
                );
            }

            auto symbol = symbol_table->lookup(peaking_string);
            if (!symbol || symbol->token_value != Symbol::K_RESERVED) {
                throw ParseErrorException(
                    location,
                    "Only concept / define / let / struct allowed to appear here"
                );
            }

            if (symbol->token_value == R_CONCEPT) {
            }
            else if (symbol->token_value == R_DEFINE) {
            }
            else if (symbol->token_value == R_LET) {
                parseLetStmt();
            }
            else if (symbol->token_value == R_STRUCT) {
            }
            else {
                throw ParseErrorException(
                    location,
                    "Only concept / define / let / struct allowed to appear here"
                );
            }
        }
        catch (const ParseErrorException &e) {
            ++error_nr;
            std::cerr << e.what() << std::endl;
            while (true) {
                if (_peak() == T_ID) {
                    auto symbol = symbol_table->lookup(peaking_string);
                    if (symbol && symbol->klass == Symbol::K_RESERVED &&
                        (
                            symbol->token_value == R_CONCEPT ||
                            symbol->token_value == R_DEFINE ||
                            symbol->token_value == R_LET ||
                            symbol->token_value == R_STRUCT
                        )
                    ) {
                        break;
                    }
                }
                else if (_peak() == T_EOF) {
                    break;
                }
                else {
                    _next();
                }
            }
        }
    }

    return error_nr == 0;
}

void
Parser::_registerReserved()
{
#define reserved(name, token_value)     \
    symbol_table->definedSymbol(        \
        name, Location("<reserved>"), name, Symbol::K_RESERVED, token_value, false, nullptr)

    reserved("concept", R_CONCEPT);
    reserved("define", R_DEFINE);
    reserved("let", R_LET);
    reserved("struct", R_STRUCT);

#undef reserved
}

void
Parser::parseLetStmt()
{
    assert(_peak() == T_ID);

    auto *symbol = symbol_table->lookup(peaking_string);
    assert(symbol);
    assert(symbol->token_value == R_LET);

    do {
        if (_next() != T_ID) {
            throw ParseExpectErrorException(location, "identifier", _tokenLiteral());
        }

        if (symbol_table->lookupDefineScope(peaking_string) == symbol_table->currentScope()) {
            throw ParseRedefinedErrorException(
                location,
                peaking_string,
                symbol_table->lookup(peaking_string)->location
            );
        }

        if (_next() != '=') {
            throw ParseExpectErrorException(location, "'='", _tokenLiteral());
        }
        _next();
        parseExpression();

        symbol_table->definedSymbol(
            peaking_string,
            location,
            peaking_string,
            Symbol::K_VARIABLE,
            0,
            false,
            last_type
        );
    } while (_next() == ',');

    if (_peak() != ';') {
        throw ParseExpectErrorException(location, "';'", _tokenLiteral());
    }
    _next();
}

void
Parser::parseExpression()
{ parseAssignmentExpr(); }

void
Parser::parseAssignmentExpr()
{
    parseConditionalExpr();

    while (
        (_peak() >= static_cast<int>(T_OR_ASSIGN) &&
             _peak() <= static_cast<int>(T_ADD_ASSIGN))
        || _peak() == '='
    ) {
        auto op = _peak();
        if (!is_left_value) {
            throw ParseTypeErrorException(
                location,
                "only left value can be assigned"
            );
        }

        Type *left_hand_type = last_type;

        _next();
        parseAssignmentExpr();

        Type *right_hand_type = last_type;

        if (left_hand_type->isNumber() && right_hand_type->isNumber()) {
            last_type = left_hand_type;
            is_left_value = true;
        }
        else if (op != '=') {
            throw ParseTypeErrorException(
                location,
                "modify and assign cannot be applied between " + left_hand_type->to_string() + " and " +
                right_hand_type->to_string()
            );
        }
        else if (left_hand_type->equalTo(right_hand_type)) {
            last_type = left_hand_type;
            is_left_value = true;
        }
        else if (
            left_hand_type->isConcept() &&
            right_hand_type->isStruct() &&
            dynamic_cast<StructType*>(right_hand_type)->implementedConcept(
                dynamic_cast<ConceptType*>(left_hand_type)
            )
        ) {
            last_type = left_hand_type;
            is_left_value = true;
        }
        else {
            throw ParseTypeErrorException(
                location,
                "assignment cannot be applied between " + left_hand_type->to_string() + " and " +
                right_hand_type->to_string()
            );
        }
    }
}

void
Parser::parseConditionalExpr()
{
    parseLogicOrExpr();

    if (_peak() == '?') {
        if (!last_type->isNumber() || !last_type->isPointer()) {
            throw ParseTypeErrorException(
                location,
                last_type->to_string() + " is not judgable"
            );
        }

        _next();
        parseConditionalExpr();

        Type *then_part_type = last_type;

        if (_peak() != ':') {
            throw ParseExpectErrorException(location, "':'", _tokenLiteral());
        }
        _next();
        parseConditionalExpr();

        Type *else_part_type = last_type;

        if (then_part_type->isNumber() && else_part_type->isNumber()) {
            IntegeralType *then_int_type = dynamic_cast<IntegeralType*>(then_part_type);
            IntegeralType *else_int_type = dynamic_cast<IntegeralType*>(else_part_type);
            size_t bitwise_size = std::max(
                then_int_type->getBitwiseWidth(),
                else_int_type->getBitwiseWidth()
            );

            if (!then_int_type->isSigned() || !else_int_type->isSigned()) {
                last_type = type_pool->getUnsignedIntegerType(bitwise_size);
            }
            else {
                last_type = type_pool->getSignedIntegerType(bitwise_size);
            }
        }
        else if (then_part_type->equalTo(else_part_type)) {
            last_type = then_part_type;
        }
        else {
            throw ParseTypeErrorException(
                location,
                "conditional expression has different types: " + then_part_type->to_string() + " and " +
                else_part_type->to_string()
            );
        }
        is_left_value = false;
    }
}

void
Parser::parseLogicOrExpr()
{
    parseLogicAndExpr();

    while (_peak() == T_LOGIC_OR) {
        Type *left_hand_type = last_type;
        auto op = _peak();

        _next();
        parseLogicAndExpr();

        Type *right_hand_type = last_type;

        if (!left_hand_type->isNumber() || !right_hand_type->isNumber()) {
            throw ParseTypeErrorException(
                location,
                std::string("operation `") + std::string(1, op) + "` cannot be applied between " +
                left_hand_type->to_string() + " and " + right_hand_type->to_string()
            );
        }

        IntegeralType *left_hand_int_type = dynamic_cast<IntegeralType*>(left_hand_type);
        IntegeralType *right_hand_int_type = dynamic_cast<IntegeralType*>(right_hand_type);
        size_t bitwise_width = std::max(
            left_hand_int_type->getBitwiseWidth(),
            right_hand_int_type->getBitwiseWidth()
        );
        if (!left_hand_int_type->isSigned() || !right_hand_int_type->isSigned()) {
            last_type = type_pool->getUnsignedIntegerType(bitwise_width);
        }
        else {
            last_type = type_pool->getSignedIntegerType(bitwise_width);
        }
        is_left_value = false;
    }
}

void
Parser::parseLogicAndExpr()
{
    parseBitwiseOrExpr();

    while (_peak() == T_LOGIC_AND)  {
        Type *left_hand_type = last_type;
        auto op = _peak();

        _next();
        parseBitwiseOrExpr();

        Type *right_hand_type = last_type;

        if (!left_hand_type->isNumber() || !right_hand_type->isNumber()) {
            throw ParseTypeErrorException(
                location,
                std::string("operation `") + std::string(1, op) + "` cannot be applied between " +
                left_hand_type->to_string() + " and " + right_hand_type->to_string()
            );
        }

        IntegeralType *left_hand_int_type = dynamic_cast<IntegeralType*>(left_hand_type);
        IntegeralType *right_hand_int_type = dynamic_cast<IntegeralType*>(right_hand_type);
        size_t bitwise_width = std::max(
            left_hand_int_type->getBitwiseWidth(),
            right_hand_int_type->getBitwiseWidth()
        );
        if (!left_hand_int_type->isSigned() || !right_hand_int_type->isSigned()) {
            last_type = type_pool->getUnsignedIntegerType(bitwise_width);
        }
        else {
            last_type = type_pool->getSignedIntegerType(bitwise_width);
        }
        is_left_value = false;
    }
}

void
Parser::parseBitwiseOrExpr()
{
    parseBitwiseXorExpr();

    while (_peak() == '|') {
        Type *left_hand_type = last_type;
        auto op = _peak();

        _next();
        parseBitwiseXorExpr();

        Type *right_hand_type = last_type;

        if (!left_hand_type->isNumber() || !right_hand_type->isNumber()) {
            throw ParseTypeErrorException(
                location,
                std::string("operation `") + std::string(1, op) + "` cannot be applied between " +
                left_hand_type->to_string() + " and " + right_hand_type->to_string()
            );
        }

        IntegeralType *left_hand_int_type = dynamic_cast<IntegeralType*>(left_hand_type);
        IntegeralType *right_hand_int_type = dynamic_cast<IntegeralType*>(right_hand_type);
        size_t bitwise_width = std::max(
            left_hand_int_type->getBitwiseWidth(),
            right_hand_int_type->getBitwiseWidth()
        );
        if (!left_hand_int_type->isSigned() || !right_hand_int_type->isSigned()) {
            last_type = type_pool->getUnsignedIntegerType(bitwise_width);
        }
        else {
            last_type = type_pool->getSignedIntegerType(bitwise_width);
        }
        is_left_value = false;
    }
}

void
Parser::parseBitwiseXorExpr()
{
    parseBitwiseAndExpr();

    while (_peak() == '^') {
        Type *left_hand_type = last_type;
        auto op = _peak();

        _next();
        parseBitwiseAndExpr();

        Type *right_hand_type = last_type;

        if (!left_hand_type->isNumber() || !right_hand_type->isNumber()) {
            throw ParseTypeErrorException(
                location,
                std::string("operation `") + std::string(1, op) + "` cannot be applied between " +
                left_hand_type->to_string() + " and " + right_hand_type->to_string()
            );
        }

        IntegeralType *left_hand_int_type = dynamic_cast<IntegeralType*>(left_hand_type);
        IntegeralType *right_hand_int_type = dynamic_cast<IntegeralType*>(right_hand_type);
        size_t bitwise_width = std::max(
            left_hand_int_type->getBitwiseWidth(),
            right_hand_int_type->getBitwiseWidth()
        );
        if (!left_hand_int_type->isSigned() || !right_hand_int_type->isSigned()) {
            last_type = type_pool->getUnsignedIntegerType(bitwise_width);
        }
        else {
            last_type = type_pool->getSignedIntegerType(bitwise_width);
        }
        is_left_value = false;
    }
}

void
Parser::parseBitwiseAndExpr()
{
    parseEqualityExpr();

    while (_peak() == '&') {
        Type *left_hand_type = last_type;
        auto op = _peak();

        _next();
        parseEqualityExpr();

        Type *right_hand_type = last_type;

        if (!left_hand_type->isNumber() || !right_hand_type->isNumber()) {
            throw ParseTypeErrorException(
                location,
                std::string("operation `") + std::string(1, op) + "` cannot be applied between " +
                left_hand_type->to_string() + " and " + right_hand_type->to_string()
            );
        }

        IntegeralType *left_hand_int_type = dynamic_cast<IntegeralType*>(left_hand_type);
        IntegeralType *right_hand_int_type = dynamic_cast<IntegeralType*>(right_hand_type);
        size_t bitwise_width = std::max(
            left_hand_int_type->getBitwiseWidth(),
            right_hand_int_type->getBitwiseWidth()
        );
        if (!left_hand_int_type->isSigned() || !right_hand_int_type->isSigned()) {
            last_type = type_pool->getUnsignedIntegerType(bitwise_width);
        }
        else {
            last_type = type_pool->getSignedIntegerType(bitwise_width);
        }
        is_left_value = false;
    }
}

void
Parser::parseEqualityExpr()
{
    parseCompareExpr();

    while (_peak() == T_EQ || _peak() == T_NE) {
        Type *left_hand_type = last_type;

        _next();
        parseCompareExpr();

        Type *right_hand_type = last_type;

        if (
            (!left_hand_type->isNumber() || !right_hand_type->isNumber()) &&
            !left_hand_type->equalTo(right_hand_type)
        ) {
            throw ParseTypeErrorException(
                location,
                left_hand_type->to_string() + " and " + right_hand_type->to_string() +
                " is not comparable"
            );
        }
        last_type = type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS);
        is_left_value = false;
    }
}

void
Parser::parseCompareExpr()
{
    parseShiftExpr();

    while (_peak() == '<' || _peak() == T_LE ||
           _peak() == '>' || _peak() == T_GE) {
        Type *left_hand_type = last_type;

        _next();
        parseShiftExpr();

        Type *right_hand_type = last_type;

        if (!left_hand_type->isNumber() || !right_hand_type->isNumber()) {
            throw ParseTypeErrorException(
                location,
                left_hand_type->to_string() + " and " + right_hand_type->to_string() +
                " is not comparable"
            );
        }
        last_type = type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS);
        is_left_value = false;
    }
}

void
Parser::parseShiftExpr()
{
    parseAdditiveExpr();

    while (_peak() == T_SHL || _peak() == T_SHR) {
        Type *left_hand_type = last_type;
        auto op = _peak();

        _next();
        parseAdditiveExpr();

        Type *right_hand_type = last_type;

        if (!left_hand_type->isNumber() || !right_hand_type->isNumber()) {
            throw ParseTypeErrorException(
                location,
                std::string("operation `") + std::string(1, op) + "` cannot be applied between " +
                left_hand_type->to_string() + " and " + right_hand_type->to_string()
            );
        }

        last_type = left_hand_type;
        is_left_value = false;
    }
}

void
Parser::parseAdditiveExpr()
{
    parseMultiplitiveExpr();

    while (_peak() == '+' || _peak() == '-') {
        Type *left_hand_type = last_type;
        auto op = _peak();

        _next();
        parseMultiplitiveExpr();

        Type *right_hand_type = last_type;

        if (!left_hand_type->isNumber() || !right_hand_type->isNumber()) {
            throw ParseTypeErrorException(
                location,
                std::string("operation `") + std::string(1, op) + "` cannot be applied between " +
                left_hand_type->to_string() + " and " + right_hand_type->to_string()
            );
        }

        IntegeralType *left_hand_int_type = dynamic_cast<IntegeralType*>(left_hand_type);
        IntegeralType *right_hand_int_type = dynamic_cast<IntegeralType*>(right_hand_type);
        size_t bitwise_width = std::max(
            left_hand_int_type->getBitwiseWidth(),
            right_hand_int_type->getBitwiseWidth()
        );
        if (left_hand_int_type->isSigned() || right_hand_int_type->isSigned()) {
            last_type = type_pool->getSignedIntegerType(bitwise_width);
        }
        else {
            last_type = type_pool->getUnsignedIntegerType(bitwise_width);
        }
        is_left_value = false;
    }
}

void
Parser::parseMultiplitiveExpr()
{
    parsePrefixExpr();

    while (_peak() == '*' || _peak() == '/' || _peak() == '%') {
        Type *left_hand_type = last_type;
        auto op = _peak();

        _next();
        parsePrefixExpr();

        Type *right_hand_type = last_type;

        if (!left_hand_type->isNumber() || !right_hand_type->isNumber()) {
            throw ParseTypeErrorException(
                location,
                std::string("operation `") + std::string(1, op) + "` cannot be applied between " +
                left_hand_type->to_string() + " and " + right_hand_type->to_string()
            );
        }

        IntegeralType *left_hand_int_type = dynamic_cast<IntegeralType*>(left_hand_type);
        IntegeralType *right_hand_int_type = dynamic_cast<IntegeralType*>(right_hand_type);
        size_t bitwise_width = std::max(
            left_hand_int_type->getBitwiseWidth(),
            right_hand_int_type->getBitwiseWidth()
        );
        if (left_hand_int_type->isSigned() || right_hand_int_type->isSigned()) {
            last_type = type_pool->getSignedIntegerType(bitwise_width);
        }
        else {
            last_type = type_pool->getUnsignedIntegerType(bitwise_width);
        }
        is_left_value = false;
    }
}

void
Parser::parsePrefixExpr()
{
    if (_peak() == T_INC) {
        _next();
        parsePostfixExpr();
        if (!last_type->isIncreasable()) {
            throw ParseTypeErrorException(
                location,
                "Self increment cannot be applied on " + last_type->to_string()
            );
        }
        if (!is_left_value) {
            throw ParseTypeErrorException(
                location,
                "Self increment can only be applied on left value"
            );
        }
    }
    else if (_peak() == T_DEC) {
        _next();
        parsePostfixExpr();
        if (!last_type->isIncreasable()) {
            throw ParseTypeErrorException(
                location,
                "Self decrement cannot be applied on " + last_type->to_string()
            );
        }
        if (!is_left_value) {
            throw ParseTypeErrorException(
                location,
                "Self decrement can only be applied on left value"
            );
        }
    }
    else {
        parsePostfixExpr();
    }
}

void
Parser::parsePostfixExpr()
{
    parseUnaryExpr();

    while (true) {
        if (_peak() == T_INC) {
            if (!last_type->isIncreasable()) {
                throw ParseTypeErrorException(
                    location,
                    "Self increment cannot be applied on " + last_type->to_string()
                );
            }
            if (!is_left_value) {
                throw ParseTypeErrorException(
                    location,
                    "Self increment can only be applied on left value"
                );
            }
            is_left_value = false;
        }
        else if (_peak() == T_DEC) {
            if (!last_type->isIncreasable()) {
                throw ParseTypeErrorException(
                    location,
                    "Self decrement cannot be applied on " + last_type->to_string()
                );
            }
            if (!is_left_value) {
                throw ParseTypeErrorException(
                    location,
                    "Self decrement can only be applied on left value"
                );
            }
            is_left_value = false;
        }
        else {
            break;
        }
    }
}

void
Parser::parseUnaryExpr()
{
    switch (_peak()) {
        case (Token)'(':
        {
            _next();
            parseExpression();
            if (_peak() != ')') {
                throw ParseExpectErrorException(location, "')'", _tokenLiteral());
            }
            _next();
            break;
        }
        case T_ID:
        {
            auto symbol = symbol_table->lookup(peaking_string);
            if (!symbol) {
                throw ParseUndefinedErrorException(location, peaking_string);
            }

            last_type = symbol->type;
            is_left_value = true;

            _next();
            break;
        }
        default:
            assert(false);
    }
}
