//
// Created by c on 5/17/16.
//

#ifndef CYAN_LOOP_MARKER_HPP
#define CYAN_LOOP_MARKER_HPP

#include <set>
#include <fstream>

#include "optimizer.hpp"

namespace cyan {

class LoopMarker : public Optimizer
{
    std::set<BasicBlock *> scanned;
    std::set<BasicBlock *> marked;

    bool isDominating(BasicBlock *child, BasicBlock *parent);
    void scan(BasicBlock *block);
    void mark(BasicBlock *block, BasicBlock *loop_header);

public:
    LoopMarker(IR *ir)
        : Optimizer(ir)
    {
        for (auto &func_iter : ir->function_table) {
            if (!func_iter.second->block_list.size()) { continue; }
            scanned.clear();

            for (auto &block_ptr : func_iter.second->block_list) {
                block_ptr->loop_header = nullptr;
                block_ptr->depth = 0;
            }
            scan(func_iter.second->block_list.front().get());
        }
    }

    void outputResult(std::ostream &os) const;
};

}

#endif //CYAN_LOOP_MARKER_HPP
