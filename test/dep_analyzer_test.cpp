//
// Created by c on 5/16/16.
//

#include <fstream>
#include "gtest/gtest.h"

#include "../lib/parse.hpp"
#include "../lib/dep_analyzer.hpp"

using namespace cyan;

TEST(dep_analyzer_test, basic_test)
{
    static const char SOURCE[] =
        "let a = (1 && 2) || (3 && 4);"
    ;

    Parser *parser = new Parser(SOURCE);
    ASSERT_TRUE(parser->parse());

    std::ofstream original_out("dep_analyzer_basic_test.ir");
    auto ir = parser->release().release();
    ir->output(original_out);

    std::ofstream analyzed_out("dep_analyzer_basic_test_analyze_result.ir");
    DepAnalyzer(ir).outputResult(analyzed_out);
}

TEST(dep_analyzer_test, loop_test)
{
    static const char SOURCE[] =
        "function main() {\n"
        "    let i = 0;\n"
        "    while (i < 10) {\n"
        "        if (i > 5) break;\n"
        "        i = i + 1;\n"
        "    }\n"
        "}\n"
    ;

    Parser *parser = new Parser(SOURCE);
    ASSERT_TRUE(parser->parse());

    std::ofstream original_out("dep_analyzer_loop_test.ir");
    auto ir = parser->release().release();
    ir->output(original_out);

    std::ofstream analyzed_out("dep_analyzer_loop_test_analyze_result.ir");
    DepAnalyzer(ir).outputResult(analyzed_out);
}
