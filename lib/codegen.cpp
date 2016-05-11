//
// Created by c on 5/10/16.
//

#include "codegen.hpp"

using namespace cyan;

std::ostream &
CodeGen::generate(std::ostream &os)
{
    ir->output(os);
    return os;
}

#define impl_gen(type)          \
void                            \
CodeGen::gen(type *)            \
{ }

inst_foreach(impl_gen)
