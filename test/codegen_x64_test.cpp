//
// Created by c on 5/12/16.
//

#include <fstream>

#include "gtest/gtest.h"

#include "../lib/parse.hpp"
#include "../lib/codegen_x64.hpp"

using namespace cyan;

#define define_codegen_x64_test(name, source)                           \
    TEST(codegen_x64_test, name)                                        \
    {                                                                   \
        Parser *parser = new Parser(new ScreenOutputErrorCollector());  \
        ASSERT_TRUE(parser->parse(source));                             \
        CodeGenX64 *uut = new CodeGenX64(parser->release().release());  \
        std::ofstream ir_out("codegen_x64_" #name ".ir");               \
        uut->get()->output(ir_out) << std::endl;                        \
        std::ofstream as_out("codegen_x64_" #name ".s");                \
        uut->generate(as_out);                                          \
    }

define_codegen_x64_test(basic_test,
    "let a = 1 + 2;"
)

define_codegen_x64_test(multi_func_test,
    "let a = 1 + 2;\n"
    "function main() : i32 {\n"
    "    let b = 1;\n"
    "    if (a == 2) {\n"
    "        return b;\n"
    "    }\n"
    "    return a;\n"
    "}\n"
)

define_codegen_x64_test(phi_node_test,
    "let a = 1 && 2 || 3 && 4;"
)

define_codegen_x64_test(loop_test,
    "function main() {\n"
    "    let i = 0;\n"
    "    while (i < 10) {\n"
    "        i = i + 1;\n"
    "    }\n"
    "}\n"
)

define_codegen_x64_test(register_collect_test,
    "function main() {\n"
    "    let a = (1 + 2) * (1 - 2);\n"
    "    let b = 3 * a + 4;\n"
    "    let c = a + b;\n"
    "}\n"
)

define_codegen_x64_test(function_call_test,
    "function max(a : i32, b : i32) : i32 {\n"
    "    return a > b ? a : b;\n"
    "}\n"
    "function main() : i32 {\n"
    "    let a = 10;\n"
    "    let b = 5;\n"
    "    max(a, b);\n"
    "    return max(a, b);\n"
    "}\n"
)

define_codegen_x64_test(too_many_registers,
    "let a = 1 * (2 + (3 * (4 + (5 * (6 + (7 * (8 + (9 * (10 + (11 * (12 + (13 * (14 + (15 * 15))))))))))))));"
)

define_codegen_x64_test(too_many_arguments,
    "function callee(a : i32, b : i32, c : i32, d : i32, e : i32, f : i32, g : i32, h : i32, i : i32, j : i32) {\n"
    "}\n"
    "function main() {\n"
    "    callee(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);\n"
    "}\n"
)
