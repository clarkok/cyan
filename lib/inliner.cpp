//
// Created by c on 5/15/16.
//

#include "inliner.hpp"
#include "ir.hpp"

using namespace cyan;

void
Inliner::constructCallingGraph()
{
    calling_graph.clear();
    for (auto &func_pair : ir->function_table) {
        calling_graph.emplace(
            func_pair.second.get(),
            std::unique_ptr<FunctionNode>(new FunctionNode())
        );
    }

    for (auto &func_pair : ir->function_table) {
        for (auto &block_ptr : func_pair.second->block_list) {
            for (auto &inst_ptr : block_ptr->inst_list) {
                if (inst_ptr->is<CallInst>()) {
                    auto *callee = tryPrecalculateFunction(inst_ptr->to<CallInst>());
                    if (callee) {
                        calling_graph[callee]->callers.emplace(func_pair.second.get());
                        calling_graph[func_pair.second.get()]->callees.emplace(callee);
                    }
                }
            }
        }
    }
}

Function *
Inliner::tryPrecalculateFunction(CallInst *inst)
{
    if (inst->getFunction()->is<GlobalInst>()) {
        auto global_inst = inst->getFunction()->to<GlobalInst>();
        if (ir->function_table.find(global_inst->getValue()) != ir->function_table.end()) {
            return ir->function_table[global_inst->getValue()].get();
        }
    }
    // TODO method call
    return nullptr;
}

void
Inliner::performInline(
    Function *caller,
    BasicBlock *block,
    std::list<std::unique_ptr<Instruction> >::iterator inst_iter,
    Function *callee
) {
    std::map<Instruction *, Instruction *> value_map;
    std::map<BasicBlock *, BasicBlock *> block_map;
    std::vector<Instruction *> argument_alloc;

    assert((*inst_iter)->is<CallInst>());
    auto call_inst = (*inst_iter)->to<CallInst>();

    auto new_bb = block->split(
        inst_iter,
        block->getName() + ".split." + std::to_string(caller->countLocalTemp())
    );
    caller->block_list.emplace_back(new_bb);

    auto result_phi_builder = PhiInst::Builder(
        (*inst_iter)->getType(),
        new_bb,
        (*inst_iter)->getName()
    );

    for (auto &arg : *call_inst) {
        auto imm_inst = new UnsignedImmInst(
            ir->type_pool->getUnsignedIntegerType(CYAN_PRODUCT_BITS),
            1,
            block,
            "_" + std::to_string(caller->countLocalTemp())
        );
        auto alloc_inst = new AllocaInst(
            arg->getType(),
            imm_inst,
            block,
            "_" + std::to_string(caller->countLocalTemp())
        );
        auto store_inst = new StoreInst(
            arg->getType(),
            alloc_inst,
            arg,
            block,
            ""
        );
        block->inst_list.emplace_back(imm_inst);
        block->inst_list.emplace_back(alloc_inst);
        block->inst_list.emplace_back(store_inst);
        argument_alloc.emplace_back(alloc_inst);
    }

    for (auto &block_ptr : callee->block_list) {
        caller->block_list.emplace_back(new BasicBlock(
            callee->getName() + "." + block_ptr->name + "." + std::to_string(caller->countLocalTemp()),
            block_ptr->getDepth()
        ));

        block_map.emplace(block_ptr.get(), caller->block_list.back().get());
    }

    for (auto &block_ptr : callee->block_list) {
        auto new_block = block_map[block_ptr.get()];
        bool return_block = false;

        for (auto &inst : block_ptr->inst_list) {
            if (inst->is<ArgInst>()) {
                auto arg_inst = inst->to<ArgInst>();
                if (value_map.find(arg_inst) == value_map.end()) {
                    value_map.emplace(arg_inst, argument_alloc[arg_inst->getValue()]);
                }
            }
            else if (inst->is<PhiInst>()) {
                auto phi_inst = inst->to<PhiInst>();
                new_block->inst_list.emplace_back(phi_inst->cloneBranch(
                    block_map,
                    value_map,
                    callee->getName() + "." + inst->getName() + "." + std::to_string(caller->countLocalTemp())
                ));
            }
            else if (inst->is<RetInst>()) {
                auto ret_inst = inst->to<RetInst>();
                return_block = true;
                if (ret_inst->getReturnValue()) {
                    result_phi_builder.addBranch(
                        value_map.at(ret_inst->getReturnValue()),
                        new_block
                    );
                }
                break;
            }
            else {
                new_block->inst_list.emplace_back(inst->clone(
                    new_block,
                    value_map,
                    callee->getName() + "." + inst->getName() + "." + std::to_string(caller->countLocalTemp())
                ));
            }
        }

        if (
            return_block ||
            (!block_ptr->condition && !block_ptr->then_block)
        ) { new_block->then_block = new_bb; }
        else {
            new_block->condition  = block_ptr->condition;
            new_block->then_block = block_ptr->then_block ? block_map.at(block_ptr->then_block) : nullptr;
            new_block->else_block = block_ptr->else_block ? block_map.at(block_ptr->else_block) : nullptr;
        }
    }

    for (auto &block_ptr : callee->block_list) {
        auto new_block = block_map[block_ptr.get()];
        for (auto &inst_ptr : new_block->inst_list) {
            inst_ptr->resolve(value_map);
        }
        if (new_block->condition) {
            new_block->condition = value_map.at(new_block->condition);
        }
    }

    block->condition = nullptr;
    block->then_block = block_map.at(callee->block_list.front().get());
    block->else_block = nullptr;

    Instruction *result_inst = nullptr;

    if (result_phi_builder.get()->branches_size() == 1) {
        result_inst = result_phi_builder.get()->begin()->value;
    }
    else if (result_phi_builder.get()->branches_size()) {
        new_bb->inst_list.emplace_front(result_inst = result_phi_builder.release());
    }
    else {
        new_bb->inst_list.emplace_front(result_inst = new UnsignedImmInst(
            ir->type_pool->getUnsignedIntegerType(CYAN_PRODUCT_BITS),
            0,
            new_bb,
            call_inst->getName()
        ));
    }

    for (auto &block_ptr : caller->block_list) {
        for (auto &inst_ptr : block_ptr->inst_list) {
            if (inst_ptr->is<PhiInst>()) {
                inst_ptr->to<PhiInst>()->replaceBranch(call_inst, result_inst, new_bb);
            }
            else {
                inst_ptr->replaceUsage(call_inst, result_inst);
            }
        }
    }
    block->inst_list.erase(inst_iter);
}

void
Inliner::resortFunctions()
{
    while (calling_graph.size()) {
        auto func_iter = calling_graph.begin();
        auto callee_nr = func_iter->second->callees.size();

        for (
            auto iter = calling_graph.begin();
            iter != calling_graph.end();
            ++iter
        ) {
            if (iter->second->callees.size() < callee_nr) {
                func_iter = iter;
                callee_nr = iter->second->callees.size();
            }
        }

        if (
            func_iter->first->inst_size() <= INLINE_INST_NR_LIMIT ||
            func_iter->second->callers.size() <= INLINE_CALLER_NR_LIMIT
        ) {
            for (auto &caller : func_iter->second->callers) {
                std::list<std::list<std::unique_ptr<Instruction> >::iterator> call_inst_list;
                std::list<BasicBlock *> owner_block_list;

                for (auto &block_ptr : caller->block_list) {
                    for (
                        auto inst_iter = block_ptr->inst_list.begin();
                        inst_iter != block_ptr->inst_list.end();
                        ++inst_iter
                    ) {
                        if (
                            (*inst_iter)->is<CallInst>() &&
                            tryPrecalculateFunction((*inst_iter)->to<CallInst>()) == func_iter->first
                        ) {
                            call_inst_list.emplace_back(inst_iter);
                            owner_block_list.emplace_back(block_ptr.get());
                        }
                    }
                }

                while (call_inst_list.size()) {
                    performInline(
                        caller,
                        owner_block_list.back(),
                        call_inst_list.back(),
                        func_iter->first
                    );
                    call_inst_list.pop_back();
                    owner_block_list.pop_back();
                }
            }
        }

        for (auto &caller : func_iter->second->callers) {
            calling_graph[caller]->callees.erase(func_iter->first);
        }

        calling_graph.erase(func_iter);
    }
}

void
Inliner::unusedFunctionEliminate()
{
    for (auto &func_pair : calling_graph) {
        if (func_pair.first->getName() == "__init__" ||
            func_pair.first->getName() == "main")
        { continue; }
        if (!func_pair.second->callers.size()) {
            ir->function_table.erase(func_pair.first->getName());
        }
    }
}

void
Inliner::outputCallingGraph(std::ostream &os)
{
    for (auto &func_pair : calling_graph) {
        os << func_pair.first->getName() << std::endl;;

        os << "callers" << std::endl;
        for (auto &callers : func_pair.second->callers) {
            os << "  > " << callers->getName() << std::endl;
        }

        os << "callees" << std::endl;
        for (auto &callees : func_pair.second->callees) {
            os << "  < " << callees->getName() << std::endl;
        }

        os << std::endl;
    }
}
