//
// Created by c on 5/10/16.
//

#ifndef CYAN_CODEGEN_X64_HPP
#define CYAN_CODEGEN_X64_HPP

#include <map>
#include <memory>

#include "codegen.hpp"

namespace X64 {
struct Instruction
{
    virtual ~Instruction() = default;
    virtual std::string to_string() const = 0;
};

struct Block
{
    std::string name;
    cyan::BasicBlock *ir_block;
    std::list<std::unique_ptr<Instruction> > inst_list;

    Block(std::string name, cyan::BasicBlock *ir_block)
        : name(name), ir_block(ir_block)
    { }
};

enum class Register;
struct Operand;
}

namespace cyan {

class CodeGenX64 : public CodeGen
{
public:
    static const int GENERAL_PURPOSE_REGISTER_NR = 14;
    static const int MEMORY_OPERATION_COST = 10;

private:
    std::map<Instruction *, size_t> inst_used;
    std::vector<std::unique_ptr<X64::Block> > block_list;
    std::map<BasicBlock *, X64::Block *> block_map;
    std::map<Instruction *, std::shared_ptr<X64::Operand> > inst_result;
    std::map<AllocaInst *, int> allocate_map;
    int stack_allocate_counter = 0;

    decltype(block_list.rbegin()) current_block_iter;

    std::ostream &generateFunc(std::ostream &os, Function *func);

    int getAllocInstOffset(AllocaInst *inst);
    int calculateArgumentOffset(int argument);
    std::shared_ptr<X64::Operand> resolveOperand(Instruction *inst);
    std::shared_ptr<X64::Operand> resolveMemory(Instruction *inst);
    void setOrMoveOperand(Instruction *, std::shared_ptr<X64::Operand>);
    std::shared_ptr<X64::Operand> newValue();
    void prependInst(X64::Instruction *);

public:
    CodeGenX64(IR *ir)
        : CodeGen(ir)
    { }

    virtual std::ostream &generate(std::ostream &os);

    inst_foreach(define_gen)
};

}

#endif //CYAN_CODEGEN_X64_HPP
