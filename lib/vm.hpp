#ifndef _CYAN_VM_HPP_
#define _CYAN_VM_HPP_

#include <array>
#include <cstdint>
#include <vector>
#include <stack>
#include <memory>

#include "cyan.hpp"
#include "codegen.hpp"

#define CYAN_USE_COMPUTED_GOTO  1

namespace cyan {

namespace vm {

#if __CYAN_64__
using OperatorT     = uint16_t;
using ShiftT        = uint16_t;
using RegisterT     = uint32_t;
using ImmediateT    = uint64_t;
#else
using OperatorT     = uint8_t;
using ShiftT        = uint8_t;
using RegisterT     = uint16_t;
using ImmediateT    = uint32_t;
#endif

static_assert(sizeof(ImmediateT) == sizeof(uintptr_t), "ImmdiateT should be large enough");

enum InstOperator
{
    I_UNKNOWN = 0,

    I_ARG,
    I_BR,
    I_BNR,
    I_GLOB,
    I_JUMP,
    I_LI,

    InstOperator_ImmediateInst = I_LI,

    I_ADD,
    I_ALLOC,
    I_AND,
    I_CALL,
    I_DELETE,
    I_DIV,
    I_DIVU,
    I_LOAD8,
    I_LOAD8U,
    I_LOAD16,
    I_LOAD16U,
    I_LOAD32,
    I_LOAD32U,
    I_LOAD64,
    I_LOAD64U,
    I_MOD,
    I_MODU,
    I_MOV,
    I_MUL,
    I_MULU,
    I_NEW,
    I_NOR,
    I_OR,
    I_POP,
    I_PUSH,
    I_RET,
    I_SEQ,
    I_SHL,
    I_SHLU,
    I_SHR,
    I_SHRU,
    I_SLE,
    I_SLEU,
    I_SLT,
    I_SLTU,
    I_STORE8,
    I_STORE8U,
    I_STORE16,
    I_STORE16U,
    I_STORE32,
    I_STORE32U,
    I_STORE64,
    I_STORE64U,
    I_SUB,
    I_XOR,

    InstOperator_NR
};

struct Instruction
{
    OperatorT           i_op;
    ShiftT              i_shift;
    RegisterT           i_rd;
    union {
        ImmediateT      _imm;
        struct {
            RegisterT   _rs;
            RegisterT   _rt;
        } _regs;
    } _info;

    Instruction(OperatorT op, ShiftT shift, RegisterT rd, ImmediateT imm)
        : i_op(op), i_shift(shift), i_rd(rd)
    { _info._imm = imm; }

    Instruction(OperatorT op, ShiftT shift, RegisterT rd, RegisterT rs, RegisterT rt)
        : i_op(op), i_shift(shift), i_rd(rd)
    {
        _info._regs._rs = rs;
        _info._regs._rt = rt;
    }
};

#define i_imm   _info._imm
#define i_rt    _info._regs._rt
#define i_rs    _info._regs._rs

static_assert(sizeof(Instruction) == 2 * sizeof(ImmediateT), "Instruction should be 2 in sizeof ImmediateT");

using Slot = uintptr_t;
using SignedSlot = intptr_t;
using GlobalSegment = std::vector<Slot>;

struct Function
{
    virtual ~Function() = default;
};

struct VMFunction : Function
{
    std::vector<Instruction> inst_list;
    size_t register_nr = 1;
};

struct LibFunction : Function
{
    virtual Slot call(const Slot *argument) = 0;
};

struct Frame
{
    VMFunction *func;
    std::vector<Slot> regs;
    size_t frame_pointer;
    const Instruction *pc;

    Frame(VMFunction *func, size_t frame_pointer)
        : func(func), regs(func->register_nr, 0), frame_pointer(frame_pointer), pc(func->inst_list.data())
    { }

    inline Slot &
    operator [] (size_t index)
    { return regs[index]; }
};

class MovInst;

class VirtualMachine
{
public:
    constexpr static size_t STACK_SIZE = 1024 * 512;    // 512K stack

private:
    GlobalSegment globals;
    std::vector<char> string_pool;
    std::stack<std::unique_ptr<Frame> > frame_stack;
    std::array<char, STACK_SIZE> stack;
    size_t stack_pointer = STACK_SIZE;
    std::map<std::string, std::unique_ptr<Function> > functions;

    Slot run();

    VirtualMachine() = default;
public:
    ~VirtualMachine() = default;

    struct Generate : public ::cyan::CodeGen
    {
    private:
        std::unique_ptr<VirtualMachine> _product;
        VMFunction *current_func;
        std::map<std::string, size_t> global_map;
        std::map<BasicBlock *, size_t> block_map;
        std::map<::cyan::Instruction *, RegisterT> value_map;
        std::list<std::unique_ptr<::cyan::Instruction> > phi_ref;

        Generate(VirtualMachine *product, IR *ir)
            : CodeGen(ir), _product(product)
        { }

        void generateFunc(::cyan::Function *func);
    public:
        virtual std::ostream &generate(std::ostream &os);
        void generate();
        inst_foreach(define_gen);
        void gen(MovInst *);

        inline std::unique_ptr<VirtualMachine>
        release()
        { return std::move(_product); }

        inline void
        registerLibFunction(std::string name, LibFunction *func)
        { _product->functions.emplace(name, std::unique_ptr<Function>(func)); }

        friend class VirtualMachine;
    };

    Slot start();

    static std::unique_ptr<Generate> GenerateFactory(IR *ir);
};

}

}

#endif
