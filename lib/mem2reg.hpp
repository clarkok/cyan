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
    std::set<Instruction *> alloc_insts;
    std::map<BasicBlock *, Instruction *> version_map;
    std::map<Instruction *, Instruction *> value_map;
    std::unique_ptr<Instruction> inst_to_replace;
    std::set<Instruction *> scanned_phi;

    void scanAllAllocInst(Function *func);
    void filterAllocInst(Function *func);
    void performReplace(Function *func);
    void replaceInBasicBlock(Function *func, BasicBlock *block, Instruction *inst);
    Instruction *requestLatestValue(Function *func, BasicBlock *block, Instruction *inst);
    Instruction *_phiScanner(PhiInst *phi_inst);
    void resolveEmptyPhi(Function *func);
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
