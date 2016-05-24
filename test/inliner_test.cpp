//
// Created by Clarkok on 5/15/16
//

#include <fstream>

#include "gtest/gtest.h"

#include "../lib/parse.hpp"
#include "../lib/inliner.hpp"

using namespace cyan;

TEST(inliner_test, function_test)
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

    Parser *parser = new Parser(new ScreenOutputErrorCollector());
    ASSERT_TRUE(parser->parse(SOURCE));

    std::ofstream original_out("inliner_function_test_original.ir");
    auto ir = parser->release().release();
    ir->output(original_out);

    Inliner *uut = new Inliner(ir);

    std::ofstream call_graph_out("inliner_function_test_call_graph.txt");
    uut->outputCallingGraph(call_graph_out);

    std::ofstream optimized_out("inliner_function_test_optimized.ir");
    uut->release()->output(optimized_out);
}

TEST(inliner_test, method_test)
{
    static const char SOURCE[] =
        "concept Person {\n"
        "    function getAge() : i32;\n"
        "}\n"
        "struct Student {\n"
        "    age : i32\n"
        "}\n"
        "impl Student : Person {\n"
        "    function getAge() : i32{\n"
        "        return this.age;\n"
        "    }\n"
        "}\n"
        "function main(p : Student) {\n"
        "    p.Person.getAge();\n"
        "}\n"
    ;

    Parser *parser = new Parser(new ScreenOutputErrorCollector());
    ASSERT_TRUE(parser->parse(SOURCE));

    std::ofstream original_out("inliner_method_test_original.ir");
    auto ir = parser->release().release();
    ir->output(original_out);

    Inliner *uut = new Inliner(ir);

    std::ofstream call_graph_out("inliner_method_test_call_graph.txt");
    uut->outputCallingGraph(call_graph_out);

    std::ofstream optimized_out("inliner_method_test_optimized.ir");
    uut->release()->output(optimized_out);
}
