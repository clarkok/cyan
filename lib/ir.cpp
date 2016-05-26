//
// Created by Clarkok on 16/5/3.
//

#include "ir.hpp"

using namespace cyan;

std::ostream &
BasicBlock::output(std::ostream &os) const
{
    os << getName() << ":" << std::endl;
    os << "{\n"
       << "\tdominator: " << (dominator ? dominator->getName() : "(null)") << ",\n"
       << "\tdepth: " << depth << ",\n"
       << "\tloop_header: " << (loop_header ? loop_header->getName() : "(null)") << ",\n"
       << "\tpreceders: [";
    for (auto &preceder : preceders) {
        os << preceder->getName() << ",";
    }
    os << "]\n}\n";

    for (auto &inst : inst_list) {
        os << "\t" << inst->to_string() << std::endl;
    }

    if (condition) {
        os << "\t\t\tbr\t" << condition->getName() 
           << "\t" << then_block->getName()
           << ",\t" << else_block->getName()
           << std::endl;
    }
    else if (then_block) {
        os << "\t\t\tj\t" << then_block->getName() << std::endl;
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
    for (auto &global : global_defines) {
        os << "global " << global.first << "\t: " << global.second->to_string() << std::endl;
    }

    for (auto &string_pair : string_pool) {
        os << "string " << string_pair.second << "\t: \"" << string_pair.first << "\"" << std::endl;
    }

    os << std::endl;

    for (auto &func : function_table) {
        func.second->output(os) << std::endl;
    }

    return os;
}
