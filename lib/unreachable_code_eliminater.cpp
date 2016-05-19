//
// Created by c on 5/19/16.
//

#include "unreachable_code_eliminater.hpp"

using namespace cyan;

void
UnreachableCodeEliminater::combineSplitBlocks(Function *func)
{
    block_map.clear();
    for (auto &block_ptr : func->block_list) {
        if (block_ptr->preceders.size() == 1) {
            auto preceder = *(block_ptr->preceders.begin());
            while (block_map.find(preceder) != block_map.end()) {
                preceder = block_map.at(preceder);
            }
            if (preceder->condition) { continue; }
            assert(preceder->then_block == block_ptr.get());
            preceder->append(block_ptr.get());
            block_map.emplace(block_ptr.get(), preceder);
        }
    }
}

void
UnreachableCodeEliminater::markUnreachableBlocks(Function *func)
{
    for (auto &block_ptr : func->block_list) {
        if (block_ptr->condition) {
            if (block_ptr->condition->is<SignedImmInst>()) {
                auto value = block_ptr->condition->to<SignedImmInst>()->getValue();
                if (!value) {
                    block_ptr->then_block = block_ptr->else_block;
                }
                block_ptr->else_block = nullptr;
                block_ptr->condition = nullptr;
            }
            else if (block_ptr->condition->is<UnsignedImmInst>()) {
                auto value = block_ptr->condition->to<UnsignedImmInst>()->getValue();
                if (!value) {
                    block_ptr->then_block = block_ptr->else_block;
                }
                block_ptr->else_block = nullptr;
                block_ptr->condition = nullptr;
            }
        }
    }
}

void
UnreachableCodeEliminater::clearUnreachableBlocks(Function *func)
{
    func->block_list.remove_if(
        [func](const std::unique_ptr<BasicBlock> &block_ptr) {
            if (block_ptr.get() == func->block_list.front().get()) { return false; }
            return block_ptr->preceders.empty();
        }
    );
}