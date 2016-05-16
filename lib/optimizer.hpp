//
// Created by c on 5/15/16
//

#ifndef CYAN_OPTIMIZER_HPP
#define CYAN_OPTIMIZER_HPP

#include <memory>

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

}

#endif // CYAN_OPTIMIZER_HPP
