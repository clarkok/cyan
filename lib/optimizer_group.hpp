//
// Created by c on 5/24/16.
//

#ifndef CYAN_OPTIMIZER_GROUP_HPP
#define CYAN_OPTIMIZER_GROUP_HPP

#include <functional>
#include <type_traits>

#include "optimizer.hpp"
#include "dep_analyzer.hpp"
#include "loop_marker.hpp"
#include "mem2reg.hpp"
#include "unreachable_code_eliminater.hpp"
#include "dead_code_eliminater.hpp"
#include "inst_rewriter.hpp"
#include "inliner.hpp"

namespace cyan {

typedef std::function<IR *(IR *)> OptimizerWrapper;

template <typename T,
          typename = std::enable_if<std::is_base_of<Optimizer, T>::value> >
IR *
optimize_with(IR *ir)
{ return T(ir).release(); }

template <class... Optimizers>
class OptimizerGroup : public Optimizer
{
    static OptimizerWrapper wrappers[];
public:
    OptimizerGroup(IR *_ir)
        : Optimizer(_ir)
    {
        for (auto &wrapper : wrappers) {
            ir.reset(wrapper(ir.release()));
        }
    }
};

template <class... Optimizers>
OptimizerWrapper OptimizerGroup<Optimizers...>::wrappers[] = { optimize_with<Optimizers>... };

typedef OptimizerGroup<
    > OptimizerLevel0;

typedef OptimizerGroup<
    DepAnalyzer,
    LoopMarker,
    Mem2Reg,
    PhiEliminator,
    UnreachableCodeEliminater,
    DepAnalyzer,
    LoopMarker,
    PhiEliminator,
    DeadCodeEliminater
    > OptimizerLevel1;

typedef OptimizerGroup<
    DepAnalyzer,
    LoopMarker,
    Mem2Reg,
    PhiEliminator,
    InstRewriter,
    UnreachableCodeEliminater,
    DepAnalyzer,
    LoopMarker,
    PhiEliminator,
    DeadCodeEliminater
    > OptimizerLevel2;

typedef OptimizerGroup<
    Inliner,
    DepAnalyzer,
    LoopMarker,
    Mem2Reg,
    PhiEliminator,
    InstRewriter,
    UnreachableCodeEliminater,
    DepAnalyzer,
    LoopMarker,
    PhiEliminator,
    DeadCodeEliminater
    > OptimizerLevel3;
}

#endif //CYAN_OPTIMIZERGROUP_HPP
