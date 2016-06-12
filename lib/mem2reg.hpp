//
// Created by c on 5/16/16.
//

#ifndef CYAN_MEM2REG_HPP
#define CYAN_MEM2REG_HPP

#include <sstream>
#include <set>
#include <map>

#include "optimizer.hpp"
#include "phi_eliminator.hpp"

namespace cyan {

class Mem2Reg : public Optimizer
{
    std::map<intptr_t, Instruction *> argument_map;
    std::set<Instruction *> alloc_insts;
    std::map<Instruction *, BasicBlock *> defined_block;
    std::map<BasicBlock *, Instruction *> version_map;
    std::map<Instruction *, Instruction *> value_map;
    std::list<std::unique_ptr<Instruction > > insts_to_remove;

    void allocaForArgument(Function *func);
    void scanAllAllocInst(Function *func);
    void filterAllocInst(Function *func);
    void performReplace(Function *func);
    void replaceInBlock(Function *func, BasicBlock *block_ptr, Instruction *inst);
    void resolveMultipleReplace();
    void replaceUsage(Function *func);

    void outputReplacableAllocInst(Function *func, std::ostream &os);

    std::ostream &debug_out;

public:
    static std::stringstream trash_out;

    Mem2Reg(IR *_ir, std::ostream &os = trash_out)
        : Optimizer(_ir), debug_out(os)
    {
        for (auto &func_iter : _ir->function_table) {
            allocaForArgument(func_iter.second.get());
            while (true) {
                scanAllAllocInst(func_iter.second.get());
                filterAllocInst(func_iter.second.get());
                if (!alloc_insts.size()) break;

                outputReplacableAllocInst(func_iter.second.get(), os);
                performReplace(func_iter.second.get());
            }
        }
    }
};

}

#endif //CYAN_MEM2REG_HPP
