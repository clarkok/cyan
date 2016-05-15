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

TEST(parser_test, logic_or_test)
{
    static const char SOURCE[] =
        "let a = 1 || 2 || 3;";

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

    // uut->release()->output(std::cout);
}

TEST(parser_test, if_else_test)
{
    static const char SOURCE[] =
        "function main() {\n"
        "    let a = 1,\n"
        "        b = 2,\n"
        "        c = 0;\n"
        "    if (1) {\n"
        "        c = a + b;\n"
        "    }\n"
        "    else {\n"
        "        c = a - b;\n"
        "    }\n"
        "}\n"
    ;

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, single_if_test)
{
    static const char SOURCE[] =
        "function main() {\n"
        "    let a = 1;\n"
        "    if (a)\n"
        "        a = 0;\n"
        "    if (a) {\n"
        "        a = 1;\n"
        "    }\n"
        "}\n"
    ;

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, nested_if_test)
{
    static const char SOURCE[] =
        "function main() {\n"
        "    let a = 0;\n"
        "    if (1)\n"
        "        if (2)\n"
        "            a = 1;\n"
        "        else\n"
        "            a = 2;\n"
        "    else\n"
        "        if (3)\n"
        "            a = 3;\n"
        "        else\n"
        "            a = 4;\n"
        "}\n"
    ;

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, while_test)
{
    static const char SOURCE[] =
        "function main() {\n"
        "    let i = 0;\n"
        "    while (i < 10) {\n"
        "        i = i + 1;\n"
        "    }\n"
        "}"
    ;

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, continue_test)
{
    static const char SOURCE[] =
        "function main() {\n"
        "    let i = 0;\n"
        "    while (i < 10) {\n"
        "        i = i + 1;\n"
        "        if (i < 5)\n"
        "            continue;\n"
        "        else\n"
        "            i = i + 1;\n"
        "    }\n"
        "}"
    ;

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, break_test)
{
    static const char SOURCE[] =
        "function main() {\n"
        "    let i = 0;\n"
        "    while (i < 10) {\n"
        "        i = i + 1;\n"
        "        if (i < 5)\n"
        "            break;\n"
        "        else\n"
        "            i = i + 1;\n"
        "    }\n"
        "}"
    ;

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, nested_loop_test)
{
    static const char SOURCE[] =
        "function main() {\n"
        "    let i = 0;\n"
        "    while (i < 10) {\n"
        "        i = i + 1;\n"
        "        let j = i - 1;\n"
        "        while (1) {\n"
        "            if (j == 0) break;\n"
        "        }\n"
        "        if (j == 0) continue;\n"
        "    }\n"
        "}"
    ;

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, concept_test)
{
    static const char SOURCE[] =
        "concept Person {\n"
        "    function getID() : u32;\n"
        "    function getAge() : u32;\n"
        "}\n"
        "concept Student : Person {\n"
        "    function getID() : u32 { return 0; }\n"
        "}\n"
    ;

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, struct_test)
{
    static const char SOURCE[] =
        "struct Student {\n"
        "    id : i32,\n"
        "    age : i32\n"
        "}\n"
    ;

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, struct_member_test)
{
    static const char SOURCE[] = 
        "struct Student {\n"
        "    id : i32,\n"
        "    age : i32\n"
        "}\n"
        "function main(st : Student) : i32 {\n"
        "    return st.id;\n"
        "}\n"
    ;

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, struct_member_struct_test)
{
    static const char SOURCE[] = 
        "struct LinkedList {\n"
        "    next : LinkedList,\n"
        "    value : i32\n"
        "}\n"
        "function main(list : LinkedList) : i32 {\n"
        "    return list.next.next.value;\n"
        "}\n"
    ;

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, function_call_test)
{
    static const char SOURCE[] =
        "function test(a : i32, b : i32) : i32{\n"
        "    if (a > b) return a;\n"
        "    else return b;\n"
        "}\n"
        "function main() {\n"
        "    test(1, 2);\n"
        "    main();\n"
        "}\n"
    ;

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, method_call_test)
{
    static const char SOURCE[] =
        "concept Person {\n"
        "    function getID() : u32;\n"
        "}\n"
        "function main(st : Person) : u32 {\n"
        "    return st.getID();\n"
        "}\n"
    ;

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, impl_test)
{
    static const char SOURCE[] =
        "concept Person {\n"
        "    function getID() : u32;\n"
        "}\n"
        "struct Student {\n"
        "    id : u32\n"
        "}\n"
        "impl Student : Person {\n"
        "    function getID() : u32 {\n"
        "        return this.id;\n"
        "    }\n"
        "}\n"
        "function main(st : Person) : u32 {\n"
        "    return st.getID();\n"
        "}\n"
    ;

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, left_value_argument_test)
{
    static const char SOURCE[] =
        "function swap(a : &i32, b : &i32) {\n"
        "    let t = a;\n"
        "    a = b;\n"
        "    b = t;\n"
        "}\n"
        "function main() {\n"
        "    let a = 1, b = 2;\n"
        "    swap(a, b);\n"
        "}\n"
    ;

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}

TEST(parser_test, struct_template_define_test)
{
    static const char SOURCE[] =
        "concept Deletable {\n"
        "    function delete();\n"
        "}\n"
        "struct List<T : Deletable> {\n"
        "    ptr : T"
        "}\n"
    ;

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());

    // uut->release()->output(std::cout);
}
