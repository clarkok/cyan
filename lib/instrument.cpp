//
// Created by Clarkok on 16/5/3.
//

#include "ir.hpp"
#include "instrument.hpp"

using namespace cyan;

namespace std {

std::string
to_string(std::string s)
{ return s; }

}

#define inst_header(asm_name)               \
    ("(" + getType()->to_string() + ")\t" asm_name "\t")

#define value_header(asm_name)              \
    (getName() + " =\t" + inst_header(asm_name))

#define imm_inst_to_string(Inst, asm_name)                      \
    std::string                                                 \
    Inst::to_string() const                                     \
    { return value_header(asm_name) + std::to_string(value); }

imm_inst_to_string(SignedImmInst,   "sli")
imm_inst_to_string(UnsignedImmInst, "uli")
imm_inst_to_string(GlobalInst,      "glob")
imm_inst_to_string(ArgInst,         "arg")

#undef imm_inst_to_string

#define binary_inst_to_string(Inst, asm_name)                                       \
    std::string                                                                     \
    Inst::to_string() const                                                         \
    { return value_header(asm_name) + left->getName() + ",\t" + right->getName(); }

binary_inst_to_string(AddInst,      "add")
binary_inst_to_string(SubInst,      "sub")
binary_inst_to_string(MulInst,      "mul")
binary_inst_to_string(DivInst,      "div")
binary_inst_to_string(ModInst,      "mod")

binary_inst_to_string(ShlInst,      "shl")
binary_inst_to_string(ShrInst,      "shr")
binary_inst_to_string(OrInst,       "or")
binary_inst_to_string(AndInst,      "and")
binary_inst_to_string(NorInst,      "nor")
binary_inst_to_string(XorInst,      "xor")

binary_inst_to_string(SeqInst,      "seq")
binary_inst_to_string(SltInst,      "slt")
binary_inst_to_string(SleInst,      "sle")

#undef binary_inst_to_string

std::string
LoadInst::to_string() const
{ return value_header("load") + address->getName(); }

std::string
StoreInst::to_string() const
{ return "\t" + inst_header("store") + address->getName() + ",\t" + value->getName(); }

std::string
AllocaInst::to_string() const
{ return value_header("alloc") + space->getName(); }

std::string
PhiInst::to_string() const
{
    std::string ret(value_header("phi"));

    for (auto iter = cbegin(); iter != cend(); ++iter) {
        ret += "[" + iter->preceder->getName() + "," + iter->value->getName() + "], ";
    }

    ret.pop_back();
    ret.pop_back();

    return ret;
}
