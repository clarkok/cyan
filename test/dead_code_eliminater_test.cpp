//
// Created by c on 5/19/16.
//

#include <fstream>

#include "gtest/gtest.h"

#include "../lib/parse.hpp"
#include "../lib/dep_analyzer.hpp"
#include "../lib/inst_rewriter.hpp"
#include "../lib/dead_code_eliminater.hpp"

using namespace cyan;

TEST(dead_code_eliminater_test, basic_test)
{
    static const char SOURCE[] =
        "let a = 1 + 2 * 3 / 4;"
    ;

    Parser *parser = new Parser(new ScreenOutputErrorCollector());
    ASSERT_TRUE(parser->parse(SOURCE));

    std::ofstream original_out("dead_code_eliminater_basic_original.ir");
    auto ir = parser->release().release();
    ir = InstRewriter(ir).release();
    ir->output(original_out);

    std::ofstream optimized_out("dead_code_eliminater_basic_optimized.ir");
    ir = DeadCodeEliminater(ir).release();
    ir->output(optimized_out);
}
