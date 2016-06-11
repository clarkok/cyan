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

    while (!_endOfInput()) {
        while (!_endOfInput() && isspace(_current())) {
            _forward();
        }
        token_start = current;

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
                else {
                    peaking_token = '&';
                }
            }
            else {
                _forward();
                peaking_token = '&';
            }
            break;
        }
        else if (_current() == '|') {
            if (!_endOfInput(1)) {
                _forward();
                if (_current() == '|') {
                    peaking_token = T_LOGIC_OR;
                    _forward();
                }
                else if (_current() == '=') {
                    peaking_token = T_OR_ASSIGN;
                    _forward();
                }
                else {
                    peaking_token = '|';
                }
            }
            else {
                _forward();
                peaking_token = '|';
            }
            break;
        }
        else if (_current() == '=') {
            if (!_endOfInput(1)) {
                _forward();
                if (_current() == '=') {
                    peaking_token = T_EQ;
                    _forward();
                }
                else {
                    peaking_token = '=';
                }
            }
            else {
                _forward();
                peaking_token = '=';
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
Parser::_parse()
{
    _registerReserved();

    while (_peak() != T_EOF) {
        try {
            if (_peak() != T_ID) {
                throw ParseErrorException(
                    location,
                    "`" + _tokenLiteral() + "` cannot appera here"
                );
            }

            auto symbol = symbol_table->lookup(peaking_string);
            if (!symbol || symbol->klass != Symbol::K_RESERVED) {
                throw ParseErrorException(
                    location,
                    "`" + _tokenLiteral() + "` cannot appera here"
                );
            }

            if (symbol->token_value == R_CONCEPT) {
                parseConceptDefine();
            }
            else if (symbol->token_value == R_FUNCTION) {
                parseFunctionDefine();
            }
            else if (symbol->token_value == R_LET) {
                parseGlobalLetStmt();
            }
            else if (symbol->token_value == R_STRUCT) {
                parseStructDefine();
            }
            else if (symbol->token_value == R_IMPL) {
                parseImplDefine();
            }
            else {
                throw ParseErrorException(
                    location,
                    "`" + _tokenLiteral() + "` cannot appera here"
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

    reserved("break", R_BREAK);
    reserved("concept", R_CONCEPT);
    reserved("continue", R_CONTINUE);
    reserved("delete", R_DELETE);
    reserved("else", R_ELSE);
    reserved("function", R_FUNCTION);
    reserved("if", R_IF);
    reserved("impl", R_IMPL);
    reserved("let", R_LET);
    reserved("new", R_NEW);
    reserved("return", R_RETURN);
    reserved("struct", R_STRUCT);
    reserved("while", R_WHILE);

#undef reserved

#define primitive(name, type)           \
    symbol_table->defineSymbol(         \
        name, Location("<primitive>"), name, Symbol::K_PRIMITIVE, 0, false, type)

    primitive("i8", type_pool->getSignedIntegerType(8));
    primitive("i16", type_pool->getSignedIntegerType(16));
    primitive("i32", type_pool->getSignedIntegerType(32));
    primitive("i64", type_pool->getSignedIntegerType(64));
    primitive("u8", type_pool->getSignedIntegerType(8));
    primitive("u16", type_pool->getSignedIntegerType(16));
    primitive("u32", type_pool->getSignedIntegerType(32));
    primitive("u64", type_pool->getSignedIntegerType(64));

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
        symbol->klass == Symbol::K_GLOBAL ||
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
        symbol->klass == Symbol::K_GLOBAL ||
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

Symbol *
Parser::checkConceptDefined(std::string name)
{
    auto symbol = symbol_table->lookup(name);
    if (!symbol) { return nullptr; }
    else if (symbol->klass == Symbol::K_CONCEPT) {
        if (symbol->type) {
            throw ParseRedefinedErrorException(
                location,
                name,
                symbol->location
            );
        }
        else {
            return symbol;
        }
    }

    throw ParseRedefinedErrorException(
        location,
        name,
        symbol->location
    );
}

Symbol *
Parser::checkStructDefined(std::string name)
{
    auto symbol = symbol_table->lookup(name);
    if (!symbol) { return nullptr; }
    else if (symbol->klass == Symbol::K_STRUCT) {
        if (symbol->type) {
            throw ParseRedefinedErrorException(
                location,
                name,
                symbol->location
            );
        }
        else {
            return symbol;
        }
    }

    throw ParseRedefinedErrorException(
        location,
        name,
        symbol->location
    );
}

Type *
Parser::checkTypeName(std::string name)
{
    auto symbol = symbol_table->lookup(name);
    if (!symbol) {
        throw ParseUndefinedErrorException(
            location,
            name
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

Type *
Parser::parseType()
{
    if (_peak() != T_ID) {
        throw ParseExpectErrorException(
            location,
            "type name",
            _tokenLiteral()
        );
    }

    _next();
    auto type_name = peaking_string;
    auto ret = checkTypeName(type_name);

    while (_peak() == '[') {
        if (_next() != ']') {
            throw ParseExpectErrorException(
                location,
                "`]'",
                _tokenLiteral()
            );
        }
        _next();
        ret = type_pool->getArrayType(ret);
    }

    return ret;
}

Type *
Parser::parseLeftHandType(bool &use_left_hand)
{
    use_left_hand = false;
    if (_peak() == '&') {
        use_left_hand = true;
        _next();
    }
    return parseType();
}

Type *
Parser::resolveForwardType(Type *type)
{
    while (type->is<ForwardType>()) {
        auto name = type->to<ForwardType>()->getName();
        try { type = checkTypeName(name); }
        catch (const ParseErrorException &e) {
            throw ParseTypeErrorException(
                location,
                "forward type " + name + " is not defined"
            );
        }
    }
    return type;
}

void
Parser::parseGlobalLetStmt()
{
    current_function = ir_builder->findFunction("_init_");
    current_block = current_function->newBasicBlock(loop_stack.size());

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
    auto forward_symbol = checkFunctionDefined(function_name, last_type->to<FunctionType>());

    if (!forward_symbol) {
        forward_symbol = symbol_table->defineSymbolInRoot(
            function_name,
            location,
            function_name,
            Symbol::K_FUNCTION,
            0,
            false,
            last_type
        );
    }

    if (_peak() == ';') {
        _next();
        return;
    }
    else if (_peak() != '{') {
        throw ParseExpectErrorException(location, "function body", _tokenLiteral());
    }

    current_function = ir_builder->newFunction(function_name, last_type->to<FunctionType>());
    current_block = current_function->newBasicBlock(loop_stack.size(), "entry");
    forward_symbol->token_value = reinterpret_cast<intptr_t>(current_function->get());

    try { parseFunctionBody(); }
    catch (const ParseErrorException &e) {
        symbol_table->popScope();
        throw e;
    }

    symbol_table->popScope();
}

void
Parser::parseConceptDefine()
{
    assert(_peak() == T_ID);
    auto *symbol = symbol_table->lookup(peaking_string);
    assert(symbol);
    assert(symbol->token_value = R_CONCEPT);

    if (_next() != T_ID) {
        throw ParseExpectErrorException(location, "concept name", _tokenLiteral());
    }
    auto concept_name = peaking_string;
    auto *forward_decl = checkConceptDefined(concept_name);

    if (_next() == ';') {
        _next();
        if (!forward_decl) {
            symbol_table->defineSymbol(
                concept_name,
                location,
                concept_name,
                Symbol::K_CONCEPT,
                0,
                false,
                type_pool->getForwardType(concept_name)
            );
        }
        return;
    }

    ConceptType *base_concept = nullptr;
    if (_peak() == ':') {
        if (_next() != T_ID) {
            throw ParseExpectErrorException(
                location,
                "base concept",
                _tokenLiteral()
            );
        }

        auto base_concept_name = peaking_string;
        symbol = symbol_table->lookup(base_concept_name);
        if (!symbol || symbol->klass != Symbol::K_CONCEPT) {
            throw ParseTypeErrorException(
                location,
                "`" + base_concept_name + "` is not a concept"
            );
        }
        base_concept = symbol->type->to<ConceptType>();

        _next();
    }

    if (_peak() != '{') {
        throw ParseExpectErrorException(
            location,
            "concept definition",
            _tokenLiteral()
        );
    }
    _next();

    auto builder = type_pool->getConceptTypeBuilder(concept_name, base_concept);

    while (_peak() != T_EOF && _peak() != '}') {
        symbol = symbol_table->lookup(peaking_string);
        if (
            _peak() != T_ID ||
            !symbol ||
            symbol->klass != Symbol::K_RESERVED ||
            symbol->token_value != R_FUNCTION
            ) {
            throw ParseExpectErrorException(
                location,
                "concept methods",
                _tokenLiteral()
            );
        }

        if (_next() != T_ID) {
            throw ParseExpectErrorException(
                location,
                "concept method name",
                _tokenLiteral()
            );
        }
        auto method_name = peaking_string;
        checkVariableDefined(method_name);
        _next();

        symbol_table->pushScope();

        try {
            parsePrototype();
            MethodType *prototype = type_pool->getMethodType(
                builder.get(),
                last_type->to<FunctionType>()
            );

            if (_peak() == '{') {
                symbol_table->defineSymbol(
                    "this",
                    location,
                    "this",
                    Symbol::K_ARGUMENT,
                    prototype->arguments_size(),
                    false,
                    builder.get()
                );

                current_function = ir_builder->newFunction(
                    concept_name + "::" + method_name,
                    last_type->to<FunctionType>()
                );
                current_block = current_function->newBasicBlock(loop_stack.size(), "entry");
                try {
                    builder.addMethod(method_name, prototype, current_function->get());
                }
                catch (const std::exception &e) {
                    throw ParseErrorException(
                        location,
                        e.what()
                    );
                }
                parseFunctionBody();
            }
            else {
                if (_peak() != ';') {
                    throw ParseExpectErrorException(
                        location,
                        "';'",
                        _tokenLiteral()
                    );
                }
                try {
                    builder.addMethod(method_name, prototype, nullptr);
                }
                catch (const std::exception &e) {
                    throw ParseErrorException(
                        location,
                        e.what()
                    );
                }
                _next();
            }
        }
        catch (const ParseErrorException &e) {
            symbol_table->popScope();
            error_collector->error(e);
            while (_peak() != T_EOF && _peak() != '}') {
                if (_peak() == T_ID) {
                    symbol = symbol_table->lookup(peaking_string);
                    if (
                        symbol &&
                        symbol->klass == Symbol::K_RESERVED &&
                        symbol->token_value == R_FUNCTION
                    ) {
                        break;
                    }
                }
                _next();
            }
            continue;
        }
        symbol_table->popScope();
    }
    if (_peak() == T_EOF) {
        throw ParseErrorException(
            location,
            "unexpected EOF"
        );
    }
    _next();

    builder.addMethod(
        "__delete",
        MethodType::fromFunction(
            builder.get(),
            FunctionType::Builder().release(
                type_pool->getPointerType(type_pool->getVoidType())
            )
        ),
        nullptr
    );

    if (forward_decl) {
        forward_decl->type = builder.commit();
    }
    else {
        symbol_table->defineSymbol(
            concept_name,
            location,
            concept_name,
            Symbol::K_CONCEPT,
            0,
            false,
            builder.commit()
        );
    }
}

void
Parser::parseStructDefine()
{
    assert(_peak() == T_ID);
    auto *symbol = symbol_table->lookup(peaking_string);
    assert(symbol);
    assert(symbol->token_value == R_STRUCT);

    if (_next() != T_ID) {
        throw ParseExpectErrorException(location, "struct name", _tokenLiteral());
    }
    auto struct_name = peaking_string;
    auto *forward_decl = checkStructDefined(struct_name);

    if (!forward_decl) {
        forward_decl = symbol_table->defineSymbol(
            struct_name,
            location,
            struct_name,
            Symbol::K_STRUCT,
            0,
            false,
            type_pool->getForwardType(struct_name)
        );
    }

    if (_next() == ';') {
        _next();
        return;
    }

    if (_peak() != '{') {
        throw ParseExpectErrorException(
            location,
            "struct definition",
            _tokenLiteral()
        );
    }
    _next();

    auto builder = type_pool->getStructTypeBuilder(struct_name);

    while (_peak() != T_EOF && _peak() != '}') {
        if (_peak() != T_ID) {
            throw ParseExpectErrorException(
                location,
                "struct member name",
                _tokenLiteral()
            );
        }
        auto member_name = peaking_string;

        if (_next() != ':') {
            throw ParseExpectErrorException(
                location,
                "struct member type",
                _tokenLiteral()
            );
        }
        _next();

        Type *type = parseType();
        builder.addMember(member_name, type);

        if (_peak() == '}') {
            break;
        }
        else if (_peak() != ',') {
            throw ParseExpectErrorException(
                location,
                "','",
                _tokenLiteral()
            );
        }
        _next();
    }

    forward_decl->type = builder.commit();

    if (_peak() == T_EOF) {
        throw ParseErrorException(
            location,
            "unexpected EOF"
        );
    }
    _next();
}

void
Parser::parseImplDefine()
{
    assert(_peak() == T_ID);
    auto *symbol = symbol_table->lookup(peaking_string);
    assert(symbol);
    assert(symbol->token_value == R_IMPL);

    if (_next() != T_ID) {
        throw ParseExpectErrorException(location, "struct name", _tokenLiteral());
    }
    Type *type = checkTypeName(peaking_string);
    if (!type->is<StructType>()) {
        throw ParseTypeErrorException(
            location,
            "`" + peaking_string + "` is not a struct name"
        );
    }
    auto struct_type = type->to<StructType>();

    if (_next() != ':') {
        throw ParseExpectErrorException(location, "concept", _tokenLiteral());
    }

    if (_next() != T_ID) {
        throw ParseExpectErrorException(location, "concept name", _tokenLiteral());
    }
    Type *concept_type_base = checkTypeName(peaking_string);
    if (!concept_type_base->is<ConceptType>()) {
        throw ParseTypeErrorException(
            location,
            "`" + peaking_string + "` is not a concept"
        );
    }
    auto concept_type = concept_type_base->to<ConceptType>();
    struct_type->implementConcept(concept_type);

    auto casted_struct_type = type_pool->getCastedStructType(struct_type, concept_type);
    if (_next() != '{') {
        throw ParseExpectErrorException(
            location,
            "impl body",
            _tokenLiteral()
        );
    }
    _next();

    while (_peak() != T_EOF && _peak() != '}') {
        symbol = symbol_table->lookup(peaking_string);
        if (
            _peak() != T_ID ||
            !symbol ||
            symbol->klass != Symbol::K_RESERVED ||
            symbol->token_value != R_FUNCTION
            ) {
            throw ParseExpectErrorException(
                location,
                "impl methods",
                _tokenLiteral()
            );
        }

        if (_next() != T_ID) {
            throw ParseExpectErrorException(
                location,
                "impl method name",
                _tokenLiteral()
            );
        }
        auto method_name = peaking_string;
        _next();

        symbol_table->pushScope();

        try {
            parsePrototype();
            MethodType *prototype = type_pool->getMethodType(
                casted_struct_type,
                last_type->to<FunctionType>()
            );

            if (_peak() != '{') {
                throw ParseExpectErrorException(
                    location,
                    "method body",
                    _tokenLiteral()
                );
            }

            symbol_table->defineSymbol(
                "this",
                location,
                "this",
                Symbol::K_ARGUMENT,
                prototype->arguments_size(),
                false,
                casted_struct_type
            );

            current_function = ir_builder->newFunction(
                concept_type->getName() + "$" + struct_type->getName() + "::" + method_name,
                last_type->to<FunctionType>()
            );
            current_block = current_function->newBasicBlock(loop_stack.size(), "entry");
            try {
                casted_struct_type->implement(method_name, prototype, current_function->get());
            }
            catch (const std::exception &e) {
                throw ParseErrorException(
                    location,
                    e.what()
                );
            }
            parseFunctionBody();
        }
        catch (const ParseErrorException &e) {
            symbol_table->popScope();
            error_collector->error(e);
            while (_peak() != T_EOF && _peak() != '}') {
                if (_peak() == T_ID) {
                    symbol = symbol_table->lookup(peaking_string);
                    if (
                        symbol &&
                        symbol->klass == Symbol::K_RESERVED &&
                        symbol->token_value == R_FUNCTION
                    ) {
                        break;
                    }
                }
                _next();
            }
            continue;
        }
        symbol_table->popScope();
    }
    if (_peak() == T_EOF) {
        throw ParseErrorException(
            location,
            "unexpected EOF"
        );
    }
    _next();

    for (auto &m : *casted_struct_type) {
        if (!m.impl) {
            if (m.name == "__delete") {
                auto function_builder = ir_builder->newFunction(
                    concept_type->getName() + "$" + struct_type->getName() + "::__delete",
                    m.prototype
                );
                auto entry_block = function_builder->newBasicBlock(0, "entry");
                entry_block->RetInst(
                    type_pool->getPointerType(type_pool->getVoidType()),
                    entry_block->SubInst(
                        type_pool->getPointerType(type_pool->getVoidType()),
                        entry_block->LoadInst(
                            type_pool->getPointerType(type_pool->getVoidType()),
                            entry_block->ArgInst(
                                type_pool->getPointerType(
                                    type_pool->getCastedStructType(struct_type, concept_type)
                                ),
                                0
                            )
                        ),
                        entry_block->SignedImmInst(
                            type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS),
                            struct_type->getConceptOffset(concept_type->getName())
                        )
                    )
                );
                m.impl = function_builder->get();
            }
            else {
                throw ParseErrorException(
                    location,
                    "method `" + m.name + "` not implemented"
                );
            }
        }
    }
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

        _next();
        bool use_left_value;
        Type *argument_type = parseLeftHandType(use_left_value);
        if (use_left_value) {
            if (argument_type->is<ConceptType>()) {
                throw ParseTypeErrorException(
                    location,
                    "concept argument cannot be left value"
                );
            }
            argument_type = type_pool->getPointerType(argument_type);
        }

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
        if (_peak() != ',') {
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
        _next();
        function_type = builder.commit(parseType());
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
    current_block->RetInst(type_pool->getVoidType(), nullptr);
}

void
Parser::parseStatement()
{
    if (_peak() == T_ID) {
        auto symbol = symbol_table->lookup(peaking_string);
        if (!symbol) {
            throw ParseExpectErrorException(location, "statement", _tokenLiteral());
        }
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
                case R_WHILE:
                    parseWhileStmt();
                    return;
                case R_BREAK:
                    parseBreakStmt();
                    return;
                case R_CONTINUE:
                    parseContinueStmt();
                    return;
                case R_DELETE:
                    parseDeleteStmt();
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

    auto then_block = current_function->newBasicBlock(loop_stack.size());
    auto else_block = current_function->newBasicBlock(loop_stack.size());

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

    current_block = current_function->newBasicBlock(loop_stack.size());
    then_block->JumpInst(current_block->get());
    else_block->JumpInst(current_block->get());
}

void
Parser::parseWhileStmt()
{
    assert(_peak() == T_ID);

    auto *symbol = symbol_table->lookup(peaking_string);
    assert(symbol);
    assert(symbol->token_value == R_WHILE);
    _next();

    if (_peak() != '(') {
        throw ParseExpectErrorException(
            location,
            "while condition",
            _tokenLiteral()
        );
    }
    _next();

    auto continue_block_b = current_function->newBasicBlock(loop_stack.size());
    auto follow_block_b = current_function->newBasicBlock(loop_stack.size());

    auto continue_block = continue_block_b->get();
    auto follow_block = follow_block_b->get();

    current_block->JumpInst(continue_block);
    current_block = std::move(continue_block_b);

    parseExpression();
    if (!last_type->is<NumericType>()) {
        throw ParseTypeErrorException(
            location,
            "type " + last_type->to_string() + " cannot be used as condition"
        );
    }
    auto condition_value = result_inst;
    if (is_left_value) {
        condition_value = current_block->LoadInst(last_type, condition_value);
    }

    auto loop_body = current_function->newBasicBlock(loop_stack.size());
    current_block->BrInst(condition_value, loop_body->get(), follow_block);
    current_block = std::move(loop_body);

    loop_stack.emplace(continue_block, follow_block);

    if (_peak() != ')') {
        throw ParseExpectErrorException(
            location,
            "')'",
            _tokenLiteral()
        );
    }
    _next();

    try {
        parseStatement();
    }
    catch (const ParseErrorException &e) {
        loop_stack.pop();
        throw e;
    }

    current_block->JumpInst(continue_block);
    current_block = std::move(follow_block_b);
    loop_stack.pop();
}

void
Parser::parseBreakStmt()
{
    assert(_peak() == T_ID);

    auto *symbol = symbol_table->lookup(peaking_string);
    assert(symbol);
    assert(symbol->token_value == R_BREAK);
    _next();

    if (loop_stack.empty()) {
        throw ParseErrorException(
            location,
            "break appear out of loop"
        );
    }

    current_block->JumpInst(loop_stack.top().follow_block);
    current_block = current_function->newBasicBlock(loop_stack.size());

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
Parser::parseContinueStmt()
{
    assert(_peak() == T_ID);

    auto *symbol = symbol_table->lookup(peaking_string);
    assert(symbol);
    assert(symbol->token_value == R_CONTINUE);
    _next();

    if (loop_stack.empty()) {
        throw ParseErrorException(
            location,
            "continue appear out of loop"
        );
    }

    current_block->JumpInst(loop_stack.top().continue_block);
    current_block = current_function->newBasicBlock(loop_stack.size());

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
Parser::parseDeleteStmt()
{
    assert(_peak() == T_ID);

    auto *symbol = symbol_table->lookup(peaking_string);
    assert(symbol);
    assert(symbol->token_value == R_DELETE);
    _next();

    parseExpression();
    if (is_left_value) {
        result_inst = current_block->LoadInst(
            last_type,
            result_inst
        );
    }

    if (last_type->is<ConceptType>()) {
        auto concept_type = last_type->to<ConceptType>();
        int offset = concept_type->getMethodOffset("__delete");
        assert(offset != std::numeric_limits<int>::max());
        auto method = concept_type->getMethodByOffset(offset);

        result_inst = current_block->newCallBuilder(
            method.prototype->getReturnType(),
            current_block->LoadInst(
                method.prototype,
                current_block->AddInst(
                    type_pool->getPointerType(method.prototype),
                    result_inst,
                    current_block->SignedImmInst(
                        type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS),
                        offset
                    )
                )
            )
        )
        .addArgument(result_inst)
        .commit();
    }
    else if (
        !last_type->is<StructType>() &&
        !last_type->is<ArrayType>()
    ) {
        throw ParseTypeErrorException(
            location,
            "type " + last_type->to_string() + " cannot be deleted"
        );
    }

    current_block->DeleteInst(last_type, result_inst);

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
        if (!last_type->is<NumericType>() && !last_type->is<PointerType>()) {
            error_collector->error(ParseTypeErrorException(
                location,
                last_type->to_string() + " cannot be used as condition"
            ));
        }

        auto then_block = current_function->newBasicBlock(loop_stack.size());
        auto else_block = current_function->newBasicBlock(loop_stack.size());
        auto follow_block = current_function->newBasicBlock(loop_stack.size());

        current_block->BrInst(result_inst, then_block->get(), else_block->get());
        current_block.swap(then_block);

        _next();
        parseConditionalExpr();

        Type *then_part_type = last_type;
        if (is_left_value) {
            result_inst = current_block->LoadInst(last_type, result_inst);
        }

        auto result_phi = follow_block->newPhiBuilder(then_part_type);
        result_phi.addBranch(result_inst, current_block->get());
        current_block->JumpInst(follow_block->get());

        current_block.swap(else_block);

        if (_peak() != ':') {
            throw ParseExpectErrorException(location, "':'", _tokenLiteral());
        }
        _next();
        parseConditionalExpr();
        if (is_left_value) {
            result_inst = current_block->LoadInst(last_type, result_inst);
        }

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
        auto follow_block = current_function->newBasicBlock(loop_stack.size());
        auto result_phi = follow_block->newPhiBuilder(last_type);

        while (_peak() == T_LOGIC_OR) {
            Type *left_hand_type = last_type;
            auto op = _peak();
            auto new_block = current_function->newBasicBlock(loop_stack.size());

            current_block->BrInst(result_inst, follow_block->get(), new_block->get());
            result_phi.addBranch(result_inst, current_block->get());

            current_block.swap(new_block);

            _next();
            parseLogicAndExpr();

            Type *right_hand_type = last_type;

            if (!left_hand_type->is<NumericType>() || !right_hand_type->is<NumericType>()) {
                error_collector->error(ParseTypeErrorException(
                    location,
                    std::string("operation `") + std::string(1, static_cast<char>(op)) + "` cannot be applied between " +
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
        auto follow_block = current_function->newBasicBlock(loop_stack.size());
        auto result_phi = follow_block->newPhiBuilder(last_type);

        while (_peak() == T_LOGIC_AND)  {
            Type *left_hand_type = last_type;
            auto op = _peak();
            auto new_block = current_function->newBasicBlock(loop_stack.size());

            current_block->BrInst(result_inst, new_block->get(), follow_block->get());
            result_phi.addBranch(result_inst, current_block->get());

            current_block.swap(new_block);

            _next();
            parseBitwiseOrExpr();

            Type *right_hand_type = last_type;

            if (!left_hand_type->is<NumericType>() || !right_hand_type->is<NumericType>()) {
                error_collector->error(ParseTypeErrorException(
                    location,
                    std::string("operation `") + std::string(1, static_cast<char>(op)) + "` cannot be applied between " +
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
                std::string("operation `") + std::string(1, static_cast<char>(op)) + "` cannot be applied between " +
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
                std::string("operation `") + std::string(1, static_cast<char>(op)) + "` cannot be applied between " +
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
                std::string("operation `") + std::string(1, static_cast<char>(op)) + "` cannot be applied between " +
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
                std::string("operation `") + std::string(1, static_cast<char>(op)) + "` cannot be applied between " +
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
                std::string("operation `") + std::string(1, static_cast<char>(op)) + "` cannot be applied between " +
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
                std::string("operation `") + std::string(1, static_cast<char>(op)) + "` cannot be applied between " +
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

    Instruction *this_value = nullptr;

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
        else if (_peak() == '(') {
            _next();
            if (!last_type->is<FunctionType>()) {
                throw ParseTypeErrorException(
                    location,
                    "type " + last_type->to_string() + " is not callable"
                );
            }

            auto function_type = last_type->to<FunctionType>();

            if (is_left_value) {
                result_inst = current_block->LoadInst(last_type, result_inst);
            }

            auto builder = current_block->newCallBuilder(
                function_type->getReturnType(),
                result_inst
            );
            auto iter = function_type->begin();

            while (_peak() != T_EOF && _peak() != ')') {
                parseExpression();
                if (iter == function_type->end()) {
                    throw ParseTypeErrorException(
                        location,
                        "function requires only " + 
                        std::to_string(function_type->arguments_size()) + " arguments"
                    );
                }
                Type *required_type = *iter++;
                bool use_left_value = required_type->is<PointerType>() && !required_type->is<ArrayType>();

                if (use_left_value) {
                    required_type = required_type->to<PointerType>()->getBaseType();
                }

                if (
                    (required_type->is<NumericType>() && last_type->is<NumericType>()) ||
                    required_type->equalTo(last_type)
                ) {
                    if (use_left_value && is_left_value) {
                        builder.addArgument(result_inst);
                    }
                    else if (use_left_value && !is_left_value) {
                        throw ParseTypeErrorException(
                            location,
                            "function requires left value"
                        );
                    }
                    else {
                        if (is_left_value) {
                            result_inst = current_block->LoadInst(last_type, result_inst);
                        }
                        builder.addArgument(result_inst);
                    }
                }
                else if (
                    required_type->is<ConceptType>() &&
                    last_type->is<StructType>() &&
                    last_type->to<StructType>()->implementedConcept(
                        required_type->to<ConceptType>()
                    )
                ) {
                    auto struct_type = last_type->to<StructType>();
                    auto concept_type = required_type->to<ConceptType>();
                    auto offset = struct_type->getConceptOffset(concept_type->getName());
                    if (is_left_value) {
                        result_inst = current_block->LoadInst(last_type, result_inst);
                    }
                    result_inst = current_block->AddInst(
                        type_pool->getCastedStructType(struct_type, concept_type),
                        result_inst,
                        current_block->SignedImmInst(
                            type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS),
                            offset
                        )
                    );
                    builder.addArgument(result_inst);
                }
                else if (
                    required_type->is<ConceptType>() &&
                    last_type->is<ConceptType>() &&
                    last_type->to<ConceptType>()->isInheritedFrom(
                        required_type->to<ConceptType>()
                    )
                ) {
                    if (is_left_value) {
                        result_inst = current_block->LoadInst(last_type, result_inst);
                    }
                    builder.addArgument(result_inst);
                }
                else {
                    throw ParseTypeErrorException(
                        location,
                        "function requires type " + required_type->to_string() + 
                        ", but type " + last_type->to_string() + " met"
                    );
                }
                if (_peak() == ')') {
                    break;
                }
                else if (_peak() != ',') {
                    throw ParseExpectErrorException(
                        location,
                        "','",
                        _tokenLiteral()
                    );
                }
                _next();
            }
            if (iter != function_type->end()) {
                throw ParseTypeErrorException(
                    location,
                    "function requires " + std::to_string(function_type->arguments_size()) +
                    " arguments"
                );
            }
            if (this_value) {
                builder.addArgument(this_value);
                this_value = nullptr;
            }
            if (_peak() == T_EOF) {
                throw ParseErrorException(
                    location,
                    "unexpected EOF"
                );
            }
            _next();

            is_left_value = false;
            last_type = function_type->getReturnType();
            result_inst = builder.commit();
        }
        else if (_peak() == '.') {
            last_type = resolveForwardType(last_type);

            if (
                !last_type->is<StructType>() &&
                !last_type->is<ConceptType>() &&
                !last_type->is<CastedStructType>()
            ) {
                throw ParseTypeErrorException(
                    location,
                    "type " + last_type->to_string() + " cannot have members"
                );
            }
            if (_next() != T_ID) {
                throw ParseExpectErrorException(
                    location,
                    "member name",
                    _tokenLiteral()
                );
            }
            auto member_name = peaking_string;
            _next();

            if (is_left_value) {
                result_inst = current_block->LoadInst(last_type, result_inst);
            }
            this_value = result_inst;

            if (last_type->is<StructType>()) {
                auto struct_type = last_type->to<StructType>();
                int offset = struct_type->getMemberOffset(member_name);
                if (offset == std::numeric_limits<int>::max()) {
                    offset = struct_type->getConceptOffset(member_name);

                    if (offset == std::numeric_limits<int>::max()) {
                        throw ParseErrorException(
                            location,
                            "struct " + struct_type->getName() + 
                            " does not have member `" + member_name + "`"
                        );
                    }

                    last_type = type_pool->getCastedStructType(
                        struct_type,
                        struct_type->getConceptByOffset(offset)
                    );
                    result_inst = current_block->AddInst(
                        last_type,
                        result_inst,
                        current_block->SignedImmInst(
                            type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS),
                            offset
                        )
                    );
                    is_left_value = false;
                }
                else {
                    auto member = struct_type->getMemberByOffset(offset);
                    last_type = member.type;
                    is_left_value = true;
                    result_inst = current_block->AddInst(
                        type_pool->getPointerType(last_type),
                        result_inst,
                        current_block->SignedImmInst(
                            type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS),
                            offset
                        )
                    );
                }
            }
            else if (last_type->is<CastedStructType>()) {
                auto casted_struct_type = last_type->to<CastedStructType>();
                int offset = casted_struct_type->getMemberOffset(member_name);
                if (offset == std::numeric_limits<int>::max()) {
                    offset = casted_struct_type->getMethodOffset(member_name);

                    if (offset == std::numeric_limits<int>::max()) {
                        throw ParseErrorException(
                            location,
                            "struct " + casted_struct_type->getOriginalStruct()->getName() +
                            " does not have member `" + member_name + "`"
                        );
                    }

                    auto method = casted_struct_type->getMethodByOffset(offset);
                    result_inst = current_block->LoadInst(
                        type_pool->getVTableType(casted_struct_type),
                        result_inst
                    );
                    last_type = method.prototype;
                    is_left_value = true;
                    result_inst = current_block->AddInst(
                        type_pool->getPointerType(last_type),
                        result_inst,
                        current_block->SignedImmInst(
                            type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS),
                            offset
                        )
                    );
                }
                else {
                    auto member = casted_struct_type->getMemberByOffset(offset);
                    last_type = member.type;
                    is_left_value = true;
                    result_inst = current_block->AddInst(
                        type_pool->getPointerType(last_type),
                        result_inst,
                        current_block->SignedImmInst(
                            type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS),
                            offset
                        )
                    );
                }
            }
            else {
                auto concept_type = last_type->to<ConceptType>();
                int offset = concept_type->getMethodOffset(member_name);
                if (offset == std::numeric_limits<int>::max()) {
                    throw ParseErrorException(
                        location,
                        "concept " + concept_type->getName() +
                        " does not have method `" + member_name + "`"
                    );
                }

                auto method = concept_type->getMethodByOffset(offset);
                last_type = method.prototype;
                is_left_value = true;
                result_inst = current_block->AddInst(
                    type_pool->getPointerType(last_type),
                    result_inst,
                    current_block->SignedImmInst(
                        type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS),
                        offset
                    )
                );
            }
        }
        else if (_peak() == '[') {
            last_type = resolveForwardType(last_type);
            if (!last_type->is<ArrayType>()) {
                throw ParseTypeErrorException(
                    location,
                    "type " + last_type->to_string() + " is not an array"
                );
            }
            _next();

            auto array_type = last_type->to<ArrayType>();
            auto array_inst = result_inst;
            if (is_left_value) {
                array_inst = current_block->LoadInst(
                    array_type,
                    array_inst
                );
            }

            parseExpression();
            if (!last_type->is<NumericType>()) {
                throw ParseTypeErrorException(
                    location,
                    "type " + last_type->to_string() + " cannot be used as index"
                );
            }
            if (is_left_value) {
                result_inst = current_block->LoadInst(
                    last_type,
                    result_inst
                );
            }

            result_inst = current_block->AddInst(
                type_pool->getPointerType(array_type->getBaseType()),
                array_inst,
                result_inst
            );
            last_type = array_type->getBaseType();
            is_left_value = true;

            if (_peak() != ']') {
                throw ParseExpectErrorException(
                    location,
                    "`]'",
                    _tokenLiteral()
                );
            }
            _next();
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

            if (symbol->klass == Symbol::K_RESERVED && symbol->token_value == R_NEW) {
                parseNewExpr();
                break;
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

                if (last_type->is<PointerType>() && !last_type->is<ArrayType>()) {
                    result_inst = current_block->LoadInst(
                        last_type,
                        result_inst
                    );

                    last_type = last_type->to<PointerType>()->getBaseType();
                }
            }
            else if (symbol->klass == Symbol::K_VARIABLE) {
                result_inst = reinterpret_cast<Instruction *>(symbol->token_value);
            }
            else if (symbol->klass == Symbol::K_FUNCTION) {
                result_inst = current_block->GlobalInst(
                    last_type->to<FunctionType>(),
                    variable_name
                );
                is_left_value = false;
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
        case T_STRING:
        {
            ir_builder->get()->string_pool.emplace(
                peaking_string,
                ".STR_" + std::to_string(ir_builder->get()->string_pool.size())
            );

            auto result_type = type_pool->getArrayType(type_pool->getSignedIntegerType(CYAN_CHAR_BITS));
            last_type = result_type;
            is_left_value = false;
            result_inst = current_block->GlobalInst(
                result_type,
                ir_builder->get()->string_pool.at(peaking_string)
            );

            _next();
            break;
        }
        default:
            throw ParseExpectErrorException(location, "unary", _tokenLiteral());
    }
}

void
Parser::parseNewExpr()
{
    assert(_peak() == T_ID);

    auto *symbol = symbol_table->lookup(peaking_string);
    assert(symbol);
    assert(symbol->token_value == R_NEW);

    if (_next() != T_ID) {
        throw ParseExpectErrorException(
            location,
            "Type",
            _tokenLiteral()
        );
    }
    _next();

    Type *base_type = checkTypeName(peaking_string);
    size_t size = base_type->size();

    if (_peak() == '[') {
        while (_next() == ']') {
            base_type = type_pool->getArrayType(base_type);
            if (_next() != '[') {
                throw ParseExpectErrorException(
                    location,
                    "array length",
                    _tokenLiteral()
                );
            }
        }
        parseExpression();

        if (is_left_value) {
            result_inst = current_block->LoadInst(last_type, result_inst);
        }
        if (!last_type->is<SignedIntegerType>() && !last_type->is<UnsignedIntegerType>()) {
            throw ParseTypeErrorException(
                location,
                "array length must be a integer"
            );
        }

        if (
            base_type->is<StructType>() ||
            base_type->is<ConceptType>() ||
            base_type->is<ArrayType>()
        ) {
            size = CYAN_PRODUCT_BITS / 8;
        }

        auto size_inst = current_block->MulInst(
            type_pool->getUnsignedIntegerType(CYAN_PRODUCT_BITS),
            current_block->UnsignedImmInst(
                type_pool->getUnsignedIntegerType(CYAN_PRODUCT_BITS),
                size
            ),
            result_inst
        );
        result_inst = current_block->NewInst(
            type_pool->getArrayType(base_type),
            size_inst
        );
        last_type = type_pool->getArrayType(base_type);
        is_left_value = false;

        if (_peak() != ']') {
            throw ParseExpectErrorException(
                location,
                "`]`",
                _tokenLiteral()
            );
        }
        _next();
    }
    else {
        if (!base_type->is<StructType>()) {
            throw ParseTypeErrorException(
                location,
                "type " + base_type->to_string() + " is not a completed struct"
            );
        }

        result_inst = current_block->NewInst(
            base_type,
            current_block->UnsignedImmInst(
                type_pool->getUnsignedIntegerType(CYAN_PRODUCT_BITS),
                size
            )
        );
        last_type = base_type;
        is_left_value = false;
    }
}

bool
Parser::parse(const char *content)
{
    location = Location(content);
    buffer.reset(new Buffer(std::strlen(content) + 1, content));
    current = buffer->cbegin();
    token_start = buffer->cbegin();

    return _parse();
}

bool
Parser::parseFile(const char *filename)
{
    std::FILE *f = std::fopen(filename, "r");
    if (!f) { throw ParseIOException(filename); }

    std::fseek(f, 0, SEEK_END);
    auto file_size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    char content[file_size + 1];
    std::fread(content, file_size, 1, f);
    content[file_size] = '\0';

    location = Location(filename);
    buffer.reset(new Buffer(static_cast<size_t>(file_size) + 1, content));
    current = buffer->cbegin();
    token_start = buffer->cbegin();

    return _parse();
}

std::unique_ptr<IR>
Parser::release()
{
    for (auto &symbol_pair : *symbol_table->rootScope()) {
        if (symbol_pair.second->klass == Symbol::K_GLOBAL) {
            ir_builder->defineGlobal(symbol_pair.first, symbol_pair.second->type);
        }
    }

    return std::unique_ptr<IR>(ir_builder->release(type_pool.release()));
}
