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
}

TEST(parser_test, let_define_stmt)
{
    static const char SOURCE[] =
        "let a = 1 + 2, b = a, c = a + b;";

    Parser *uut = new Parser(SOURCE);
    ASSERT_TRUE(uut->parse());
}
