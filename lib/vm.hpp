#ifndef _CYAN_VM_HPP_
#define _CYAN_VM_HPP_

#include <array>
#include <cstdint>
#include <vector>
#include <stack>
#include <memory>

#include "cyan.hpp"
#include "codegen.hpp"

namespace cyan {

namespace vm {

#if __CYAN_64__
using OperatorT     = uint16_t;
using TypeT         = uint16_t;
using RegisterT     = uint32_t;
using ImmediateT    = uint64_t;
#else
using OperatorT     = uint8_t;
using TypeT         = uint8_t;
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
    I_LOAD,
    I_MOD,
    I_MOV,
    I_MUL,
    I_NEW,
    I_NOR,
    I_OR,
    I_POP,
    I_PUSH,
    I_RET,
    I_SEQ,
    I_SHL,
    I_SHR,
    I_SLE,
    I_SLT,
    I_STORE,
    I_SUB,
    I_XOR,

    InstOperator_NR
};

enum InstType
{
    T_SIGNED,
    T_UNSIGNED,
    T_POINTER
};

#define get_type(type, shift)   \
    (((type) << 4) | (shift))

#define type2type(type)         \
    ((type) >> 4)

#define type2shift(type)        \
    ((type) & 0xF)

struct Instruction
{
    OperatorT           i_op;
    TypeT               i_type;
    RegisterT           i_rd;
    union {
        ImmediateT      _imm;
        struct {
            RegisterT   _rs;
            RegisterT   _rt;
        } _regs;
    } _info;

    Instruction(OperatorT op, TypeT type, RegisterT rd, ImmediateT imm)
        : i_op(op), i_type(type), i_rd(rd)
    { _info._imm = imm; }

    Instruction(OperatorT op, TypeT type, RegisterT rd, RegisterT rs, RegisterT rt)
        : i_op(op), i_type(type), i_rd(rd)
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
    size_t stack_usage;
    const Instruction *pc;

    Frame(VMFunction *func)
        : func(func), regs(func->register_nr, 0), stack_usage(0), pc(func->inst_list.data())
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
        TypeT resolveType(::cyan::Instruction *inst);
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
