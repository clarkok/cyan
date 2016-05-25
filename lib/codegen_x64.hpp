//
// Created by c on 5/10/16.
//

#ifndef CYAN_CODEGEN_X64_HPP
#define CYAN_CODEGEN_X64_HPP

#include <map>
#include <vector>
#include <memory>

#include "codegen.hpp"

#define x64_inst_foreach(macro) \
    macro(Label)                \
    macro(Mov)                  \
    macro(Add)                  \
    macro(And)                  \
    macro(Call)                 \
    macro(CallPreserve)         \
    macro(CallRestore)          \
    macro(Cmp)                  \
    macro(Idiv)                 \
    macro(Imod)                 \
    macro(Imul)                 \
    macro(Jmp)                  \
    macro(Je)                   \
    macro(Jne)                  \
    macro(Jg)                   \
    macro(Jge)                  \
    macro(Jl)                   \
    macro(Jle)                  \
    macro(LeaOffset)            \
    macro(LeaGlobal)            \
    macro(Neg)                  \
    macro(Not)                  \
    macro(Or)                   \
    macro(Pop)                  \
    macro(Push)                 \
    macro(Ret)                  \
    macro(Sal)                  \
    macro(Sar)                  \
    macro(SetE)                 \
    macro(SetL)                 \
    macro(SetLe)                \
    macro(Shr)                  \
    macro(Sub)                  \
    macro(Xor)

namespace cyan {
class CodeGenX64;
}

namespace X64 {

struct Instruction
{
    virtual ~Instruction() = default;
    virtual std::string to_string() const = 0;

    template <typename T>
    const T* to() const
    { return dynamic_cast<const T*>(this); }

    template <typename T>
    T* to()
    { return dynamic_cast<T*>(this); }

    template <typename T>
    bool is() const
    { return to<T>() != nullptr; }

    virtual void registerAllocate(cyan::CodeGenX64 *codegen) = 0;
};

#define forward_define_inst(inst)   \
struct inst;

x64_inst_foreach(forward_define_inst)

#undef forward_define_inst

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

    typedef size_t Position;
    typedef std::pair<Position, Position> LiveRange;

    static std::string escapeAsmName(std::string original);

private:
    std::map<Instruction *, size_t> inst_used;
    std::vector<std::unique_ptr<X64::Block> > block_list;
    std::map<BasicBlock *, X64::Block *> block_map;
    std::map<Instruction *, std::shared_ptr<X64::Operand> > inst_result;
    std::map<AllocaInst *, int> allocate_map;
    int stack_allocate_counter = 0;

    std::vector<std::unique_ptr<X64::Instruction> > inst_list;
    std::map<X64::Operand *, LiveRange> live_range;
    std::map<LiveRange, X64::Operand *> live_range_r;
    std::map<X64::Operand *, size_t> operand_swap_out_cost;
    Function *current_func;

    size_t current_inst_index;
    std::set<X64::Register> available_registers;
    std::set<int> available_slots;
    std::map<X64::Register, X64::Operand *> current_mapped_register;
    std::set<X64::Register> used_registers;

    void registerValueLiveRange(X64::Operand *value, int loop_depth);

    void generateFunc(Function *func);
    void writeFunctionHeader(Function *func, std::ostream &os);
    void writeFunctionFooter(Function *func, std::ostream &os);
    void registerValueLiveRangeOfInst(X64::Instruction *inst, int loop_depth);
    void allocateRegisters();

    X64::Operand *allocateAll(const std::set<X64::Register> &skip_list, X64::Operand *operand);
    void freeAll(X64::Operand *operand);
    void requestRegister(X64::Register, X64::Operand *operand);
    void allocateFor(X64::Operand *operand);

    int allocateStackSlot(int size);
    int stackSlotOffset(int slot);
    int getAllocInstOffset(AllocaInst *inst);
    int calculateArgumentOffset(int argument);
    std::shared_ptr<X64::Operand> resolveOperand(Instruction *inst);
    std::shared_ptr<X64::Operand> resolveMemory(Instruction *inst);
    void setOrMoveOperand(Instruction *, std::shared_ptr<X64::Operand>);
    std::shared_ptr<X64::Operand> newValue();

public:
    CodeGenX64(IR *ir)
        : CodeGen(ir)
    { }

    virtual std::ostream &generate(std::ostream &os);

#define register_alloc_decl(inst)  \
    void registerAllocate(X64::inst *);

    x64_inst_foreach(register_alloc_decl)

#undef register_alloc_decl

    inst_foreach(define_gen)
};

}

#endif //CYAN_CODEGEN_X64_HPP
