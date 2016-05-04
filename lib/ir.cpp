//
// Created by Clarkok on 16/5/3.
//

#include "ir.hpp"

using namespace cyan;

std::ostream &
BasicBlock::output(std::ostream &os) const
{
    os << getName() << ":" << std::endl;

    for (auto &inst : inst_list) {
        os << "\t" << inst->to_string() << std::endl;
    }

    if (condition) {
        os << "\tbr\t" << condition->getName() 
           << "\t" << then_block->getName()
           << ",\t" << else_block->getName()
           << std::endl;
    }
    else if (then_block) {
        os << "\tj\t" << then_block->getName() << std::endl;
    }

    return os;
}

std::ostream &
Function::output(std::ostream &os) const
{
    os << getName() << " {";

    for (auto &block : block_list) {
        block->output(os << std::endl);
    }

    os << "} // " << getName() << std::endl;

    return os;
}

std::ostream &
IR::output(std::ostream &os) const
{
    for (auto &func : function_table) {
        func.second->output(os) << std::endl;
    }

    return os;
}
