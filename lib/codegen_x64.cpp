//
// Created by c on 5/10/16.
//

#include <list>
#include <memory>
#include <set>
#include <map>
#include <cctype>

#include "codegen_x64.hpp"
#include "optimizer.hpp"
#include "dead_code_eliminater.hpp"

using namespace cyan;

namespace X64 {

enum class Register
{
    RBX,
    RCX,
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
    RSP,
    RAX,    // treat RAX,RDX as a reserved register
    RDX
};

#define GP_REG_START    ::X64::Register::RBX
#define GP_REG_END      ::X64::Register::RBP

inline Register
next(Register reg)
{ return static_cast<Register>(static_cast<std::underlying_type<Register>::type>(reg) + 1); }

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
    assert(false);
}

std::string
escape_string(std::string content)
{
    std::string ret = "\"";
    for (auto &ch : content) {
        if (std::isprint(ch)) {
            ret.push_back(ch);
        }
        else {
            ret.push_back('\\');
            switch (ch) {
                case '\t':  ret.push_back('t'); break;
                case '\f':  ret.push_back('f'); break;
                case '\v':  ret.push_back('v'); break;
                case '\n':  ret.push_back('n'); break;
                case '\r':  ret.push_back('r'); break;
                case '\\':  ret.push_back('\\'); break;
                default:
                    ret.push_back('x');
                    ret.push_back("0123456789ABCDEF"[ch >> 4]);
                    ret.push_back("0123456789ABCDEF"[ch & 0xF]);
                    break;
            }
        }
    }
    ret.push_back('\"');
    return ret;
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
    std::shared_ptr<Operand> base;
    intptr_t offset;

    OffsetMemoryOperand(std::shared_ptr<Operand> base, intptr_t offset)
        : base(base), offset(offset)
    { }

    virtual std::string
    to_string() const
    {
        return "QWORD PTR [" + base->to_string() + (offset >= 0 ? "+" : "") + std::to_string(offset) + "]";
    }
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
    { return CodeGenX64::escapeAsmName(name); }
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

struct Label : public Instruction
{
    std::string name;

    Label(std::string name)
        : name(name)
    { }

    virtual std::string
    to_string() const
    { return "\n" + CodeGenX64::escapeAsmName(name) + ":"; }

    virtual void registerAllocate(cyan::CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    {
        if (dst->is<MemoryOperand>() && src->is<MemoryOperand>()) {
            list.emplace(
                iter,
                new X64::Mov(temp_reg, src)
            );
            src = temp_reg;
        }
    }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    {
        if (dst->is<MemoryOperand>() && src->is<MemoryOperand>()) {
            list.emplace(
                iter,
                new X64::Mov(temp_reg, src)
            );
            src = temp_reg;
        }
    }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    {
        if (dst->is<MemoryOperand>() && src->is<MemoryOperand>()) {
            list.emplace(
                iter,
                new X64::Mov(temp_reg, src)
            );
            src = temp_reg;
        }
    }
};

struct Call : public Instruction
{
    std::shared_ptr<Operand> func, rax;
    std::list<X64::Register> saved_registers;

    Call(std::shared_ptr<Operand> func, std::shared_ptr<Operand> rax)
        : func(func), rax(rax)
    { }

    virtual std::string
    to_string() const
    { return "call " + func->to_string(); }

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    {
        if (left->is<MemoryOperand>() && right->is<MemoryOperand>()) {
            list.emplace(
                iter,
                new X64::Mov(temp_reg, right)
            );
            right = temp_reg;
        }
    }
};

struct Idiv : public Instruction
{
    std::shared_ptr<Operand> dst, src;

    Idiv(std::shared_ptr<Operand> dst, std::shared_ptr<Operand> src)
        : dst(dst), src(src)
    { }

    // TODO implement div instruction
    virtual std::string to_string() const = 0;

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    {
        if (dst->is<MemoryOperand>() && src->is<MemoryOperand>()) {
            list.emplace(
                iter,
                new X64::Mov(temp_reg, src)
            );
            src = temp_reg;
        }
    }
};

struct Imod : public Instruction
{
    std::shared_ptr<Operand> dst, src;

    Imod(std::shared_ptr<Operand> dst, std::shared_ptr<Operand> src)
        : dst(dst), src(src)
    { }

    // TODO implement mod instruction
    virtual std::string to_string() const = 0;

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    {
        if (dst->is<MemoryOperand>() && src->is<MemoryOperand>()) {
            list.emplace(
                iter,
                new X64::Mov(temp_reg, src)
            );
            src = temp_reg;
        }
    }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    {
        if (dst->is<MemoryOperand>() && src->is<MemoryOperand>()) {
            list.emplace(
                iter,
                new X64::Mov(temp_reg, src)
            );
            src = temp_reg;
        }
    }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    {
        if (dst->is<MemoryOperand>() && src->is<MemoryOperand>()) {
            list.emplace(
                iter,
                new X64::Mov(temp_reg, src)
            );
            src = temp_reg;
        }
    }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
};

struct Ret : public Instruction
{
    std::shared_ptr<Operand> rax;

    Ret(std::shared_ptr<Operand> rax)
        : rax(rax)
    { }

    virtual std::string
    to_string() const
    { return "ret"; }

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    { }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    {
        if (dst->is<MemoryOperand>() && src->is<MemoryOperand>()) {
            list.emplace(
                iter,
                new X64::Mov(temp_reg, src)
            );
            src = temp_reg;
        }
    }
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

    virtual void registerAllocate(CodeGenX64 *codegen) { codegen->registerAllocate(this); }
    virtual void
    resolveTooManyMemoryLocations(InstList &list, InstIterator iter, SharedOperand &temp_reg)
    {
        if (dst->is<MemoryOperand>() && src->is<MemoryOperand>()) {
            list.emplace(
                iter,
                new X64::Mov(temp_reg, src)
            );
            src = temp_reg;
        }
    }
};

class ResortSwappableOperand : public Optimizer
{
public:
    ResortSwappableOperand(IR *_ir)
        : Optimizer(_ir)
    {
        for (auto &func_pair : ir->function_table) {
            auto func = func_pair.second.get();
            for (auto &block_ptr : func->block_list) {
                for (auto &inst_ptr : block_ptr->inst_list) {
                    if (
                        inst_ptr->is<AddInst>() ||
                        inst_ptr->is<MulInst>() ||
                        inst_ptr->is<OrInst>() ||
                        inst_ptr->is<AndInst>() ||
                        inst_ptr->is<NorInst>() ||
                        inst_ptr->is<XorInst>() ||
                        inst_ptr->is<SeqInst>()
                    ) {
                        auto binary_inst = inst_ptr->to<BinaryInst>();
                        if (binary_inst->getLeft()->is<ImmediateInst>()) {
                            auto t = binary_inst->getLeft();
                            binary_inst->setLeft(binary_inst->getRight());
                            binary_inst->setRight(t);
                        }

                        if (
                            binary_inst->getRight()->getType()->is<ArrayType>() ||
                            binary_inst->getRight()->getType()->is<PointerType>() ||
                            binary_inst->getRight()->getType()->is<StructType>() ||
                            binary_inst->getRight()->getType()->is<ConceptType>() ||
                            binary_inst->getRight()->getType()->is<FunctionType>()
                        ) {
                            auto t = binary_inst->getLeft();
                            binary_inst->setLeft(binary_inst->getRight());
                            binary_inst->setRight(t);
                        }
                    }
                }
            }
        }
    }
};

class ResolvePointerArithmetic : public Optimizer
{
public:
    ResolvePointerArithmetic(IR *_ir)
        : Optimizer(_ir)
    {
        for (auto &func_pair : ir->function_table) {
            auto func = func_pair.second.get();
            for (auto &block_ptr : func->block_list) {
                for (
                    auto inst_iter = block_ptr->inst_list.begin();
                    inst_iter != block_ptr->inst_list.end();
                    ++inst_iter
                ) {
                    if ((*inst_iter)->is<AddInst>()) {
                        auto add_inst = (*inst_iter)->to<AddInst>();
                        if (
                            add_inst->getLeft()->getType()->is<PointerType>() ||
                            add_inst->getLeft()->getType()->is<ConceptType>() ||
                            add_inst->getLeft()->getType()->is<StructType>() ||
                            add_inst->getLeft()->getType()->is<VTableType>()
                        ) {
                            if (add_inst->getRight()->is<SignedImmInst>()) {
                                block_ptr->inst_list.emplace(
                                    inst_iter,
                                    add_inst->setRight(
                                        new SignedImmInst(
                                            ir->type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS),
                                            add_inst->getRight()->to<SignedImmInst>()->getValue() * 8,
                                            block_ptr.get(),
                                            "$" + std::to_string(
                                                add_inst->getRight()->to<SignedImmInst>()->getValue() * 8)
                                        )
                                    )
                                );
                            }
                            else if (add_inst->getRight()->is<UnsignedImmInst>()) {
                                block_ptr->inst_list.emplace(
                                    inst_iter,
                                    add_inst->setRight(
                                        new UnsignedImmInst(
                                            ir->type_pool->getUnsignedIntegerType(CYAN_PRODUCT_BITS),
                                            add_inst->getRight()->to<UnsignedImmInst>()->getValue() * 8,
                                            block_ptr.get(),
                                            "$" + std::to_string(
                                                add_inst->getRight()->to<UnsignedImmInst>()->getValue() * 8)
                                        )
                                    )
                                );
                            }
                            else {
                                auto imm_inst = new SignedImmInst(
                                    ir->type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS),
                                    8,
                                    block_ptr.get(),
                                    "$8"
                                );
                                block_ptr->inst_list.emplace(
                                    inst_iter,
                                    imm_inst
                                );
                                block_ptr->inst_list.emplace(
                                    inst_iter,
                                    add_inst->setRight(
                                        new MulInst(
                                            ir->type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS),
                                            add_inst->getRight(),
                                            imm_inst,
                                            block_ptr.get(),
                                            "_" + std::to_string(func->countLocalTemp())
                                        )
                                    )
                                );
                            }
                        }
                    }
                    else if ((*inst_iter)->is<SubInst>()) {
                        auto sub_inst = (*inst_iter)->to<SubInst>();
                        if (
                            sub_inst->getLeft()->getType()->is<PointerType>() ||
                            sub_inst->getLeft()->getType()->is<ConceptType>() ||
                            sub_inst->getLeft()->getType()->is<StructType>() ||
                            sub_inst->getLeft()->getType()->is<VTableType>()
                        ) {
                            if (sub_inst->getRight()->is<SignedImmInst>()) {
                                block_ptr->inst_list.emplace(
                                    inst_iter,
                                    sub_inst->setRight(
                                        new SignedImmInst(
                                            ir->type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS),
                                            sub_inst->getRight()->to<SignedImmInst>()->getValue() * 8,
                                            block_ptr.get(),
                                            "$" + std::to_string(
                                                sub_inst->getRight()->to<SignedImmInst>()->getValue() * 8)
                                        )
                                    )
                                );
                            }
                            else if (sub_inst->getRight()->is<UnsignedImmInst>()) {
                                block_ptr->inst_list.emplace(
                                    inst_iter,
                                    sub_inst->setRight(
                                        new UnsignedImmInst(
                                            ir->type_pool->getUnsignedIntegerType(CYAN_PRODUCT_BITS),
                                            sub_inst->getRight()->to<UnsignedImmInst>()->getValue() * 8,
                                            block_ptr.get(),
                                            "$" + std::to_string(
                                                sub_inst->getRight()->to<UnsignedImmInst>()->getValue() * 8)
                                        )
                                    )
                                );
                            }
                            else {
                                assert(
                                    sub_inst->getRight()->getType()->is<SignedIntegerType>() ||
                                    sub_inst->getRight()->getType()->is<UnsignedIntegerType>()
                                );
                                auto imm_inst = new SignedImmInst(
                                    ir->type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS),
                                    8,
                                    block_ptr.get(),
                                    "$8"
                                );
                                block_ptr->inst_list.emplace(
                                    inst_iter,
                                    imm_inst
                                );
                                block_ptr->inst_list.emplace(
                                    inst_iter,
                                    sub_inst->setRight(
                                        new MulInst(
                                            ir->type_pool->getSignedIntegerType(CYAN_PRODUCT_BITS),
                                            sub_inst->getRight(),
                                            imm_inst,
                                            block_ptr.get(),
                                            "_" + std::to_string(func->countLocalTemp())
                                        )
                                    )
                                );
                            }
                        }
                    }
                }
            }
        }
    }
};

}

std::string
CodeGenX64::escapeAsmName(std::string original)
{
    std::string ret;

    for (auto ch : original) {
        if (std::isalnum(ch) || ch == '_') {
            ret.push_back(ch);
        }
        else if (ch == '.') {
            ret.push_back('_');
        }
        else {
            ret.push_back('$');
        }
    }

    return ret;
}

std::ostream &
CodeGenX64::generate(std::ostream &os)
{
    ir.reset(
        X64::ResolvePointerArithmetic(
            X64::ResortSwappableOperand(ir.release()).release()
        ).release()
    );

    // ir->output(std::cout);

    os << ".intel_syntax" << std::endl;

    if (ir->global_defines.size()) {
        os << "\n.data" << std::endl;
        for (auto &global : ir->global_defines) {
            os << "\t" << escapeAsmName(global.first) << ":\t.quad 0" << std::endl;
        }
        os << std::endl;
    }

    if (ir->string_pool.size()) {
        os << "\n.section .rodata" << std::endl;
        for (auto &string_pair : ir->string_pool) {
            os << "\t" << string_pair.second << ":\t.asciz " <<
            X64::escape_string(string_pair.first) << std::endl;
        }
    }

    ir->type_pool->foreachCastedStructType([&os](CastedStructType *casted) {
        os << "\n.section .rodata" << std::endl << escapeAsmName(casted->to_string() + "__vtable") << ":\n";
        for (auto &method : *casted) {
            os << "\t.quad " << escapeAsmName(method.impl->name) << "\n";
        }
    });

    os << std::endl << ".text" << std::endl;
    for (auto &func : ir->function_table) {
        current_func = func.second.get();
        auto func_name = escapeAsmName(func.first);

        os << "\t.globl " << func_name << "\n"
           << "\t.type " << func_name << " @function\n"
           << func_name << ":" << std::endl;

        generateFunc(func.second.get());
        allocateRegisters();

        writeFunctionHeader(func.second.get(), os);

        for (auto &inst_ptr : inst_list) {
            os << "\t" << inst_ptr->to_string() << "\n";
        }

        writeFunctionFooter(func.second.get(), os);

        os << func_name << ".end:\n"
           << "\t.size " << func_name << ", .-"
           << func_name << "\n" << std::endl;
    }

    return os;
}

void
CodeGenX64::generateFunc(Function *func)
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

    for (auto &block_ptr : func->block_list) {
        for (auto &inst_ptr : block_ptr->inst_list) {
            if (
                inst_ptr->is<StoreInst>() ||
                inst_ptr->is<DeleteInst>() ||
                inst_ptr->is<RetInst>()
                ) {
                inst_ptr->codegen(this);
            }
            else if (inst_ptr->is<CallInst>()) {
                if (inst_result.find(inst_ptr.get()) == inst_result.end()) {
                    inst_result.emplace(
                        inst_ptr.get(),
                        newValue()
                    );
                }
                inst_ptr->codegen(this);
            }
        }
    }

    for (
        auto block_iter = func->block_list.begin();
        block_iter != func->block_list.end();
        ++block_iter
    ) {
        auto block_ptr = block_iter->get();
#define tail_condition_jump(jump_true, jump_false)                                  \
        if (block_ptr->then_block != std::next(block_iter)->get()) {                \
            block_map[block_ptr]->inst_list.emplace_back(new jump_true(             \
                std::make_shared<X64::LabelOperand>(                                \
                    func->getName() + "." + block_ptr->then_block->getName()        \
                )                                                                   \
            ));                                                                     \
            if (block_ptr->else_block != std::next(block_iter)->get()) {            \
                block_map[block_ptr]->inst_list.emplace_back(new X64::Jmp(          \
                    std::make_shared<X64::LabelOperand>(                            \
                        func->getName() + "." + block_ptr->else_block->getName()    \
                    )                                                               \
                ));                                                                 \
            }                                                                       \
        }                                                                           \
        else {                                                                      \
            block_map[block_ptr]->inst_list.emplace_back(new jump_false(            \
                std::make_shared<X64::LabelOperand>(                                \
                    func->getName() + "." + block_ptr->else_block->getName()        \
                )                                                                   \
            ));                                                                     \
        }

        if (block_ptr->condition) {
            if (inst_result.find(block_ptr->condition) == inst_result.end()) {
                inst_result.emplace(block_ptr->condition, newValue());
                block_ptr->condition->codegen(this);
                inst_used[block_ptr->condition]++;
            }

            if (block_ptr->condition->is<SeqInst>()) {
                tail_condition_jump(X64::Je, X64::Jne)
            }
            else if (block_ptr->condition->is<SltInst>()) {
                tail_condition_jump(X64::Jl, X64::Jge)
            }
            else if (block_ptr->condition->is<SleInst>()) {
                tail_condition_jump(X64::Jle, X64::Jg)
            }
            else {
                block_map[block_ptr]->inst_list.emplace_back(new X64::Cmp(
                    inst_result.at(block_ptr->condition),
                    std::shared_ptr<X64::Operand>(new X64::ImmediateOperand(0))
                ));
                tail_condition_jump(X64::Jne, X64::Je)
            }
        }
        else {
            if (block_ptr->then_block) {
                if (block_ptr->then_block != std::next(block_iter)->get()) {
                    block_map[block_ptr]->inst_list.emplace_back(new X64::Jmp(
                        std::make_shared<X64::LabelOperand>(
                            escapeAsmName(func->getName() + "." + block_ptr->then_block->getName())
                        )
                    ));
                }
            }
            else {
                if (std::next(block_iter) != func->block_list.end()) {
                    block_map[block_ptr]->inst_list.emplace_back(new X64::Jmp(
                        std::make_shared<X64::LabelOperand>(
                            escapeAsmName(func->getName()) + "_exit"
                        )
                    ));
                }
            }
        }
#undef tail_condition_jump
    }
}

void
CodeGenX64::writeFunctionHeader(Function *func, std::ostream &os)
{
    os << "\tpush %rbp\n"
       << "\tmov %rbp, %rsp\n";

    if (func->getName() != "_init_") {
#define push_argument(__i, __reg)                       \
        if (func->prototype->arguments_size() >= __i) { \
            os << "\tpush " __reg "\n";                 \
        }

        push_argument(0, "%rdi");
        push_argument(1, "%rsi");
        push_argument(2, "%rdx");
        push_argument(3, "%rcx");
        push_argument(4, "%r8");
        push_argument(5, "%r9");

#undef push_arguemnt
    }

    os << "\tsub %rsp, " << stack_allocate_counter * 8 << "\n";

#define push_caller_regs(__reg)                                 \
    if (used_registers.find(__reg) != used_registers.end()) {   \
        os << "\tpush " + X64::to_string(__reg) + "\n";         \
    }

    push_caller_regs(X64::Register::RBX);
    push_caller_regs(X64::Register::R12);
    push_caller_regs(X64::Register::R13);
    push_caller_regs(X64::Register::R14);
    push_caller_regs(X64::Register::R15);

#undef push_caller_regs
}

void
CodeGenX64::writeFunctionFooter(Function *func, std::ostream &os)
{
    os << "\n" << escapeAsmName(func->getName()) + "_exit:\n";

    if (func->getName() != "_init_") {
#define pop_caller_regs(__reg)                                      \
        if (used_registers.find(__reg) != used_registers.end()) {   \
            os << "\tpop " + X64::to_string(__reg) + "\n";          \
        }

        pop_caller_regs(X64::Register::R15);
        pop_caller_regs(X64::Register::R14);
        pop_caller_regs(X64::Register::R13);
        pop_caller_regs(X64::Register::R12);
        pop_caller_regs(X64::Register::RBX);

#undef pop_caller_regs
    }

    os << "\tmov %rsp, %rbp\n"
       << "\tpop %rbp\n"
       << "\tret\n";
}

void
CodeGenX64::registerValueLiveRange(X64::Operand *value, int loop_depth)
{
    if (
        value->is<X64::ValueOperand>() ||
        value->is<X64::RegisterOperand>()
    ) {
        if (live_range.find(value) == live_range.end()) {
            live_range.emplace(
                value,
                LiveRange(inst_list.size(), inst_list.size())
            );
        }
        else {
            live_range.at(value).second = inst_list.size();
        }
        operand_swap_out_cost[value] += MEMORY_OPERATION_COST << (loop_depth * 4);
    }
}

void
CodeGenX64::registerValueLiveRangeOfInst(X64::Instruction *inst, int loop_depth)
{
    if (inst->is<X64::Mov>()) {
        registerValueLiveRange(inst->to<X64::Mov>()->src.get(), loop_depth);
        registerValueLiveRange(inst->to<X64::Mov>()->dst.get(), loop_depth);
    }
    else if (inst->is<X64::Add>()) {
        registerValueLiveRange(inst->to<X64::Add>()->src.get(), loop_depth);
        registerValueLiveRange(inst->to<X64::Add>()->dst.get(), loop_depth);
    }
    else if (inst->is<X64::And>()) {
        registerValueLiveRange(inst->to<X64::And>()->src.get(), loop_depth);
        registerValueLiveRange(inst->to<X64::And>()->dst.get(), loop_depth);
    }
    else if (inst->is<X64::Cmp>()) {
        registerValueLiveRange(inst->to<X64::Cmp>()->left.get(), loop_depth);
        registerValueLiveRange(inst->to<X64::Cmp>()->right.get(), loop_depth);
    }
    else if (inst->is<X64::Imul>()) {
        registerValueLiveRange(inst->to<X64::Imul>()->src.get(), loop_depth);
        registerValueLiveRange(inst->to<X64::Imul>()->dst.get(), loop_depth);
    }
    else if (inst->is<X64::LeaOffset>()) {
        registerValueLiveRange(inst->to<X64::LeaOffset>()->dst.get(), loop_depth);
        registerValueLiveRange(inst->to<X64::LeaOffset>()->base.get(), loop_depth);
    }
    else if (inst->is<X64::LeaGlobal>()) {
        registerValueLiveRange(inst->to<X64::LeaGlobal>()->dst.get(), loop_depth);
    }
    else if (inst->is<X64::Neg>()) {
        registerValueLiveRange(inst->to<X64::Neg>()->dst.get(), loop_depth);
    }
    else if (inst->is<X64::Not>()) {
        registerValueLiveRange(inst->to<X64::Not>()->dst.get(), loop_depth);
    }
    else if (inst->is<X64::Or>()) {
        registerValueLiveRange(inst->to<X64::Or>()->dst.get(), loop_depth);
        registerValueLiveRange(inst->to<X64::Or>()->src.get(), loop_depth);
    }
    else if (inst->is<X64::Pop>()) {
        registerValueLiveRange(inst->to<X64::Pop>()->dst.get(), loop_depth);
    }
    else if (inst->is<X64::Push>()) {
        registerValueLiveRange(inst->to<X64::Push>()->src.get(), loop_depth);
    }
    else if (inst->is<X64::Sal>()) {
        registerValueLiveRange(inst->to<X64::Sal>()->dst.get(), loop_depth);
        registerValueLiveRange(inst->to<X64::Sal>()->src.get(), loop_depth);
    }
    else if (inst->is<X64::Sar>()) {
        registerValueLiveRange(inst->to<X64::Sar>()->dst.get(), loop_depth);
        registerValueLiveRange(inst->to<X64::Sar>()->src.get(), loop_depth);
    }
    else if (inst->is<X64::SetE>()) {
        registerValueLiveRange(inst->to<X64::SetE>()->dst.get(), loop_depth);
    }
    else if (inst->is<X64::SetL>()) {
        registerValueLiveRange(inst->to<X64::SetL>()->dst.get(), loop_depth);
    }
    else if (inst->is<X64::SetLe>()) {
        registerValueLiveRange(inst->to<X64::SetLe>()->dst.get(), loop_depth);
    }
    else if (inst->is<X64::Shr>()) {
        registerValueLiveRange(inst->to<X64::Shr>()->dst.get(), loop_depth);
        registerValueLiveRange(inst->to<X64::Shr>()->src.get(), loop_depth);
    }
    else if (inst->is<X64::Sub>()) {
        registerValueLiveRange(inst->to<X64::Sub>()->dst.get(), loop_depth);
        registerValueLiveRange(inst->to<X64::Sub>()->src.get(), loop_depth);
    }
    else if (inst->is<X64::Xor>()) {
        registerValueLiveRange(inst->to<X64::Xor>()->dst.get(), loop_depth);
        registerValueLiveRange(inst->to<X64::Xor>()->src.get(), loop_depth);
    }
    else if (inst->is<X64::Call>()) {
        registerValueLiveRange(inst->to<X64::Call>()->rax.get(), loop_depth);
        registerValueLiveRange(inst->to<X64::Call>()->func.get(), loop_depth);
    }
    else if (inst->is<X64::Ret>()) {
        registerValueLiveRange(inst->to<X64::Ret>()->rax.get(), loop_depth);
    }
}

void
CodeGenX64::allocateRegisters()
{
    std::map<BasicBlock *, Position> block_begin;

    inst_list.clear();
    live_range.clear();
    live_range_r.clear();
    operand_swap_out_cost.clear();
    for (auto &block_ptr : block_list) {
        block_begin.emplace(block_ptr->ir_block, inst_list.size());

        inst_list.emplace_back(new X64::Label(current_func->getName() + "." + block_ptr->name));
        for (auto &inst_ptr : block_ptr->inst_list) {
            auto inst = inst_ptr.release();
            registerValueLiveRangeOfInst(inst, block_ptr->ir_block->getDepth());
            inst_list.emplace_back(inst);
        }

        if (block_begin.find(block_ptr->ir_block->then_block) != block_begin.end()) {
            Position dst_block_begin = block_begin[block_ptr->ir_block->then_block];
            Position src_block_begin = block_begin[block_ptr->ir_block];

            for (auto &range_pair : live_range) {
                if (range_pair.second.first < dst_block_begin &&
                    range_pair.second.second > src_block_begin) {
                    range_pair.second.second = inst_list.size();
                }
            }
        }

        if (block_begin.find(block_ptr->ir_block->else_block) != block_begin.end()) {
            Position dst_block_begin = block_begin[block_ptr->ir_block->else_block];
            Position src_block_begin = block_begin[block_ptr->ir_block];

            for (auto &range_pair : live_range) {
                if (range_pair.second.first < dst_block_begin &&
                    range_pair.second.second > src_block_begin) {
                    range_pair.second.second = inst_list.size();
                }
            }
        }
    }

    for (auto &range_pair : live_range) {
        live_range_r.emplace(range_pair.second, range_pair.first);
    }

    current_inst_index = 0;
    available_registers.clear();
    for (auto reg = GP_REG_START; reg < GP_REG_END; reg = X64::next(reg)) {
        available_registers.emplace(reg);
    }
    available_slots.clear();
    current_mapped_register.clear();
    used_registers.clear();

    auto rax = std::shared_ptr<X64::Operand>(new X64::RegisterOperand(X64::Register::RAX));

    for (
        auto inst_iter = inst_list.begin();
        inst_iter != inst_list.end();
        ++inst_iter
    ) {
        if ((*inst_iter)->is<X64::Label>()) {
            std::cout << (*inst_iter)->to<X64::Label>()->name << std::endl;
        }

        (*inst_iter)->registerAllocate(this);
        (*inst_iter)->resolveTooManyMemoryLocations(inst_list, inst_iter, rax);
        if ((*inst_iter)->is<X64::CallPreserve>()) {
#define save_register(__r)                                                              \
            if (available_registers.find(__r) == available_registers.end()) {           \
                inst_list.emplace(                                                      \
                    inst_iter,                                                          \
                    new X64::Push(                                                      \
                        std::shared_ptr<X64::Operand>(new X64::RegisterOperand(__r))    \
                    )                                                                   \
                );                                                                      \
                (*inst_iter)->to<X64::CallPreserve>()->call_inst->                      \
                    saved_registers.emplace_front(__r);                                 \
            }
            for (
                auto reg = GP_REG_START;
                reg != GP_REG_END;
                reg = X64::next(reg)
            ) {
                save_register(reg);
            }
#undef save_register
        }
        else if ((*inst_iter)->is<X64::CallRestore>()) {
            auto call_inst = (*inst_iter)->to<X64::CallRestore>()->call_inst;
            for (auto reg : call_inst->saved_registers) {
                inst_list.emplace(
                    inst_iter,
                    new X64::Pop(
                        std::shared_ptr<X64::Operand>(new X64::RegisterOperand(reg))
                    )
                );
            }
        }
        ++current_inst_index;
    }
}

void
CodeGenX64::allocateFor(X64::Operand *operand)
{
    if (operand->is<X64::ValueOperand>()) {
        if (!operand->to<X64::ValueOperand>()->actual_operand) {
            operand->to<X64::ValueOperand>()->actual_operand.reset(allocateAll({}, operand));
        }

        if (live_range[operand].second == current_inst_index) {
            assert(operand->to<X64::ValueOperand>()->actual_operand);
            freeAll(operand->to<X64::ValueOperand>()->actual_operand.get());
        }
    }
    else if (operand->is<X64::RegisterOperand>()) {
        requestRegister(operand->to<X64::RegisterOperand>()->reg, operand);

        if (live_range[operand].second == current_inst_index) {
            freeAll(operand);
        }
    }
}

X64::Operand *
CodeGenX64::allocateAll(const std::set<X64::Register> &skip_list, X64::Operand *operand)
{
    if (available_registers.size()) {
        auto reg = *(available_registers.begin());

        available_registers.erase(available_registers.begin());
        current_mapped_register.emplace(reg, operand);
        used_registers.emplace(reg);

        return new X64::RegisterOperand(reg);
    }

    assert(operand->is<X64::ValueOperand>());
    X64::Operand *victim = operand;
    size_t cost = operand_swap_out_cost[operand];

    for (
        auto reg = GP_REG_START;
        reg < GP_REG_END;
        reg = X64::next(reg)
    ) {
        if (skip_list.find(reg) == skip_list.end()) {
            X64::Operand *current_mapped_operand = current_mapped_register[reg];
            if (current_mapped_operand->is<X64::RegisterOperand>()) { continue; }

            if (operand_swap_out_cost[current_mapped_operand] < cost) {
                victim = current_mapped_operand;
                cost = operand_swap_out_cost[victim];
            }
        }
    }

    assert(victim->is<X64::ValueOperand>());
    X64::Operand *allocated_operand = nullptr;
    if (available_slots.size()) {
        allocated_operand = new X64::StackMemoryOperand(*(available_slots.begin()));
        available_slots.erase(available_slots.begin());
    }
    else {
        allocated_operand = new X64::StackMemoryOperand(stackSlotOffset(allocateStackSlot(1)));
    }

    if (victim == operand) {
        return allocated_operand;
    }
    else {
        if (victim->to<X64::ValueOperand>()->actual_operand->is<X64::RegisterOperand>()) {
            auto reg = victim->to<X64::ValueOperand>()->actual_operand->to<X64::RegisterOperand>()->reg;
            current_mapped_register[reg] = operand;
        }

        auto ret = victim->to<X64::ValueOperand>()->actual_operand.release();
        victim->to<X64::ValueOperand>()->actual_operand.reset(allocated_operand);
        return ret;
    }
}

void
CodeGenX64::freeAll(X64::Operand *operand)
{
    if (operand->is<X64::RegisterOperand>()) {
        auto reg = operand->to<X64::RegisterOperand>()->reg;
        if (reg >= GP_REG_END) { return; }

        current_mapped_register.erase(reg);
        available_registers.emplace(reg);
        return;
    }
    else if (operand->is<X64::StackMemoryOperand>()) {
        available_slots.emplace(operand->to<X64::StackMemoryOperand>()->offset);
        return;
    }
    // assert(false);
}

void
CodeGenX64::requestRegister(X64::Register reg, X64::Operand *operand)
{
    if (reg >= GP_REG_END) { return; }

    used_registers.emplace(reg);

    if (available_registers.find(reg) != available_registers.end()) {
        available_registers.erase(reg);
        current_mapped_register[reg] = operand;
        return;
    }
    assert(current_mapped_register.find(reg) != current_mapped_register.end());

    auto victim = current_mapped_register[reg];
    if (victim == operand) { return; }

    assert(victim);
    if (!victim->is<X64::ValueOperand>()) {
        std::cerr << "Fixed register overlay: " <<
            X64::to_string(victim->to<X64::RegisterOperand>()->reg) << std::endl;
        assert(victim->is<X64::ValueOperand>());
    }
    victim->to<X64::ValueOperand>()->actual_operand.reset(allocateAll({reg}, victim));
    current_mapped_register[reg] = operand;
}

void
CodeGenX64::registerAllocate(X64::Label *)
{ }

void
CodeGenX64::registerAllocate(X64::Add *inst)
{
    allocateFor(inst->src.get());
    allocateFor(inst->dst.get());
}

void
CodeGenX64::registerAllocate(X64::And *inst)
{
    allocateFor(inst->src.get());
    allocateFor(inst->dst.get());
}

void
CodeGenX64::registerAllocate(X64::Call *)
{ }

void
CodeGenX64::registerAllocate(X64::CallPreserve *)
{ }

void
CodeGenX64::registerAllocate(X64::CallRestore *)
{ }

void
CodeGenX64::registerAllocate(X64::Cmp *inst)
{
    allocateFor(inst->left.get());
    allocateFor(inst->right.get());
}

void
CodeGenX64::registerAllocate(X64::Idiv *)
{ }

void
CodeGenX64::registerAllocate(X64::Imod *)
{ }

void
CodeGenX64::registerAllocate(X64::Imul *inst)
{
    allocateFor(inst->src.get());
    allocateFor(inst->dst.get());
}

void
CodeGenX64::registerAllocate(X64::Jmp *)
{ }

void
CodeGenX64::registerAllocate(X64::Je *)
{ }

void
CodeGenX64::registerAllocate(X64::Jne *)
{ }

void
CodeGenX64::registerAllocate(X64::Jg *)
{ }

void
CodeGenX64::registerAllocate(X64::Jge *)
{ }

void
CodeGenX64::registerAllocate(X64::Jl *)
{ }

void
CodeGenX64::registerAllocate(X64::Jle *)
{ }

void
CodeGenX64::registerAllocate(X64::LeaOffset *inst)
{
    allocateFor(inst->base.get());
    allocateFor(inst->dst.get());
}

void
CodeGenX64::registerAllocate(X64::LeaGlobal *inst)
{ allocateFor(inst->dst.get()); }

void
CodeGenX64::registerAllocate(X64::Mov *inst)
{
    allocateFor(inst->src.get());
    allocateFor(inst->dst.get());
}

void
CodeGenX64::registerAllocate(X64::Neg *inst)
{ allocateFor(inst->dst.get()); }

void
CodeGenX64::registerAllocate(X64::Not *inst)
{ allocateFor(inst->dst.get()); }

void
CodeGenX64::registerAllocate(X64::Or *inst)
{
    allocateFor(inst->src.get());
    allocateFor(inst->dst.get());
}

void
CodeGenX64::registerAllocate(X64::Pop *inst)
{ allocateFor(inst->dst.get()); }

void
CodeGenX64::registerAllocate(X64::Push *inst)
{ allocateFor(inst->src.get()); }

void
CodeGenX64::registerAllocate(X64::Ret *)
{ }

void
CodeGenX64::registerAllocate(X64::Sal *inst)
{
    allocateFor(inst->src.get());
    allocateFor(inst->dst.get());
}

void
CodeGenX64::registerAllocate(X64::Sar *inst)
{
    allocateFor(inst->src.get());
    allocateFor(inst->dst.get());
}

void
CodeGenX64::registerAllocate(X64::SetE *inst)
{ allocateFor(inst->dst.get()); }

void
CodeGenX64::registerAllocate(X64::SetL *inst)
{ allocateFor(inst->dst.get()); }

void
CodeGenX64::registerAllocate(X64::SetLe *inst)
{ allocateFor(inst->dst.get()); }

void
CodeGenX64::registerAllocate(X64::Shr *inst)
{
    allocateFor(inst->src.get());
    allocateFor(inst->dst.get());
}

void
CodeGenX64::registerAllocate(X64::Sub *inst)
{
    allocateFor(inst->src.get());
    allocateFor(inst->dst.get());
}

void
CodeGenX64::registerAllocate(X64::Xor *inst)
{
    allocateFor(inst->src.get());
    allocateFor(inst->dst.get());
}

int
CodeGenX64::allocateStackSlot(int size)
{ return stack_allocate_counter += size; }

int
CodeGenX64::stackSlotOffset(int slot)
{ return slot * -8 - 48; }

int
CodeGenX64::getAllocInstOffset(AllocaInst *inst)
{
    if (allocate_map.find(inst) == allocate_map.end()) {
        assert(inst->getSpace()->is<UnsignedImmInst>());
        allocate_map.emplace(
            inst,
            allocateStackSlot(static_cast<int>(inst->getSpace()->to<UnsignedImmInst>()->getValue()))
        );
    }

    return stackSlotOffset(allocate_map[inst]);
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
    }
    if (inst_result.find(inst) == inst_result.end()) {
        auto operand = newValue();
        inst_result.emplace(inst, operand);
        inst_used[inst]++;
        inst->codegen(this);
    }
    return inst_result.at(inst);
}

std::shared_ptr<X64::Operand>
CodeGenX64::resolveMemory(Instruction *inst)
{
    if (inst->is<GlobalInst>()) {
        inst->unreference();
        return std::shared_ptr<X64::Operand>(new X64::GlobalMemoryOperand(inst->to<GlobalInst>()->getValue()));
    }
    else if (inst->is<ArgInst>()) {
        inst->unreference();
        return std::shared_ptr<X64::Operand>(
            new X64::StackMemoryOperand(calculateArgumentOffset(static_cast<int>(
                inst->to<ArgInst>()->getValue())))
        );
    }
    else if (inst->is<AllocaInst>()) {
        inst->unreference();
        return std::shared_ptr<X64::Operand>(
            new X64::StackMemoryOperand(getAllocInstOffset(
                inst->to<AllocaInst>()))
        );
    }
    else {
        if (inst_result.find(inst) == inst_result.end()) {
            auto operand = newValue();
            inst_result.emplace(inst, operand);
            inst_used[inst]++;
            inst->codegen(this);
        }
        return std::shared_ptr<X64::Operand>(
            new X64::OffsetMemoryOperand(
                inst_result.at(inst),
                0
            )
        );
    }
}

void
CodeGenX64::setOrMoveOperand(Instruction *inst, std::shared_ptr<X64::Operand> dst)
{
    if (
        inst_result.find(inst) == inst_result.end() &&
        inst->getReferencedCount() == 1
    ) {
        inst_result.emplace(inst, dst);
        inst->codegen(this);
    }
    else {
        if (inst_result.find(inst) == inst_result.end()) {
            inst_result.emplace(inst, newValue());
        }
        block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Mov(
            dst,
            inst_result.at(inst)
        ));
    }
    inst_used[inst]++;
}

std::shared_ptr<X64::Operand>
CodeGenX64::newValue()
{ return std::shared_ptr<X64::Operand>(new X64::ValueOperand()); }

void
CodeGenX64::gen(Instruction *inst)
{ assert(false); }

void
CodeGenX64::gen(SignedImmInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Mov(
        inst_result.at(inst),
        std::shared_ptr<X64::Operand>(new X64::ImmediateOperand(inst->getValue()))
    ));
}

void
CodeGenX64::gen(UnsignedImmInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Mov(
        inst_result.at(inst),
        std::shared_ptr<X64::Operand>(new X64::ImmediateOperand(inst->getValue()))
    ));
}

void
CodeGenX64::gen(GlobalInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::LeaGlobal(
        inst_result.at(inst),
        std::shared_ptr<X64::Operand>(new X64::GlobalMemoryOperand(inst->getValue()))
    ));
}

void
CodeGenX64::gen(ArgInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::LeaOffset(
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
    setOrMoveOperand(inst->getLeft(), inst_result.at(inst));
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Add(
        inst_result.at(inst),
        resolveOperand(inst->getRight())
    ));
}

void
CodeGenX64::gen(SubInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    setOrMoveOperand(inst->getLeft(), inst_result.at(inst));
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Sub(
        inst_result.at(inst),
        resolveOperand(inst->getRight())
    ));
}

void
CodeGenX64::gen(MulInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    setOrMoveOperand(inst->getLeft(), inst_result.at(inst));
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Imul(
        inst_result.at(inst),
        resolveOperand(inst->getRight())
    ));
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
    setOrMoveOperand(inst->getLeft(), inst_result.at(inst));
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Sal(
        inst_result.at(inst),
        resolveOperand(inst->getRight())
    ));
}

void
CodeGenX64::gen(ShrInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    setOrMoveOperand(inst->getLeft(), inst_result.at(inst));
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Shr(
        inst_result.at(inst),
        resolveOperand(inst->getRight())
    ));
}

void
CodeGenX64::gen(OrInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    setOrMoveOperand(inst->getLeft(), inst_result.at(inst));
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Or(
        inst_result.at(inst),
        resolveOperand(inst->getRight())
    ));
}

void
CodeGenX64::gen(AndInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    setOrMoveOperand(inst->getLeft(), inst_result.at(inst));
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::And(
        inst_result.at(inst),
        resolveOperand(inst->getRight())
    ));
}

void
CodeGenX64::gen(NorInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    setOrMoveOperand(inst->getLeft(), inst_result.at(inst));
    if (
        (inst->getRight()->is<SignedImmInst>() &&
            inst->getRight()->to<SignedImmInst>()->getValue() == 0) ||
        (inst->getRight()->is<UnsignedImmInst>() &&
            inst->getRight()->to<UnsignedImmInst>()->getValue() == 0)
    ) {
        inst->getRight()->unreference();
    }
    else {
        block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Or(
            inst_result.at(inst),
            resolveOperand(inst->getRight())
        ));
    }
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Not(
        inst_result.at(inst)
    ));
}

void
CodeGenX64::gen(XorInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    setOrMoveOperand(inst->getLeft(), inst_result.at(inst));
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Xor(
        inst_result.at(inst),
        resolveOperand(inst->getRight())
    ));
}

void
CodeGenX64::gen(SeqInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());

    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Cmp(
        resolveOperand(inst->getLeft()),
        resolveOperand(inst->getRight())
    ));

    if (
        inst->getOwnerBlock()->condition != inst ||
        inst->getReferencedCount() > 1
    ) {
        block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::SetE(
            inst_result.at(inst)
        ));
    }
}

void
CodeGenX64::gen(SltInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());

    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Cmp(
        resolveOperand(inst->getLeft()),
        resolveOperand(inst->getRight())
    ));

    if (
        inst->getOwnerBlock()->condition != inst ||
        inst->getReferencedCount() > 1
        ) {
        block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::SetL(
            inst_result.at(inst)
        ));
    }
}

void
CodeGenX64::gen(SleInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());

    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Cmp(
        resolveOperand(inst->getLeft()),
        resolveOperand(inst->getRight())
    ));

    if (
        inst->getOwnerBlock()->condition != inst ||
        inst->getReferencedCount() > 1
        ) {
        block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::SetLe(
            inst_result.at(inst)
        ));
    }
}

void
CodeGenX64::gen(LoadInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Mov(
        inst_result.at(inst),
        resolveMemory(inst->getAddress())
    ));
}

void
CodeGenX64::gen(StoreInst *inst)
{
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Mov(
        resolveMemory(inst->getAddress()),
        resolveOperand(inst->getValue())
    ));
}

void
CodeGenX64::gen(AllocaInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::LeaOffset(
        inst_result.at(inst),
        std::shared_ptr<X64::Operand>(new X64::RegisterOperand(X64::Register::RBP)),
        std::shared_ptr<X64::Operand>(new X64::ImmediateOperand(getAllocInstOffset(inst)))
    ));
}

void
CodeGenX64::gen(CallInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());

    std::shared_ptr<X64::Operand> func;
    if (inst->getFunction()->is<GlobalInst>()) {
        func = std::shared_ptr<X64::Operand>(
            new X64::LabelOperand(inst->getFunction()->to<GlobalInst>()->getValue())
        );
    }
    else {
        if (inst_result.find(inst->getFunction()) == inst_result.end()) {
            inst_result.emplace(inst->getFunction(), newValue());
        }
        inst->getFunction()->codegen(this);
        func = inst_result.at(inst->getFunction());
    }

    auto call_inst = new X64::Call(
        func,
        std::shared_ptr<X64::Operand>(new X64::RegisterOperand(X64::Register::RAX))
    );

    std::vector<std::shared_ptr<X64::Operand> > argument_operands;
    for (size_t i = 0; i < inst->arguments_size(); ++i) {
        argument_operands.push_back(resolveOperand(inst->getArgumentByIndex(i)));
    }

    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::CallPreserve(call_inst));
#define call_set_argument_register(__i, __r)                                            \
    do {                                                                                \
        block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Mov(          \
            std::shared_ptr<X64::Operand>(new X64::RegisterOperand(__r)),               \
            argument_operands[__i]                                                      \
        ));                                                                             \
    } while (0)

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
                inst->getArgumentByIndex(i)->codegen(this);
            }
            inst_used[inst->getArgumentByIndex(i)]++;
        }
    } while (false);
#undef call_set_argument_register
    if (inst->arguments_size()) {
        for (auto i = inst->arguments_size() - 1; i >= 6; --i) {
            block_map[inst->getOwnerBlock()]->inst_list.emplace_back(
                new X64::Push(inst_result[inst->getArgumentByIndex(i)]));
        }
    }
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(call_inst);
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Mov(
        inst_result.at(inst),
        call_inst->rax
    ));
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::CallRestore(call_inst));
}

void
CodeGenX64::gen(RetInst *inst)
{
    // clear possible jumps
    inst->getOwnerBlock()->condition = nullptr;
    inst->getOwnerBlock()->then_block = inst->getOwnerBlock()->else_block = nullptr;

    // make outer function append the `ret` inst
    if (inst->getReturnValue()) {
        if (inst_result.find(inst->getReturnValue()) != inst_result.end()) {
            block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Mov(
                std::shared_ptr<X64::Operand>(new X64::RegisterOperand(X64::Register::RAX)),
                inst_result.at(inst->getReturnValue())
            ));
        }
        else {
            inst_result.emplace(
                inst->getReturnValue(),
                newValue()
            );
            inst->getReturnValue()->codegen(this);

            block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Mov(
                std::shared_ptr<X64::Operand>(new X64::RegisterOperand(X64::Register::RAX)),
                inst_result.at(inst->getReturnValue())
            ));
        }
    }
    inst_used[inst->getReturnValue()]++;
}

void
CodeGenX64::gen(NewInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());
    auto call_inst = new X64::Call(
        std::shared_ptr<X64::Operand>(new X64::LabelOperand("malloc")),
        std::shared_ptr<X64::Operand>(new X64::RegisterOperand(X64::Register::RAX))
    );

    if (inst_result.find(inst->getSpace()) != inst_result.end()) {
        block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Mov(
            std::shared_ptr<X64::Operand>(new X64::RegisterOperand(X64::Register::RDI)),
            inst_result.at(inst->getSpace())
        ));
    }
    else {
        inst_result.emplace(
            inst->getSpace(),
            std::shared_ptr<X64::Operand>(new X64::RegisterOperand(X64::Register::RDI))
        );
        inst->getSpace()->codegen(this);
    }
    inst_used[inst->getSpace()]++;
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::CallPreserve(call_inst));
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(call_inst);
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Mov(
        inst_result.at(inst),
        call_inst->rax
    ));
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::CallRestore(call_inst));

    if (inst->getType()->is<StructType>()) {
        auto struct_type = inst->getType()->to<StructType>();
        auto rax = std::shared_ptr<X64::Operand>(new X64::RegisterOperand(X64::Register::RAX));
        auto rdx = std::shared_ptr<X64::Operand>(new X64::RegisterOperand(X64::Register::RDX));
        for (size_t i = 0; i < struct_type->concept_size(); ++i) {
            block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Mov(
                rax,
                inst_result.at(inst)
            ));
            block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::LeaGlobal(
                rdx,
                std::shared_ptr<X64::Operand>(new X64::GlobalMemoryOperand(
                    escapeAsmName(
                        ir->type_pool->getCastedStructType(
                            struct_type,
                            struct_type->getConceptByOffset(static_cast<int>(
                                                                struct_type->members_size() + i))
                        )->to_string() + "__vtable"
                    )
                ))
            ));
            block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Mov(
                std::shared_ptr<X64::Operand>(new X64::OffsetMemoryOperand(
                    rax,
                    (struct_type->members_size() + i) * 8
                )),
                rdx
            ));
        }
    }
}

void
CodeGenX64::gen(DeleteInst *inst)
{
    auto call_inst = new X64::Call(
        std::shared_ptr<X64::Operand>(new X64::LabelOperand("free")),
        std::shared_ptr<X64::Operand>(new X64::RegisterOperand(X64::Register::RAX))
    );

    if (inst_result.find(inst->getTarget()) != inst_result.end()) {
        block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::Mov(
            std::shared_ptr<X64::Operand>(new X64::RegisterOperand(X64::Register::RDI)),
            inst_result.at(inst->getTarget())
        ));
    }
    else {
        inst_result.emplace(
            inst->getTarget(),
            std::shared_ptr<X64::Operand>(new X64::RegisterOperand(X64::Register::RDI))
        );
        inst->getTarget()->codegen(this);
    }
    inst_used[inst->getTarget()]++;
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::CallPreserve(call_inst));
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(call_inst);
    block_map[inst->getOwnerBlock()]->inst_list.emplace_back(new X64::CallRestore(call_inst));
}

void
CodeGenX64::gen(PhiInst *inst)
{
    assert(inst_result.find(inst) != inst_result.end());

    for (auto &branch : *inst) {
        if (inst_result.find(branch.value) == inst_result.end()) {
            inst_result.emplace(branch.value, inst_result.at(inst));
            branch.value->codegen(this);
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

