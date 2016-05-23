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
        ir->output(debug_out);
        debug_out << "========================================" << std::endl;
        debug_out << alloc_inst->getName() << std::endl;
        debug_out << "========================================" << std::endl;

        version_map.clear();
        value_map.clear();
        for (auto &block_ptr : func->block_list) {
            replaceInBasicBlock(func, block_ptr.get(), alloc_inst);
        }
        resolveEmptyPhi(func);
        resolveMultipleReplace();
        replaceUsage(func);
    }
}

void
Mem2Reg::replaceInBasicBlock(Function *func, BasicBlock *block, Instruction *inst)
{
    if (version_map.find(block) != version_map.end()) return;

    if (block->preceders.size() < 2) {
        if (block->preceders.size()) {
            replaceInBasicBlock(func, *(block->preceders.begin()), inst);
            version_map[block] = version_map.at(*(block->preceders.begin()));
        }

        for (
            auto inst_iter = block->inst_list.begin();
            inst_iter != block->inst_list.end();
        ) {
            if ((*inst_iter)->is<AllocaInst>() && inst_iter->get() == inst) {
                inst_to_replace.reset(inst_iter->release());
                inst_iter = block->inst_list.erase(inst_iter);
                continue;
            }

            if ((*inst_iter)->is<LoadInst>() && (*inst_iter)->to<LoadInst>()->getAddress() == inst) {
                assert(version_map.find(block) != version_map.end());
                assert(version_map.at(block));
                value_map.emplace(inst_iter->get(), version_map.at(block));
                inst_iter = block->inst_list.erase(inst_iter);
                continue;
            }

            if ((*inst_iter)->is<StoreInst>() && (*inst_iter)->to<StoreInst>()->getAddress() == inst) {
                version_map[block] = (*inst_iter)->to<StoreInst>()->getValue();
                inst_iter = block->inst_list.erase(inst_iter);
                continue;
            }

            ++inst_iter;
        }
    }
    else {
        auto builder = PhiInst::Builder(
            inst->getType(),
            block,
            inst->getName() + "." + std::to_string(func->countLocalTemp())
        );

        version_map[block] = builder.get();

        for (
            auto inst_iter = block->inst_list.begin();
            inst_iter != block->inst_list.end();
        ) {
            if ((*inst_iter)->is<AllocaInst>() && inst_iter->get() == inst) {
                inst_to_replace.reset(inst_iter->release());
                inst_iter = block->inst_list.erase(inst_iter);
                continue;
            }

            if ((*inst_iter)->is<LoadInst>() && (*inst_iter)->to<LoadInst>()->getAddress() == inst) {
                value_map.emplace(inst_iter->get(), version_map.at(block));
                inst_iter = block->inst_list.erase(inst_iter);
                continue;
            }

            if ((*inst_iter)->is<StoreInst>() && (*inst_iter)->to<StoreInst>()->getAddress() == inst) {
                version_map[block] = (*inst_iter)->to<StoreInst>()->getValue();
                inst_iter = block->inst_list.erase(inst_iter);
                continue;
            }

            ++inst_iter;
        }

        std::set<Instruction *> prev_values;
        for (auto &preceder : block->preceders) {
            replaceInBasicBlock(func, preceder, inst);

            auto value = version_map.at(preceder);
            while (value_map.find(value) != value_map.end()) {
                assert(value);
                value = value_map.at(value);
            }

            prev_values.emplace(value);
            builder.addBranch(value, preceder);
        }

        if (prev_values.find(nullptr) != prev_values.end()) {
            value_map.emplace(builder.get(), nullptr);
        }
        else if (prev_values.size() == 1) {
            if (prev_values.find(builder.get()) != prev_values.end()) {
                value_map.emplace(builder.get(), nullptr);
            }
            else {
                value_map.emplace(builder.get(), *(prev_values.begin()));
            }
        }
        else if (prev_values.size() == 2 && prev_values.find(builder.get()) != prev_values.end()) {
            prev_values.erase(builder.get());
            value_map.emplace(builder.get(), *(prev_values.begin()));
        }
        else {
            block->inst_list.emplace_front(builder.release());
        }
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

Instruction *
Mem2Reg::_phiScanner(PhiInst *phi_inst)
{
    if (scanned_phi.find(phi_inst) != scanned_phi.end()) { return phi_inst; }
    scanned_phi.emplace(phi_inst);

    for (auto &branch : *phi_inst) {
        while (value_map.find(branch.value) != value_map.end()) {
            assert(branch.value);
            branch.value = value_map.at(branch.value);
        }
        if (!branch.value) {
            return nullptr;
        }
    }

    for (auto &branch : *phi_inst) {
        if (branch.value->is<PhiInst>()) {
            if (!_phiScanner(branch.value->to<PhiInst>())) {
                branch.value = nullptr;
                return nullptr;
            }
        }
    }

    return phi_inst;
}

void
Mem2Reg::resolveEmptyPhi(Function *func)
{
    scanned_phi.clear();
    for (auto &block_ptr : func->block_list) {
        for (auto &inst_ptr : block_ptr->inst_list) {
            if (inst_ptr->is<PhiInst>()) {
                _phiScanner(inst_ptr->to<PhiInst>());
            }
        }
    }

    scanned_phi.clear();
    for (auto &block_ptr : func->block_list) {
        for (
            auto inst_iter = block_ptr->inst_list.begin();
            inst_iter != block_ptr->inst_list.end();
        ) {
            if ((*inst_iter)->is<PhiInst>() && !_phiScanner((*inst_iter)->to<PhiInst>())) {
                inst_iter = block_ptr->inst_list.erase(inst_iter);
            }
            else {
                inst_iter++;
            }
        }
    }
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
