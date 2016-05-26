//
// Created by c on 5/19/16.
//

#include "dead_code_eliminater.hpp"

using namespace cyan;

void
DeadCodeEliminater::clearInstReference(Function *func)
{
    for (auto &block_ptr : func->block_list) {
        for (auto &inst_ptr : block_ptr->inst_list) {
            inst_ptr->clearReferences();
        }
    }
}

void
DeadCodeEliminater::scanInstReference(Function *func)
{
    for (auto &block_ptr : func->block_list) {
        for (auto &inst_ptr : block_ptr->inst_list) {
            if (
                inst_ptr->is<CallInst>() ||
                inst_ptr->is<RetInst>() ||
                inst_ptr->is<StoreInst>() ||
                inst_ptr->is<DeleteInst>()
            ) {
                _scanner(inst_ptr.get());
            }
        }
        if (block_ptr->condition) {
            _scanner(block_ptr->condition);
        }
    }
}

void
DeadCodeEliminater::removeInst(Function *func)
{
    for (auto &block_ptr : func->block_list) {
        block_ptr->inst_list.remove_if(
            [](const std::unique_ptr<Instruction> &inst_ptr) {
                return (inst_ptr->getReferencedCount() == 0);
            }
        );
    }
}

void
DeadCodeEliminater::_scanner(Instruction *inst)
{
    if (scanned.find(inst) != scanned.end()) { return; }
    scanned.emplace(inst);

    inst->reference();
    if (inst->is<BinaryInst>()) {
        _scanner(inst->to<BinaryInst>()->getLeft());
        _scanner(inst->to<BinaryInst>()->getRight());
    }
    else if (inst->is<LoadInst>()) {
        _scanner(inst->to<LoadInst>()->getAddress());
    }
    else if (inst->is<StoreInst>()) {
        _scanner(inst->to<StoreInst>()->getAddress());
        _scanner(inst->to<StoreInst>()->getValue());
    }
    else if (inst->is<AllocaInst>()) {
        _scanner(inst->to<AllocaInst>()->getSpace());
    }
    else if (inst->is<CallInst>()) {
        _scanner(inst->to<CallInst>()->getFunction());
        for (auto &arg : *(inst->to<CallInst>())) {
            _scanner(arg);
        }
    }
    else if (inst->is<RetInst>()) {
        if (inst->to<RetInst>()->getReturnValue()) {
            _scanner(inst->to<RetInst>()->getReturnValue());
        }
    }
    else if (inst->is<PhiInst>()) {
        for (auto &branch : *(inst->to<PhiInst>())) {
            _scanner(branch.value);
        }
    }
    else if (inst->is<DeleteInst>()) {
        _scanner(inst->to<DeleteInst>()->getTarget());
    }
    else if (inst->is<NewInst>()) {
        _scanner(inst->to<NewInst>()->getSpace());
    }
    else if (inst->is<ImmediateInst>()) {
    }
    else {
        assert(false);
    }
}
