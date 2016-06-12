#include <cstdlib>

#include "vm.hpp"

using namespace cyan;

namespace cyan {
namespace vm {

class MovInst : public ::cyan::Instruction
{
    ::cyan::Instruction *src;

public:
    MovInst(::cyan::Instruction *_src, std::string name)
        : ::cyan::Instruction(_src->getType(), _src->getOwnerBlock(), name), src(_src)
    { }

    inline ::cyan::Instruction *
    getSource() const
    { return src; }

    virtual std::string
    to_string() const
    { return getName() + " =\t(" + getType()->to_string() + ")\tmov\t" + src->getName(); }

    virtual void
    codegen(CodeGen *_gen)
    {
        assert(dynamic_cast<VirtualMachine::Generate*>(_gen));
        dynamic_cast<VirtualMachine::Generate*>(_gen)->gen(this);
    }

    virtual void
    referenceOperand() const
    { src->reference(); }

    virtual void
    unreferenceOperand() const
    { src->unreference(); }

    virtual bool
    isCodeGenRoot() const
    { return false; }

    virtual ::cyan::Instruction *
    clone(BasicBlock *bb, std::map<Instruction *, Instruction *> &value_map, std::string name = "") const
    {
        auto ret = new MovInst(src, name);
        value_map.emplace(
            const_cast<MovInst*>(this),
            const_cast<MovInst*>(ret)
        );
        return ret;
    }

    virtual void
    resolve(const std::map<Instruction *, Instruction *> &value_map)
    {
        if (value_map.find(src) != value_map.end()) {
            src = value_map.at(src);
            assert(src);
        }
    }

    virtual void
    replaceUsage(Instruction *original, Instruction *replace)
    { if (src == original) { src = replace; } }

    virtual bool
    usedInstruction(Instruction *inst) const
    { return src == inst; }
};

}
}

void
vm::VirtualMachine::Generate::generate()
{
    for (auto &string_pair : ir->string_pool) {
        global_map.emplace(string_pair.second, _product->globals.size());
        _product->globals.push_back(_product->string_pool.size());
        _product->string_pool.insert(_product->string_pool.end(), string_pair.first.begin(), string_pair.first.end());
        _product->string_pool.push_back('\0');
    }

    for (auto &global : _product->globals) {
        global += reinterpret_cast<Slot>(_product->string_pool.data());
    }

    for (auto &global : ir->global_defines) {
        global_map.emplace(global.first, _product->globals.size());
        _product->globals.push_back(0);
    }

    for (auto &func_pair : ir->function_table) {
        if (func_pair.second.get()) {
            assert(_product->functions.find(func_pair.first) == _product->functions.end());
            _product->functions.emplace(
                func_pair.first,
                std::unique_ptr<Function>(new VMFunction())
            );
        }
        else {
            std::cerr << func_pair.first << std::endl;
            assert(false);
        }
    }

    ir->type_pool->foreachCastedStructType([&] (CastedStructType *casted) {
        auto vtable_name = casted->to_string() + "__vtable";
        global_map.emplace(vtable_name, _product->globals.size());
        for (auto &method : *casted) {
            _product->globals.push_back(reinterpret_cast<uintptr_t>(_product->functions.at(method.impl->name).get()));
        }
    });

    for (auto &func_pair : _product->functions) {
        global_map.emplace(
            func_pair.first,
            _product->globals.size()
        );
        _product->globals.push_back(reinterpret_cast<Slot>(dynamic_cast<Function*>(func_pair.second.get())));
    }

    for (auto &func_pair : ir->function_table) {
        if (func_pair.second.get()) {
            current_func = dynamic_cast<VMFunction*>(_product->functions.at(func_pair.first).get());
            generateFunc(func_pair.second.get());
        }
    }
}

std::ostream &
vm::VirtualMachine::Generate::generate(std::ostream &os)
{
    generate();
    return os;
}

void
vm::VirtualMachine::Generate::generateFunc(::cyan::Function *func)
{
    if (!func->block_list.size()) {
        current_func->inst_list.emplace_back(
            I_RET,
            0,
            0,
            0
        );
    }

    block_map.clear();
    value_map.clear();

    for (auto &bb_ptr : func->block_list) {
        for (
            auto inst_iter = bb_ptr->inst_list.begin();
            inst_iter != bb_ptr->inst_list.end();
        ) {
            if (inst_iter->get()->is<PhiInst>()) {
                auto phi_inst = inst_iter->get()->to<PhiInst>();
                auto dst_reg = current_func->register_nr++;
                value_map.emplace(phi_inst, dst_reg);
                for (auto &branch : *phi_inst) {
                    auto mov_inst = new MovInst(branch.value, phi_inst->getName() + "." + branch.preceder->getName());
                    branch.preceder->inst_list.emplace_back(mov_inst);
                    value_map.emplace(mov_inst, dst_reg);
                }
                phi_ref.emplace_back(std::move(*inst_iter));
                inst_iter = bb_ptr->inst_list.erase(inst_iter);
            }
            else {
                value_map.emplace(inst_iter->get(), current_func->register_nr++);
                inst_iter++;
            }
        }
    }

    for (
        auto bb_iter = func->block_list.begin();
        bb_iter != func->block_list.end();
        ++bb_iter
    ) {
        auto &bb_ptr = *bb_iter;
        block_map.emplace(bb_ptr.get(), current_func->inst_list.size());
        for (auto &inst_ptr : bb_ptr->inst_list) {
            inst_ptr->codegen(this);
        }
        if (bb_ptr->condition) {
            if (bb_ptr->then_block == std::next(bb_iter)->get()) {
                current_func->inst_list.emplace_back(
                    I_BNR,
                    0,
                    value_map.at(bb_ptr->condition),
                    reinterpret_cast<ImmediateT>(bb_ptr->else_block)
                );
            }
            else if (bb_ptr->else_block == std::next(bb_iter)->get()) {
                current_func->inst_list.emplace_back(
                    I_BR,
                    0,
                    value_map.at(bb_ptr->condition),
                    reinterpret_cast<ImmediateT>(bb_ptr->then_block)
                );
            }
            else {
                current_func->inst_list.emplace_back(
                    I_BR,
                    0,
                    value_map.at(bb_ptr->condition),
                    reinterpret_cast<ImmediateT>(bb_ptr->then_block)
                );
                current_func->inst_list.emplace_back(
                    I_JUMP,
                    0,
                    0,
                    reinterpret_cast<ImmediateT>(bb_ptr->else_block)
                );
            }
        }
        else if (bb_ptr->then_block) {
            if (bb_ptr->then_block != std::next(bb_iter)->get()) {
                current_func->inst_list.emplace_back(
                    I_JUMP,
                    0,
                    0,
                    reinterpret_cast<ImmediateT>(bb_ptr->then_block)
                );
            }
        }
        else {
            current_func->inst_list.emplace_back(
                I_RET,
                0,
                0,
                0
            );
        }
    }

    for (auto &inst : current_func->inst_list) {
        if (inst.i_op == I_JUMP || inst.i_op == I_BR || inst.i_op == I_BNR) {
            inst.i_imm = block_map.at(reinterpret_cast<BasicBlock *>(inst.i_imm));
        }
    }

    /*
    std::cerr << func->getName() << std::endl;
    std::cerr << "  Block Map:" << std::endl;
    for (auto &block_pair : block_map) {
        std::cerr << "    " << block_pair.first->getName() << "\t" << block_pair.second << std::endl;
    }
    std::cerr << "  Value Map:" << std::endl;
    for (auto &value_pair : value_map) {
        std::cerr << "    " << value_pair.first->getName() << "\t" << value_pair.second << std::endl;
    }
    std::cerr << std::endl;
    */
}

void
vm::VirtualMachine::Generate::gen(::cyan::Instruction *)
{ assert(false); }

void
vm::VirtualMachine::Generate::gen(SignedImmInst *inst)
{
    current_func->inst_list.emplace_back(
        I_LI,
        0,
        value_map.at(inst),
        inst->getValue()
    );
}

void
vm::VirtualMachine::Generate::gen(UnsignedImmInst *inst) 
{
    current_func->inst_list.emplace_back(
        I_LI,
        0,
        value_map.at(inst),
        inst->getValue()
    );
}

void
vm::VirtualMachine::Generate::gen(GlobalInst *inst)
{
    if (
        inst->getType()->is<FunctionType>() ||
        inst->getType()->is<ArrayType>()
    ) {
        auto temp_reg = current_func->register_nr++;
        current_func->inst_list.emplace_back(
            I_GLOB,
            0,
            temp_reg,
            global_map.at(inst->getValue())
        );
        current_func->inst_list.emplace_back(
#if __CYAN_64__
            I_LOAD64U,
#else
            I_LOAD32U,
#endif
            0,
            value_map.at(inst),
            temp_reg,
            0
        );
    }
    else {
        current_func->inst_list.emplace_back(
            I_GLOB,
            0,
            value_map.at(inst),
            global_map.at(inst->getValue())
        );
    }
}

void
vm::VirtualMachine::Generate::gen(ArgInst *inst)
{
    current_func->inst_list.emplace_back(
        I_ARG,
        0,
        value_map.at(inst),
        inst->getValue()
    );
}

void
vm::VirtualMachine::Generate::gen(AddInst *inst)
{
    if (
        inst->getRight()->getType()->is<PointerType>() ||
        inst->getRight()->getType()->is<VTableType>() ||
        inst->getRight()->getType()->is<StructType>() ||
        inst->getRight()->getType()->is<ConceptType>() ||
        inst->getRight()->getType()->is<FunctionType>()
    ) {
        auto t = inst->getLeft();
        inst->setLeft(inst->getRight());
        inst->setRight(t);
    }

    ShiftT shift;
    if (inst->getLeft()->getType()->is<PointerType>()) {
        auto base_type = inst->getLeft()->getType()->to<PointerType>()->getBaseType();
        if (base_type->is<NumericType>()) {
            shift = __builtin_ctz(base_type->to<NumericType>()->getBitwiseWidth()) - 3;
        }
        else {
            shift = __builtin_ctz(CYAN_PRODUCT_BYTES);
        }
    }
    else if (
        inst->getLeft()->getType()->is<VTableType>() ||
        inst->getLeft()->getType()->is<StructType>() ||
        inst->getLeft()->getType()->is<ConceptType>() ||
        inst->getLeft()->getType()->is<FunctionType>()
    ) {
        shift = __builtin_ctz(CYAN_PRODUCT_BYTES);
    }
    else {
        shift = 0;
    }

    current_func->inst_list.emplace_back(
        I_ADD,
        shift,
        value_map.at(inst),
        value_map.at(inst->getLeft()),
        value_map.at(inst->getRight())
    );
}

void
vm::VirtualMachine::Generate::gen(SubInst *inst)
{
    ShiftT shift;
    if (inst->getLeft()->getType()->is<PointerType>()) {
        auto base_type = inst->getLeft()->getType()->to<PointerType>()->getBaseType();
        if (base_type->is<NumericType>()) {
            shift = __builtin_ctz(base_type->to<NumericType>()->getBitwiseWidth()) - 3;
        }
        else {
            shift = __builtin_ctz(CYAN_PRODUCT_BYTES);
        }
    }
    else if (
        inst->getLeft()->getType()->is<VTableType>() ||
        inst->getLeft()->getType()->is<StructType>() ||
        inst->getLeft()->getType()->is<ConceptType>() ||
        inst->getLeft()->getType()->is<FunctionType>()
    ) {
        shift = __builtin_ctz(CYAN_PRODUCT_BYTES);
    }
    else {
        shift = 0;
    }

    current_func->inst_list.emplace_back(
        I_SUB,
        shift,
        value_map.at(inst),
        value_map.at(inst->getLeft()),
        value_map.at(inst->getRight())
    );
}

void
vm::VirtualMachine::Generate::gen(MulInst *inst)
{
    bool use_unsigned = (inst->getLeft()->getType()->is<UnsignedIntegerType>() ||
                         inst->getRight()->getType()->is<UnsignedIntegerType>());

    current_func->inst_list.emplace_back(
        use_unsigned ? I_MULU : I_MUL,
        0,
        value_map.at(inst),
        value_map.at(inst->getLeft()),
        value_map.at(inst->getRight())
    );
}

void
vm::VirtualMachine::Generate::gen(DivInst *inst)
{
    bool use_unsigned = (inst->getLeft()->getType()->is<UnsignedIntegerType>() ||
                         inst->getRight()->getType()->is<UnsignedIntegerType>());

    current_func->inst_list.emplace_back(
        use_unsigned ? I_DIVU : I_DIV,
        0,
        value_map.at(inst),
        value_map.at(inst->getLeft()),
        value_map.at(inst->getRight())
    );
}

void
vm::VirtualMachine::Generate::gen(ModInst *inst)
{
    bool use_unsigned = (inst->getLeft()->getType()->is<UnsignedIntegerType>() ||
                         inst->getRight()->getType()->is<UnsignedIntegerType>());

    current_func->inst_list.emplace_back(
        use_unsigned ? I_MODU : I_MOD,
        0,
        value_map.at(inst),
        value_map.at(inst->getLeft()),
        value_map.at(inst->getRight())
    );
}

void
vm::VirtualMachine::Generate::gen(ShlInst *inst)
{
    bool use_unsigned = inst->getLeft()->getType()->is<UnsignedIntegerType>();

    current_func->inst_list.emplace_back(
        use_unsigned ? I_SHLU : I_SHL,
        0,
        value_map.at(inst),
        value_map.at(inst->getLeft()),
        value_map.at(inst->getRight())
    );
}

void
vm::VirtualMachine::Generate::gen(ShrInst *inst)
{
    bool use_unsigned = inst->getLeft()->getType()->is<UnsignedIntegerType>();

    current_func->inst_list.emplace_back(
        use_unsigned ? I_SHRU : I_SHR,
        0,
        value_map.at(inst),
        value_map.at(inst->getLeft()),
        value_map.at(inst->getRight())
    );
}

void
vm::VirtualMachine::Generate::gen(OrInst *inst)
{
    current_func->inst_list.emplace_back(
        I_OR,
        0,
        value_map.at(inst),
        value_map.at(inst->getLeft()),
        value_map.at(inst->getRight())
    );
}

void
vm::VirtualMachine::Generate::gen(AndInst *inst)
{
    current_func->inst_list.emplace_back(
        I_AND,
        0,
        value_map.at(inst),
        value_map.at(inst->getLeft()),
        value_map.at(inst->getRight())
    );
}

void
vm::VirtualMachine::Generate::gen(NorInst *inst)
{
    current_func->inst_list.emplace_back(
        I_NOR,
        0,
        value_map.at(inst),
        value_map.at(inst->getLeft()),
        value_map.at(inst->getRight())
    );
}

void
vm::VirtualMachine::Generate::gen(XorInst *inst)
{
    current_func->inst_list.emplace_back(
        I_XOR,
        0,
        value_map.at(inst),
        value_map.at(inst->getLeft()),
        value_map.at(inst->getRight())
    );
}

void
vm::VirtualMachine::Generate::gen(SeqInst *inst)
{
    current_func->inst_list.emplace_back(
        I_SEQ,
        0,
        value_map.at(inst),
        value_map.at(inst->getLeft()),
        value_map.at(inst->getRight())
    );
}

void
vm::VirtualMachine::Generate::gen(SltInst *inst)
{
    bool use_unsigned = (inst->getLeft()->getType()->is<UnsignedIntegerType>() ||
                         inst->getRight()->getType()->is<UnsignedIntegerType>());

    current_func->inst_list.emplace_back(
        use_unsigned ? I_SLTU : I_SLT,
        0,
        value_map.at(inst),
        value_map.at(inst->getLeft()),
        value_map.at(inst->getRight())
    );
}

void
vm::VirtualMachine::Generate::gen(SleInst *inst)
{
    bool use_unsigned = (inst->getLeft()->getType()->is<UnsignedIntegerType>() ||
                         inst->getRight()->getType()->is<UnsignedIntegerType>());

    current_func->inst_list.emplace_back(
        use_unsigned ? I_SLEU : I_SLE,
        0,
        value_map.at(inst),
        value_map.at(inst->getLeft()),
        value_map.at(inst->getRight())
    );
}

void
vm::VirtualMachine::Generate::gen(LoadInst *inst)
{
    OperatorT op;
    if (inst->getAddress()->getType()->is<PointerType>()) {
        auto base_type = inst->getAddress()->getType()->to<PointerType>()->getBaseType();
        if (base_type->is<NumericType>()) {
            auto bits = base_type->to<NumericType>()->getBitwiseWidth();
            auto shift = __builtin_ctz(bits);
            auto is_unsigned = base_type->is<UnsignedIntegerType>() ? 1 : 0;
            op = I_LOAD8 + (shift - 3) * 2 + is_unsigned;
        }
        else {
#if __CYAN_64__
            op = I_LOAD64U;
#else
            op = I_LOAD32U;
#endif
        }
    }
    else {
#if __CYAN_64__
        op = I_LOAD64U;
#else
        op = I_LOAD32U;
#endif
    }

    current_func->inst_list.emplace_back(
        op,
        0,
        value_map.at(inst),
        value_map.at(inst->getAddress()),
        0
    );
}

void
vm::VirtualMachine::Generate::gen(StoreInst *inst)
{
    OperatorT op;
    auto base_type = inst->getAddress()->getType()->to<PointerType>()->getBaseType();
    if (base_type->is<NumericType>()) {
        auto bits = base_type->to<NumericType>()->getBitwiseWidth();
        auto shift = __builtin_ctz(bits);
        auto is_unsigned = base_type->is<UnsignedIntegerType>() ? 1 : 0;
        op = I_STORE8 + (shift - 3) * 2 + is_unsigned;
    }
    else {
#if __CYAN_64__
        op = I_STORE64U;
#else
        op = I_STORE32U;
#endif
    }

    current_func->inst_list.emplace_back(
        op,
        0,
        value_map.at(inst),
        value_map.at(inst->getAddress()),
        value_map.at(inst->getValue())
    );
}

void
vm::VirtualMachine::Generate::gen(AllocaInst *inst)
{
    current_func->inst_list.emplace_back(
        I_ALLOC,
        0,
        value_map.at(inst),
        value_map.at(inst->getSpace()),
        0
    );
}

void
vm::VirtualMachine::Generate::gen(CallInst *inst)
{
    if (inst->arguments_size()) {
        for (intptr_t i = inst->arguments_size() - 1; i >= 0; --i) {
            current_func->inst_list.emplace_back(
                I_PUSH,
                0,
                value_map.at(inst->getArgumentByIndex(i)),
                0
            );
        }
    }

    current_func->inst_list.emplace_back(
        I_CALL,
        0,
        value_map.at(inst),
        value_map.at(inst->getFunction()),
        0
    );

    if (inst->arguments_size()) {
        current_func->inst_list.emplace_back(
            I_POP,
            0,
            0,
            inst->arguments_size()
        );
    }
}

void
vm::VirtualMachine::Generate::gen(RetInst *inst)
{
    current_func->inst_list.emplace_back(
        I_RET,
        0,
        value_map.at(inst),
        inst->getReturnValue() ? value_map.at(inst->getReturnValue()) : 0,
        0
    );
}

void
vm::VirtualMachine::Generate::gen(NewInst *inst)
{
    current_func->inst_list.emplace_back(
        I_NEW,
        0,
        value_map.at(inst),
        value_map.at(inst->getSpace()),
        0
    );

    if (inst->getType()->is<StructType>()) {
        auto struct_type = inst->getType()->to<StructType>();
        auto global_inst = current_func->register_nr++;
        auto addr_inst = current_func->register_nr++;
        auto one_inst = current_func->register_nr++;
        current_func->inst_list.emplace_back(
            I_LI,
            0,
            addr_inst,
            struct_type->members_size()
        );
        current_func->inst_list.emplace_back(
            I_LI,
            0,
            one_inst,
            1
        );
        current_func->inst_list.emplace_back(
            I_ADD,
            __builtin_ctz(CYAN_PRODUCT_BYTES),
            addr_inst,
            value_map.at(inst),
            addr_inst
        );
        for (size_t i = 0; i < struct_type->concept_size(); ++i) {
            current_func->inst_list.emplace_back(
                I_GLOB,
                0,
                global_inst,
                global_map.at(
                    ir->type_pool->getCastedStructType(
                        struct_type,
                        struct_type->getConceptByOffset(
                            static_cast<int>(struct_type->members_size() + i)
                        )
                    )->to_string() + "__vtable"
                )
            );
            current_func->inst_list.emplace_back(
#if __CYAN_64__
                I_STORE64U,
#else
                I_STORE32U,
#endif
                0,
                0,
                addr_inst,
                global_inst
            );
            current_func->inst_list.emplace_back(
                I_ADD,
                __builtin_ctz(CYAN_PRODUCT_BYTES),
                addr_inst,
                addr_inst,
                one_inst
            );
        }
    }
}

void
vm::VirtualMachine::Generate::gen(DeleteInst *inst)
{
    current_func->inst_list.emplace_back(
        I_DELETE,
        0,
        value_map.at(inst),
        value_map.at(inst->getTarget()),
        0
    );
}

void
vm::VirtualMachine::Generate::gen(PhiInst *inst)
{ assert(false); }

void
vm::VirtualMachine::Generate::gen(vm::MovInst *inst)
{
    current_func->inst_list.emplace_back(
        I_MOV,
        0,
        value_map.at(inst),
        value_map.at(inst->getSource()),
        0
    );
}

vm::Slot
vm::VirtualMachine::run()
{
    Frame *current_frame = frame_stack.top().get();
    const Instruction *pc = current_frame->pc;
    auto inst = pc;

#if CYAN_USE_COMPUTED_GOTO

#define VM_CASE(label)          label:
#define VM_DISPATCH()                       \
    do {                                    \
        inst = pc++;                        \
        goto *DISPATCH_TABLE[inst->i_op];   \
    } while(false)

#else

#define VM_CASE(label)          case I_##label:
#define VM_DISPATCH()           break

#endif

#if CYAN_USE_COMPUTED_GOTO
    static void *DISPATCH_TABLE[] = {
        &&UNKNOWN,
        &&ARG,
        &&BR,
        &&BNR,
        &&GLOB,
        &&JUMP,
        &&LI,
        &&ADD,
        &&ALLOC,
        &&AND,
        &&CALL,
        &&DELETE,
        &&DIV,
        &&DIVU,
        &&LOAD8,
        &&LOAD8U,
        &&LOAD16,
        &&LOAD16U,
        &&LOAD32,
        &&LOAD32U,
        &&LOAD64,
        &&LOAD64U,
        &&MOD,
        &&MODU,
        &&MOV,
        &&MUL,
        &&MULU,
        &&NEW,
        &&NOR,
        &&OR,
        &&POP,
        &&PUSH,
        &&RET,
        &&SEQ,
        &&SHL,
        &&SHLU,
        &&SHR,
        &&SHRU,
        &&SLE,
        &&SLEU,
        &&SLT,
        &&SLTU,
        &&STORE8,
        &&STORE8U,
        &&STORE16,
        &&STORE16U,
        &&STORE32,
        &&STORE32U,
        &&STORE64,
        &&STORE64U,
        &&SUB,
        &&XOR,
    };
#else
    while (true) {
        inst = pc++;

        switch (inst->i_op) {
#endif
            VM_CASE(ARG)
                {
                    (*current_frame)[inst->i_rd] = reinterpret_cast<Slot>(
                            stack.data() + current_frame->frame_pointer + inst->i_imm * CYAN_PRODUCT_BYTES);
                    VM_DISPATCH();
                }
            VM_CASE(BR)
                {
                    if ((*current_frame)[inst->i_rd]) {
                        pc = current_frame->func->inst_list.data() + inst->i_imm;
                    }
                    VM_DISPATCH();
                }
            VM_CASE(BNR)
                {
                    if (!(*current_frame)[inst->i_rd]) {
                        pc = current_frame->func->inst_list.data() + inst->i_imm;
                    }
                    VM_DISPATCH();
                }
            VM_CASE(GLOB)
                {
                    (*current_frame)[inst->i_rd] = reinterpret_cast<Slot>(
                            globals.data() + inst->i_imm);
                    VM_DISPATCH();
                }
            VM_CASE(JUMP)
                {
                    pc = current_frame->func->inst_list.data() + inst->i_imm;
                    VM_DISPATCH();
                }
            VM_CASE(LI)
                {
                    (*current_frame)[inst->i_rd] = inst->i_imm;
                    VM_DISPATCH();
                }
            VM_CASE(ADD)
                {
                    (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] +
                                                  ((*current_frame)[inst->i_rt] << inst->i_shift);
                    VM_DISPATCH();
                }
            VM_CASE(ALLOC)
                {
                    auto slots = (*current_frame)[inst->i_rs];
                    (*current_frame)[inst->i_rd] = reinterpret_cast<Slot>(
                            stack.data() + (stack_pointer -= slots * CYAN_PRODUCT_BYTES)
                        );
                    VM_DISPATCH();
                }
            VM_CASE(AND)
                {
                    (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] &
                                                   (*current_frame)[inst->i_rt];
                    VM_DISPATCH();
                }
            VM_CASE(CALL)
                {
                    auto func = reinterpret_cast<Function*>((*current_frame)[inst->i_rs]);
                    if (dynamic_cast<VMFunction*>(func)) {
                        auto vm_func = dynamic_cast<VMFunction*>(func);
                        frame_stack.emplace(new Frame(vm_func, stack_pointer));
                        current_frame->pc = pc - 1;
                        current_frame = frame_stack.top().get();
                        pc = current_frame->pc;
                    }
                    else {
                        auto lib_func = dynamic_cast<LibFunction*>(func);
                        (*current_frame)[inst->i_rd] = lib_func->call(reinterpret_cast<const Slot *>(stack.data() + stack_pointer));
                    }
                    VM_DISPATCH();
                }
            VM_CASE(DELETE)
                {
                    std::free(reinterpret_cast<void*>((*current_frame)[inst->i_rs]));
                    VM_DISPATCH();
                }
            VM_CASE(DIV)
                {
                    (*current_frame)[inst->i_rd] = static_cast<SignedSlot>((*current_frame)[inst->i_rs]) /
                                                   static_cast<SignedSlot>((*current_frame)[inst->i_rt]);
                    VM_DISPATCH();
                }
            VM_CASE(DIVU)
                {
                    (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] /
                                                   (*current_frame)[inst->i_rt];
                    VM_DISPATCH();
                }
            VM_CASE(LOAD8)
                {
                    (*current_frame)[inst->i_rd] = *reinterpret_cast<const int8_t*>((*current_frame)[inst->i_rs]);
                    VM_DISPATCH();
                }
            VM_CASE(LOAD8U)
                {
                    (*current_frame)[inst->i_rd] = *reinterpret_cast<const uint8_t*>((*current_frame)[inst->i_rs]);
                    VM_DISPATCH();
                }
            VM_CASE(LOAD16)
                {
                    (*current_frame)[inst->i_rd] = *reinterpret_cast<const int16_t*>((*current_frame)[inst->i_rs]);
                    VM_DISPATCH();
                }
            VM_CASE(LOAD16U)
                {
                    (*current_frame)[inst->i_rd] = *reinterpret_cast<const uint16_t*>((*current_frame)[inst->i_rs]);
                    VM_DISPATCH();
                }
            VM_CASE(LOAD32)
                {
                    (*current_frame)[inst->i_rd] = *reinterpret_cast<const int32_t*>((*current_frame)[inst->i_rs]);
                    VM_DISPATCH();
                }
            VM_CASE(LOAD32U)
                {
                    (*current_frame)[inst->i_rd] = *reinterpret_cast<const uint32_t*>((*current_frame)[inst->i_rs]);
                    VM_DISPATCH();
                }
            VM_CASE(LOAD64)
                {
                    (*current_frame)[inst->i_rd] = *reinterpret_cast<const int64_t*>((*current_frame)[inst->i_rs]);
                    VM_DISPATCH();
                }
            VM_CASE(LOAD64U)
                {
                    (*current_frame)[inst->i_rd] = *reinterpret_cast<const uint64_t*>((*current_frame)[inst->i_rs]);
                    VM_DISPATCH();
                }
            VM_CASE(MOD)
                {
                    (*current_frame)[inst->i_rd] = static_cast<SignedSlot>((*current_frame)[inst->i_rs]) %
                                                   static_cast<SignedSlot>((*current_frame)[inst->i_rt]);
                    VM_DISPATCH();
                }
            VM_CASE(MODU)
                {
                   (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] %
                                                   (*current_frame)[inst->i_rt];
                   VM_DISPATCH();
                }
            VM_CASE(MOV)
                {
                    (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs];
                    VM_DISPATCH();
                }
            VM_CASE(MUL)
                {
                    (*current_frame)[inst->i_rd] = static_cast<SignedSlot>((*current_frame)[inst->i_rs]) *
                                                   static_cast<SignedSlot>((*current_frame)[inst->i_rt]);
                    VM_DISPATCH();
                }
            VM_CASE(MULU)
                {
                    (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] *
                                                   (*current_frame)[inst->i_rt];
                    VM_DISPATCH();
                }
            VM_CASE(NEW)
                {
                    (*current_frame)[inst->i_rd] = reinterpret_cast<Slot>(std::malloc((*current_frame)[inst->i_rs]));
                    VM_DISPATCH();
                }
            VM_CASE(NOR)
                {
                    (*current_frame)[inst->i_rd] = ~((*current_frame)[inst->i_rs] |
                                                     (*current_frame)[inst->i_rt]);
                    VM_DISPATCH();
                }
            VM_CASE(OR)
                {
                    (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] |
                                                   (*current_frame)[inst->i_rt];
                    VM_DISPATCH();
                }
            VM_CASE(POP)
                {
                    auto slots = inst->i_imm;
                    stack_pointer += slots * CYAN_PRODUCT_BYTES;
                    VM_DISPATCH();
                }
            VM_CASE(PUSH)
                {
                    *reinterpret_cast<Slot*>(
                        stack.data() + (stack_pointer -= CYAN_PRODUCT_BYTES)
                    ) = (*current_frame)[inst->i_rd];
                    VM_DISPATCH();
                }
            VM_CASE(RET)
                {
                    auto ret_val = (*current_frame)[inst->i_rs];
                    stack_pointer = current_frame->frame_pointer;
                    frame_stack.pop();
                    if (frame_stack.empty()) { return ret_val; }

                    current_frame = frame_stack.top().get();
                    pc = current_frame->pc + 1;
                    (*current_frame)[current_frame->pc->i_rd] = ret_val;
                    VM_DISPATCH();
                }
            VM_CASE(SEQ)
                {
                    (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] ==
                                                   (*current_frame)[inst->i_rt];
                    VM_DISPATCH();
                }
            VM_CASE(SHL)
                {
                    (*current_frame)[inst->i_rd] = static_cast<SignedSlot>((*current_frame)[inst->i_rs]) <<
                                                   static_cast<SignedSlot>((*current_frame)[inst->i_rt]);
                    VM_DISPATCH();
                }
            VM_CASE(SHLU)
                {
                    (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] <<
                                                   (*current_frame)[inst->i_rt];
                    VM_DISPATCH();
                }
            VM_CASE(SHR)
                {
                    (*current_frame)[inst->i_rd] = static_cast<SignedSlot>((*current_frame)[inst->i_rs]) >>
                                                   static_cast<SignedSlot>((*current_frame)[inst->i_rt]);
                    VM_DISPATCH();
                }
            VM_CASE(SHRU)
                {
                    (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] >>
                                                   (*current_frame)[inst->i_rt];
                    VM_DISPATCH();
                }
            VM_CASE(SLE)
                {
                    (*current_frame)[inst->i_rd] = static_cast<SignedSlot>((*current_frame)[inst->i_rs]) <=
                                                   static_cast<SignedSlot>((*current_frame)[inst->i_rt]);
                    VM_DISPATCH();
                }
            VM_CASE(SLEU)
                {
                    (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] <=
                                                   (*current_frame)[inst->i_rt];
                    VM_DISPATCH();
                }
            VM_CASE(SLT)
                {
                    (*current_frame)[inst->i_rd] = static_cast<SignedSlot>((*current_frame)[inst->i_rs]) <
                                                   static_cast<SignedSlot>((*current_frame)[inst->i_rt]);
                    VM_DISPATCH();
                }
            VM_CASE(SLTU)
                {
                    (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] <
                                                   (*current_frame)[inst->i_rt];
                    VM_DISPATCH();
                }
            VM_CASE(STORE8)
                {
                    *reinterpret_cast<int8_t*>((*current_frame)[inst->i_rs]) = (*current_frame)[inst->i_rt];
                    VM_DISPATCH();
                }
            VM_CASE(STORE8U)
                {
                    *reinterpret_cast<uint8_t*>((*current_frame)[inst->i_rs]) = (*current_frame)[inst->i_rt];
                    VM_DISPATCH();
                }
            VM_CASE(STORE16)
                {
                    *reinterpret_cast<int16_t*>((*current_frame)[inst->i_rs]) = (*current_frame)[inst->i_rt];
                    VM_DISPATCH();
                }
            VM_CASE(STORE16U)
                {
                    *reinterpret_cast<uint16_t*>((*current_frame)[inst->i_rs]) = (*current_frame)[inst->i_rt];
                    VM_DISPATCH();
                }
            VM_CASE(STORE32)
                {
                    *reinterpret_cast<int32_t*>((*current_frame)[inst->i_rs]) = (*current_frame)[inst->i_rt];
                    VM_DISPATCH();
                }
            VM_CASE(STORE32U)
                {
                    *reinterpret_cast<uint32_t*>((*current_frame)[inst->i_rs]) = (*current_frame)[inst->i_rt];
                    VM_DISPATCH();
                }
            VM_CASE(STORE64)
                {
                    *reinterpret_cast<int64_t*>((*current_frame)[inst->i_rs]) = (*current_frame)[inst->i_rt];
                    VM_DISPATCH();
                }
            VM_CASE(STORE64U)
                {
                    *reinterpret_cast<uint64_t*>((*current_frame)[inst->i_rs]) = (*current_frame)[inst->i_rt];
                    VM_DISPATCH();
                }
            VM_CASE(SUB)
                {
                    (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] -
                                                  ((*current_frame)[inst->i_rt] << inst->i_shift);
                    VM_DISPATCH();
                }
            VM_CASE(XOR)
                {
                    (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] ^
                                                   (*current_frame)[inst->i_rt];
                    VM_DISPATCH();
                }
#if CYAN_USE_COMPUTED_GOTO
            UNKNOWN:
                assert(false);
#else
            default:
                assert(false);
        }
    }
#endif
}

vm::Slot
vm::VirtualMachine::start()
{
    auto init_func = dynamic_cast<VMFunction*>(functions.at("_init_").get());
    assert(init_func);
    frame_stack.emplace(new Frame(init_func, stack_pointer));
    run();

    auto main_func = dynamic_cast<VMFunction*>(functions.at("main").get());
    assert(main_func);
    frame_stack.emplace(new Frame(main_func, stack_pointer));
    return run();
}

std::unique_ptr<vm::VirtualMachine::Generate>
vm::VirtualMachine::GenerateFactory(IR *ir)
{
    return std::unique_ptr<Generate>(new Generate(
            new VirtualMachine(),
            ir
        ));
}
