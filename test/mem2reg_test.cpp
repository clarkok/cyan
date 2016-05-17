//
// Created by c on 5/16/16.
//

#include <fstream>
#include "gtest/gtest.h"

#include "../lib/parse.hpp"
#include "../lib/inliner.hpp"
#include "../lib/dep_analyzer.hpp"
#include "../lib/mem2reg.hpp"

using namespace cyan;

TEST(mem2reg_test, basic_test)
{
    static const char SOURCE[] =
        "function main() {\n"
        "    let i = 0;\n"
        "    while (i < 10) {\n"
        "        i = i + 1;\n"
        "    }\n"
        "}\n"
    ;

    Parser *parser = new Parser(SOURCE);
    ASSERT_TRUE(parser->parse());

    std::ofstream original_out("mem2reg_basic_test_original.ir");
    auto ir = parser->release().release();
    ir->output(original_out);

    std::ofstream analyzed_out("mem2reg_basic_test_analyze_result.ir");
    std::ofstream optimized_out("mem2reg_basic_test_optimized.ir");
    Mem2Reg(DepAnalyzer(ir).release(), analyzed_out).release()->output(optimized_out);
}

TEST(mem2reg_test, after_inline)
{
    static const char SOURCE[] =
        "let a = 10, b = 15;\n"
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
        "    compare_and_swap(a, b);\n"
        "}\n";

    Parser *parser = new Parser(SOURCE);
    ASSERT_TRUE(parser->parse());

    std::ofstream original_out("mem2reg_after_inline_test_original.ir");
    auto ir = parser->release().release();
    ir->output(original_out);

    ir = Inliner(ir).release();
    std::ofstream after_inliner_out("mem2reg_after_inline_test_inlined.ir");
    ir->output(after_inliner_out);

    std::ofstream analyzed_out("mem2reg_after_inline_test_analyze_result.ir");

    auto dep_analyzer = new DepAnalyzer(ir);
    dep_analyzer->outputResult(analyzed_out);

    std::ofstream optimized_out("mem2reg_after_inline_test_optimized.ir");
    Mem2Reg(dep_analyzer->release(), analyzed_out).release()->output(optimized_out);
}