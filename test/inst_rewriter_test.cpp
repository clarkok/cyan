//
// Created by c on 5/19/16
//

#include <fstream>

#include "gtest/gtest.h"

#include "../lib/parse.hpp"
#include "../lib/dep_analyzer.hpp"
#include "../lib/loop_marker.hpp"
#include "../lib/mem2reg.hpp"
#include "../lib/inst_rewriter.hpp"
#include "../lib/phi_eliminator.hpp"

using namespace cyan;

TEST(inst_rewriter_test, constant_prop)
{
    static const char SOURCE[] =
        "let a = 1 + 2 * 3 / 4;"
    ;

    Parser *parser = new Parser(new ScreenOutputErrorCollector());
    ASSERT_TRUE(parser->parse(SOURCE));

    std::ofstream original_out("inst_rewriter_constant_prop_original.ir");
    auto ir = parser->release().release();
    ir->output(original_out);

    std::ofstream optimized_out("inst_rewriter_constant_prop_optimized.ir");
    ir = InstRewriter(ir).release();
    ir->output(optimized_out);
}

TEST(inst_rewriter_test, common_expression)
{
    static const char SOURCE[] =
        "function main(a : i64, b : i64) {\n"
        "    let t1 = a;\n"
        "    let t2 = b;\n"
        "    let t3 = (t1 + t2) * (t1 + t2);\n"
        "}\n"
    ;

    Parser *parser = new Parser(new ScreenOutputErrorCollector());
    ASSERT_TRUE(parser->parse(SOURCE));

    std::ofstream original_out("inst_rewriter_common_expression_original.ir");
    auto ir = Mem2Reg(
        DepAnalyzer(
            parser->release().release()
        ).release()
    ).release();
    ir->output(original_out);

    std::ofstream optimized_out("inst_rewriter_common_expression_optimized.ir");
    ir = InstRewriter(ir).release();
    ir->output(optimized_out);
}

TEST(inst_rewriter_test, loop_invariant)
{
    static const char SOURCE[] =
        "function main(a : i64, b : i64) {\n"
        "    let i = 0;\n"
        "    let ta = a, tb = b;\n"
        "    while (i < 10) {\n"
        "        i = i + ta * tb;\n"
        "    }\n"
        "}\n"
    ;

    Parser *parser = new Parser(new ScreenOutputErrorCollector());
    ASSERT_TRUE(parser->parse(SOURCE));

    std::ofstream original_out("inst_rewriter_loop_invariant_original.ir");
    auto ir = LoopMarker(
        PhiEliminator(
            Mem2Reg(
                DepAnalyzer(
                    parser->release().release()
                ).release()
            ).release()
        ).release()
    ).release();
    ir->output(original_out);

    std::ofstream optimized_out("inst_rewriter_loop_invariant_optimized.ir");
    ir = InstRewriter(ir).release();
    ir->output(optimized_out);
}