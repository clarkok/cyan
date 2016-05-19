//
// Created by c on 5/19/16.
//

#include <map>
#include <set>

#include "phi_eliminator.hpp"

using namespace cyan;

void
PhiEliminator::eliminateInFunction(Function *func)
{
    std::map<Instruction *, Instruction *> value_map;

    for (auto &block_ptr : func->block_list) {
        for (
            auto inst_iter = block_ptr->inst_list.begin();
            inst_iter != block_ptr->inst_list.end();
        ) {
            if ((*inst_iter)->is<PhiInst>()) {
                auto phi_inst = (*inst_iter)->to<PhiInst>();

                std::set<Instruction *> branch_values;
                for (auto &branch : *phi_inst) {
                    branch_values.emplace(branch.value);
                }

                if (branch_values.size() == 1) {
                    value_map.emplace(phi_inst, *branch_values.begin());
                    inst_iter = block_ptr->inst_list.erase(inst_iter);
                }
                else if (
                    branch_values.size() == 2 &&
                    branch_values.find(phi_inst) != branch_values.end()
                ) {
                    branch_values.erase(phi_inst);
                    value_map.emplace(phi_inst, *branch_values.begin());
                    inst_iter = block_ptr->inst_list.erase(inst_iter);
                }
                else {
                    inst_iter++;
                }
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
    }
}
