//
// Created by Clarkok on 16/4/29.
//

#ifndef CYAN_PARSE_HPP
#define CYAN_PARSE_HPP

#include <exception>
#include <string>
#include <sstream>

#include "location.hpp"
#include "symbols.hpp"
#include "type.hpp"
#include "error_collector.hpp"
#include "ir_builder.hpp"

namespace cyan {

class Parser
{
public:
    static constexpr int ERROR_NR_LIMIT = 10;

    typedef char TChar;

    struct Buffer
    {
        size_t size;
        TChar *content;

        Buffer(size_t size, const char *content)
            : size(size), content(new TChar[size])
        { memcpy(this->content, content, size); }

        Buffer(Buffer &&buffer)
            : size(buffer.size), content(buffer.content)
        { buffer.content = nullptr; }

        Buffer(const Buffer &) = delete;

        ~Buffer()
        { if (content) delete[] content; }

        inline TChar *begin()               { return content; }
        inline TChar *end()                 { return content + size; }
        inline const TChar *cbegin() const  { return content; }
        inline const TChar *cend()   const  { return content + size; }
    };

    struct ParseErrorException : std::exception
    {
        std::string _what;

        ParseErrorException(Location location, std::string message)
            : _what(message + "\n     " + std::to_string(location) + "\n")
        { }

        const char *
        what() const noexcept
        { return _what.c_str(); }
    };

    struct ParseExpectErrorException : ParseErrorException
    {
        ParseExpectErrorException(Location location, std::string expected, std::string actual)
            : ParseErrorException(location, "Expected " + expected + ", but met " + actual)
        { }
    };

    struct ParseRedefinedErrorException : ParseErrorException
    {
        ParseRedefinedErrorException(Location location, std::string name, Location original)
            : ParseErrorException(
                location,
                "Symbol `" + name + "` redefined, originally defined at " + std::to_string(original)
            )
        { }
    };

    struct ParseInvalidVariableNameException : ParseErrorException
    {
        ParseInvalidVariableNameException(Location location, std::string name)
            : ParseErrorException(
                location,
                "`" + name + "` is not a valid variable name"
            )
        { }
    };

    struct ParseUndefinedErrorException : ParseErrorException
    {
        ParseUndefinedErrorException(Location location, std::string name)
            : ParseErrorException(
                location,
                "Symbol `" + name + "` undefined"
            )
        { }
    };

    struct ParseTypeErrorException : ParseErrorException
    {
        ParseTypeErrorException(Location location, std::string msg)
            : ParseErrorException(location, msg)
        { }
    };

    enum Token : TChar
    {
        T_UNPEAKED = 0,
        T_EOF = -1,
        T_ID = -2,
        T_INTEGER = -3,
        T_STRING = -4,

        T_LOGIC_OR = -5,
        T_LOGIC_AND = -6,

        T_EQ = -7,
        T_NE = -8,

        T_GE = -9,
        T_LE = -10,

        T_SHL = -11,
        T_SHR = -12,

        T_INC = -13,
        T_DEC = -14,

        T_ADD_ASSIGN = -15,
        T_SUB_ASSIGN = -16,
        T_MUL_ASSIGN = -17,
        T_DIV_ASSIGN = -18,
        T_MOD_ASSIGN = -19,
        T_SHL_ASSIGN = -20,
        T_SHR_ASSIGN = -21,
        T_AND_ASSIGN = -22,
        T_XOR_ASSIGN = -23,
        T_OR_ASSIGN = -24,
    };

    enum Reserved : intptr_t
    {
        R_CONCEPT,
        R_FUNCTION,
        R_LET,
        R_STRUCT,
    };

protected:
    Location location;
    Buffer buffer;
    decltype(buffer.cbegin()) current;
    decltype(buffer.cbegin()) token_start;
    int peaking_token;
    intptr_t peaking_int;
    std::string peaking_string;
    std::unique_ptr<SymbolTable> symbol_table;
    std::unique_ptr<TypePool> type_pool;
    CounterErrorCollector *error_counter;
    std::unique_ptr<ErrorCollector> error_collector;
    std::unique_ptr<IRBuilder> ir_builder;
    std::unique_ptr<IRBuilder::FunctionBuilder> current_function;
    std::unique_ptr<IRBuilder::BlockBuilder> current_block;

    Type *last_type = nullptr;
    bool is_left_value = false;
    Instrument *result_inst;

private:
    inline TChar
    _current(int offset = 0)
    { return current[offset]; }

    inline TChar
    _forward()
    {
        if (*current++ == '\n') {
            location.column = 0;
            ++location.line;
        }
        else {
            ++location.column;
        }
        return _current();
    }

    inline bool
    _endOfInput(int offset = 0)
    { return buffer.cend() - current <= offset; }

    inline std::string
    _tokenLiteral() const
    { return std::string(token_start, current); }

    Token _parseId();
    Token _parseNumber();
    Token _parseString();

    void _parseBlockComment();
    void _parseLineComment();

    int _peak();
    int _next();

    void _registerReserved();

protected:
    void checkVariableDefined(std::string name);
    Symbol *checkFunctionDefined(std::string name, FunctionType *type);
    Type *checkTypeName(std::string name);

    void parseGlobalLetStmt();
    void parseFunctionDefine();
    void parseLetStmt();

    void parsePrototype();
    void parseFunctionBody();

    void parseStatement();

    void parseExpression();
    void parseAssignmentExpr();
    void parseConditionalExpr();
    void parseLogicOrExpr();
    void parseLogicAndExpr();
    void parseBitwiseOrExpr();
    void parseBitwiseXorExpr();
    void parseBitwiseAndExpr();
    void parseEqualityExpr();
    void parseCompareExpr();
    void parseShiftExpr();
    void parseAdditiveExpr();
    void parseMultiplitiveExpr();
    void parsePrefixExpr();
    void parsePostfixExpr();
    void parseUnaryExpr();

public:
    Parser(const char *content)
        : location("\"" + std::string(content) + "\""),
          buffer(std::strlen(content) + 1, content),
          current(buffer.cbegin()),
          peaking_token(T_UNPEAKED),
          peaking_int(0),
          peaking_string(""),
          symbol_table(new SymbolTable()),
          type_pool(new TypePool()),
          error_counter(new LimitErrorCollector(10)),
          error_collector(
              dynamic_cast<ErrorCollector*>(
                  ChainErrorCollector::Builder()
                      .addCollector(new ScreenOutputErrorCollector())
                      .addCollector(error_counter)
                      .release()
              )
          ),
          ir_builder(new IRBuilder())
    { ir_builder->newFunction("_init_"); }

    virtual ~Parser() = default;

    bool parse();

    inline IR *
    release()
    { return ir_builder->release(); }
};

}

#endif //CYAN_PARSE_HPP
