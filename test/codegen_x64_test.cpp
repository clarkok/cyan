//
// Created by c on 5/12/16.
//

#include "gtest/gtest.h"

#include "../lib/parse.hpp"
#include "../lib/codegen_x64.hpp"

using namespace cyan;

TEST(codegen_x64_test, basic_test)
{
    static const char SOURCE[] =
        "let a = 1 + 2;";

    Parser *parser = new Parser(SOURCE);
    ASSERT_TRUE(parser->parse());

    CodeGenX64 *uut = new CodeGenX64(parser->release().release());
    // uut->get()->output(std::cout) << std::endl;
    uut->generate(std::cout);
}

TEST(codegen_x64_test, multi_func_test)
{
    static const char SOURCE[] =
        "let a = 1 + 2;\n"
        "function main() : i32 {\n"
        "    let b = 1;\n"
        "    if (a == 2) {\n"
        "        return b;\n"
        "    }\n"
        "    return a;\n"
        "}\n"
    ;

    Parser *parser = new Parser(SOURCE);
    ASSERT_TRUE(parser->parse());

    CodeGenX64 *uut = new CodeGenX64(parser->release().release());
    uut->get()->output(std::cout) << std::endl;
    uut->generate(std::cout);
}
