//
// Created by Clarkok on 16/5/3.
//

#include <cassert>

#include "ir.hpp"
#include "ir_builder.hpp"

using namespace cyan;

Instruction *
IRBuilder::BlockBuilder::SignedImmInst(SignedIntegerType *type, intptr_t value, std::string name)
{
    assert(!productEnded());
    Instruction *ret;
    product->inst_list.emplace_back(ret = new class SignedImmInst(type, value, tempName(name)));
    return ret;
}

Instruction *
IRBuilder::BlockBuilder::UnsignedImmInst(UnsignedIntegerType *type, uintptr_t value, std::string name)
{
    assert(!productEnded());
    Instruction *ret;
    product->inst_list.emplace_back(ret = new class UnsignedImmInst(type, value, tempName(name)));
    return ret;
}

Instruction *
IRBuilder::BlockBuilder::GlobalInst(PointerType *type, std::string value, std::string name)
{
    assert(!productEnded());
    Instruction *ret;
    product->inst_list.emplace_back(ret = new class GlobalInst(type, value, tempName(name)));
    return ret;
}

Instruction *
IRBuilder::BlockBuilder::ArgInst(PointerType *type, intptr_t value, std::string name)
{
    assert(!productEnded());
    Instruction *ret;
    product->inst_list.emplace_back(ret = new class ArgInst(type, value, tempName(name)));
    return ret;
}

Instruction *
IRBuilder::BlockBuilder::AddInst(Type *type, Instruction *left, Instruction *right, std::string name)
{
    assert(!productEnded());
    Instruction *ret;
    product->inst_list.emplace_back(ret = new class AddInst(type, left, right, tempName(name)));
    left->reference();
    right->reference();
    return ret;
}

Instruction *
IRBuilder::BlockBuilder::SubInst(Type *type, Instruction *left, Instruction *right, std::string name)
{
    assert(!productEnded());
    Instruction *ret;
    product->inst_list.emplace_back(ret = new class SubInst(type, left, right, tempName(name)));
    left->reference();
    right->reference();
    return ret;
}

Instruction *
IRBuilder::BlockBuilder::MulInst(Type *type, Instruction *left, Instruction *right, std::string name)
{
    assert(!productEnded());
    Instruction *ret;
    product->inst_list.emplace_back(ret = new class MulInst(type, left, right, tempName(name)));
    left->reference();
    right->reference();
    return ret;
}

Instruction *
IRBuilder::BlockBuilder::DivInst(Type *type, Instruction *left, Instruction *right, std::string name)
{
    assert(!productEnded());
    Instruction *ret;
    product->inst_list.emplace_back(ret = new class DivInst(type, left, right, tempName(name)));
    left->reference();
    right->reference();
    return ret;
}

Instruction *
IRBuilder::BlockBuilder::ModInst(Type *type, Instruction *left, Instruction *right, std::string name)
{
    assert(!productEnded());
    Instruction *ret;
    product->inst_list.emplace_back(ret = new class ModInst(type, left, right, tempName(name)));
    left->reference();
    right->reference();
    return ret;
}

Instruction *
IRBuilder::BlockBuilder::ShlInst(Type *type, Instruction *left, Instruction *right, std::string name)
{
    assert(!productEnded());
    Instruction *ret;
    product->inst_list.emplace_back(ret = new class ShlInst(type, left, right, tempName(name)));
    left->reference();
    right->reference();
    return ret;
}

Instruction *
IRBuilder::BlockBuilder::ShrInst(Type *type, Instruction *left, Instruction *right, std::string name)
{
    assert(!productEnded());
    Instruction *ret;
    product->inst_list.emplace_back(ret = new class ShrInst(type, left, right, tempName(name)));
    left->reference();
    right->reference();
    return ret;
}

Instruction *
IRBuilder::BlockBuilder::OrInst(Type *type, Instruction *left, Instruction *right, std::string name)
{
    assert(!productEnded());
    Instruction *ret;
    product->inst_list.emplace_back(ret = new class OrInst(type, left, right, tempName(name)));
    left->reference();
    right->reference();
    return ret;
}

Instruction *
IRBuilder::BlockBuilder::AndInst(Type *type, Instruction *left, Instruction *right, std::string name)
{
    assert(!productEnded());
    Instruction *ret;
    product->inst_list.emplace_back(ret = new class AndInst(type, left, right, tempName(name)));
    left->reference();
    right->reference();
    return ret;
}

Instruction *
IRBuilder::BlockBuilder::NorInst(Type *type, Instruction *left, Instruction *right, std::string name)
{
    assert(!productEnded());
    Instruction *ret;
    product->inst_list.emplace_back(ret = new class NorInst(type, left, right, tempName(name)));
    left->reference();
    right->reference();
    return ret;
}

Instruction *
IRBuilder::BlockBuilder::XorInst(Type *type, Instruction *left, Instruction *right, std::string name)
{
    assert(!productEnded());
    Instruction *ret;
    product->inst_list.emplace_back(ret = new class XorInst(type, left, right, tempName(name)));
    left->reference();
    right->reference();
    return ret;
}

Instruction *
IRBuilder::BlockBuilder::SeqInst(Type *type, Instruction *left, Instruction *right, std::string name)
{
    assert(!productEnded());
    Instruction *ret;
    product->inst_list.emplace_back(ret = new class SeqInst(type, left, right, tempName(name)));
    left->reference();
    right->reference();
    return ret;
}

Instruction *
IRBuilder::BlockBuilder::SltInst(Type *type, Instruction *left, Instruction *right, std::string name)
{
    assert(!productEnded());
    Instruction *ret;
    product->inst_list.emplace_back(ret = new class SltInst(type, left, right, tempName(name)));
    left->reference();
    right->reference();
    return ret;
}

Instruction *
IRBuilder::BlockBuilder::SleInst(Type *type, Instruction *left, Instruction *right, std::string name)
{
    assert(!productEnded());
    Instruction *ret;
    product->inst_list.emplace_back(ret = new class SleInst(type, left, right, tempName(name)));
    left->reference();
    right->reference();
    return ret;
}

Instruction *
IRBuilder::BlockBuilder::LoadInst(Type *type, Instruction *address, std::string name)
{
    assert(!productEnded());
    Instruction *ret;
    product->inst_list.emplace_back(ret = new class LoadInst(type, address, tempName(name)));
    address->reference();
    return ret;
}

Instruction *
IRBuilder::BlockBuilder::StoreInst(Type *type, Instruction *address, Instruction *value, std::string name)
{
    assert(!productEnded());
    Instruction *ret;
    product->inst_list.emplace_back(ret = new class StoreInst(type, address, value, tempName(name)));
    address->reference();
    value->reference();
    return ret;
}

Instruction *
IRBuilder::BlockBuilder::AllocaInst(Type *type, Instruction *space, std::string name)
{
    assert(!productEnded());
    Instruction *ret;
    product->inst_list.emplace_back(ret = new class AllocaInst(type, space, tempName(name)));
    space->reference();
    return ret;
}

Instruction *
IRBuilder::BlockBuilder::RetInst(Type *type, Instruction *return_value)
{
    assert(!productEnded());
    Instruction *ret;
    product->inst_list.emplace_back(ret = new class RetInst(type, return_value));
    if (return_value) {
        return_value->reference();
    }
    return ret;
}

void
IRBuilder::BlockBuilder::JumpInst(BasicBlock *block)
{
    assert(!productEnded());
    product->then_block = block;
}

void
IRBuilder::BlockBuilder::BrInst(Instruction *condition, BasicBlock *then_block, BasicBlock *else_block)
{
    assert(!productEnded());
    product->condition = condition;
    product->then_block = then_block;
    product->else_block = else_block;
    condition->reference();
}

Instruction *
IRBuilder::BlockBuilder::PhiInst(class PhiInst *inst)
{
    assert(!productEnded());

    for (auto &branch : *inst) {
        branch.value->reference();
    }

    product->inst_list.emplace_back(inst);
    return inst;
}

Instruction *
IRBuilder::BlockBuilder::PhiBuilder::commit()
{ return owner->PhiInst(builder.release()); }

Instruction *
IRBuilder::BlockBuilder::CallInst(class CallInst *inst)
{
    assert(!productEnded());

    for (auto &argument : *inst) {
        argument->reference();
    }

    product->inst_list.emplace_back(inst);
    return inst;
}

Instruction *
IRBuilder::BlockBuilder::CallBuilder::commit()
{ return owner->CallInst(builder.release()); }

std::unique_ptr<IRBuilder::BlockBuilder>
IRBuilder::FunctionBuilder::newBasicBlock(std::string name)
{
    product->block_list.emplace_back(
        std::unique_ptr<BasicBlock>(new BasicBlock(tempName(name)))
    );
    return std::unique_ptr<BlockBuilder>(new BlockBuilder(owner, this, product->block_list.back().get()));
}

std::unique_ptr<IRBuilder::FunctionBuilder>
IRBuilder::newFunction(std::string name, FunctionType *prototype)
{
    Function *func;
    product->function_table.emplace(name, std::unique_ptr<Function>(func = new Function(name, prototype)));
    return std::unique_ptr<FunctionBuilder>(new FunctionBuilder(this, func));
}

std::unique_ptr<IRBuilder::FunctionBuilder>
IRBuilder::findFunction(std::string name)
{
    assert(product->function_table.find(name) != product->function_table.end());
    return std::unique_ptr<FunctionBuilder>(
            new FunctionBuilder(this, product->function_table[name].get())
        );
}
