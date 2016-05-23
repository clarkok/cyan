//
// Created by c on 5/16/16.
//

#include <queue>

#include "mem2reg.hpp"

using namespace cyan;

std::stringstream Mem2Reg::trash_out;

void
Mem2Reg::scanAllAllocInst(Function *func)
{
    alloc_insts.clear();

    for (auto &block_ptr : func->block_list) {
        for (auto &iter_ptr : block_ptr->inst_list) {
            if (iter_ptr->is<AllocaInst>()) {
                alloc_insts.emplace(iter_ptr.get());
            }
        }
    }
}

void
Mem2Reg::filterAllocInst(Function *func)
{
    for (auto &block_ptr : func->block_list) {
        for (auto &inst_ptr : block_ptr->inst_list) {
            if (inst_ptr->is<LoadInst>()) { }
            else if (inst_ptr->is<StoreInst>()) {
                if (alloc_insts.find(inst_ptr->to<StoreInst>()->getValue()) != alloc_insts.end()) {
                    alloc_insts.erase(inst_ptr->to<StoreInst>()->getValue());
                }
            }
            else {
                for (
                    auto alloc_iter = alloc_insts.begin();
                    alloc_iter != alloc_insts.end();
                ) {
                    if (inst_ptr->usedInstruction(*alloc_iter)) {
                        alloc_iter = alloc_insts.erase(alloc_iter);
                    }
                    else {
                        ++alloc_iter;
                    }
                }
            }
        }
    }
}

void
Mem2Reg::performReplace(Function *func)
{
    for (auto alloc_inst : alloc_insts) {
        debug_out << "========================================" << std::endl;
        debug_out << alloc_inst->getName() << std::endl;
        debug_out << "========================================" << std::endl;

        version_map.clear();
        value_map.clear();
        for (auto &block_ptr : func->block_list) {
            replaceInBasicBlock(func, block_ptr.get(), alloc_inst);
        }
        resolveMultipleReplace();
        replaceUsage(func);

        ir->output(debug_out);
    }
}

void
Mem2Reg::replaceInBasicBlock(Function *func, BasicBlock *block, Instruction *inst)
{
    if (version_map.find(block) != version_map.end()) return;

    for (
        auto inst_iter = block->inst_list.begin();
        inst_iter != block->inst_list.end();
    ) {
        if ((*inst_iter)->is<AllocaInst>() && inst_iter->get() == inst) {
            inst_to_replace.reset(inst_iter->release());
            inst_iter = block->inst_list.erase(inst_iter);
            continue;
        }

        if ((*inst_iter)->is<StoreInst>() && (*inst_iter)->to<StoreInst>()->getAddress() == inst) {
            version_map[block] = (*inst_iter)->to<StoreInst>()->getValue();
            inst_iter = block->inst_list.erase(inst_iter);
            continue;
        }

        if ((*inst_iter)->is<LoadInst>() && (*inst_iter)->to<LoadInst>()->getAddress() == inst) {
            if (version_map.find(block) != version_map.end()) {
                value_map.emplace(inst_iter->get(), version_map.at(block));
            }
            else {
                auto builder = PhiInst::Builder(
                    inst->getType()->to<PointerType>()->getBaseType(),
                    block,
                    inst->getName() + "." + std::to_string(func->countLocalTemp())
                );
                version_map.emplace(block, builder.get());
                value_map.emplace(inst_iter->get(), builder.get());

                for (auto preceder : block->preceders) {
                    builder.addBranch(requestLatestValue(func, preceder, inst), preceder);
                }

                std::set<Instruction *> prev_values;
                for (auto &branch : *builder.get()) {
                    prev_values.emplace(branch.value);
                }

                if (prev_values.size() == 1) {
                    value_map.emplace(builder.get(), builder.get()->begin()->value);
                }
                else {
                    block->inst_list.emplace_front(builder.release());
                }
            }
            inst_iter = block->inst_list.erase(inst_iter);
            continue;
        }

        ++inst_iter;
    }
    if (version_map.find(block) == version_map.end()) {
        version_map.emplace(block, nullptr);
    }
}

Instruction *
Mem2Reg::requestLatestValue(Function *func, BasicBlock *block, Instruction *inst)
{
    if (version_map.find(block) == version_map.end()) {
        replaceInBasicBlock(func, block, inst);
    }
    if (version_map.at(block)) {
        return version_map.at(block);
    }

    auto builder = PhiInst::Builder(
        inst->getType()->to<PointerType>()->getBaseType(),
        block,
        inst->getName() + "." + std::to_string(func->countLocalTemp())
    );
    version_map[block] = builder.get();

    for (auto preceder : block->preceders) {
        builder.addBranch(requestLatestValue(func, preceder, inst), preceder);
    }

    std::set<Instruction *> prev_values;
    for (auto &branch : *builder.get()) {
        prev_values.emplace(branch.value);
    }

    if (prev_values.size() == 1) {
        value_map.emplace(builder.get(), builder.get()->begin()->value);
    }
    else {
        block->inst_list.emplace_front(builder.release());
    }

    return version_map.at(block);
}

void
Mem2Reg::resolveMultipleReplace()
{
    std::map<Instruction *, Instruction *> resolved_load_map;

    for (auto &load_pair : value_map) {
        auto resolved = load_pair.second;
        while (value_map.find(resolved) != value_map.end()) {
            resolved = value_map.at(resolved);
        }
        assert(resolved);
        resolved_load_map.emplace(load_pair.first, resolved);
    }
    resolved_load_map.swap(value_map);
}

void
Mem2Reg::replaceUsage(Function *func)
{
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
Mem2Reg::outputReplacableAllocInst(Function *func, std::ostream &os)
{
    os << func->getName() << "\n";
    for (auto &alloc : alloc_insts) {
        os << "\t" << alloc->getName() << "\n";
    }
    os << std::endl;
}
