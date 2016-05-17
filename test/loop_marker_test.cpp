//
// Created by c on 5/17/16
//

#include <fstream>
#include "gtest/gtest.h"

#include "../lib/parse.hpp"
#include "../lib/dep_analyzer.hpp"
#include "../lib/loop_marker.hpp"

using namespace cyan;

TEST(loop_marker_test, basic_test)
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

    std::ofstream original_out("loop_marker_basic_test.ir");
    auto ir = parser->release().release();
    ir->output(original_out);

    std::ofstream analyzed_out("loop_marker_basic_test_analyze_result.txt");
    auto dep_analyzer = new DepAnalyzer(ir);
    dep_analyzer->outputResult(analyzed_out);

    LoopMarker(dep_analyzer->release()).outputResult(analyzed_out);
}

TEST(loop_marker_test, nested_loop_test)
{
    static const char SOURCE[] =
        "function main() {\n"
        "    let i = 0;\n"
        "    while (i < 10) {\n"
        "        if (i > 5) break;\n"
        "        i = i + 1;\n"
        "        let j = 0;\n"
        "        while (j < i) {\n"
        "            j = j + i;\n"
        "        }\n"
        "    }\n"
        "}\n"
    ;

    Parser *parser = new Parser(SOURCE);
    ASSERT_TRUE(parser->parse());

    std::ofstream original_out("loop_marker_nested_loop_test.ir");
    auto ir = parser->release().release();
    ir->output(original_out);

    std::ofstream analyzed_out("loop_marker_nested_loop_test_analyze_result.txt");
    auto dep_analyzer = new DepAnalyzer(ir);
    dep_analyzer->outputResult(analyzed_out);

    LoopMarker(dep_analyzer->release()).outputResult(analyzed_out);
}
