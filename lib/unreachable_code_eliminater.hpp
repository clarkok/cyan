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
    std::map<Instruction *, Instruction *> value_map;

    void markUnreachableBlocks(Function *func);
    void combineSplitBlocks(Function *func);
    void unregisterPhiInBlock(BasicBlock *block, BasicBlock *preceder);
    void resolvePhiPreceders(Function *func);
    void clearUnreachableBlocks(Function *func);
public:
    UnreachableCodeEliminater(IR *ir_)
        : Optimizer(ir_)
    {
        for (auto &func_pair : ir->function_table) {
            block_map.clear();
            value_map.clear();
            markUnreachableBlocks(func_pair.second.get());
            combineSplitBlocks(func_pair.second.get());
            resolvePhiPreceders(func_pair.second.get());
            clearUnreachableBlocks(func_pair.second.get());
        }
    }
};

}

#endif //CYAN_UNREACHABLE_CODE_ELIMINATER_HPP
