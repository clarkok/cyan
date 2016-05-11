//
// Created by c on 5/10/16.
//

#ifndef CYAN_CODEGEN_X64_HPP
#define CYAN_CODEGEN_X64_HPP

#include <map>

#include "codegen.hpp"

namespace X64 {
struct Block;
enum class Register;
}

namespace cyan {

class CodeGenX64 : public CodeGen
{
public:
    static const int GENERAL_PURPOSE_REGISTER_NR = 14;
    static const int MEMORY_OPERATION_COST = 10;

private:
    std::map<X64::Register, int> reg_usage;
    std::map<Instrument *, X64::Register> inst_reg;

    std::vector<std::unique_ptr<X64::Block> > block_list;
    std::map<cyan::BasicBlock *, X64::Block *> block_map;
    std::map<X64::Block *, cyan::BasicBlock *> block_map_r;
    decltype(block_list.begin()) current_block;
    std::ostream &generateFunc(std::ostream &os, Function *func);

    std::vector<int> argument_stacked;
    int function_slot_count;

    int allocateStackSlot();
    int calculateArgumentOffset(intptr_t argument);

    size_t calculateRegisterCost(X64::Register);
    X64::Register findCheapestRegister();
    void swapOutRegister(X64::Register);
    X64::Register allocateRegister();

public:
    CodeGenX64(IR *ir)
        : CodeGen(ir)
    { }

    virtual std::ostream &generate(std::ostream &os);

    inst_foreach(define_gen)
};

}

#endif //CYAN_CODEGEN_X64_HPP
