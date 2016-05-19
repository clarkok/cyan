//
// Created by c on 5/19/16.
//

#include <fstream>

#include "gtest/gtest.h"

#include "../lib/parse.hpp"
#include "../lib/dep_analyzer.hpp"
#include "../lib/inst_rewriter.hpp"
#include "../lib/dead_code_eliminater.hpp"
#include "../lib/inliner.hpp"
#include "../lib/unreachable_code_eliminater.hpp"

using namespace cyan;

TEST(unreachable_code_elimimater, basic_test)
{
    static const char SOURCE[] =
        "function max(a : i64, b : i64) : i64 {\n"
        "    return (a > b) ? a : b;\n"
        "}\n"
        "function min(a : i64, b : i64) : i64 {\n"
        "    return (a < b) ? a : b;\n"
        "}\n"
        "function compare_and_swap(a : &i64, b : &i64) {\n"
        "    let max_num = max(a, b);\n"
        "    let min_num = min(a, b);\n"
        "    a = max_num;\n"
        "    b = min_num;\n"
        "}\n"
        "function main() {\n"
        "    let a = 10, b = 15;\n"
        "    if (1) {\n"
        "        compare_and_swap(a, b);\n"
        "    }\n"
        "    else {\n"
        "        a = 0;\n"
        "    }\n"
        "}\n"
    ;

    Parser *parser = new Parser(SOURCE);
    ASSERT_TRUE(parser->parse());

    std::ofstream original_out("unreachable_code_eliminater_basic_original.ir");
    auto ir = parser->release().release();
    ir = Inliner(ir).release();
    ir = DepAnalyzer(ir).release();
    ir->output(original_out);

    std::ofstream optimized_out("unreachable_code_eliminater_basic_optimized.ir");
    ir = UnreachableCodeEliminater(ir).release();
    ir->output(optimized_out);
}
