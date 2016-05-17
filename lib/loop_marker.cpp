//
// Created by c on 5/17/16.
//

#include "loop_marker.hpp"

using namespace cyan;

bool
LoopMarker::isDominating(BasicBlock *child, BasicBlock *parent)
{
    while (child) {
        if (child == parent) return true;
        child = child->dominator;
    }
    return false;
}

void
LoopMarker::scan(BasicBlock *block)
{
    if (scanned.find(block) != scanned.end()) return;
    scanned.emplace(block);

    if (block->then_block) {
        if (isDominating(block, block->then_block)) {
            marked.clear();
            mark(block, block->then_block);
        }
        else {
            scan(block->then_block);
        }
    }

    if (block->condition) {
        if (block->else_block) {
            if (isDominating(block, block->else_block)) {
                marked.clear();
                mark(block, block->else_block);
            }
            else {
                scan(block->else_block);
            }
        }
    }
}

void
LoopMarker::mark(BasicBlock *block, BasicBlock *loop_header)
{
    if (marked.find(block) != marked.end()) { return; }
    marked.emplace(block);

    if (!block->loop_header) {
        block->loop_header = loop_header;
    }
    block->depth ++;
    if (block == loop_header) { return; }

    for (auto &preceder : block->preceders) {
        mark(preceder, loop_header);
    }
}

void
LoopMarker::outputResult(std::ostream &os) const
{
    for (auto &func_iter : ir->function_table) {
        os << func_iter.first << ":\n";
        for (auto &block_ptr : func_iter.second->block_list) {
            os << "\t" << block_ptr->getName() << ":\n";
            os << "\t loop_header: " << (
                block_ptr->loop_header
                    ? block_ptr->loop_header->getName() 
                    : "(null)"
                )<< "\n";
            os << "\t depth: " << block_ptr->depth << "\n";
            os << std::endl;
        }
        os << std::endl;
    }
}
