//
// Created by c on 5/10/16.
//

#include <list>
#include <memory>
#include <set>
#include <map>

#include "codegen_x64.hpp"

using namespace cyan;

namespace X64 {

enum class Register
{
    RAX,
    RBX,
    RCX,
    RDX,
    RSI,
    RDI,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15,
    RBP,
    RSP
};

#define register_foreach(__r)                                       \
    for (                                                           \
        X64::Register __r = X64::Register::RBX;                     \
        __r != X64::Register::R15;                                  \
        __r = static_cast<X64::Register>(static_cast<int>(__r) + 1) \
    )

std::string
to_string(Register reg)
{
#define to_string_case(enum_value, string_value)    \
    case Register::enum_value: return string_value

    switch (reg) {
        to_string_case(RAX, "%rax");
        to_string_case(RBX, "%rbx");
        to_string_case(RCX, "%rcx");
        to_string_case(RDX, "%rdx");
        to_string_case(RBP, "%rbp");
        to_string_case(RSI, "%rsi");
        to_string_case(RDI, "%rdi");
        to_string_case(RSP, "%rsp");
        to_string_case(R8,  "%r8");
        to_string_case(R9,  "%r9");
        to_string_case(R10, "%r10");
        to_string_case(R11, "%r11");
        to_string_case(R12, "%r12");
        to_string_case(R13, "%r13");
        to_string_case(R14, "%r14");
        to_string_case(R15, "%r15");
    }

#undef to_string_case
}

struct Operand
{
    virtual ~Operand() = default;
    virtual std::string to_string() const = 0;

    template <typename T>
    bool is() const
    { return dynamic_cast<const T*>(this) != nullptr; }

    template <typename T>
    const T *to() const
    { return dynamic_cast<const T*>(this); }

    template <typename T>
    T *to()
    { return dynamic_cast<T*>(this); }
};

struct ValueOperand : public Operand
{
    std::unique_ptr<Operand> actual_operand;
    static size_t counter;
    size_t count;

    ValueOperand()
        : count(++counter)
    { }

    /*
    virtual std::string
    to_string() const
    { return actual_operand->to_string(); }
     */

    virtual std::string
    to_string() const
    {
        if (actual_operand) {
            return actual_operand->to_string();
        }
        else {
            return "value " + std::to_string(count);
        }
    }
};

size_t ValueOperand::counter = 0;

struct RegisterOperand : public Operand
{
    Register reg;

    RegisterOperand(Register reg)
        : reg(reg)
    { }

    virtual std::string
    to_string() const
    { return X64::to_string(reg); }
};

struct MemoryOperand : public Operand
{
    virtual std::string to_string() const = 0;
};

struct OffsetMemoryOperand : public MemoryOperand
{
    std::shared_ptr<Operand> base, offset;

    OffsetMemoryOperand(std::shared_ptr<Operand> base, std::shared_ptr<Operand> offset)
        : base(base), offset(offset)
    { }

    virtual std::string
    to_string() const
    { assert(false); }  // TODO
};

struct StackMemoryOperand : public MemoryOperand
{
    int offset;

    StackMemoryOperand(int offset)
        : offset(offset)
    { }

    virtual std::string
    to_string() const
    { return "QWORD PTR [%rbp" + ((offset >= 0 ? "+" : "") + std::to_string(offset)) + "]"; }
};

struct GlobalMemoryOperand : public MemoryOperand
{
    std::string name;

    GlobalMemoryOperand(std::string name)
        : name(name)
    { }

    virtual std::string
    to_string() const
    { return "QWORD PTR [" + name + "]"; }
};

struct LabelOperand : public Operand
{
    std::string name;

    LabelOperand(std::string name)
        : name(name)
    { }

    virtual std::string
    to_string() const
    { return name; }
};

struct ImmediateOperand : public Operand
{
    intptr_t value;

    ImmediateOperand(intptr_t value)
        : value(value)
    { }

    virtual std::string
    to_string() const
    { return std::to_string(value); }
};

struct Mov : public Instruction
{
    std::shared_ptr<Operand> dst, src;

    Mov(std::shared_ptr<Operand> dst, std::shared_ptr<Operand> src)
        : dst(dst), src(src)
    { }

    virtual std::string
    to_string() const
    { return "mov " + dst->to_string() + ", " + src->to_string(); }
};

struct Add : public Instruction
{
    std::shared_ptr<Operand> dst, src;

    Add(std::shared_ptr<Operand> dst, std::shared_ptr<Operand> src)
        : dst(dst), src(src)
    { }

    virtual std::string
    to_string() const
    { return "add " + dst->to_string() + ", " + src->to_string(); };
};

struct And : public Instruction
{
    std::shared_ptr<Operand> dst, src;

    And(std::shared_ptr<Operand> dst, std::shared_ptr<Operand> src)
        : dst(dst), src(src)
    { }

    virtual std::string
    to_string() const
    { return "and " + dst->to_string() + ", " + src->to_string(); };
};

struct Call : public Instruction
{
    std::shared_ptr<LabelOperand> label;

    Call(std::shared_ptr<LabelOperand> label)
        : label(label)
    { }

    virtual std::string
    to_string() const
    { return "call " + label->to_string(); }
};

struct CallPreserve : public Instruction
{
    Call *call_inst;

    CallPreserve(Call *call_inst)
        : call_inst(call_inst)
    { }

    virtual std::string
    to_string() const
    { return "// preserving"; }
};

struct CallRestore : public Instruction
{
    Call *call_inst;

    CallRestore(Call *call_inst)
        : call_inst(call_inst)
    { }

    virtual std::string
    to_string() const
    { return "// restoring"; }
};

struct Cmp : public Instruction
{
    std::shared_ptr<Operand> left, right;

    Cmp(std::shared_ptr<Operand> left, std::shared_ptr<Operand> right)
        : left(left), right(right)
    { }

    virtual std::string
    to_string() const
    { return "cmp " + left->to_string() + ", " + right->to_string(); }
};

struct Idiv : public Instruction
{
    std::shared_ptr<Operand> dst, src;

    Idiv(std::shared_ptr<Operand> dst, std::shared_ptr<Operand> src)
        : dst(dst), src(src)
    { }

    // TODO implement div instruction
    virtual std::string to_string() const = 0;
};

struct Imod : public Instruction
{
    std::shared_ptr<Operand> dst, src;

    Imod(std::shared_ptr<Operand> dst, std::shared_ptr<Operand> src)
        : dst(dst), src(src)
    { }

    // TODO implement mod instruction
    virtual std::string to_string() const = 0;
};

struct Imul : public Instruction
{
    std::shared_ptr<Operand> dst, src;

    Imul(std::shared_ptr<Operand> dst, std::shared_ptr<Operand> src)
        : dst(dst), src(src)
    { }

    virtual std::string
    to_string() const
    { return "imul " + dst->to_string() + ", " + src->to_string(); }
};

struct Jmp : public Instruction
{
    std::shared_ptr<LabelOperand> label;

    Jmp(std::shared_ptr<LabelOperand> label)
        : label(label)
    { }

    virtual std::string
    to_string() const
    { return "jmp " + label->to_string(); }
};

struct Je : public Instruction
{
    std::shared_ptr<LabelOperand> label;

    Je(std::shared_ptr<LabelOperand> label)
        : label(label)
    { }

    virtual std::string
    to_string() const
    { return "je " + label->to_string(); }
};

struct Jne : public Instruction
{
    std::shared_ptr<LabelOperand> label;

    Jne(std::shared_ptr<LabelOperand> label)
        : label(label)
    { }

    virtual std::string
    to_string() const
    { return "jne " + label->to_string(); }
};

struct Jg : public Instruction
{
    std::shared_ptr<LabelOperand> label;

    Jg(std::shared_ptr<LabelOperand> label)
        : label(label)
    { }

    virtual std::string
    to_string() const
    { return "jg " + label->to_string(); }
};

struct Jge : public Instruction
{
    std::shared_ptr<LabelOperand> label;

    Jge(std::shared_ptr<LabelOperand> label)
        : label(label)
    { }

    virtual std::string
    to_string() const
    { return "jge " + label->to_string(); }
};

struct Jl : public Instruction
{
    std::shared_ptr<LabelOperand> label;

    Jl(std::shared_ptr<LabelOperand> label)
        : label(label)
    { }

    virtual std::string
    to_string() const
    { return "jl " + label->to_string(); }
};

struct Jle : public Instruction
{
    std::shared_ptr<LabelOperand> label;

    Jle(std::shared_ptr<LabelOperand> label)
        : label(label)
    { }

    virtual std::string
    to_string() const
    { return "jle " + label->to_string(); }
};

struct Jz : public Instruction
{
    std::shared_ptr<LabelOperand> label;

    Jz(std::shared_ptr<LabelOperand> label)
        : label(label)
    { }

    virtual std::string
    to_string() const
    { return "jz " + label->to_string(); }
};

struct Jnz : public Instruction
{
    std::shared_ptr<LabelOperand> label;

    Jnz(std::shared_ptr<LabelOperand> label)
        : label(label)
    { }

    virtual std::string
    to_string() const
    { return "jnz " + label->to_string(); }
};

struct LeaOffset : public Instruction
{
    std::shared_ptr<Operand> dst, base, offset;

    LeaOffset(
        std::shared_ptr<Operand> dst,
        std::shared_ptr<Operand> base,
        std::shared_ptr<Operand> offset
    )
        : dst(dst), base(base), offset(offset)
    { }

    virtual std::string
    to_string() const
    {
        return "lea " + dst->to_string() + ", [" +
            base->to_string() + (offset->to<ImmediateOperand>()->value >= 0 ? "+" : "") +
            offset->to_string() + "]";
    }
};

struct LeaGlobal : public Instruction
{
    std::shared_ptr<Operand> dst, global;

    LeaGlobal(
        std::shared_ptr<Operand> dst,
        std::shared_ptr<Operand> global
    )
        : dst(dst), global(global)
    { }

    virtual std::string
    to_string() const
    { return "lea " + dst->to_string() + ", " + global->to_string(); }
};

struct Neg : public Instruction
{
    std::shared_ptr<Operand> dst;

    Neg(std::shared_ptr<Operand> dst)
        : dst(dst)
    { }

    virtual std::string
    to_string() const
    { return "neg " + dst->to_string(); }
};

struct Not : public Instruction
{
    std::shared_ptr<Operand> dst;

    Not(std::shared_ptr<Operand> dst)
        : dst(dst)
    { }

    virtual std::string
    to_string() const
    { return "not " + dst->to_string(); }
};

struct Or : public Instruction
{
    std::shared_ptr<Operand> dst, src;

    Or(std::shared_ptr<Operand> dst, std::shared_ptr<Operand> src)
        : dst(dst), src(src)
    { }

    virtual std::string
    to_string() const
    { return "or " + dst->to_string() + ", " + src->to_string(); }
};

struct Pop : public Instruction
{
    std::shared_ptr<Operand> dst;

    Pop(std::shared_ptr<Operand> dst)
        : dst(dst)
    { }

    virtual std::string
    to_string() const
    { return "pop " + dst->to_string(); }
};

struct Push : public Instruction
{
    std::shared_ptr<Operand> src;

    Push(std::shared_ptr<Operand> src)
        : src(src)
    { }

    virtual std::string
    to_string() const
    { return "push " + src->to_string(); }
};

struct Ret : public Instruction
{
    virtual std::string
    to_string() const
    { return "ret"; }
};

struct Sal : public Instruction
{
    std::shared_ptr<Operand> dst, src;

    Sal(std::shared_ptr<Operand> dst, std::shared_ptr<Operand> src)
        : dst(dst), src(src)
    { }

    virtual std::string
    to_string() const
    { return "sal " + dst->to_string() + ", " + src->to_string(); }
};

struct Sar : public Instruction
{
    std::shared_ptr<Operand> dst, src;

    Sar(std::shared_ptr<Operand> dst, std::shared_ptr<Operand> src)
        : dst(dst), src(src)
    { }

    virtual std::string
    to_string() const
    { return "sar " + dst->to_string() + ", " + src->to_string(); }
};

struct SetE : public Instruction
{
    std::shared_ptr<Operand> dst;

    SetE(std::shared_ptr<Operand> dst)
        : dst(dst)
    { }

    virtual std::string
    to_string() const
    { return "sete " + dst->to_string(); }
};

struct SetL : public Instruction
{
    std::shared_ptr<Operand> dst;

    SetL(std::shared_ptr<Operand> dst)
        : dst(dst)
    { }

    virtual std::string
    to_string() const
    { return "setl " + dst->to_string(); }
};

struct SetLe : public Instruction
{
    std::shared_ptr<Operand> dst;

    SetLe(std::shared_ptr<Operand> dst)
        : dst(dst)
    { }

    virtual std::string
    to_string() const
    { return "setle " + dst->to_string(); }
};

struct Shr : public Instruction
{
    std::shared_ptr<Operand> dst, src;

    Shr(std::shared_ptr<Operand> dst, std::shared_ptr<Operand> src)
        : dst(dst), src(src)
    { }

    virtual std::string
    to_string() const
    { return "sar " + dst->to_string() + ", " + src->to_string(); }
};

struct Sub : public Instruction
{
    std::shared_ptr<Operand> dst, src;

    Sub(std::shared_ptr<Operand> dst, std::shared_ptr<Operand> src)
        : dst(dst), src(src)
    { }

    virtual std::string
    to_string() const
    { return "sub " + dst->to_string() + ", " + src->to_string(); }
};

struct Xor : public Instruction
{
    std::shared_ptr<Operand> dst, src;

    Xor(std::shared_ptr<Operand> dst, std::shared_ptr<Operand> src)
        : dst(dst), src(src)
    { }

    virtual std::string
    to_string() const
    { return "xor " + dst->to_string() + ", " + src->to_string(); }
};

}

std::ostream &
CodeGenX64::generate(std::ostream &os)
{
    os << ".intel_syntax" << std::endl;

    os << ".data" << std::endl;
    for (auto &global : ir->global_defines) {
        os << "\t" << global.first << ":\t.quad 0" << std::endl;
    }

    os << std::endl << ".text" << std::endl;
    for (auto &func : ir->function_table) {
        os << "\t.globl " << func.first << "\n"
           << "\t.type " << func.first << " @function\n"
           << func.first << ":" << std::endl;

        generateFunc(os, func.second.get());

        for (auto &block_ptr : block_list) {
            os << func.second->getName() + "." + block_ptr->name << ":\n";
            for (auto &inst_ptr : block_ptr->inst_list) {
                os << "\t" << inst_ptr->to_string() << "\n";
            }
            os << std::endl;
        }

        os << func.first << ".end:\n"
           << "\t.size " << func.first << " .-" << func.first << "\n" << std::endl;
    }

    return os;
}

std::ostream &
CodeGenX64::generateFunc(std::ostream &os, Function *func)
{
    inst_used.clear();
    block_list.clear();
    block_map.clear();
    inst_result.clear();
    allocate_map.clear();
    stack_allocate_counter = 0;

    for (auto &bb_ptr : func->block_list) {
        block_list.emplace_back(new X64::Block(bb_ptr->getName(), bb_ptr.get()));
        block_map.emplace(bb_ptr.get(), block_list.back().get());
    }

    for (
        current_block_iter = block_list.rbegin();
        current_block_iter != block_list.rend();
        ++current_block_iter
    ) {
        auto bb = (*current_block_iter)->ir_block;
        if (bb->condition) {
            inst_used[bb->condition]++;
            inst_result[bb->condition] = nullptr;
        }

        for (
            auto inst_iter = bb->inst_list.rbegin();
            inst_iter != bb->inst_list.rend();
            ++inst_iter
        ) {
            if ((*inst_iter)->isCodeGenRoot() || (inst_used[inst_iter->get()])) {
                (*inst_iter)->codegen(this);
            }
            else {
                (*inst_iter)->unreferenceOperand();
            }
        }

        if (bb->condition) {
            if (bb->condition->is<SeqInst>()) {
                if (
                    current_block_iter == block_list.rbegin() ||
                    bb->then_block != (*std::prev(current_block_iter))->ir_block
                ) {
                    (*current_block_iter)->inst_list.emplace_back(new X64::Je(
                        std::shared_ptr<X64::LabelOperand>(
                            new X64::LabelOperand(func->getName() + "." + bb->then_block->getName())
                        )
                    ));

                    if (
                        current_block_iter == block_list.rbegin() ||
                        bb->else_block != (*std::prev(current_block_iter))->ir_block
                    ) {
                        (*current_block_iter)->inst_list.emplace_back(new X64::Jmp(
                            std::shared_ptr<X64::LabelOperand>(
                                new X64::LabelOperand(func->getName() + "." + bb->else_block->getName())
                            )
                        ));
                    }
                }
                else {
                    (*current_block_iter)->inst_list.emplace_back(new X64::Jne(
                        std::shared_ptr<X64::LabelOperand>(
                            new X64::LabelOperand(func->getName() + "." + bb->else_block->getName())
                        )
                    ));
                }
            }
            else if (bb->condition->is<SltInst>()) {
                if (
                    current_block_iter == block_list.rbegin() ||
                    bb->then_block != (*std::prev(current_block_iter))->ir_block
                ) {
                    (*current_block_iter)->inst_list.emplace_back(new X64::Jl(
                        std::shared_ptr<X64::LabelOperand>(
                            new X64::LabelOperand(func->getName() + "." + bb->then_block->getName())
                        )
                    ));

                    if (
                        current_block_iter == block_list.rbegin() ||
                        bb->else_block != (*std::prev(current_block_iter))->ir_block
                    ) {
                        (*current_block_iter)->inst_list.emplace_back(new X64::Jmp(
                            std::shared_ptr<X64::LabelOperand>(
                                new X64::LabelOperand(func->getName() + "." + bb->else_block->getName())
                            )
                        ));
                    }
                }
                else {
                    (*current_block_iter)->inst_list.emplace_back(new X64::Jge(
                        std::shared_ptr<X64::LabelOperand>(
                            new X64::LabelOperand(func->getName() + "." + bb->else_block->getName())
                        )
                    ));
                }
            }
            else if (bb->condition->is<SleInst>()) {
                if (
                    current_block_iter == block_list.rbegin() ||
                    bb->then_block != (*std::prev(current_block_iter))->ir_block
                ) {
                    (*current_block_iter)->inst_list.emplace_back(new X64::Jle(
                        std::shared_ptr<X64::LabelOperand>(
                            new X64::LabelOperand(func->getName() + "." + bb->then_block->getName())
                        )
                    ));

                    if (
                        current_block_iter == block_list.rbegin() ||
                        bb->else_block != (*std::prev(current_block_iter))->ir_block
                    ) {
                        (*current_block_iter)->inst_list.emplace_back(new X64::Jmp(
                            std::shared_ptr<X64::LabelOperand>(
                                new X64::LabelOperand(func->getName() + "." + bb->else_block->getName())
                            )
                        ));
                    }
                }
                else {
                    (*current_block_iter)->inst_list.emplace_back(new X64::Jg(
                        std::shared_ptr<X64::LabelOperand>(
                            new X64::LabelOperand(func->getName() + "." + bb->else_block->getName())
                        )
                    ));
                }
            }
            else {
                if (
                    current_block_iter == block_list.rbegin() ||
                    bb->then_block != (*std::prev(current_block_iter))->ir_block
                ) {
                    (*current_block_iter)->inst_list.emplace_back(new X64::Jnz(
                        std::shared_ptr<X64::LabelOperand>(
                            new X64::LabelOperand(func->getName() + "." + bb->then_block->getName())
                        )
                    ));

                    if (
                        current_block_iter == block_list.rbegin() ||
                        bb->else_block != (*std::prev(current_block_iter))->ir_block
                    ) {
                        (*current_block_iter)->inst_list.emplace_back(new X64::Jmp(
                            std::shared_ptr<X64::LabelOperand>(
                                new X64::LabelOperand(func->getName() + "." + bb->else_block->getName())
                            )
                        ));
                    }
                }
                else {
                    (*current_block_iter)->inst_list.emplace_back(new X64::Jz(
                        std::shared_ptr<X64::LabelOperand>(
                            new X64::LabelOperand(func->getName() + "." + bb->else_block->getName())
                        )
                    ));
                }
            }
        }
        else {
            if (!bb->then_block) {
                (*current_block_iter)->inst_list.emplace_back(new X64::Ret());
            }
            else if (
                current_block_iter == block_list.rbegin() ||
                bb->then_block != (*std::prev(current_block_iter))->ir_block
            ) {
                (*current_block_iter)->inst_list.emplace_back(new X64::Jmp(
                    std::shared_ptr<X64::LabelOperand>(
                        new X64::LabelOperand(func->getName() + "." + bb->then_block->getName())
                    )
                ));
            }
        }
    }

    return os;
}

int
CodeGenX64::getAllocInstOffset(AllocaInst *inst)
{
    if (allocate_map.find(inst) == allocate_map.end()) {
        assert(inst->getSpace()->is<UnsignedImmInst>());
        allocate_map.emplace(
            inst,
            stack_allocate_counter += inst->getSpace()->to<UnsignedImmInst>()->getValue()
        );
    }

    return allocate_map[inst] * 8 - 48;
}

int
CodeGenX64::calculateArgumentOffset(int argument)
{
    if (argument >= 6) {
        return (8 * argument) - 48;
    }
    else {  // reserved stack for arguments
        return (-8 * argument) - 8;
    }
}

std::shared_ptr<X64::Operand>
CodeGenX64::resolveOperand(Instruction *inst)
{
    if (inst->is<SignedImmInst>()) {
        auto signed_imm_inst = inst->to<SignedImmInst>();
        signed_imm_inst->unreference();
        return std::shared_ptr<X64::Operand>(new X64::ImmediateOperand(signed_imm_inst->getValue()));
    }
    else if (inst->is<UnsignedImmInst>()) {
        auto unsigned_imm_inst = inst->to<UnsignedImmInst>();
        unsigned_imm_inst->unreference();
        return std::shared_ptr<X64::Operand>(new X64::ImmediateOperand(unsigned_imm_inst->getValue()));
    }
    else if (inst->is<LoadInst>()) {
        auto load_inst = inst->to<LoadInst>();

        if (load_inst->getAddress()->is<GlobalInst>()) {
            load_inst->unreference();
            return std::shared_ptr<X64::Operand>(new X64::GlobalMemoryOperand(
                load_inst->getAddress()->to<GlobalInst>()->getValue()));
        }
        else if (load_inst->getAddress()->is<ArgInst>()) {
            load_inst->unreference();
            return std::shared_ptr<X64::Operand>(
                new X64::StackMemoryOperand(calculateArgumentOffset(static_cast<int>(
                    load_inst->getAddress()->to<ArgInst>()->getValue())))
            );
        }
        else if (load_inst->getAddress()->is<AllocaInst>()) {
            load_inst->unreference();
            return std::shared_ptr<X64::Operand>(
                new X64::StackMemoryOperand(getAllocInstOffset(
                    load_inst->getAddress()->to<AllocaInst>()))
            );
        }
        else {
            auto operand = newValue();
            inst_result.emplace(load_inst, operand);
            inst_used[load_inst]++;

            return operand;
        }
    }
    else {
        auto operand = newValue();
        inst_result.emplace(inst, operand);
        inst_used[inst]++;
        return operand;
    }
}

std::shared_ptr<X64::Operand>
CodeGenX64::resolveMemory(Instruction *inst)
{
    if (inst->is<GlobalInst>()) {
        return std::shared_ptr<X64::Operand>(new X64::GlobalMemoryOperand(inst->to<GlobalInst>()->getValue()));
    }
    else if (inst->is<ArgInst>()) {
        return std::shared_ptr<X64::Operand>(
            new X64::StackMemoryOperand(calculateArgumentOffset(static_cast<int>(
                inst->to<ArgInst>()->getValue())))
        );
    }
    else if (inst->is<AllocaInst>()) {
        return std::shared_ptr<X64::Operand>(
            new X64::StackMemoryOperand(getAllocInstOffset(
                inst->to<AllocaInst>()))
        );
    }
    else {
        auto operand = newValue();
        inst_result.emplace(inst, operand);
        inst_used[inst]++;
        return std::shared_ptr<X64::Operand>(
            new X64::OffsetMemoryOperand(
                operand,
                nullptr
            )
        );
    }
}

void
CodeGenX64::setOrMoveOperand(Instruction *inst, std::shared_ptr<X64::Operand> dst)
{
    if (inst_result.find(inst) != inst_result.end()) {
        prependInst(new X64::Mov(
            dst,
            inst_result.at(inst)
        ));
    }
    else {
        inst_result.emplace(inst, dst);
    }
    inst_used[inst]++;
}

std::shared_ptr<X64::Operand>
CodeGenX64::newValue()
{ return std::shared_ptr<X64::Operand>(new X64::ValueOperand()); }

void
CodeGenX64::prependInst(X64::Instruction *inst)
{ (*current_block_iter)->inst_list.emplace_front(inst); }

void
CodeGenX64::gen(Instruction *inst)
{ assert(false); }

void
CodeGenX64::gen(SignedImmInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    prependInst(new X64::Mov(
        inst_result.at(inst),
        std::shared_ptr<X64::Operand>(new X64::ImmediateOperand(inst->getValue()))
    ));
}

void
CodeGenX64::gen(UnsignedImmInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    prependInst(new X64::Mov(
        inst_result.at(inst),
        std::shared_ptr<X64::Operand>(new X64::ImmediateOperand(inst->getValue()))
    ));
}

void
CodeGenX64::gen(GlobalInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    prependInst(new X64::LeaGlobal(
        inst_result.at(inst),
        std::shared_ptr<X64::Operand>(new X64::GlobalMemoryOperand(inst->getValue()))
    ));
}

void
CodeGenX64::gen(ArgInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    prependInst(new X64::LeaOffset(
        inst_result.at(inst),
        std::shared_ptr<X64::Operand>(new X64::RegisterOperand(X64::Register::RBP)),
        std::shared_ptr<X64::Operand>(new X64::ImmediateOperand(
            calculateArgumentOffset(static_cast<int>(inst->getValue()))
        ))
    ));
}

void
CodeGenX64::gen(AddInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    prependInst(new X64::Add(
        inst_result.at(inst),
        resolveOperand(inst->getRight())
    ));
    setOrMoveOperand(inst->getLeft(), inst_result.at(inst));
}

void
CodeGenX64::gen(SubInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    prependInst(new X64::Sub(
        inst_result.at(inst),
        resolveOperand(inst->getRight())
    ));
    setOrMoveOperand(inst->getLeft(), inst_result.at(inst));
}

void
CodeGenX64::gen(MulInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    prependInst(new X64::Imul(
        inst_result.at(inst),
        resolveOperand(inst->getRight())
    ));
    setOrMoveOperand(inst->getLeft(), inst_result.at(inst));
}

void
CodeGenX64::gen(DivInst *inst)
{ assert(false); }

void
CodeGenX64::gen(ModInst *inst)
{ assert(false); }

void
CodeGenX64::gen(ShlInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    prependInst(new X64::Sal(
        inst_result.at(inst),
        resolveOperand(inst->getRight())
    ));
    setOrMoveOperand(inst->getLeft(), inst_result.at(inst));
}

void
CodeGenX64::gen(ShrInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    prependInst(new X64::Shr(
        inst_result.at(inst),
        resolveOperand(inst->getRight())
    ));
    setOrMoveOperand(inst->getLeft(), inst_result.at(inst));
}

void
CodeGenX64::gen(OrInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    prependInst(new X64::Or(
        inst_result.at(inst),
        resolveOperand(inst->getRight())
    ));
    setOrMoveOperand(inst->getLeft(), inst_result.at(inst));
}

void
CodeGenX64::gen(AndInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    prependInst(new X64::And(
        inst_result.at(inst),
        resolveOperand(inst->getRight())
    ));
    setOrMoveOperand(inst->getLeft(), inst_result.at(inst));
}

void
CodeGenX64::gen(NorInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());

    prependInst(new X64::Not(inst_result.at(inst)));

    if (
        (
            inst->getRight()->is<SignedImmInst>() &&
            inst->getRight()->to<SignedImmInst>()->getValue() == 0
        ) ||
        (
            inst->getRight()->is<UnsignedImmInst>() &&
            inst->getRight()->to<UnsignedImmInst>()->getValue() == 0
        )
    ) {
        setOrMoveOperand(inst->getLeft(), inst_result.at(inst));
        inst->getRight()->unreference();
    }
    else {
        prependInst(new X64::Or(
            inst_result.at(inst),
            resolveOperand(inst->getRight())
        ));
        setOrMoveOperand(inst->getLeft(), inst_result.at(inst));
    }
}

void
CodeGenX64::gen(XorInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    prependInst(new X64::Xor(
        inst_result.at(inst),
        resolveOperand(inst->getRight())
    ));
    setOrMoveOperand(inst->getLeft(), inst_result.at(inst));
}

void
CodeGenX64::gen(SeqInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());

    if (inst_result.at(inst)) {
        prependInst(new X64::SetE(
            inst_result.at(inst)
        ));
    }
    prependInst(new X64::Cmp(
        resolveOperand(inst->getLeft()),
        resolveOperand(inst->getRight())
    ));
}

void
CodeGenX64::gen(SltInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());

    if (inst_result.at(inst)) {
        prependInst(new X64::SetE(
            inst_result.at(inst)
        ));
    }
    prependInst(new X64::Cmp(
        resolveOperand(inst->getLeft()),
        resolveOperand(inst->getRight())
    ));
}

void
CodeGenX64::gen(SleInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());

    if (inst_result.at(inst)) {
        prependInst(new X64::SetE(
            inst_result.at(inst)
        ));
    }
    prependInst(new X64::Cmp(
        resolveOperand(inst->getLeft()),
        resolveOperand(inst->getRight())
    ));
}

void
CodeGenX64::gen(LoadInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());

    prependInst(new X64::Mov(
        inst_result.at(inst),
        resolveMemory(inst->getAddress())
    ));
}

void
CodeGenX64::gen(StoreInst *inst)
{
    prependInst(new X64::Mov(
        resolveMemory(inst->getAddress()),
        resolveOperand(inst->getValue())
    ));
}

void
CodeGenX64::gen(AllocaInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());

    prependInst(new X64::LeaOffset(
        inst_result.at(inst),
        std::shared_ptr<X64::Operand>(new X64::RegisterOperand(X64::Register::RBP)),
        std::shared_ptr<X64::Operand>(new X64::ImmediateOperand(getAllocInstOffset(inst)))
    ));
}

void
CodeGenX64::gen(CallInst *inst)
{
    auto call_inst = new X64::Call(
        std::shared_ptr<X64::LabelOperand>(new X64::LabelOperand(inst->getFunction()->getName()))
    );

    prependInst(new X64::CallRestore(call_inst));
    prependInst(call_inst);

#define call_set_argument_register(__i, __r)                                    \
    if (inst_result.find(inst->getArgumentByIndex(__i)) != inst_result.end()) { \
        prependInst(new X64::Mov(                                               \
            std::shared_ptr<X64::Operand>(new X64::RegisterOperand(__r)),       \
            inst_result.at(inst->getArgumentByIndex(__i))                       \
        ));                                                                     \
    }                                                                           \
    else {                                                                      \
        inst_result.emplace(                                                    \
            inst->getArgumentByIndex(__i),                                      \
            std::shared_ptr<X64::Operand>(new X64::RegisterOperand(__r))        \
        );                                                                      \
    }                                                                           \
    inst_used[inst->getArgumentByIndex(__i)]++

    do {
        if (inst->arguments_size() == 0) { break; }
        call_set_argument_register(0, X64::Register::RDI);
        if (inst->arguments_size() == 1) { break; }
        call_set_argument_register(1, X64::Register::RSI);
        if (inst->arguments_size() == 2) { break; }
        call_set_argument_register(2, X64::Register::RDX);
        if (inst->arguments_size() == 3) { break; }
        call_set_argument_register(3, X64::Register::RCX);
        if (inst->arguments_size() == 4) { break; }
        call_set_argument_register(4, X64::Register::R8);
        if (inst->arguments_size() == 5) { break; }
        call_set_argument_register(5, X64::Register::R9);

        for (auto i = inst->arguments_size() - 1; i >= 6; --i) {
            if (inst_result.find(inst->getArgumentByIndex(i)) == inst_result.end()) {
                inst_result[inst->getArgumentByIndex(i)] = newValue();
            }
            prependInst(new X64::Push(inst_result[inst->getArgumentByIndex(i)]));
            inst_used[inst->getArgumentByIndex(i)]++;
        }
    } while (false);
    prependInst(new X64::CallPreserve(call_inst));

#undef call_set_argument_register

    prependInst(new X64::CallPreserve(call_inst));
}

void
CodeGenX64::gen(RetInst *inst)
{
    (*current_block_iter)->inst_list.clear();   // discard all instruction in current block

    // clear possible jumps
    (*current_block_iter)->ir_block->condition = nullptr;
    (*current_block_iter)->ir_block->then_block = (*current_block_iter)->ir_block->else_block = nullptr;

    // make outer function append the `ret` inst
    if (inst->getReturnValue()) {
        if (inst_result.find(inst->getReturnValue()) != inst_result.end()) {
            prependInst(new X64::Mov(
                std::shared_ptr<X64::Operand>(new X64::RegisterOperand(X64::Register::RAX)),
                inst_result.at(inst->getReturnValue())
            ));
        }
        else {
            inst_result[inst->getReturnValue()] =
                std::shared_ptr<X64::Operand>(new X64::RegisterOperand(X64::Register::RAX));
        }
    }
    inst_used[inst->getReturnValue()]++;
}

void
CodeGenX64::gen(PhiInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());

    for (auto &branch : *inst) {
        if (inst_result.find(branch.value) == inst_result.end()) {
            inst_result[branch.value] = inst_result.at(inst);
        }
        else {
            block_map[branch.preceder]->inst_list.emplace_back(new X64::Mov(
                inst_result.at(inst),
                inst_result.at(branch.value)
            ));
        }
        inst_used[branch.value]++;
    }
}

