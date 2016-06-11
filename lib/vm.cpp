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

    std::cerr << func->getName() << std::endl;
    std::cerr << "  Block Map:" << std::endl;
    for (auto &block_pair : block_map) {
        std::cerr << "    " << block_pair.first->getName() << "\t" << block_pair.second << std::endl;
    }
    std::cerr << "  Inst Map:" << std::endl;
    for (auto &value_pair : value_map) {
        std::cerr << "    " << value_pair.first->getName() << "\t" << value_pair.second << std::endl;
    }
    std::cerr << std::endl;
}

cyan::vm::TypeT
vm::VirtualMachine::Generate::resolveType(::cyan::Instruction *inst)
{
    if (inst->is<AddInst>() || inst->is<SubInst>()) {
        auto bin_inst = inst->to<BinaryInst>();
        if (
            bin_inst->getLeft()->getType()->is<PointerType>() ||
            bin_inst->getLeft()->getType()->is<VTableType>() ||
            bin_inst->getRight()->getType()->is<PointerType>() ||
            bin_inst->getRight()->getType()->is<VTableType>()
        ) {
            return get_type(T_POINTER, __builtin_ctz(CYAN_PRODUCT_BYTES));
        }
        else if (
            bin_inst->getLeft()->getType()->is<UnsignedIntegerType>() ||
            bin_inst->getRight()->getType()->is<UnsignedIntegerType>()
        ) {
            return get_type(T_UNSIGNED, 0);
        }
        else {
            return get_type(T_SIGNED, 0);
        }
    }
    else if (inst->is<BinaryInst>()) {
        auto bin_inst = inst->to<BinaryInst>();
        assert(bin_inst->getLeft()->getType()->is<NumericType>() &&
               bin_inst->getRight()->getType()->is<NumericType>());
        if (bin_inst->getLeft()->getType()->is<UnsignedIntegerType>() ||
            bin_inst->getRight()->getType()->is<UnsignedIntegerType>()) {
            return get_type(T_UNSIGNED, 0);
        }
        else {
            return get_type(T_SIGNED, 0);
        }
    }
    else if (inst->is<StoreInst>()) {
        auto store_inst = inst->to<StoreInst>();
        if (store_inst->getValue()->getType()->is<NumericType>()) {
            auto bits = store_inst->getValue()->getType()->to<NumericType>()->getBitwiseWidth();
            return get_type(T_POINTER, __builtin_ctz(bits / 8));
        }
        else {
            return get_type(T_POINTER, __builtin_ctz(CYAN_PRODUCT_BYTES));
        }
    }
    else if (inst->is<LoadInst>()) {
        auto load_inst = inst->to<LoadInst>();
        if (load_inst->getAddress()->getType()->to<PointerType>()->getBaseType()->is<NumericType>()) {
            auto bits = load_inst->getAddress()->getType()->to<PointerType>()->getBaseType()->to<NumericType>()->getBitwiseWidth();
            return get_type(T_POINTER, __builtin_ctz(bits / 8));
        }
        else {
            return get_type(T_POINTER, __builtin_ctz(CYAN_PRODUCT_BYTES));
        }
    }
    else {
        return 0;
    }
}

void
vm::VirtualMachine::Generate::gen(::cyan::Instruction *)
{ assert(false); }

void
vm::VirtualMachine::Generate::gen(SignedImmInst *inst)
{
    current_func->inst_list.emplace_back(
        I_LI,
        get_type(T_SIGNED, 0),
        value_map.at(inst),
        inst->getValue()
    );
}

void
vm::VirtualMachine::Generate::gen(UnsignedImmInst *inst) 
{
    current_func->inst_list.emplace_back(
        I_LI,
        get_type(T_UNSIGNED, 0),
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
            get_type(T_POINTER, 0),
            temp_reg,
            global_map.at(inst->getValue())
        );
        current_func->inst_list.emplace_back(
            I_LOAD,
            get_type(T_POINTER, __builtin_ctz(CYAN_PRODUCT_BYTES)),
            value_map.at(inst),
            temp_reg,
            0
        );
    }
    else {
        current_func->inst_list.emplace_back(
            I_GLOB,
            get_type(T_POINTER, 0),
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
        get_type(T_POINTER, 0),
        value_map.at(inst),
        inst->getValue()
    );
}

void
vm::VirtualMachine::Generate::gen(AddInst *inst)
{
    if (
        inst->getRight()->getType()->is<PointerType>() ||
        inst->getRight()->getType()->is<VTableType>()
    ) {
        current_func->inst_list.emplace_back(
            I_ADD,
            resolveType(inst),
            value_map.at(inst),
            value_map.at(inst->getRight()),
            value_map.at(inst->getLeft())
        );
    }
    else {
        current_func->inst_list.emplace_back(
            I_ADD,
            resolveType(inst),
            value_map.at(inst),
            value_map.at(inst->getLeft()),
            value_map.at(inst->getRight())
        );
    }
}

void
vm::VirtualMachine::Generate::gen(SubInst *inst)
{
    current_func->inst_list.emplace_back(
        I_SUB,
        resolveType(inst),
        value_map.at(inst),
        value_map.at(inst->getLeft()),
        value_map.at(inst->getRight())
    );
}

void
vm::VirtualMachine::Generate::gen(MulInst *inst)
{
    current_func->inst_list.emplace_back(
        I_MUL,
        resolveType(inst),
        value_map.at(inst),
        value_map.at(inst->getLeft()),
        value_map.at(inst->getRight())
    );
}

void
vm::VirtualMachine::Generate::gen(DivInst *inst)
{
    current_func->inst_list.emplace_back(
        I_DIV,
        resolveType(inst),
        value_map.at(inst),
        value_map.at(inst->getLeft()),
        value_map.at(inst->getRight())
    );
}

void
vm::VirtualMachine::Generate::gen(ModInst *inst)
{
    current_func->inst_list.emplace_back(
        I_MOD,
        resolveType(inst),
        value_map.at(inst),
        value_map.at(inst->getLeft()),
        value_map.at(inst->getRight())
    );
}

void
vm::VirtualMachine::Generate::gen(ShlInst *inst)
{
    current_func->inst_list.emplace_back(
        I_SHL,
        resolveType(inst),
        value_map.at(inst),
        value_map.at(inst->getLeft()),
        value_map.at(inst->getRight())
    );
}

void
vm::VirtualMachine::Generate::gen(ShrInst *inst)
{
    current_func->inst_list.emplace_back(
        I_SHR,
        resolveType(inst),
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
        resolveType(inst),
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
        resolveType(inst),
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
        resolveType(inst),
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
        resolveType(inst),
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
        resolveType(inst),
        value_map.at(inst),
        value_map.at(inst->getLeft()),
        value_map.at(inst->getRight())
    );
}

void
vm::VirtualMachine::Generate::gen(SltInst *inst)
{
    current_func->inst_list.emplace_back(
        I_SLT,
        resolveType(inst),
        value_map.at(inst),
        value_map.at(inst->getLeft()),
        value_map.at(inst->getRight())
    );
}

void
vm::VirtualMachine::Generate::gen(SleInst *inst)
{
    current_func->inst_list.emplace_back(
        I_SLE,
        resolveType(inst),
        value_map.at(inst),
        value_map.at(inst->getLeft()),
        value_map.at(inst->getRight())
    );
}

void
vm::VirtualMachine::Generate::gen(LoadInst *inst)
{
    current_func->inst_list.emplace_back(
        I_LOAD,
        resolveType(inst),
        value_map.at(inst),
        value_map.at(inst->getAddress()),
        0
    );
}

void
vm::VirtualMachine::Generate::gen(StoreInst *inst)
{
    current_func->inst_list.emplace_back(
        I_STORE,
        resolveType(inst),
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
        resolveType(inst),
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
        resolveType(inst),
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
        resolveType(inst),
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
        resolveType(inst),
        value_map.at(inst),
        value_map.at(inst->getSpace()),
        0
    );
}

void
vm::VirtualMachine::Generate::gen(DeleteInst *inst)
{
    current_func->inst_list.emplace_back(
        I_DELETE,
        resolveType(inst),
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
        resolveType(inst),
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

    while (true) {
        auto inst = pc++;

        switch (inst->i_op) {
            case I_ARG:
                {
                    (*current_frame)[inst->i_rd] = reinterpret_cast<Slot>(
                            stack.data() + stack_pointer + inst->i_imm * CYAN_PRODUCT_BYTES);
                    break;
                }
            case I_BR:
                {
                    if ((*current_frame)[inst->i_rd]) {
                        pc = current_frame->func->inst_list.data() + inst->i_imm;
                    }
                    break;
                }
            case I_BNR:
                {
                    if (!(*current_frame)[inst->i_rd]) {
                        pc = current_frame->func->inst_list.data() + inst->i_imm;
                    }
                    break;
                }
            case I_GLOB:
                {
                    (*current_frame)[inst->i_rd] = reinterpret_cast<Slot>(
                            globals.data() + inst->i_imm);
                    break;
                }
            case I_JUMP:
                {
                    pc = current_frame->func->inst_list.data() + inst->i_imm;
                    break;
                }
            case I_LI:
                {
                    (*current_frame)[inst->i_rd] = inst->i_imm;
                    break;
                }
            case I_ADD:
                {
                    (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] +
                                                  ((*current_frame)[inst->i_rt] << type2shift(inst->i_type));
                    break;
                }
            case I_ALLOC:
                {
                    auto slots = (*current_frame)[inst->i_rs];
                    current_frame->stack_usage += slots * CYAN_PRODUCT_BYTES;
                    (*current_frame)[inst->i_rd] = reinterpret_cast<Slot>(
                            stack.data() + (stack_pointer -= slots * CYAN_PRODUCT_BYTES)
                        );
                    break;
                }
            case I_AND:
                {
                    (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] &
                                                   (*current_frame)[inst->i_rt];
                    break;
                }
            case I_CALL:
                {
                    auto func = reinterpret_cast<Function*>((*current_frame)[inst->i_rs]);
                    if (dynamic_cast<VMFunction*>(func)) {
                        auto vm_func = dynamic_cast<VMFunction*>(func);
                        frame_stack.emplace(new Frame(vm_func));
                        current_frame->pc = pc - 1;
                        current_frame = frame_stack.top().get();
                        pc = current_frame->pc;
                    }
                    else {
                        auto lib_func = dynamic_cast<LibFunction*>(func);
                        (*current_frame)[inst->i_rd] = lib_func->call(reinterpret_cast<const Slot *>(stack.data() + stack_pointer));
                    }
                    break;
                }
            case I_DELETE:
                {
                    std::free(reinterpret_cast<void*>((*current_frame)[inst->i_rs]));
                    break;
                }
            case I_DIV:
                {
                    if (type2type(inst->i_type) == T_SIGNED) {
                        (*current_frame)[inst->i_rd] = static_cast<SignedSlot>((*current_frame)[inst->i_rs]) /
                                                       static_cast<SignedSlot>((*current_frame)[inst->i_rt]);
                    }
                    else {
                        (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] /
                                                       (*current_frame)[inst->i_rt];
                    }
                    break;
                }
            case I_LOAD:
                {
                    switch ((type2shift(inst->i_type) << 1) | (type2type(inst->i_type) == T_SIGNED)) {
                        case 0:
                            (*current_frame)[inst->i_rd] = *reinterpret_cast<const uint8_t*>((*current_frame)[inst->i_rs]);
                            break;
                        case 1:
                            (*current_frame)[inst->i_rd] = *reinterpret_cast<const int8_t*>((*current_frame)[inst->i_rs]);
                            break;
                        case 2:
                            (*current_frame)[inst->i_rd] = *reinterpret_cast<const uint16_t*>((*current_frame)[inst->i_rs]);
                            break;
                        case 3:
                            (*current_frame)[inst->i_rd] = *reinterpret_cast<const int16_t*>((*current_frame)[inst->i_rs]);
                            break;
                        case 4:
                            (*current_frame)[inst->i_rd] = *reinterpret_cast<const uint32_t*>((*current_frame)[inst->i_rs]);
                            break;
                        case 5:
                            (*current_frame)[inst->i_rd] = *reinterpret_cast<const int32_t*>((*current_frame)[inst->i_rs]);
                            break;
                        case 6:
                            (*current_frame)[inst->i_rd] = *reinterpret_cast<const uint64_t*>((*current_frame)[inst->i_rs]);
                            break;
                        case 7:
                            (*current_frame)[inst->i_rd] = *reinterpret_cast<const int64_t*>((*current_frame)[inst->i_rs]);
                            break;
                        default:
                            assert(false);
                    }
                    break;
                }
            case I_MOD:
                {
                    if (type2type(inst->i_type) == T_SIGNED) {
                        (*current_frame)[inst->i_rd] = static_cast<SignedSlot>((*current_frame)[inst->i_rs]) %
                                                       static_cast<SignedSlot>((*current_frame)[inst->i_rt]);
                    }
                    else {
                        (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] %
                                                       (*current_frame)[inst->i_rt];
                    }
                    break;
                }
            case I_MOV:
                {
                    (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs];
                    break;
                }
            case I_MUL:
                {
                    if (type2type(inst->i_type) == T_SIGNED) {
                        (*current_frame)[inst->i_rd] = static_cast<SignedSlot>((*current_frame)[inst->i_rs]) *
                                                       static_cast<SignedSlot>((*current_frame)[inst->i_rt]);
                    }
                    else {
                        (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] *
                                                       (*current_frame)[inst->i_rt];
                    }
                    break;
                }
            case I_NEW:
                {
                    (*current_frame)[inst->i_rd] = reinterpret_cast<Slot>(std::malloc((*current_frame)[inst->i_rs]));
                    break;
                }
            case I_NOR:
                {
                    (*current_frame)[inst->i_rd] = ~((*current_frame)[inst->i_rs] |
                                                     (*current_frame)[inst->i_rt]);
                    break;
                }
            case I_OR:
                {
                    (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] |
                                                   (*current_frame)[inst->i_rt];
                    break;
                }
            case I_POP:
                {
                    auto slots = inst->i_imm;
                    current_frame->stack_usage -= slots * CYAN_PRODUCT_BYTES;
                    stack_pointer += slots * CYAN_PRODUCT_BYTES;
                    break;
                }
            case I_PUSH:
                {
                    current_frame->stack_usage += CYAN_PRODUCT_BYTES;
                    *reinterpret_cast<Slot*>(
                        stack.data() + (stack_pointer -= CYAN_PRODUCT_BYTES)
                    ) = (*current_frame)[inst->i_rd];
                    break;
                }
            case I_RET:
                {
                    auto ret_val = (*current_frame)[inst->i_rs];
                    stack_pointer += current_frame->stack_usage;
                    frame_stack.pop();
                    if (frame_stack.empty()) { return ret_val; }

                    current_frame = frame_stack.top().get();
                    pc = current_frame->pc + 1;
                    (*current_frame)[current_frame->pc->i_rd] = ret_val;
                    break;
                }
            case I_SEQ:
                {
                    (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] ==
                                                   (*current_frame)[inst->i_rt];
                    break;
                }
            case I_SHL:
                {
                    if (type2type(inst->i_type) == T_SIGNED) {
                        (*current_frame)[inst->i_rd] = static_cast<SignedSlot>((*current_frame)[inst->i_rs]) <<
                                                       static_cast<SignedSlot>((*current_frame)[inst->i_rt]);
                    }
                    else {
                        (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] <<
                                                       (*current_frame)[inst->i_rt];
                    }
                    break;
                }
            case I_SHR:
                {
                    if (type2type(inst->i_type) == T_SIGNED) {
                        (*current_frame)[inst->i_rd] = static_cast<SignedSlot>((*current_frame)[inst->i_rs]) >>
                                                       static_cast<SignedSlot>((*current_frame)[inst->i_rt]);
                    }
                    else {
                        (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] >>
                                                       (*current_frame)[inst->i_rt];
                    }
                    break;
                }
            case I_SLE:
                {
                    if (type2type(inst->i_type) == T_SIGNED) {
                        (*current_frame)[inst->i_rd] = static_cast<SignedSlot>((*current_frame)[inst->i_rs]) <=
                                                       static_cast<SignedSlot>((*current_frame)[inst->i_rt]);
                    }
                    else {
                        (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] <=
                                                       (*current_frame)[inst->i_rt];
                    }
                    break;
                }
            case I_SLT:
                {
                    if (type2type(inst->i_type) == T_SIGNED) {
                        (*current_frame)[inst->i_rd] = static_cast<SignedSlot>((*current_frame)[inst->i_rs]) <
                                                       static_cast<SignedSlot>((*current_frame)[inst->i_rt]);
                    }
                    else {
                        (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] <
                                                       (*current_frame)[inst->i_rt];
                    }
                    break;
                }
            case I_STORE:
                {
                    switch ((type2shift(inst->i_type) << 1) | (type2type(inst->i_type) == T_SIGNED)) {
                        case 0:
                            *reinterpret_cast<uint8_t*>((*current_frame)[inst->i_rs]) = (*current_frame)[inst->i_rt];
                            break;
                        case 1:
                            *reinterpret_cast<int8_t*>((*current_frame)[inst->i_rs]) = (*current_frame)[inst->i_rt];
                            break;
                        case 2:
                            *reinterpret_cast<uint16_t*>((*current_frame)[inst->i_rs]) = (*current_frame)[inst->i_rt];
                            break;
                        case 3:
                            *reinterpret_cast<int16_t*>((*current_frame)[inst->i_rs]) = (*current_frame)[inst->i_rt];
                            break;
                        case 4:
                            *reinterpret_cast<uint32_t*>((*current_frame)[inst->i_rs]) = (*current_frame)[inst->i_rt];
                            break;
                        case 5:
                            *reinterpret_cast<int32_t*>((*current_frame)[inst->i_rs]) = (*current_frame)[inst->i_rt];
                            break;
                        case 6:
                            *reinterpret_cast<uint64_t*>((*current_frame)[inst->i_rs]) = (*current_frame)[inst->i_rt];
                            break;
                        case 7:
                            *reinterpret_cast<int64_t*>((*current_frame)[inst->i_rs]) = (*current_frame)[inst->i_rt];
                            break;
                        default:
                            assert(false);
                    }
                    break;
                }
            case I_SUB:
                {
                    (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] -
                                                  ((*current_frame)[inst->i_rt] << type2shift(inst->i_type));
                    break;
                }
            case I_XOR:
                {
                    (*current_frame)[inst->i_rd] = (*current_frame)[inst->i_rs] ^
                                                   (*current_frame)[inst->i_rt];
                    break;
                }
            default:
                assert(false);
        }
    }
}

vm::Slot
vm::VirtualMachine::start()
{
    auto init_func = dynamic_cast<VMFunction*>(functions.at("_init_").get());
    assert(init_func);
    frame_stack.emplace(new Frame(init_func));
    run();

    auto main_func = dynamic_cast<VMFunction*>(functions.at("main").get());
    assert(main_func);
    frame_stack.emplace(new Frame(main_func));
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
