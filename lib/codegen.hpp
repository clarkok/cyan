//
// Created by c on 5/10/16.
//

#ifndef CYAN_CODEGEN_HPP
#define CYAN_CODEGEN_HPP

#include <iostream>
#include <memory>

#include "ir.hpp"

namespace cyan {

#define define_gen(type)                    \
    virtual void gen(type *inst);

class CodeGen
{
protected:
    std::unique_ptr<IR> ir;

public:
    CodeGen(IR *ir)
        : ir(ir)
    { }

    inline IR*
    release()
    { return ir.release(); }

    inline IR*
    get() const
    { return ir.get(); }

    virtual ~CodeGen() = default;
    virtual std::ostream &generate(std::ostream &os);

inst_foreach(define_gen)
};

}

#endif //CYAN_CODEGEN_HPP
