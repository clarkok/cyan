//
// Created by c on 5/15/16
//

#ifndef CYAN_INLINER_HPP
#define CYAN_INLINER_HPP

#include <map>
#include <iostream>

#include "optimizer.hpp"

namespace cyan {

class Inliner : public Optimizer
{
public:
    static const int INLINE_INST_NR_LIMIT = 112;
    static const int INLINE_CALLER_NR_LIMIT = 2;

private:
    struct FunctionNode
    {
        std::set<Function *> callers;
        std::set<Function *> callees;

        FunctionNode() = default;
        ~FunctionNode() = default;

        FunctionNode(FunctionNode &&node)
            : callers(std::move(node.callers)), callees(std::move(node.callees))
        { }
    };

    std::map<Function *, std::unique_ptr<FunctionNode> > calling_graph;

    void constructCallingGraph();
    Function *tryPrecalculateFunction(CallInst *inst);
    void performInline(
        Function *caller,
        BasicBlock *block,
        std::list<std::unique_ptr<Instruction> >::iterator inst_iter,
        Function *callee
    );
    void resortFunctions();
    void unusedFunctionEliminate();

public:
    Inliner(IR *ir)
        : Optimizer(ir)
    {
        constructCallingGraph();
        resortFunctions();
        constructCallingGraph();
        unusedFunctionEliminate();
    }

    void outputCallingGraph(std::ostream &os);
};

}

#endif // CYAN_INLINER_HPP
