//
// Created by c on 5/16/16.
//

#ifndef CYAN_DEP_ANALYZER_HPP
#define CYAN_DEP_ANALYZER_HPP

#include "optimizer.hpp"

namespace cyan {

class DepAnalyzer : public Optimizer
{
    std::set<BasicBlock *> scanned;

    void scanDep(BasicBlock *block);
    void setPreceder(BasicBlock *block, BasicBlock *preceder);

    BasicBlock *findDominator(BasicBlock *p1, BasicBlock *p2);

public:
    DepAnalyzer(IR *ir)
        : Optimizer(ir)
    {
        for (auto &func : ir->function_table) {
            if (func.second->block_list.size()) {
                for (auto &block_ptr : func.second->block_list) {
                    block_ptr->depth = 0;
                    block_ptr->dominator = nullptr;
                    block_ptr->preceders.clear();
                }
                scanned.clear();
                scanDep(func.second->block_list.front().get());
            }
        }
    }

    void outputResult(std::ostream &os) const;
};

}

#endif //CYAN_DEP_ANALYZER_HPP
