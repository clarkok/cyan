//
// Created by c on 5/19/16.
//

#include <fstream>

#include "gtest/gtest.h"

#include "../lib/parse.hpp"
#include "../lib/dep_analyzer.hpp"
#include "../lib/loop_marker.hpp"
#include "../lib/mem2reg.hpp"
#include "../lib/phi_eliminator.hpp"

using namespace cyan;

TEST(phi_eliminator_test, basic_test)
{
    static const char SOURCE[] =
        "function main(a : i64, b : i64) {\n"
        "    let i = 0;\n"
        "    let ta = a, tb = b;\n"
        "    while (i < 10) {\n"
        "        i = i + ta + tb;\n"
        "    }\n"
        "}\n"
    ;

    Parser *parser = new Parser(SOURCE);
    ASSERT_TRUE(parser->parse());

    std::ofstream original_out("phi_eliminator_basic_test_original.ir");
    auto ir = Mem2Reg(
        DepAnalyzer(
            parser->release().release()
        ).release()
    ).release();
    ir->output(original_out);

    std::ofstream optimized_out("phi_eliminator_basic_test_optimized.ir");
    ir = PhiEliminator(ir).release();
    ir->output(optimized_out);
}
