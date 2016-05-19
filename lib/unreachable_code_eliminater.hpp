//
// Created by c on 5/19/16.
//

#ifndef CYAN_UNREACHABLE_CODE_ELIMINATER_HPP
#define CYAN_UNREACHABLE_CODE_ELIMINATER_HPP

#include "optimizer.hpp"

#include "dep_analyzer.hpp"

namespace cyan {

class UnreachableCodeEliminater : public Optimizer
{
    std::map<BasicBlock *, BasicBlock *> block_map;

    void combineSplitBlocks(Function *func);
    void markUnreachableBlocks(Function *func);
    void clearUnreachableBlocks(Function *func);
public:
    UnreachableCodeEliminater(IR *ir_)
        : Optimizer(ir_)
    {
        for (auto &func_pair : ir->function_table) {
            markUnreachableBlocks(func_pair.second.get());
        }

        ir.reset(DepAnalyzer(release()).release());

        for (auto &func_pair : ir->function_table) {
            combineSplitBlocks(func_pair.second.get());
        }

        ir.reset(DepAnalyzer(release()).release());

        for (auto &func_pair : ir->function_table) {
            clearUnreachableBlocks(func_pair.second.get());
        }
    }
};

}

#endif //CYAN_UNREACHABLE_CODE_ELIMINATER_HPP
