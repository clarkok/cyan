//
// Created by c on 5/17/16.
//

#ifndef CYAN_INST_REWRITER_HPP
#define CYAN_INST_REWRITER_HPP

#include "optimizer.hpp"

namespace cyan {

class InstRewriter : public Optimizer
{
    typedef std::pair<Instruction *, Instruction *> OperandPair;
    typedef std::map<OperandPair, Instruction *> BinaryResultMap;
    typedef std::map<intptr_t, Instruction *> ImmediateMap;
    typedef std::map<std::string, Instruction *> GlobalMap;

    struct InternalResultSet
    {
        BinaryResultMap add_results;
        BinaryResultMap sub_results;
        BinaryResultMap mul_results;
        BinaryResultMap div_results;
        BinaryResultMap mod_results;
        BinaryResultMap shl_results;
        BinaryResultMap shr_results;
        BinaryResultMap or_results;
        BinaryResultMap and_results;
        BinaryResultMap nor_results;
        BinaryResultMap xor_results;
        BinaryResultMap seq_results;
        BinaryResultMap slt_results;
        BinaryResultMap sle_results;
    };

    std::map<BasicBlock *, std::unique_ptr<InternalResultSet> > block_result;
    std::map<Instruction *, Instruction *> value_map;
    std::map<intptr_t, Instruction *> imm_map;

    void rewriteFunction(Function *func);
    void rewriteBlock(Function *func, BasicBlock *block);

    Instruction *calculateConstant(Function *func, BinaryInst *inst);
    Instruction *findCalculated(BasicBlock *block, BinaryInst *inst);
    void registerResult(BasicBlock *block, BinaryInst *inst);

    Instruction *findAddResult(BasicBlock *block, Instruction *left, Instruction *right) const;
    Instruction *findSubResult(BasicBlock *block, Instruction *left, Instruction *right) const;
    Instruction *findMulResult(BasicBlock *block, Instruction *left, Instruction *right) const;
    Instruction *findDivResult(BasicBlock *block, Instruction *left, Instruction *right) const;
    Instruction *findModResult(BasicBlock *block, Instruction *left, Instruction *right) const;
    Instruction *findShlResult(BasicBlock *block, Instruction *left, Instruction *right) const;
    Instruction *findShrResult(BasicBlock *block, Instruction *left, Instruction *right) const;
    Instruction *findOrResult(BasicBlock *block, Instruction *left, Instruction *right) const;
    Instruction *findAndResult(BasicBlock *block, Instruction *left, Instruction *right) const;
    Instruction *findNorResult(BasicBlock *block, Instruction *left, Instruction *right) const;
    Instruction *findXorResult(BasicBlock *block, Instruction *left, Instruction *right) const;
    Instruction *findSeqResult(BasicBlock *block, Instruction *left, Instruction *right) const;
    Instruction *findSltResult(BasicBlock *block, Instruction *left, Instruction *right) const;
    Instruction *findSleResult(BasicBlock *block, Instruction *left, Instruction *right) const;

    std::list<std::unique_ptr<Instruction> > removed;

public:
    InstRewriter(IR *ir)
        : Optimizer(ir)
    {
        for (auto &func_iter : ir->function_table) {
            rewriteFunction(func_iter.second.get());
        }
    }
};

}

#endif //CYAN_INST_REWRITER_HPP
