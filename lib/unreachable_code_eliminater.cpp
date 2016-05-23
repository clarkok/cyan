//
// Created by c on 5/19/16.
//

#include <algorithm>

#include "unreachable_code_eliminater.hpp"

using namespace cyan;

void
UnreachableCodeEliminater::markUnreachableBlocks(Function *func)
{
    for (auto &block_ptr : func->block_list) {
        if (block_ptr->condition) {
            auto replaced_condition = block_ptr->condition;
            while (value_map.find(replaced_condition) != value_map.end()) {
                replaced_condition = value_map.at(replaced_condition);
            }
            block_ptr->condition = replaced_condition;
            if (block_ptr->condition->is<SignedImmInst>()) {
                auto value = block_ptr->condition->to<SignedImmInst>()->getValue();
                if (value) {
                    unregisterPhi(block_ptr->else_block, block_ptr.get());
                }
                else {
                    unregisterPhi(block_ptr->then_block, block_ptr.get());
                    block_ptr->then_block = block_ptr->else_block;
                }
                block_ptr->else_block = nullptr;
                block_ptr->condition = nullptr;
            }
            else if (block_ptr->condition->is<UnsignedImmInst>()) {
                auto value = block_ptr->condition->to<UnsignedImmInst>()->getValue();
                if (value) {
                    unregisterPhi(block_ptr->else_block, block_ptr.get());
                }
                else {
                    unregisterPhi(block_ptr->then_block, block_ptr.get());
                    block_ptr->then_block = block_ptr->else_block;
                }
                block_ptr->else_block = nullptr;
                block_ptr->condition = nullptr;
            }
        }
    }
}

void
UnreachableCodeEliminater::combineSplitBlocks(Function *func)
{
    for (auto &block_ptr : func->block_list) {
        if (block_ptr->preceders.size() == 1) {
            auto preceder = *(block_ptr->preceders.begin());
            while (block_map.find(preceder) != block_map.end()) {
                preceder = block_map.at(preceder);
            }
            if (preceder->condition) { continue; }
            assert(preceder->then_block == block_ptr.get());
            assert(preceder != block_ptr.get());

            unregisterPhiInBlock(block_ptr.get(), *(block_ptr->preceders.begin()));

            preceder->append(block_ptr.get());
            block_map.emplace(block_ptr.get(), preceder);
        }
    }

    for (auto &value_pair : value_map) {
        auto replaced = value_pair.second;
        while (value_map.find(replaced) != value_map.end()) {
            replaced = value_map.at(replaced);
        }
        value_pair.second = replaced;
    }

    for (auto &block_ptr : func->block_list) {
        for (auto &inst_ptr : block_ptr->inst_list) {
            inst_ptr->resolve(value_map);
        }
        if (value_map.find(block_ptr->condition) != value_map.end()) {
            block_ptr->condition = value_map.at(block_ptr->condition);
        }
    }
}

void
UnreachableCodeEliminater::clearUnreachableBlocks(Function *func)
{
    func->block_list.remove_if(
        [func](const std::unique_ptr<BasicBlock> &block_ptr) {
            if (block_ptr.get() == func->block_list.front().get()) { return false; }
            return block_ptr->preceders.size() == 0;
        }
    );
}

void
UnreachableCodeEliminater::unregisterPhiInBlock(BasicBlock *block, BasicBlock *preceder)
{
    assert(block->preceders.find(preceder) != block->preceders.end());
    block->preceders.erase(preceder);

    for (auto &inst_ptr : block->inst_list) {
        if (inst_ptr->is<PhiInst>()) {
            auto phi_inst = inst_ptr->to<PhiInst>();
            phi_inst->remove_branch(preceder);

            if (phi_inst->branches_size() == 1) {
                value_map.emplace(phi_inst, phi_inst->begin()->value);
            }
        }
    }
}

void
UnreachableCodeEliminater::unregisterPhi(BasicBlock *block, BasicBlock *preceder)
{
    unregisterPhiInBlock(block, preceder);
    if (!block->preceders.size()) {
        if (block->then_block) {
            unregisterPhi(block->then_block, block);
        }
        if (block->else_block) {
            unregisterPhi(block->else_block, block);
        }
    }
}

void
UnreachableCodeEliminater::resolvePhiPreceders(Function *func)
{
    for (auto &block_ptr : func->block_list) {
        for (auto &inst_ptr : block_ptr->inst_list) {
            if (inst_ptr->is<PhiInst>()) {
                for (auto &branch : *(inst_ptr->to<PhiInst>())) {
                    while (block_map.find(branch.preceder) != block_map.end()) {
                        branch.preceder = block_map.at(branch.preceder);
                    }
                }
            }
        }
    }
}