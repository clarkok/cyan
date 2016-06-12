//
// Created by c on 5/16/16.
//

#include <queue>

#include "mem2reg.hpp"

using namespace cyan;

std::stringstream Mem2Reg::trash_out;

void
Mem2Reg::allocaForArgument(Function *func)
{
    ir->output(debug_out);
    argument_map.clear();
    value_map.clear();

    std::list<std::unique_ptr<Instruction> > buffer;

    auto entry_block = func->block_list.begin()->get();
    for (auto &block_ptr : func->block_list) {
        for (
            auto inst_iter = block_ptr->inst_list.begin();
            inst_iter != block_ptr->inst_list.end();
        ) {
            if ((*inst_iter)->is<ArgInst>()) {
                auto arg_inst = (*inst_iter)->to<ArgInst>();
                Instruction *alloc_inst = nullptr;
                if (argument_map.find(arg_inst->getValue()) == argument_map.end()) {
                    auto new_arg_inst = new ArgInst(
                        arg_inst->getType()->to<PointerType>(),
                        arg_inst->getValue(),
                        entry_block,
                        arg_inst->getName() + "_"
                    );
                    auto imm_inst = new UnsignedImmInst(
                        ir->type_pool->getUnsignedIntegerType(CYAN_PRODUCT_BITS),
                        1,
                        entry_block,
                        "_" + std::to_string(func->countLocalTemp())
                    );
                    alloc_inst = new AllocaInst(
                        arg_inst->getType(),
                        imm_inst,
                        entry_block,
                        "_" + arg_inst->getName()
                    );
                    auto load_inst = new LoadInst(
                        arg_inst->getType()->to<PointerType>()->getBaseType(),
                        new_arg_inst,
                        entry_block,
                        "_" + std::to_string(func->countLocalTemp())
                    );
                    auto store_inst = new StoreInst(
                        arg_inst->getType()->to<PointerType>()->getBaseType(),
                        alloc_inst,
                        load_inst,
                        entry_block,
                        "_" + std::to_string(func->countLocalTemp())
                    );
                    buffer.emplace_back(new_arg_inst);
                    buffer.emplace_back(imm_inst);
                    buffer.emplace_back(alloc_inst);
                    buffer.emplace_back(load_inst);
                    buffer.emplace_back(store_inst);
                    argument_map.emplace(arg_inst->getValue(), alloc_inst);
                }
                else {
                    alloc_inst = argument_map.at(arg_inst->getValue());
                }
                value_map.emplace(arg_inst, alloc_inst);
                inst_iter = block_ptr->inst_list.erase(inst_iter);
            }
            else {
                inst_iter++;
            }
        }
    }

    for (auto &block_ptr : func->block_list) {
        for (auto &inst_ptr : block_ptr->inst_list) {
            inst_ptr->resolve(value_map);
        }
        if (block_ptr->condition) {
            if (value_map.find(block_ptr->condition) != value_map.end()) {
                block_ptr->condition = value_map.at(block_ptr->condition);
            }
        }
    }

    if (func->block_list.size()) {
        entry_block->inst_list.splice(entry_block->inst_list.begin(), buffer);
    }

    value_map.clear();
}

void
Mem2Reg::scanAllAllocInst(Function *func)
{
    alloc_insts.clear();
    defined_block.clear();

    for (auto &block_ptr : func->block_list) {
        for (auto &iter_ptr : block_ptr->inst_list) {
            if (iter_ptr->is<AllocaInst>()) {
                alloc_insts.emplace(iter_ptr.get());
                defined_block.emplace(iter_ptr.get(), block_ptr.get());
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
                        defined_block.erase(*alloc_iter);
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
        debug_out << func->getName() << " " << alloc_inst->getName() << std::endl;
        debug_out << "========================================" << std::endl;

        version_map.clear();
        value_map.clear();
        for (auto &block_ptr : func->block_list) {
            replaceInBlock(func, block_ptr.get(), alloc_inst);
        }
        resolveMultipleReplace();
        replaceUsage(func);
    }
}

void
Mem2Reg::replaceInBlock(Function *func, BasicBlock *block_ptr, Instruction *inst)
{
    if (block_ptr != defined_block[inst] && !block_ptr->isDominatedBy(defined_block[inst])) { return; }
    if (version_map.find(block_ptr) != version_map.end()) { return; }

#define saveAndRemove(inst_iter)                            \
    insts_to_remove.emplace_back(inst_iter->release());     \
    inst_iter = block_ptr->inst_list.erase(inst_iter);

    if (block_ptr == defined_block[inst]) {
        for (
            auto inst_iter = block_ptr->inst_list.begin();
            inst_iter != block_ptr->inst_list.end();
        ) {
            if (inst_iter->get() == inst) {
                saveAndRemove(inst_iter);
                continue;
            }

            if ((*inst_iter)->is<LoadInst>() && (*inst_iter)->to<LoadInst>()->getAddress() == inst) {
                assert(version_map.find(block_ptr) != version_map.end());

                value_map.emplace(inst_iter->get(), version_map.at(block_ptr));
                saveAndRemove(inst_iter);
                continue;
            }

            if ((*inst_iter)->is<StoreInst>() && (*inst_iter)->to<StoreInst>()->getAddress() == inst) {
                version_map[block_ptr] = (*inst_iter)->to<StoreInst>()->getValue();
                saveAndRemove(inst_iter);
                continue;
            }

            ++inst_iter;
        }

        assert(version_map.find(block_ptr) != version_map.end());
    }
    else {
        assert(block_ptr->preceders.size());
        auto builder = PhiInst::Builder(
            inst->getType()->to<PointerType>()->getBaseType(),
            block_ptr,
            inst->getName() + "." + std::to_string(func->countLocalTemp())
        );

        if (block_ptr->preceders.size() == 1) {
            auto preceder = *block_ptr->preceders.begin();
            assert(preceder == defined_block[inst] || preceder->isDominatedBy(defined_block[inst]));

            replaceInBlock(func, preceder, inst);
            assert(version_map.find(preceder) != version_map.end());

            version_map.emplace(block_ptr, version_map.at(preceder));
        }
        else {
            version_map.emplace(block_ptr, builder.get());
        }

        for (
            auto inst_iter = block_ptr->inst_list.begin();
            inst_iter != block_ptr->inst_list.end();
        ) {
            assert(inst_iter->get() != inst);

            if ((*inst_iter)->is<LoadInst>() && (*inst_iter)->to<LoadInst>()->getAddress() == inst) {
                assert(version_map.find(block_ptr) != version_map.end());

                value_map.emplace(inst_iter->get(), version_map.at(block_ptr));
                saveAndRemove(inst_iter);
                continue;
            }

            if ((*inst_iter)->is<StoreInst>() && (*inst_iter)->to<StoreInst>()->getAddress() == inst) {
                version_map[block_ptr] = (*inst_iter)->to<StoreInst>()->getValue();
                saveAndRemove(inst_iter);
                continue;
            }

            ++inst_iter;
        }

        if (block_ptr->preceders.size() != 1) {
            std::set<Instruction*> prev_value;
            for (auto &preceder : block_ptr->preceders) {
                assert(preceder == defined_block[inst] || preceder->isDominatedBy(defined_block[inst]));

                replaceInBlock(func, preceder, inst);
                prev_value.emplace(version_map.at(preceder));
                builder.addBranch(version_map.at(preceder), preceder);
            }

            prev_value.erase(builder.get());
            if (prev_value.size() == 1) {
                value_map.emplace(builder.get(), *prev_value.begin());
                if (version_map.at(block_ptr) == builder.get()) {
                    version_map[block_ptr] = *prev_value.begin();
                }
                insts_to_remove.emplace_back(builder.release());
            }
            else {
                block_ptr->inst_list.emplace_front(builder.release());
            }
        }
    }

#undef saveAndRemove
}

void
Mem2Reg::resolveMultipleReplace()
{
    for (auto &value_pair : value_map) {
        auto replace = value_pair.second;
        while (value_map.find(replace) != value_map.end()) {
            replace = value_map.at(replace);
        }
        value_pair.second = replace;
    }
}

void
Mem2Reg::replaceUsage(Function *func)
{
    for (auto &block_ptr : func->block_list) {
        for (auto &inst_ptr : block_ptr->inst_list) {
            inst_ptr->resolve(value_map);
        }
        if (block_ptr->condition) {
            if (value_map.find(block_ptr->condition) != value_map.end()) {
                block_ptr->condition = value_map.at(block_ptr->condition);
            }
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
