//
// Created by c on 5/19/16.
//

#ifndef CYAN_PHI_ELIMINATOR_HPP
#define CYAN_PHI_ELIMINATOR_HPP

#include "optimizer.hpp"

namespace cyan {

class PhiEliminator : public Optimizer
{
    void eliminateInFunction(Function *func);

public:
    PhiEliminator(IR *ir)
        : Optimizer(ir)
    {
        for (auto &func_pair : ir->function_table) {
            eliminateInFunction(func_pair.second.get());
        }
    }
};

}

#endif //CYAN_PHI_ELIMINATOR_HPP
