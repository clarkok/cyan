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

struct StackMemoryOperand : public MemoryOperand
{
    int offset;

    StackMemoryOperand(int offset)
        : offset(offset)
    { }

    virtual std::string
    to_string() const
    { return "QWORD PTR [%rbp+" + std::to_string(offset * CYAN_PRODUCT_BITS / 8) + "]"; }
};

struct GlobalMemoryOperand : public MemoryOperand
{
    std::string name;

    GlobalMemoryOperand(std::string name)
        : name(name)
    { }

    virtual std::string
    to_string() const
    { return "OFFSET FLAT:" + name; }
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

struct Instrument
{
    virtual ~Instrument() = default;
    virtual std::string to_string() const = 0;

    static inline bool
    registerOperandIsRegister(Operand *operand, X64::Register reg)
    { return operand->is<RegisterOperand>() && operand->to<RegisterOperand>()->reg == reg; }

    virtual bool usedRegister(X64::Register reg) const = 0;
    virtual int registerSwapOutCost(const std::map<Register, int> &reg_usage, Register reg) const = 0;
    virtual void swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) = 0;
};

struct Mov : public Instrument
{
    std::unique_ptr<Operand> dst, src;

    Mov(Operand *dst, Operand *src)
        : dst(dst), src(src)
    { }

    virtual std::string
    to_string() const
    { return "mov " + dst->to_string() + ", " + src->to_string(); }

    virtual bool
    usedRegister(X64::Register reg) const
    {
        return registerOperandIsRegister(dst.get(), reg) ||
               registerOperandIsRegister(src.get(), reg);
    }

    virtual int
    registerSwapOutCost(const std::map<Register, int> &reg_usage, X64::Register reg) const
    {
        if (registerOperandIsRegister(src.get(), reg)) {
            if (reg_usage.at(reg) == 1) {
                return CodeGenX64::MEMORY_OPERATION_COST;
            }
            else {
                return 2 * CodeGenX64::MEMORY_OPERATION_COST;
            }
        }
        else {
            return 0;
        }
    }

    virtual void
    swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) {
        if (!registerOperandIsRegister(src.get(), reg)) { return; }
        if (reg_usage.at(reg) == 1) {
            src.reset(new StackMemoryOperand(stack_offset));
        }
        else {
            inst_list.emplace(
                position,
                new Mov(
                    new RegisterOperand(reg),
                    new StackMemoryOperand(stack_offset)
                )
            );
        }
    }
};

struct Add : public Instrument
{
    std::unique_ptr<Operand> dst, src;

    Add(Operand *dst, Operand *src)
        : dst(dst), src(src)
    { }

    virtual std::string
    to_string() const
    { return "add " + dst->to_string() + ", " + src->to_string(); };

    virtual bool
    usedRegister(X64::Register reg) const
    {
        return registerOperandIsRegister(dst.get(), reg) ||
               registerOperandIsRegister(src.get(), reg);
    }

    virtual int
    registerSwapOutCost(const std::map<Register, int> &reg_usage, X64::Register reg) const
    {
        if (registerOperandIsRegister(dst.get(), reg)) {
            return 2 * CodeGenX64::MEMORY_OPERATION_COST;
        }
        else if (registerOperandIsRegister(src.get(), reg)) {
            if (reg_usage.at(reg) == 1) {
                return CodeGenX64::MEMORY_OPERATION_COST;
            }
            else {
                return 2 * CodeGenX64::MEMORY_OPERATION_COST;
            }
        }
        else {
            return 0;
        }
    }

    virtual void
    swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) {
        if (registerOperandIsRegister(dst.get(), reg)) {
            inst_list.emplace(
                position,
                new Mov(
                    new RegisterOperand(reg),
                    new StackMemoryOperand(stack_offset)
                )
            );
        }
        else if (registerOperandIsRegister(src.get(), reg)) {
            if (reg_usage.at(reg) == 1) {
                src.reset(new StackMemoryOperand(stack_offset));
            }
            else {
                inst_list.emplace(
                    position,
                    new Mov(
                        new RegisterOperand(reg),
                        new StackMemoryOperand(stack_offset)
                    )
                );
            }
        }
    }
};

struct Call : public Instrument
{
    std::unique_ptr<LabelOperand> label;

    Call(LabelOperand *label)
        : label(label)
    { }

    virtual std::string
    to_string() const
    { return "call " + label->to_string(); }

    virtual bool
    usedRegister(X64::Register) const
    { return false; }

    virtual int
    registerSwapOutCost(const std::map<Register, int> &, X64::Register) const
    { return 0; }

    virtual void
    swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) { }
};

struct Cmp : public Instrument
{
    std::unique_ptr<Operand> left, right;

    Cmp(Operand *left, Operand *right)
        : left(left), right(right)
    { }

    virtual std::string
    to_string() const
    { return "cmp " + left->to_string() + ", " + right->to_string(); }

    virtual bool
    usedRegister(X64::Register reg) const
    {
        return registerOperandIsRegister(left.get(), reg) ||
               registerOperandIsRegister(right.get(), reg);
    }

    virtual int
    registerSwapOutCost(const std::map<Register, int> &reg_usage, X64::Register reg) const
    {
        if (registerOperandIsRegister(left.get(), reg)) {
            if (right->is<MemoryOperand>() || reg_usage.at(reg) != 1) {
                return 2 * CodeGenX64::MEMORY_OPERATION_COST;
            }
            else {
                return CodeGenX64::MEMORY_OPERATION_COST;
            }
        }
        else if (registerOperandIsRegister(right.get(), reg)) {
            if (left->is<MemoryOperand>() || reg_usage.at(reg) != 1) {
                return 2 * CodeGenX64::MEMORY_OPERATION_COST;
            }
            else {
                return CodeGenX64::MEMORY_OPERATION_COST;
            }
        }
        else {
            return 0;
        }
    }

    virtual void
    swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) {
        if (registerOperandIsRegister(left.get(), reg)) {
            if (right->is<MemoryOperand>() || reg_usage.at(reg) != 1) {
                inst_list.emplace(
                    position,
                    new Mov(
                        new RegisterOperand(reg),
                        new StackMemoryOperand(stack_offset)
                    )
                );
            }
            else {
                left.reset(new StackMemoryOperand(stack_offset));
            }
        }
        else if (registerOperandIsRegister(right.get(), reg)) {
            if (left->is<MemoryOperand>() || reg_usage.at(reg) != 1) {
                inst_list.emplace(
                    position,
                    new Mov(
                        new RegisterOperand(reg),
                        new StackMemoryOperand(stack_offset)
                    )
                );
            }
            else {
                right.reset(new StackMemoryOperand(stack_offset));
            }
        }
    }
};

struct Idiv : public Instrument
{
    std::unique_ptr<Operand> dst, src;

    Idiv(Operand *dst, Operand *src)
        : dst(dst), src(src)
    { }

    // TODO implement div instruction
    virtual std::string to_string() const = 0;
    virtual bool usedRegister(X64::Register reg) const = 0;
    virtual int registerSwapOutCost(const std::map<Register, int> &, X64::Register) const = 0;

    virtual void
    swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) = 0;
};

struct Imod : public Instrument
{
    std::unique_ptr<Operand> dst, src;

    Imod(Operand *dst, Operand *src)
        : dst(dst), src(src)
    { }

    // TODO implement mod instruction
    virtual std::string to_string() const = 0;
    virtual bool usedRegister(X64::Register reg) const = 0;
    virtual int registerSwapOutCost(const std::map<Register, int> &, X64::Register) const = 0;

    virtual void
    swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) = 0;
};

struct Jmp : public Instrument
{
    std::unique_ptr<LabelOperand> label;

    Jmp(LabelOperand *label)
        : label(label)
    { }

    virtual std::string
    to_string() const
    { return "jmp " + label->to_string(); }

    virtual bool
    usedRegister(X64::Register) const
    { return false; }

    virtual int
    registerSwapOutCost(const std::map<Register, int> &, X64::Register) const
    { return 0; }

    virtual void
    swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) { }
};

struct Je : public Instrument
{
    std::unique_ptr<LabelOperand> label;

    Je(LabelOperand *label)
        : label(label)
    { }

    virtual std::string
    to_string() const
    { return "je " + label->to_string(); }

    virtual bool
    usedRegister(X64::Register) const
    { return false; }

    virtual int
    registerSwapOutCost(const std::map<Register, int> &, X64::Register) const
    { return 0; }

    virtual void
    swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) { }
};

struct Jne : public Instrument
{
    std::unique_ptr<LabelOperand> label;

    Jne(LabelOperand *label)
        : label(label)
    { }

    virtual std::string
    to_string() const
    { return "jne " + label->to_string(); }

    virtual bool
    usedRegister(X64::Register) const
    { return false; }

    virtual int
    registerSwapOutCost(const std::map<Register, int> &, X64::Register) const
    { return 0; }

    virtual void
    swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) { }
};

struct Jg : public Instrument
{
    std::unique_ptr<LabelOperand> label;

    Jg(LabelOperand *label)
        : label(label)
    { }

    virtual std::string
    to_string() const
    { return "jg " + label->to_string(); }

    virtual bool
    usedRegister(X64::Register) const
    { return false; }

    virtual int
    registerSwapOutCost(const std::map<Register, int> &, X64::Register) const
    { return 0; }

    virtual void
    swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) { }
};

struct Jge : public Instrument
{
    std::unique_ptr<LabelOperand> label;

    Jge(LabelOperand *label)
        : label(label)
    { }

    virtual std::string
    to_string() const
    { return "jge " + label->to_string(); }

    virtual bool
    usedRegister(X64::Register) const
    { return false; }

    virtual int
    registerSwapOutCost(const std::map<Register, int> &, X64::Register) const
    { return 0; }

    virtual void
    swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) { }
};

struct Jl : public Instrument
{
    std::unique_ptr<LabelOperand> label;

    Jl(LabelOperand *label)
        : label(label)
    { }

    virtual std::string
    to_string() const
    { return "jl " + label->to_string(); }

    virtual bool
    usedRegister(X64::Register) const
    { return false; }

    virtual int
    registerSwapOutCost(const std::map<Register, int> &, X64::Register) const
    { return 0; }

    virtual void
    swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) { }
};

struct Jle : public Instrument
{
    std::unique_ptr<LabelOperand> label;

    Jle(LabelOperand *label)
        : label(label)
    { }

    virtual std::string
    to_string() const
    { return "jle " + label->to_string(); }

    virtual bool
    usedRegister(X64::Register) const
    { return false; }

    virtual int
    registerSwapOutCost(const std::map<Register, int> &, X64::Register) const
    { return 0; }

    virtual void
    swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) { }
};

struct Lea : public Instrument
{
    std::unique_ptr<RegisterOperand> dst, base;
    std::unique_ptr<ImmediateOperand> offset;

    Lea(RegisterOperand *dst, RegisterOperand *base, ImmediateOperand *offset)
        : dst(dst), base(base), offset(offset)
    { }

    virtual std::string
    to_string() const
    {
        return "lea " + dst->to_string() + ", [" +
            base->to_string() + (offset->value >= 0 ? "+" : "") +
            offset->to_string() + "]";
    }

    virtual bool
    usedRegister(X64::Register reg) const
    {
        return (dst->reg == reg) ||
               (base->reg == reg);
    }

    virtual int
    registerSwapOutCost(const std::map<Register, int> &, X64::Register reg) const
    {
        if (base->reg == reg) {
            return 2 * CodeGenX64::MEMORY_OPERATION_COST;
        }
        else {
            return 0;
        }
    }

    virtual void
    swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) {
        if (base->reg == reg) {
            inst_list.emplace(
                position,
                new Mov(
                    new RegisterOperand(reg),
                    new StackMemoryOperand(stack_offset)
                )
            );
        }
    }
};

struct Neg : public Instrument
{
    std::unique_ptr<Operand> dst;

    Neg(Operand *dst)
        : dst(dst)
    { }

    virtual std::string
    to_string() const
    { return "neg " + dst->to_string(); }

    virtual bool
    usedRegister(X64::Register reg) const
    { return registerOperandIsRegister(dst.get(), reg); }

    virtual int
    registerSwapOutCost(const std::map<Register, int> &, X64::Register reg) const
    {
        if (registerOperandIsRegister(dst.get(), reg)) {
            return 2 * CodeGenX64::MEMORY_OPERATION_COST;
        }
        else {
            return 0;
        }
    }

    virtual void
    swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) {
        if (registerOperandIsRegister(dst.get(), reg)) {
            inst_list.emplace(
                position,
                new Mov(
                    new RegisterOperand(reg),
                    new StackMemoryOperand(stack_offset)
                )
            );
        }
    }
};

struct Pop : public Instrument
{
    std::unique_ptr<Operand> dst;

    Pop(Operand *dst)
        : dst(dst)
    { }

    virtual std::string
    to_string() const
    { return "pop " + dst->to_string(); }

    virtual bool
    usedRegister(X64::Register reg) const
    { return registerOperandIsRegister(dst.get(), reg); }

    virtual int
    registerSwapOutCost(const std::map<Register, int> &, X64::Register) const
    { return 0; }

    virtual void
    swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) { }
};

struct Push : public Instrument
{
    std::unique_ptr<Operand> src;

    Push(Operand *src)
        : src(src)
    { }

    virtual std::string
    to_string() const
    { return "push " + src->to_string(); }

    virtual bool
    usedRegister(X64::Register reg) const
    { return registerOperandIsRegister(src.get(), reg); }

    virtual int
    registerSwapOutCost(const std::map<Register, int> &reg_usage, X64::Register reg) const
    {
        if (registerOperandIsRegister(src.get(), reg)) {
            if (reg_usage.at(reg) == 1) {
                return CodeGenX64::MEMORY_OPERATION_COST;
            }
            else {
                return 2 * CodeGenX64::MEMORY_OPERATION_COST;
            }
        }
        else {
            return 0;
        }
    }

    virtual void
    swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) {
        if (registerOperandIsRegister(src.get(), reg)) {
            if (reg_usage.at(reg) == 1) {
                src.reset(new StackMemoryOperand(stack_offset));
            }
            else {
                inst_list.emplace(
                    position,
                    new Mov(
                        new RegisterOperand(reg),
                        new StackMemoryOperand(stack_offset)
                    )
                );
            }
        }
    }
};

struct Ret : public Instrument
{
    virtual std::string
    to_string() const
    { return "ret"; }

    virtual bool
    usedRegister(X64::Register reg) const
    { return false; }

    virtual int
    registerSwapOutCost(const std::map<Register, int> &, X64::Register) const
    { return 0; }

    virtual void
    swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) { }
};

struct Sal : public Instrument
{
    std::unique_ptr<Operand> dst, src;

    Sal(Operand *dst, Operand *src)
        : dst(dst), src(src)
    { }

    virtual std::string
    to_string() const
    { return "sal " + dst->to_string() + ", " + src->to_string(); }

    virtual bool
    usedRegister(X64::Register reg) const
    {
        return registerOperandIsRegister(dst.get(), reg) ||
               registerOperandIsRegister(src.get(), reg);
    }

    virtual int
    registerSwapOutCost(const std::map<Register, int> &reg_usage, X64::Register reg) const
    {
        if (registerOperandIsRegister(dst.get(), reg)) {
            return 2 * CodeGenX64::MEMORY_OPERATION_COST;
        }
        else if (registerOperandIsRegister(src.get(), reg)) {
            if (reg_usage.at(reg) == 1) {
                return CodeGenX64::MEMORY_OPERATION_COST;
            }
            else {
                return 2 * CodeGenX64::MEMORY_OPERATION_COST;
            }
        }
        else {
            return 0;
        }
    }

    virtual void
    swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) {
        if (registerOperandIsRegister(dst.get(), reg)) {
            inst_list.emplace(
                position,
                new Mov(
                    new RegisterOperand(reg),
                    new StackMemoryOperand(stack_offset)
                )
            );
        }
        else if (registerOperandIsRegister(src.get(), reg)) {
            if (reg_usage.at(reg) == 1) {
                src.reset(new StackMemoryOperand(stack_offset));
            }
            else {
                inst_list.emplace(
                    position,
                    new Mov(
                        new RegisterOperand(reg),
                        new StackMemoryOperand(stack_offset)
                    )
                );
            }
        }
    }
};

struct Sar : public Instrument
{
    std::unique_ptr<Operand> dst, src;

    Sar(Operand *dst, Operand *src)
        : dst(dst), src(src)
    { }

    virtual std::string
    to_string() const
    { return "sar " + dst->to_string() + ", " + src->to_string(); }

    virtual bool
    usedRegister(X64::Register reg) const
    {
        return (dst->is<RegisterOperand>() && dst->to<RegisterOperand>()->reg == reg) ||
               (src->is<RegisterOperand>() && src->to<RegisterOperand>()->reg == reg);
    }

    virtual int
    registerSwapOutCost(const std::map<Register, int> &reg_usage, X64::Register reg) const
    {
        if (registerOperandIsRegister(dst.get(), reg)) {
            return 2 * CodeGenX64::MEMORY_OPERATION_COST;
        }
        else if (registerOperandIsRegister(src.get(), reg)) {
            if (reg_usage.at(reg) == 1) {
                return CodeGenX64::MEMORY_OPERATION_COST;
            }
            else {
                return 2 * CodeGenX64::MEMORY_OPERATION_COST;
            }
        }
        else {
            return 0;
        }
    }

    virtual void
    swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) {
        if (registerOperandIsRegister(dst.get(), reg)) {
            inst_list.emplace(
                position,
                new Mov(
                    new RegisterOperand(reg),
                    new StackMemoryOperand(stack_offset)
                )
            );
        }
        else if (registerOperandIsRegister(src.get(), reg)) {
            if (reg_usage.at(reg) == 1) {
                src.reset(new StackMemoryOperand(stack_offset));
            }
            else {
                inst_list.emplace(
                    position,
                    new Mov(
                        new RegisterOperand(reg),
                        new StackMemoryOperand(stack_offset)
                    )
                );
            }
        }
    }
};

struct Shr : public Instrument
{
    std::unique_ptr<Operand> dst, src;

    Shr(Operand *dst, Operand *src)
        : dst(dst), src(src)
    { }

    virtual std::string
    to_string() const
    { return "sar " + dst->to_string() + ", " + src->to_string(); }

    virtual bool
    usedRegister(X64::Register reg) const
    {
        return (dst->is<RegisterOperand>() && dst->to<RegisterOperand>()->reg == reg) ||
               (src->is<RegisterOperand>() && src->to<RegisterOperand>()->reg == reg);
    }

    virtual int
    registerSwapOutCost(const std::map<Register, int> &reg_usage, X64::Register reg) const
    {
        if (registerOperandIsRegister(dst.get(), reg)) {
            return 2 * CodeGenX64::MEMORY_OPERATION_COST;
        }
        else if (registerOperandIsRegister(src.get(), reg)) {
            if (reg_usage.at(reg) == 1) {
                return CodeGenX64::MEMORY_OPERATION_COST;
            }
            else {
                return 2 * CodeGenX64::MEMORY_OPERATION_COST;
            }
        }
        else {
            return 0;
        }
    }

    virtual void
    swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) {
        if (registerOperandIsRegister(dst.get(), reg)) {
            inst_list.emplace(
                position,
                new Mov(
                    new RegisterOperand(reg),
                    new StackMemoryOperand(stack_offset)
                )
            );
        }
        else if (registerOperandIsRegister(src.get(), reg)) {
            if (reg_usage.at(reg) == 1) {
                src.reset(new StackMemoryOperand(stack_offset));
            }
            else {
                inst_list.emplace(
                    position,
                    new Mov(
                        new RegisterOperand(reg),
                        new StackMemoryOperand(stack_offset)
                    )
                );
            }
        }
    }
};

struct Sub : public Instrument
{
    std::unique_ptr<Operand> dst, src;

    Sub(Operand *dst, Operand *src)
        : dst(dst), src(src)
    { }

    virtual std::string
    to_string() const
    { return "sub " + dst->to_string() + ", " + src->to_string(); }

    virtual bool
    usedRegister(X64::Register reg) const
    {
        return (dst->is<RegisterOperand>() && dst->to<RegisterOperand>()->reg == reg) ||
               (src->is<RegisterOperand>() && src->to<RegisterOperand>()->reg == reg);
    }

    virtual int
    registerSwapOutCost(const std::map<Register, int> &reg_usage, X64::Register reg) const
    {
        if (registerOperandIsRegister(dst.get(), reg)) {
            return 2 * CodeGenX64::MEMORY_OPERATION_COST;
        }
        else if (registerOperandIsRegister(src.get(), reg)) {
            if (reg_usage.at(reg) == 1) {
                return CodeGenX64::MEMORY_OPERATION_COST;
            }
            else {
                return 2 * CodeGenX64::MEMORY_OPERATION_COST;
            }
        }
        else {
            return 0;
        }
    }

    virtual void
    swapOutRegister(
        const std::map<Register, int> &reg_usage,
        Register reg,
        int stack_offset,
        std::list<std::unique_ptr<Instrument> > &inst_list,
        std::list<std::unique_ptr<Instrument> >::iterator position
    ) {
        if (registerOperandIsRegister(dst.get(), reg)) {
            inst_list.emplace(
                position,
                new Mov(
                    new RegisterOperand(reg),
                    new StackMemoryOperand(stack_offset)
                )
            );
        }
        else if (registerOperandIsRegister(src.get(), reg)) {
            if (reg_usage.at(reg) == 1) {
                src.reset(new StackMemoryOperand(stack_offset));
            }
            else {
                inst_list.emplace(
                    position,
                    new Mov(
                        new RegisterOperand(reg),
                        new StackMemoryOperand(stack_offset)
                    )
                );
            }
        }
    }
};

struct Block
{
    std::string name;
    std::list<std::unique_ptr<Instrument> > inst_list;

    Block(std::string name)
        : name(name)
    { }
};

}

std::ostream &
CodeGenX64::generate(std::ostream &os)
{
    os << ".intel_syntax" << std::endl;

    os << ".data" << std::endl;
    for (auto &global : ir->global_defines) {
        os << global.first << ":\t.quad 0" << std::endl;
    }

    os << std::endl << ".text" << std::endl;
    for (auto &func : ir->function_table) {
        os << ".globl " << func.first << "\n"
           << ".type " << func.first << " @function"
           << func.first << ":" << std::endl;

        generateFunc(os, func.second.get());

        os << func.first << ".end:\n"
           << ".size " << func.first << " .-" << func.first << "\n" << std::endl;
    }

    return os;
}

std::ostream &
CodeGenX64::generateFunc(std::ostream &os, Function *func)
{
    reg_usage.clear();
    block_map.clear();
    block_map_r.clear();
    block_list.clear();
    argument_stacked.reserve(6);
    std::fill(argument_stacked.begin(), argument_stacked.end(), 0);
    function_slot_count = 0;

    for (auto &block : func->block_list) {
        block_list.emplace_back(new X64::Block(block->getName()));

        block_map.emplace(block.get(), block_list.back().get());
        block_map_r.emplace(block_list.back().get(), block.get());
    }

    current_block = block_list.rbegin().base();
    for (
        auto block_iter = func->block_list.rbegin();
        block_iter != func->block_list.rend();
        ++block_iter
    ) {
        for (
            auto inst_iter = (*block_iter)->inst_list.rbegin();
            inst_iter != (*block_iter)->inst_list.rend();
            ++inst_iter
        ) {
            if ((*inst_iter)->getReferencedCount()) {
                gen(inst_iter->get());
            }
        }
        --current_block;
    }

    return os;
}

int
CodeGenX64::allocateStackSlot()
{ return static_cast<int>(((++function_slot_count) + 2) * -(CYAN_PRODUCT_BITS / 8)); }

int
CodeGenX64::calculateArgumentOffset(intptr_t argument)
{
    if (!argument_stacked[argument]) {
        argument_stacked[argument] = allocateStackSlot();
    }
    return argument_stacked[argument];
}

size_t
CodeGenX64::calculateRegisterCost(X64::Register reg)
{
    if (reg_usage.find(reg) == reg_usage.end()) { return 0; }

    size_t distance = 0;
    for (
        auto block_iter = current_block;
        block_iter != block_list.end();
        ++block_iter
    ) {
        for (auto &inst_ptr : (*block_iter)->inst_list) {
            ++distance;
            if (inst_ptr->usedRegister(reg)) {
                return distance +
                    ((inst_ptr->registerSwapOutCost(reg_usage, reg) + CodeGenX64::MEMORY_OPERATION_COST) <<
                        (block_map_r[block_iter->get()]->getDepth() * 2 + 16));
            }
        }
    }
    return 0;
}

X64::Register
CodeGenX64::findCheapestRegister()
{
    auto ret = X64::Register::RAX;
    auto cost = calculateRegisterCost(ret);

    register_foreach(r) {
        auto r_cost = calculateRegisterCost(r);
        if (r_cost < cost) {
            ret = r;
            cost = r_cost;
        }
    }

    return ret;
}

void
CodeGenX64::swapOutRegister(X64::Register reg)
{
    int stack_offset = allocateStackSlot();
    (*current_block)->inst_list.emplace_front(
        new X64::Mov(
            new X64::StackMemoryOperand(stack_offset),
            new X64::RegisterOperand(reg)
        )
    );

    for (
        auto block_iter = current_block;
        block_iter != block_list.end();
        ++block_iter
    ) {
        for (
            auto inst_iter = (*block_iter)->inst_list.begin();
            inst_iter != (*block_iter)->inst_list.end();
            ++inst_iter
        ) {
            if ((*inst_iter)->usedRegister(reg)) {
                (*inst_iter)->swapOutRegister(
                    reg_usage,
                    reg,
                    stack_offset,
                    (*block_iter)->inst_list,
                    inst_iter
                );
                return;
            }
        }
    }
}

X64::Register
CodeGenX64::allocateRegister()
{
    if (reg_usage.size() == GENERAL_PURPOSE_REGISTER_NR) {
        auto register_to_replace = findCheapestRegister();
        swapOutRegister(register_to_replace);
        reg_usage.erase(register_to_replace);

        return register_to_replace;
    }

    register_foreach(r) {
        if (reg_usage.find(r) == reg_usage.end()) {
            return r;
        }
    }

    assert(false);
}

void
CodeGenX64::gen(Instrument *inst)
{ assert(false); }

void
CodeGenX64::gen(SignedImmInst *inst)
{
    assert(inst_reg.find(inst) != inst_reg.end());
    (*current_block)->inst_list.emplace_front(
        new X64::Mov(
            new X64::RegisterOperand(inst_reg[inst]),
            new X64::ImmediateOperand(inst->getValue())
        )
    );
}

void
CodeGenX64::gen(UnsignedImmInst *inst)
{
    assert(inst_reg.find(inst) != inst_reg.end());
    (*current_block)->inst_list.emplace_front(
        new X64::Mov(
            new X64::RegisterOperand(inst_reg[inst]),
            new X64::ImmediateOperand(inst->getValue())
        )
    );
}

void
CodeGenX64::gen(GlobalInst *inst)
{
    assert(inst_reg.find(inst) != inst_reg.end());
    (*current_block)->inst_list.emplace_front(
        new X64::Mov(
            new X64::RegisterOperand(inst_reg[inst]),
            new X64::LabelOperand(inst->getValue())
        )
    );
}

void
CodeGenX64::gen(ArgInst *inst)
{
    assert(inst_reg.find(inst) != inst_reg.end());
    (*current_block)->inst_list.emplace_front(
        new X64::Lea(
            new X64::RegisterOperand(inst_reg[inst]),
            new X64::RegisterOperand(X64::Register::RBP),
            new X64::ImmediateOperand(calculateArgumentOffset(inst->getValue()))
        )
    );
}

void
CodeGenX64::gen(AddInst *inst)
{ }

void
CodeGenX64::gen(SubInst *inst)
{ }

void
CodeGenX64::gen(MulInst *inst)
{ }

void
CodeGenX64::gen(DivInst *inst)
{ }

void
CodeGenX64::gen(ModInst *inst)
{ }

void
CodeGenX64::gen(ShlInst *inst)
{ }

void
CodeGenX64::gen(ShrInst *inst)
{ }

void
CodeGenX64::gen(OrInst *inst)
{ }

void
CodeGenX64::gen(AndInst *inst)
{ }

void
CodeGenX64::gen(NorInst *inst)
{ }

void
CodeGenX64::gen(XorInst *inst)
{ }

void
CodeGenX64::gen(SeqInst *inst)
{ }

void
CodeGenX64::gen(SltInst *inst)
{ }

void
CodeGenX64::gen(SleInst *inst)
{ }

void
CodeGenX64::gen(LoadInst *inst)
{ }

void
CodeGenX64::gen(StoreInst *inst)
{ }

void
CodeGenX64::gen(AllocaInst *inst)
{ }

void
CodeGenX64::gen(CallInst *inst)
{ }

void
CodeGenX64::gen(RetInst *inst)
{ }

void
CodeGenX64::gen(PhiInst *inst)
{ }

