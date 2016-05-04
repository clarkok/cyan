//
// Created by Clarkok on 16/5/3.
//

#include "gtest/gtest.h"

#include "../lib/parse.hpp"

using namespace cyan;

TEST(parser_test, let_stmt)
{
    static const char SOURCE[] =
        "let a = 1 + 2;";

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, let_define_stmt)
{
    static const char SOURCE[] =
        "let a = 1 + 2, b = a, c = a + b;";

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, logic_and_test)
{
    static const char SOURCE[] =
        "let a = 1 && 2 && 0;";

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, multiple_let_test)
{
    static const char SOURCE[] =
        "let a = 1;\n"
        "let b = a + 1;\n"
        "let c = a + b;\n"
    ;

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, function_test)
{
    static const char SOURCE[] =
        "function test(a : i32, b : i32) : i32 {\n"
        "    let t0 = 1;\n"
        "    let t1 = t0 + a * b;\n"
        "}\n"
    ;

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, multiple_functions_test)
{
    static const char SOURCE[] =
        "function func1(a : i32, b : i32) {\n"
        "    let t = a;\n"
        "}\n"
        "function func2() : i32 {\n"
        "    let t = 1;\n"
        "}\n"
        "function func3() {}\n"
    ;

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, function_forward_decl_test)
{
    static const char SOURCE[] =
        "function test1(a : i32, b : i32);\n"
        "function test2() { }\n"
        "function test1(a : i32, b : i32) {\n"
        "}\n"
    ;

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, return_test)
{
    static const char SOURCE[] =
        "function test_i32(a : i32, b : i32) : i32 {\n"
        "    return a;\n"
        "}\n"
        "function test(a : i32, b : i32) {\n"
        "    return;\n"
        "}\n"
    ;

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, block_test)
{
    static const char SOURCE[] =
        "function main() {\n"
        "   let a = 0;\n"
        "   let b = a;\n"
        "   {\n"
        "        let a = 1;\n"
        "        b = a;\n"
        "   }\n"
        "   b = a;\n"
        "}\n"
    ;

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    uut->release()->output(std::cout);
}