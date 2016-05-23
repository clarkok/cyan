//
// Created by c on 5/19/16.
//

#include <fstream>

#include "gtest/gtest.h"

#include "../lib/parse.hpp"
#include "../lib/inliner.hpp"
#include "../lib/dep_analyzer.hpp"
#include "../lib/loop_marker.hpp"
#include "../lib/mem2reg.hpp"
#include "../lib/phi_eliminator.hpp"
#include "../lib/inst_rewriter.hpp"
#include "../lib/unreachable_code_eliminater.hpp"
#include "../lib/dead_code_eliminater.hpp"

using namespace cyan;

TEST(combined_test, case1)
{
    static const char SOURCE[] =
        "let ga = 0, gb = 0;"
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
        "    ga = a;\n"
        "    gb = b;\n"
        "}\n"
    ;

    Parser *parser = new Parser(SOURCE);
    ASSERT_TRUE(parser->parse());

    auto ir = parser->release().release();
    {
        std::ofstream parsed_out("combined_case1_parsed.ir");
        ir->output(parsed_out);
    }

    ir = Inliner(ir).release();
    {
        std::ofstream inlined_out("combined_case1_inlined.ir");
        ir->output(inlined_out);
    }

    ir = DepAnalyzer(ir).release();
    ir = LoopMarker(ir).release();

    {
        std::ofstream mem2reg_out("combined_case1_mem2reg.ir");
        std::ofstream mem2reg_debug_out("combined_case1_mem2reg_debug.ir");
        ir = Mem2Reg(ir, mem2reg_debug_out).release();
        ir->output(mem2reg_out);
    }

    ir = PhiEliminator(ir).release();
    {
        std::ofstream phi_eliminated_out("combined_case1_phi_eliminated.ir");
        ir->output(phi_eliminated_out);
    }

    ir = InstRewriter(ir).release();
    {
        std::ofstream inst_rewriten_out("combined_case1_inst_rewriten.ir");
        ir->output(inst_rewriten_out);
    }

    ir = UnreachableCodeEliminater(ir).release();
    {
        std::ofstream unreachable_eliminated_out("combined_case1_unreachable_eliminated.ir");
        ir->output(unreachable_eliminated_out);
    }

    ir = DepAnalyzer(ir).release();
    ir = LoopMarker(ir).release();
    ir = PhiEliminator(ir).release();
    {
        std::ofstream phi_eliminated_2_out("combined_case1_phi_eliminated_2.ir");
        ir->output(phi_eliminated_2_out);
    }

    ir = DeadCodeEliminater(ir).release();
    {
        std::ofstream dead_code_eliminated("combined_case1_dead_code_eliminated.ir");
        ir->output(dead_code_eliminated);
    }
}

TEST(combined_test, case2)
{
    static const char SOURCE[] =
        "function partition(a : i64[], lo : i64, hi : i64) : i64 {\n"
        "    let pivot = a[hi];\n"
        "    let i = lo;\n"
        "    let j = lo;\n"
        "    while (j < hi) {\n"
        "        if (a[j] < pivot) {\n"
        "            let t = a[i];\n"
        "            a[i] = a[j];\n"
        "            a[j] = t;\n"
        "            i = i + 1;\n"
        "        }\n"
        "        j = j + 1;\n"
        "    }\n"
        "    let t = a[i];\n"
        "    a[i] = a[j];\n"
        "    a[j] = t;\n"
        "    return i;\n"
        "}\n"
        "function quicksort(a : i64[], lo : i64, hi : i64) {\n"
        "    if (lo < hi) {\n"
        "        let p = partition(a, lo, hi);\n"
        "        quicksort(a, lo, p - 1);\n"
        "        quicksort(a, p + 1, hi);\n"
        "    }\n"
        "}\n"
        "function main(a : i64[], n : i64) {\n"
        "    quicksort(a, 0, n - 1);\n"
        "}\n"
    ;

    Parser *parser = new Parser(SOURCE);
    ASSERT_TRUE(parser->parse());

    auto ir = parser->release().release();
    {
        std::ofstream parsed_out("combined_case2_parsed.ir");
        ir->output(parsed_out);
    }

    ir = Inliner(ir).release();
    {
        std::ofstream inlined_out("combined_case2_inlined.ir");
        ir->output(inlined_out);
    }

    ir = DepAnalyzer(ir).release();
    ir = LoopMarker(ir).release();

    {
        std::ofstream mem2reg_out("combined_case2_mem2reg.ir");
        std::ofstream mem2reg_debug_out("combined_case2_mem2reg_debug.ir");
        ir = Mem2Reg(ir, mem2reg_debug_out).release();
        ir->output(mem2reg_out);
    }

    ir = PhiEliminator(ir).release();
    {
        std::ofstream phi_eliminated_out("combined_case2_phi_eliminated.ir");
        ir->output(phi_eliminated_out);
    }

    ir = InstRewriter(ir).release();
    {
        std::ofstream inst_rewriten_out("combined_case2_inst_rewriten.ir");
        ir->output(inst_rewriten_out);
    }

    ir = UnreachableCodeEliminater(ir).release();
    {
        std::ofstream unreachable_eliminated_out("combined_case2_unreachable_eliminated.ir");
        ir->output(unreachable_eliminated_out);
    }

    ir = DepAnalyzer(ir).release();
    ir = LoopMarker(ir).release();
    ir = PhiEliminator(ir).release();
    {
        std::ofstream phi_eliminated_2_out("combined_case2_phi_eliminated_2.ir");
        ir->output(phi_eliminated_2_out);
    }

    ir = DeadCodeEliminater(ir).release();
    {
        std::ofstream dead_code_eliminated("combined_case2_dead_code_eliminated.ir");
        ir->output(dead_code_eliminated);
    }
}
