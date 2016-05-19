//
// Created by c on 5/19/16.
//

#ifndef CYAN_DEAD_CODE_ELIMINATER_HPP
#define CYAN_DEAD_CODE_ELIMINATER_HPP

#include "optimizer.hpp"

namespace cyan {

class DeadCodeEliminater : public Optimizer
{
    void clearInstReference(Function *func);
    void scanInstReference(Function *func);
    void removeInst(Function *func);

    void _scanner(Instruction *inst);
public:
    DeadCodeEliminater(IR *ir)
        : Optimizer(ir)
    {
        for (auto &func_pair : ir->function_table) {
            clearInstReference(func_pair.second.get());
            scanInstReference(func_pair.second.get());
            removeInst(func_pair.second.get());
        }
    }
};

}

#endif //CYAN_DEAD_CODE_ELIMINATER_HPP
