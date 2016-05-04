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
        else if (_current() == '&') {
            if (!_endOfInput(1)) {
                _forward();
                if (_current() == '&')  {
                    peaking_token = T_LOGIC_AND;
                    _forward();
                }
                else if (_current() == '=') {
                    peaking_token = T_AND_ASSIGN;
                    _forward();
                }
            }
            else {
                _forward();
                peaking_token = '&';
            }
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

    while (_peak() != T_EOF) {
        try {
            if (_peak() != T_ID) {
                throw ParseErrorException(
                    location,
                    "Only concept / define / let / struct allowed to appear here"
                );
            }

            auto symbol = symbol_table->lookup(peaking_string);
            if (!symbol || symbol->klass != Symbol::K_RESERVED) {
                throw ParseErrorException(
                    location,
                    "Only concept / define / let / struct allowed to appear here"
                );
            }

            if (symbol->token_value == R_CONCEPT) {
            }
            else if (symbol->token_value == R_FUNCTION) {
                parseFunctionDefine();
            }
            else if (symbol->token_value == R_LET) {
                parseGlobalLetStmt();
            }
            else if (symbol->token_value == R_STRUCT) {
            }
            else {
                throw ParseErrorException(
                    location,
                    "Only concept / function / let / struct allowed to appear here"
                );
            }
        }
        catch (const ParseErrorException &e) {
            error_collector->error(e);
            while (_peak() != T_EOF) {
                if (_peak() == T_ID) {
                    auto symbol = symbol_table->lookup(peaking_string);
                    if (symbol && symbol->klass == Symbol::K_RESERVED &&
                        (
                            symbol->token_value == R_CONCEPT ||
                            symbol->token_value == R_FUNCTION ||
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
                _next();
            }
        }
    }

    return !error_counter->getErrorCount();
}

void
Parser::_registerReserved()
{
#define reserved(name, token_value)     \
    symbol_table->defineSymbol(         \
        name, Location("<reserved>"), name, Symbol::K_RESERVED, token_value, false, nullptr)

    reserved("concept", R_CONCEPT);
    reserved("else", R_ELSE);
    reserved("function", R_FUNCTION);
    reserved("if", R_IF);
    reserved("let", R_LET);
    reserved("return", R_RETURN);
    reserved("struct", R_STRUCT);

#undef reserved

#define primitive(name, type)           \
    symbol_table->defineSymbol(         \
        name, Location("<primitive>"), name, Symbol::K_PRIMITIVE, 0, false, type)

    primitive("i8", type_pool->getSignedIntegerType(8));
    primitive("i16", type_pool->getSignedIntegerType(16));
    primitive("i32", type_pool->getSignedIntegerType(32));
    primitive("u8", type_pool->getSignedIntegerType(8));
    primitive("u16", type_pool->getSignedIntegerType(16));
    primitive("u32", type_pool->getSignedIntegerType(32));

#undef primitive
}

void
Parser::checkVariableDefined(std::string name)
{
    auto symbol = symbol_table->lookup(name);
    if (!symbol) { return; }
    if (
        symbol->klass == Symbol::K_PRIMITIVE ||
        symbol->klass == Symbol::K_CONCEPT ||
        symbol->klass == Symbol::K_STRUCT
    ) {
        throw ParseRedefinedErrorException(location, name, symbol->location);
    }
    else if (
        symbol->klass == Symbol::K_VARIABLE ||
        symbol->klass == Symbol::K_ARGUMENT ||
        symbol->klass == Symbol::K_FUNCTION
    ) {
        if (symbol_table->lookupDefineScope(name) == symbol_table->currentScope()) {
            throw ParseRedefinedErrorException(location, name, symbol->location);
        }
    }
    else {
        throw ParseInvalidVariableNameException(location, name);
    }
}

Symbol *
Parser::checkFunctionDefined(std::string name, FunctionType *type)
{
    auto symbol = symbol_table->lookup(name);
    if (!symbol) { return nullptr; }
    if (
        symbol->klass == Symbol::K_PRIMITIVE ||
        symbol->klass == Symbol::K_CONCEPT ||
        symbol->klass == Symbol::K_STRUCT
        ) {
        throw ParseRedefinedErrorException(location, name, symbol->location);
    }
    else if (
        symbol->klass == Symbol::K_VARIABLE ||
        symbol->klass == Symbol::K_ARGUMENT
    ) {
        if (symbol_table->lookupDefineScope(name) == symbol_table->currentScope()) {
            throw ParseRedefinedErrorException(location, name, symbol->location);
        }
    }
    else if (
        symbol->klass == Symbol::K_FUNCTION
    ) {
        if (!symbol->type->equalTo(type) || symbol->token_value) {
            throw ParseRedefinedErrorException(location, name, symbol->location);
        }
        return symbol;
    }
    else {
        throw ParseInvalidVariableNameException(location, name);
    }

    return nullptr;
}

Type *
Parser::checkTypeName(std::string name)
{
    auto symbol = symbol_table->lookup(name);
    if (!symbol) {
        throw ParseUndefinedErrorException(
            location,
            "type `" + name + "`"
        );
    }

    if (
        symbol->klass != Symbol::K_PRIMITIVE &&
        symbol->klass != Symbol::K_STRUCT &&
        symbol->klass != Symbol::K_CONCEPT
    ) {
        throw ParseExpectErrorException(
            location,
            "type name",
            name
        );
    }

    return symbol->type;
}

void
Parser::parseGlobalLetStmt()
{
    current_function = ir_builder->findFunction("_init_");
    current_block = current_function->newBasicBlock();

    assert(_peak() == T_ID);

    auto *symbol = symbol_table->lookup(peaking_string);
    assert(symbol);
    assert(symbol->token_value == R_LET);

    do {
        if (_next() != T_ID) {
            throw ParseExpectErrorException(location, "identifier", _tokenLiteral());
        }

        auto variable_name = peaking_string;
        checkVariableDefined(variable_name);

        if (_next() != '=') {
            throw ParseExpectErrorException(location, "'='", _tokenLiteral());
        }
        _next();
        parseExpression();

        if (is_left_value) {
            result_inst = current_block->LoadInst(last_type, result_inst);
        }
        auto variable_address = current_block->GlobalInst(type_pool->getPointerType(last_type), variable_name);
        current_block->StoreInst(last_type, variable_address, result_inst);

        symbol_table->defineSymbol(
            variable_name,
            location,
            variable_name,
            Symbol::K_GLOBAL,
            0,
            false,
            last_type
        );
    } while (_peak() == ',');

    if (_peak() != ';') {
        throw ParseExpectErrorException(location, "';'", _tokenLiteral());
    }
    _next();
}

void
Parser::parseFunctionDefine()
{
    assert(_peak() == T_ID);
    auto *symbol = symbol_table->lookup(peaking_string);
    assert(symbol);
    assert(symbol->token_value = R_FUNCTION);

    if (_next() != T_ID) {
        throw ParseExpectErrorException(location, "function name", _tokenLiteral());
    }

    auto function_name = peaking_string;
    symbol_table->pushScope();

    _next();
    try { parsePrototype(); }
    catch (const ParseErrorException &e) {
        symbol_table->popScope();
        throw e;
    }
    auto forward_symbol = checkFunctionDefined(function_name, dynamic_cast<FunctionType*>(last_type));

    if (_peak() == ';') {
        _next();
        symbol_table->defineSymbolInRoot(
            function_name,
            location,
            function_name,
            Symbol::K_FUNCTION,
            0,
            false,
            last_type
        );
        return;
    }
    else if (_peak() != '{') {
        throw ParseExpectErrorException(location, "function body", _tokenLiteral());
    }

    current_function = ir_builder->newFunction(function_name, last_type->to<FunctionType>());
    current_block = current_function->newBasicBlock("entry");
    if (forward_symbol) {
        forward_symbol->token_value = reinterpret_cast<intptr_t>(current_function->get());
    }
    else {
        symbol_table->defineSymbolInRoot(
            function_name,
            location,
            function_name,
            Symbol::K_FUNCTION,
            reinterpret_cast<intptr_t>(current_function->get()),
            false,
            last_type
        );
    }

    try { parseFunctionBody(); }
    catch (const ParseErrorException &e) {
        symbol_table->popScope();
        throw e;
    }

    symbol_table->popScope();
}

void
Parser::parsePrototype()
{
    auto builder = type_pool->getFunctionTypeBuilder();

    if (_peak() != '(') {
        throw ParseExpectErrorException(location, "function arguments", _tokenLiteral());
    }
    int argument_counter = 0;
    _next();
    while (_peak() != ')') {
        if (_peak() != T_ID) {
            throw ParseExpectErrorException(location, "function arguments name", _tokenLiteral());
        }

        auto argument_name = peaking_string;
        checkVariableDefined(argument_name);

        if (_next() != ':') {
            throw ParseExpectErrorException(location, "function arguments type", _tokenLiteral());
        }

        bool use_left_value = false;
        if (_next() == '&') {
            use_left_value = true;
            _next();
        }

        if (_peak() != T_ID) {
            throw ParseExpectErrorException(location, "function arguments type", _tokenLiteral());
        }

        Type *argument_type = checkTypeName(peaking_string);
        if (use_left_value) { argument_type = type_pool->getPointerType(argument_type); }

        symbol_table->defineSymbol(
            argument_name,
            location,
            argument_name,
            Symbol::K_ARGUMENT,
            argument_counter++,
            false,
            argument_type
        );

        builder.addArgument(argument_type);
        if (_next() != ',') {
            if (_peak() == ')') {
                break;
            }
            else {
                throw ParseExpectErrorException(location, "','", _tokenLiteral());
            }
        }
        _next();
    }

    FunctionType *function_type;
    if (_next() != ':') {
        function_type = builder.commit(type_pool->getVoidType());
    }
    else {
        if (_next() != T_ID) {
            throw ParseExpectErrorException(location, "function return type", _tokenLiteral());
        }

        function_type = builder.commit(checkTypeName(peaking_string));
        _next();
    }

    last_type = function_type;
}

void
Parser::parseFunctionBody()
{
    assert(_peak() == '{');
    _next();

    while (_peak() != T_EOF && _peak() != '}') {
        try {
            parseStatement();
        }
        catch (const ParseErrorException &e) {
            error_collector->error(e);
            while (_peak() != T_EOF && _peak() != '}' && _peak() != ';') {
                _next();
            }
            if (_peak() == ';') {
                _next();
            }
        }
    }

    if (_peak() == T_EOF) {
        throw ParseErrorException(location, "unexpected EOF");
    }
    else {
        _next();
    }
}

void
Parser::parseStatement()
{
    if (_peak() == T_ID) {
        auto symbol = symbol_table->lookup(peaking_string);
        if (symbol->klass == Symbol::K_RESERVED) {
            switch (symbol->token_value) {
                case R_LET:
                    parseLetStmt();
                    return;
                case R_RETURN:
                    parseReturnStmt();
                    return;
                case R_IF:
                    parseIfStmt();
                    return;
                default:
                    throw ParseExpectErrorException(location, "statement", _tokenLiteral());
            }
        }
    }
    else if (_peak() == '{') {
        parseBlockStmt();
        return;
    }
    parseExpressionStmt();
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

        auto variable_name = peaking_string;
        checkVariableDefined(variable_name);

        if (_next() != '=') {
            throw ParseExpectErrorException(location, "'='", _tokenLiteral());
        }
        _next();
        parseExpression();

        if (is_left_value) {
            result_inst = current_block->LoadInst(last_type, result_inst);
        }
        auto variable_address = current_block->AllocaInst(
            type_pool->getPointerType(last_type),
            current_block->UnsignedImmInst(type_pool->getUnsignedIntegerType(CYAN_PRODUCT_BITS), 1),
            variable_name
        );
        current_block->StoreInst(last_type, variable_address, result_inst);

        symbol_table->defineSymbol(
            variable_name,
            location,
            variable_name,
            Symbol::K_VARIABLE,
            reinterpret_cast<intptr_t>(variable_address),
            false,
            last_type
        );
    } while (_peak() == ',');

    if (_peak() != ';') {
        throw ParseExpectErrorException(location, "';'", _tokenLiteral());
    }
    _next();
}

void
Parser::parseReturnStmt()
{
    assert(_peak() == T_ID);

    auto *symbol = symbol_table->lookup(peaking_string);
    assert(symbol);
    assert(symbol->token_value == R_RETURN);
    _next();

    Type *ret_type = current_function->get()->getPrototype()->getReturnType();
    if (ret_type->equalTo(type_pool->getVoidType())) {
        if (_peak() != ';') {
            throw ParseTypeErrorException(
                location,
                "current function `" + current_function->get()->getName() + "` has return value of Void type"
            );
        }
        _next();
        current_block->RetInst(type_pool->getVoidType(), nullptr);
    }
    else {
        if (_peak() == ';') {
            throw ParseTypeErrorException(
                location,
                "current function `" + current_function->get()->getName() + "` should return a value of type " +
                ret_type->to_string()
            );
        }

        parseExpression();
        if (!ret_type->is<NumericType>() || !last_type->is<NumericType>()) {
            if (!ret_type->equalTo(last_type)) {
                if (
                    !ret_type->is<ConceptType>() ||
                    !last_type->is<StructType>() ||
                    dynamic_cast<StructType*>(last_type)->implementedConcept(
                        dynamic_cast<ConceptType*>(ret_type)
                    )
                    ) {
                    error_collector->error(ParseTypeErrorException(
                        location,
                        "cannot return value of type " + last_type->to_string() +
                        ", type " + ret_type->to_string() + " required"
                    ));
                }
            }
        }

        if (is_left_value) {
            result_inst = current_block->LoadInst(last_type, result_inst);
        }
        current_block->RetInst(type_pool->getVoidType(), result_inst);

        if (_peak() != ';') {
            throw ParseExpectErrorException(location, "';'", _tokenLiteral());
        }
        _next();
    }
}

void
Parser::parseBlockStmt()
{
    assert(_peak() == '{');
    _next();

    symbol_table->pushScope();
    while (_peak() != T_EOF && _peak() != '}') {
        try {
            parseStatement();
        }
        catch (const ParseErrorException &e) {
            error_collector->error(e);
            while (_peak() != T_EOF && _peak() != ';' && _peak() != '}')  {
                _next();
            }
            if (_peak() == ';') {
                _next();
            }
        }
    }
    symbol_table->popScope();

    if (_peak() == T_EOF) {
        throw ParseErrorException(
            location,
            "unexpected EOF"
        );
    }
    _next();
}

void
Parser::parseExpressionStmt()
{
    parseExpression();
    if (_peak() != ';') {
        throw ParseExpectErrorException(
            location,
            "';'",
            _tokenLiteral()
        );
    }
    _next();
}

void
Parser::parseIfStmt()
{
    assert(_peak() == T_ID);

    auto *symbol = symbol_table->lookup(peaking_string);
    assert(symbol);
    assert(symbol->token_value == R_IF);
    _next();

    if (_peak() != '(') {
        throw ParseExpectErrorException(
            location,
            "if condition",
            _tokenLiteral()
        );
    }
    _next();

    parseExpression();

    if (!last_type->is<NumericType>()) {
        throw ParseTypeErrorException(
            location,
            "type " + last_type->to_string() + " cannot be used as condition"
        );
    }
    if (is_left_value) {
        result_inst = current_block->LoadInst(last_type, result_inst);
    }

    auto head_block = std::move(current_block);
    auto condition_value = result_inst;

    if (_peak() != ')') {
        throw ParseExpectErrorException(location, "')'", _tokenLiteral());
    }
    _next();

    auto then_block = current_function->newBasicBlock();
    auto else_block = current_function->newBasicBlock();

    head_block->BrInst(condition_value, then_block->get(), else_block->get());

    // then part
    current_block = std::move(then_block);
    parseStatement();
    then_block = std::move(current_block);

    if (_peak() == T_ID) {
        symbol = symbol_table->lookup(peaking_string);
        if (symbol && symbol->token_value == R_ELSE) {
            _next();

            current_block = std::move(else_block);
            parseStatement();
            else_block = std::move(current_block);
        }
    }

    current_block = current_function->newBasicBlock();
    then_block->JumpInst(current_block->get());
    else_block->JumpInst(current_block->get());
}

void
Parser::parseExpression()
{ parseAssignmentExpr(); }

void
Parser::parseAssignmentExpr()
{
    parseConditionalExpr();

    if (
        (_peak() >= static_cast<int>(T_OR_ASSIGN) &&
             _peak() <= static_cast<int>(T_ADD_ASSIGN))
        || _peak() == '='
    ) {
        auto op = _peak();
        if (!is_left_value) {
            error_collector->error(ParseTypeErrorException(
                location,
                "only left value can be assigned"
            ));
        }

        Type *left_hand_type = last_type;
        auto left_hand_result = result_inst;

        _next();
        parseAssignmentExpr();

        Type *right_hand_type = last_type;
        auto right_hand_result = result_inst;
        if (is_left_value) {
            right_hand_result = current_block->LoadInst(right_hand_type, right_hand_result);
        }

        if (!left_hand_type->is<NumericType>() || !right_hand_type->is<NumericType>()) {
            if (op != '=') {
                error_collector->error(ParseTypeErrorException(
                    location,
                    "modify and assign cannot be applied between " + left_hand_type->to_string() + " and " +
                    right_hand_type->to_string()
                ));
            }
            else if (!left_hand_type->equalTo(right_hand_type)) {
                if (
                    !left_hand_type->is<ConceptType>() ||
                    !right_hand_type->is<StructType>() ||
                    dynamic_cast<StructType*>(right_hand_type)->implementedConcept(
                        dynamic_cast<ConceptType*>(left_hand_type)
                    )
                ) {
                    error_collector->error(ParseTypeErrorException(
                        location,
                        "assignment cannot be applied between " + left_hand_type->to_string() + " and " +
                        right_hand_type->to_string()
                    ));
                }
            }
        }

        last_type = left_hand_type;
        is_left_value = true;
        if (op != '=') {
            auto left_original = current_block->LoadInst(left_hand_type, left_hand_result);
            switch (op) {
                case T_ADD_ASSIGN:
                    right_hand_result = current_block->AddInst(left_hand_type, left_original, right_hand_result);
                    break;
                case T_SUB_ASSIGN:
                    right_hand_result = current_block->SubInst(left_hand_type, left_original, right_hand_result);
                    break;
                case T_MUL_ASSIGN:
                    right_hand_result = current_block->MulInst(left_hand_type, left_original, right_hand_result);
                    break;
                case T_DIV_ASSIGN:
                    right_hand_result = current_block->DivInst(left_hand_type, left_original, right_hand_result);
                    break;
                case T_MOD_ASSIGN:
                    right_hand_result = current_block->ModInst(left_hand_type, left_original, right_hand_result);
                    break;
                case T_SHL_ASSIGN:
                    right_hand_result = current_block->ShlInst(left_hand_type, left_original, right_hand_result);
                    break;
                case T_SHR_ASSIGN:
                    right_hand_result = current_block->ShrInst(left_hand_type, left_original, right_hand_result);
                    break;
                case T_AND_ASSIGN:
                    right_hand_result = current_block->AndInst(left_hand_type, left_original, right_hand_result);
                    break;
                case T_XOR_ASSIGN:
                    right_hand_result = current_block->XorInst(left_hand_type, left_original, right_hand_result);
                    break;
                case T_OR_ASSIGN:
                    right_hand_result = current_block->OrInst(left_hand_type, left_original, right_hand_result);
                    break;
                default:
                    assert(false);
            }
        }
        current_block->StoreInst(left_hand_type, left_hand_result, right_hand_result);
        result_inst = left_hand_result;
    }
}

void
Parser::parseConditionalExpr()
{
    parseLogicOrExpr();

    if (_peak() == '?') {
        if (!last_type->is<NumericType>() || !last_type->is<PointerType>()) {
            error_collector->error(ParseTypeErrorException(
                location,
                last_type->to_string() + " cannot be used as condition"
            ));
        }

        auto then_block = current_function->newBasicBlock();
        auto else_block = current_function->newBasicBlock();
        auto follow_block = current_function->newBasicBlock();

        current_block->BrInst(result_inst, then_block->get(), else_block->get());
        current_block.swap(then_block);

        _next();
        parseConditionalExpr();

        Type *then_part_type = last_type;

        auto result_phi = follow_block->newPhiBuilder(then_part_type);
        result_phi.addBranch(result_inst, current_block->get());
        current_block->JumpInst(follow_block->get());

        current_block.swap(else_block);

        if (_peak() != ':') {
            throw ParseExpectErrorException(location, "':'", _tokenLiteral());
        }
        _next();
        parseConditionalExpr();

        result_phi.addBranch(result_inst, current_block->get());
        current_block->JumpInst(follow_block->get());

        current_block.swap(follow_block);

        Type *else_part_type = last_type;

        if (then_part_type->is<NumericType>() && else_part_type->is<NumericType>()) {
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
            error_collector->error(ParseTypeErrorException(
                location,
                "conditional expression has different types: " + then_part_type->to_string() + " and " +
                else_part_type->to_string()
            ));
            last_type = then_part_type;
        }
        is_left_value = false;

        result_inst = result_phi.commit();
    }
}

void
Parser::parseLogicOrExpr()
{
    parseLogicAndExpr();

    if (_peak() == T_LOGIC_OR) {
        auto follow_block = current_function->newBasicBlock();
        auto result_phi = follow_block->newPhiBuilder(last_type);

        while (_peak() == T_LOGIC_OR) {
            Type *left_hand_type = last_type;
            auto op = _peak();
            auto new_block = current_function->newBasicBlock();

            current_block->BrInst(result_inst, follow_block->get(), new_block->get());
            result_phi.addBranch(result_inst, current_block->get());

            current_block.swap(new_block);

            _next();
            parseLogicAndExpr();

            Type *right_hand_type = last_type;

            if (!left_hand_type->is<NumericType>() || !right_hand_type->is<NumericType>()) {
                error_collector->error(ParseTypeErrorException(
                    location,
                    std::string("operation `") + std::string(1, op) + "` cannot be applied between " +
                    left_hand_type->to_string() + " and " + right_hand_type->to_string()
                ));
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

        current_block->JumpInst(follow_block->get());
        result_phi.addBranch(result_inst, current_block->get());

        current_block.swap(follow_block);
        result_inst = result_phi.commit();
    }
}

void
Parser::parseLogicAndExpr()
{
    parseBitwiseOrExpr();

    if (_peak() == T_LOGIC_AND) {
        auto follow_block = current_function->newBasicBlock();
        auto result_phi = follow_block->newPhiBuilder(last_type);

        while (_peak() == T_LOGIC_AND)  {
            Type *left_hand_type = last_type;
            auto op = _peak();
            auto new_block = current_function->newBasicBlock();

            current_block->BrInst(result_inst, new_block->get(), follow_block->get());
            result_phi.addBranch(result_inst, current_block->get());

            current_block.swap(new_block);

            _next();
            parseBitwiseOrExpr();

            Type *right_hand_type = last_type;

            if (!left_hand_type->is<NumericType>() || !right_hand_type->is<NumericType>()) {
                error_collector->error(ParseTypeErrorException(
                    location,
                    std::string("operation `") + std::string(1, op) + "` cannot be applied between " +
                    left_hand_type->to_string() + " and " + right_hand_type->to_string()
                ));
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

        current_block->JumpInst(follow_block->get());
        result_phi.addBranch(result_inst, current_block->get());

        current_block.swap(follow_block);
        result_inst = result_phi.commit();
    }
}

void
Parser::parseBitwiseOrExpr()
{
    parseBitwiseXorExpr();

    while (_peak() == '|') {
        Type *left_hand_type = last_type;
        auto op = _peak();
        auto left_hand_result = result_inst;
        if (is_left_value) {
            left_hand_result = current_block->LoadInst(left_hand_type, left_hand_result);
        }

        _next();
        parseBitwiseXorExpr();

        Type *right_hand_type = last_type;
        auto right_hand_result = result_inst;
        if (is_left_value) {
            right_hand_result = current_block->LoadInst(right_hand_type, right_hand_result);
        }

        if (!left_hand_type->is<NumericType>() || !right_hand_type->is<NumericType>()) {
            error_collector->error(ParseTypeErrorException(
                location,
                std::string("operation `") + std::string(1, op) + "` cannot be applied between " +
                left_hand_type->to_string() + " and " + right_hand_type->to_string()
            ));
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
        result_inst = current_block->OrInst(last_type, left_hand_result, right_hand_result);
    }
}

void
Parser::parseBitwiseXorExpr()
{
    parseBitwiseAndExpr();

    while (_peak() == '^') {
        Type *left_hand_type = last_type;
        auto op = _peak();
        auto left_hand_result = result_inst;
        if (is_left_value) {
            left_hand_result = current_block->LoadInst(left_hand_type, left_hand_result);
        }

        _next();
        parseBitwiseAndExpr();

        Type *right_hand_type = last_type;
        auto right_hand_result = result_inst;
        if (is_left_value) {
            right_hand_result = current_block->LoadInst(right_hand_type, right_hand_result);
        }

        if (!left_hand_type->is<NumericType>() || !right_hand_type->is<NumericType>()) {
            error_collector->error(ParseTypeErrorException(
                location,
                std::string("operation `") + std::string(1, op) + "` cannot be applied between " +
                left_hand_type->to_string() + " and " + right_hand_type->to_string()
            ));
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
        result_inst = current_block->XorInst(last_type, left_hand_result, right_hand_result);
    }
}

void
Parser::parseBitwiseAndExpr()
{
    parseEqualityExpr();

    while (_peak() == '&') {
        Type *left_hand_type = last_type;
        auto op = _peak();
        auto left_hand_result = result_inst;
        if (is_left_value) {
            left_hand_result = current_block->LoadInst(left_hand_type, left_hand_result);
        }

        _next();
        parseEqualityExpr();

        Type *right_hand_type = last_type;
        auto right_hand_result = result_inst;
        if (is_left_value) {
            right_hand_result = current_block->LoadInst(right_hand_type, right_hand_result);
        }

        if (!left_hand_type->is<NumericType>() || !right_hand_type->is<NumericType>()) {
            error_collector->error(ParseTypeErrorException(
                location,
                std::string("operation `") + std::string(1, op) + "` cannot be applied between " +
                left_hand_type->to_string() + " and " + right_hand_type->to_string()
            ));
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
        result_inst = current_block->AndInst(last_type, left_hand_result, right_hand_result);
    }
}

void
Parser::parseEqualityExpr()
{
    parseCompareExpr();

    while (_peak() == T_EQ || _peak() == T_NE) {
        Type *left_hand_type = last_type;
        auto op = _peak();
        auto left_hand_result = result_inst;
        if (is_left_value) {
            left_hand_result = current_block->LoadInst(left_hand_type, left_hand_result);
        }

        _next();
        parseCompareExpr();

        Type *right_hand_type = last_type;
        auto right_hand_result = result_inst;
        if (is_left_value) {
            right_hand_result = current_block->LoadInst(right_hand_type, right_hand_result);
        }

        if (
            (!left_hand_type->is<NumericType>() || !right_hand_type->is<NumericType>()) &&
            !left_hand_type->equalTo(right_hand_type)
        ) {
            error_collector->error(ParseTypeErrorException(
                location,
                left_hand_type->to_string() + " and " + right_hand_type->to_string() +
                " is not comparable"
            ));
        }
        last_type = type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS);
        is_left_value = false;
        result_inst = current_block->SeqInst(last_type, left_hand_result, right_hand_result);
        if (op == T_NE) {
            result_inst = current_block->SeqInst(
                last_type,
                result_inst,
                current_block->SignedImmInst(
                    type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS),
                    0
                )
            );
        }
    }
}

void
Parser::parseCompareExpr()
{
    parseShiftExpr();

    while (_peak() == '<' || _peak() == T_LE ||
           _peak() == '>' || _peak() == T_GE) {
        Type *left_hand_type = last_type;
        auto op = _peak();
        auto left_hand_result = result_inst;
        if (is_left_value) {
            left_hand_result = current_block->LoadInst(left_hand_type, left_hand_result);
        }

        _next();
        parseShiftExpr();

        Type *right_hand_type = last_type;
        auto right_hand_result = result_inst;
        if (is_left_value) {
            right_hand_result = current_block->LoadInst(right_hand_type, right_hand_result);
        }

        if (!left_hand_type->is<NumericType>() || !right_hand_type->is<NumericType>()) {
            error_collector->error(ParseTypeErrorException(
                location,
                left_hand_type->to_string() + " and " + right_hand_type->to_string() +
                " is not comparable"
            ));
        }
        last_type = type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS);
        is_left_value = false;
        switch (op) {
            case '<':
                result_inst = current_block->SltInst(last_type, left_hand_result, right_hand_result);
                break;
            case '>':
                result_inst = current_block->SltInst(last_type, right_hand_result, left_hand_result);
                break;
            case T_LE:
                result_inst = current_block->SleInst(last_type, left_hand_result, right_hand_result);
                break;
            case T_GE:
                result_inst = current_block->SleInst(last_type, right_hand_result, left_hand_result);
                break;
            default:
                assert(false);
        }
    }
}

void
Parser::parseShiftExpr()
{
    parseAdditiveExpr();

    while (_peak() == T_SHL || _peak() == T_SHR) {
        Type *left_hand_type = last_type;
        auto op = _peak();
        auto left_hand_result = result_inst;
        if (is_left_value) {
            left_hand_result = current_block->LoadInst(left_hand_type, left_hand_result);
        }

        _next();
        parseAdditiveExpr();

        Type *right_hand_type = last_type;
        auto right_hand_result = result_inst;
        if (is_left_value) {
            right_hand_result = current_block->LoadInst(right_hand_type, right_hand_result);
        }

        if (!left_hand_type->is<NumericType>() || !right_hand_type->is<NumericType>()) {
            error_collector->error(ParseTypeErrorException(
                location,
                std::string("operation `") + std::string(1, op) + "` cannot be applied between " +
                left_hand_type->to_string() + " and " + right_hand_type->to_string()
            ));
        }

        last_type = left_hand_type;
        is_left_value = false;

        if (op == T_SHL) {
            result_inst = current_block->ShlInst(last_type, left_hand_result, right_hand_result);
        }
        else {
            result_inst = current_block->ShrInst(last_type, left_hand_result, right_hand_result);
        }
    }
}

void
Parser::parseAdditiveExpr()
{
    parseMultiplitiveExpr();

    while (_peak() == '+' || _peak() == '-') {
        Type *left_hand_type = last_type;
        auto op = _peak();
        auto left_hand_result = result_inst;
        if (is_left_value) {
            left_hand_result = current_block->LoadInst(left_hand_type, left_hand_result);
        }

        _next();
        parseMultiplitiveExpr();

        Type *right_hand_type = last_type;
        auto right_hand_reuslt = result_inst;
        if (is_left_value) {
            right_hand_reuslt = current_block->LoadInst(right_hand_type, right_hand_reuslt);
        }

        if (!left_hand_type->is<NumericType>() || !right_hand_type->is<NumericType>()) {
            error_collector->error(ParseTypeErrorException(
                location,
                std::string("operation `") + std::string(1, op) + "` cannot be applied between " +
                left_hand_type->to_string() + " and " + right_hand_type->to_string()
            ));
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
        switch (op) {
            case '+':
                result_inst = current_block->AddInst(last_type, left_hand_result, right_hand_reuslt);
                break;
            case '-':
                result_inst = current_block->SubInst(last_type, left_hand_result, right_hand_reuslt);
                break;
            default:
                assert(false);
        }
    }
}

void
Parser::parseMultiplitiveExpr()
{
    parsePrefixExpr();

    while (_peak() == '*' || _peak() == '/' || _peak() == '%') {
        Type *left_hand_type = last_type;
        auto op = _peak();
        auto left_hand_result = result_inst;
        if (is_left_value) {
            left_hand_result = current_block->LoadInst(left_hand_type, left_hand_result);
        }

        _next();
        parsePrefixExpr();

        Type *right_hand_type = last_type;
        auto right_hand_result = result_inst;
        if (is_left_value) {
            right_hand_result = current_block->LoadInst(right_hand_type, right_hand_result);
        }

        if (!left_hand_type->is<NumericType>() || !right_hand_type->is<NumericType>()) {
            error_collector->error(ParseTypeErrorException(
                location,
                std::string("operation `") + std::string(1, op) + "` cannot be applied between " +
                left_hand_type->to_string() + " and " + right_hand_type->to_string()
            ));
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
        switch (op) {
            case '*':
                result_inst = current_block->MulInst(last_type, left_hand_result, right_hand_result);
                break;
            case '/':
                result_inst = current_block->DivInst(last_type, left_hand_result, right_hand_result);
                break;
            case '%':
                result_inst = current_block->ModInst(last_type, left_hand_result, right_hand_result);
                break;
            default:
                assert(false);
        }
    }
}

void
Parser::parsePrefixExpr()
{
    if (_peak() == T_INC) {
        _next();
        parsePostfixExpr();
        if (!last_type->is<NumericType>()) {
            error_collector->error(ParseTypeErrorException(
                location,
                "Self increment cannot be applied on " + last_type->to_string()
            ));
        }
        if (!is_left_value) {
            error_collector->error(ParseTypeErrorException(
                location,
                "Self increment can only be applied on left value"
            ));
        }

        auto original_address = result_inst;
        auto original_value = current_block->LoadInst(last_type, original_address);
        auto increased_value = current_block->AddInst(
            last_type,
            original_value,
            current_block->SignedImmInst(
                type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS),
                1
            )
        );
        current_block->StoreInst(last_type, original_address, increased_value);
    }
    else if (_peak() == T_DEC) {
        _next();
        parsePostfixExpr();
        if (!last_type->is<NumericType>()) {
            error_collector->error(ParseTypeErrorException(
                location,
                "Self decrement cannot be applied on " + last_type->to_string()
            ));
        }
        if (!is_left_value) {
            error_collector->error(ParseTypeErrorException(
                location,
                "Self decrement can only be applied on left value"
            ));
        }

        auto original_address = result_inst;
        auto original_value = current_block->LoadInst(last_type, original_address);
        auto descreased_value = current_block->SubInst(
            last_type,
            original_value,
            current_block->SignedImmInst(
                type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS),
                1
            )
        );
        current_block->StoreInst(last_type, original_address, descreased_value);
    }
    else if (_peak() == '-') {
        _next();
        parsePostfixExpr();
        if (!last_type->is<NumericType>()) {
            error_collector->error(ParseTypeErrorException(
                location,
                "`-` cannot be applied on " + last_type->to_string()
            ));
        }

        if (is_left_value) {
            result_inst = current_block->LoadInst(last_type, result_inst);
        }
        result_inst = current_block->SubInst(
            last_type,
            current_block->SignedImmInst(type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS), 0),
            result_inst
        );
        is_left_value = false;
    }
    else if (_peak() == '~') {
        _next();
        parsePostfixExpr();
        if (!last_type->is<NumericType>()) {
            error_collector->error(ParseTypeErrorException(
                location,
                "`-` cannot be applied on " + last_type->to_string()
            ));
        }

        if (is_left_value) {
            result_inst = current_block->LoadInst(last_type, result_inst);
        }
        result_inst = current_block->NorInst(
            last_type,
            current_block->UnsignedImmInst(type_pool->getUnsignedIntegerType(CYAN_PRODUCT_BITS), 0),
            result_inst
        );
        is_left_value = false;
    }
    else if (_peak() == '!') {
        _next();
        parsePostfixExpr();
        if (!last_type->is<NumericType>()) {
            error_collector->error(ParseTypeErrorException(
                location,
                "`-` cannot be applied on " + last_type->to_string()
            ));
        }

        if (is_left_value) {
            result_inst = current_block->LoadInst(last_type, result_inst);
        }
        result_inst = current_block->SeqInst(
            last_type,
            current_block->SignedImmInst(type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS), 0),
            result_inst
        );
        is_left_value = false;
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
            if (!last_type->is<NumericType>()) {
                error_collector->error(ParseTypeErrorException(
                    location,
                    "Self increment cannot be applied on " + last_type->to_string()
                ));
            }
            if (!is_left_value) {
                error_collector->error(ParseTypeErrorException(
                    location,
                    "Self increment can only be applied on left value"
                ));
            }
            is_left_value = false;

            auto original_address = result_inst;
            auto original_value = current_block->LoadInst(last_type, original_address);
            auto increased_value = current_block->AddInst(
                last_type,
                original_value,
                current_block->SignedImmInst(
                    type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS),
                    1
                )
            );
            current_block->StoreInst(last_type, original_address, increased_value);
            result_inst = original_value;
        }
        else if (_peak() == T_DEC) {
            if (!last_type->is<NumericType>()) {
                error_collector->error(ParseTypeErrorException(
                    location,
                    "Self decrement cannot be applied on " + last_type->to_string()
                ));
            }
            if (!is_left_value) {
                error_collector->error(ParseTypeErrorException(
                    location,
                    "Self decrement can only be applied on left value"
                ));
            }
            is_left_value = false;

            auto original_address = result_inst;
            auto original_value = current_block->LoadInst(last_type, original_address);
            auto decreased_value = current_block->SubInst(
                last_type,
                original_value,
                current_block->SignedImmInst(
                    type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS),
                    1
                )
            );
            current_block->StoreInst(last_type, original_address, decreased_value);
            result_inst = original_value;
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
            auto variable_name = peaking_string;
            auto symbol = symbol_table->lookup(variable_name);
            if (!symbol) {
                throw ParseUndefinedErrorException(location, variable_name);
            }

            last_type = symbol->type;
            is_left_value = true;

            if (symbol->klass == Symbol::K_GLOBAL) {
                result_inst = current_block->GlobalInst(
                    type_pool->getPointerType(last_type),
                    variable_name
                );
            }
            else if (symbol->klass == Symbol::K_ARGUMENT) {
                result_inst = current_block->ArgInst(
                    type_pool->getPointerType(last_type),
                    symbol->token_value
                );
            }
            else if (symbol->klass == Symbol::K_VARIABLE) {
                result_inst = reinterpret_cast<Instrument *>(symbol->token_value);
            }
            else {
                throw ParseExpectErrorException(location, "variable", _tokenLiteral());
            }

            _next();
            break;
        }
        case T_INTEGER:
        {
            auto result_type = type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS);
            last_type = result_type;
            is_left_value = false;
            result_inst = current_block->SignedImmInst(result_type, peaking_int);

            _next();
            break;
        }
        default:
            throw ParseExpectErrorException(location, "unary", _tokenLiteral());
    }
}
