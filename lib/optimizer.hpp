//
// Created by c on 5/15/16
//

#ifndef CYAN_OPTIMIZER_HPP
#define CYAN_OPTIMIZER_HPP

#include <memory>
#include <iostream>

#include "ir.hpp"

namespace cyan {

class Optimizer
{
protected:
    std::unique_ptr<IR> ir;

public:
    Optimizer(IR *ir)
        : ir(ir)
    { }

    inline IR *
    release()
    { return ir.release(); }

    inline IR *
    get() const
    { return ir.get(); }
};

class OutputOptimizer : public Optimizer
{
public:
    OutputOptimizer(IR *ir)
        : Optimizer(ir)
    {
        ir->output(std::cerr);
        std::cerr << "================" << std::endl;
    }
};

}

#endif // CYAN_OPTIMIZER_HPP
