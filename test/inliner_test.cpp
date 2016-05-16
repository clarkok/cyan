//
// Created by Clarkok on 5/15/16
//

#include <fstream>

#include "gtest/gtest.h"

#include "../lib/parse.hpp"
#include "../lib/inliner.hpp"

using namespace cyan;

TEST(inliner_test, functional_test)
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
        "    compare_and_swap(a, b);\n"
        "}\n";

    Parser *parser = new Parser(SOURCE);
    ASSERT_TRUE(parser->parse());

    std::ofstream original_out("inliner_functional_test_original.ir");
    auto ir = parser->release().release();
    ir->output(original_out);

    Inliner *uut = new Inliner(ir);

    std::ofstream call_graph_out("inliner_functional_test_call_graph.txt");
    uut->outputCallingGraph(call_graph_out);

    std::ofstream optimized_out("inliner_functional_test_optimized.ir");
    uut->release()->output(optimized_out);
}
