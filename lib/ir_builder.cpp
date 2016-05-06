//
// Created by Clarkok on 16/5/3.
//

#include <cassert>

#include "ir.hpp"
#include "ir_builder.hpp"

using namespace cyan;

Instrument *
IRBuilder::BlockBuilder::SignedImmInst(SignedIntegerType *type, intptr_t value, std::string name)
{
    assert(!productEnded());
    Instrument *ret;
    product->inst_list.emplace_back(ret = new class SignedImmInst(type, value, tempName(name)));
    return ret;
}

Instrument *
IRBuilder::BlockBuilder::UnsignedImmInst(UnsignedIntegerType *type, uintptr_t value, std::string name)
{
    assert(!productEnded());
    Instrument *ret;
    product->inst_list.emplace_back(ret = new class UnsignedImmInst(type, value, tempName(name)));
    return ret;
}

Instrument *
IRBuilder::BlockBuilder::GlobalInst(PointerType *type, std::string value, std::string name)
{
    assert(!productEnded());
    Instrument *ret;
    product->inst_list.emplace_back(ret = new class GlobalInst(type, value, tempName(name)));
    return ret;
}

Instrument *
IRBuilder::BlockBuilder::ArgInst(PointerType *type, intptr_t value, std::string name)
{
    assert(!productEnded());
    Instrument *ret;
    product->inst_list.emplace_back(ret = new class ArgInst(type, value, tempName(name)));
    return ret;
}

Instrument *
IRBuilder::BlockBuilder::AddInst(Type *type, Instrument *left, Instrument *right, std::string name)
{
    assert(!productEnded());
    Instrument *ret;
    product->inst_list.emplace_back(ret = new class AddInst(type, left, right, tempName(name)));
    return ret;
}

Instrument *
IRBuilder::BlockBuilder::SubInst(Type *type, Instrument *left, Instrument *right, std::string name)
{
    assert(!productEnded());
    Instrument *ret;
    product->inst_list.emplace_back(ret = new class SubInst(type, left, right, tempName(name)));
    return ret;
}

Instrument *
IRBuilder::BlockBuilder::MulInst(Type *type, Instrument *left, Instrument *right, std::string name)
{
    assert(!productEnded());
    Instrument *ret;
    product->inst_list.emplace_back(ret = new class MulInst(type, left, right, tempName(name)));
    return ret;
}

Instrument *
IRBuilder::BlockBuilder::DivInst(Type *type, Instrument *left, Instrument *right, std::string name)
{
    assert(!productEnded());
    Instrument *ret;
    product->inst_list.emplace_back(ret = new class DivInst(type, left, right, tempName(name)));
    return ret;
}

Instrument *
IRBuilder::BlockBuilder::ModInst(Type *type, Instrument *left, Instrument *right, std::string name)
{
    assert(!productEnded());
    Instrument *ret;
    product->inst_list.emplace_back(ret = new class ModInst(type, left, right, tempName(name)));
    return ret;
}

Instrument *
IRBuilder::BlockBuilder::ShlInst(Type *type, Instrument *left, Instrument *right, std::string name)
{
    assert(!productEnded());
    Instrument *ret;
    product->inst_list.emplace_back(ret = new class ShlInst(type, left, right, tempName(name)));
    return ret;
}

Instrument *
IRBuilder::BlockBuilder::ShrInst(Type *type, Instrument *left, Instrument *right, std::string name)
{
    assert(!productEnded());
    Instrument *ret;
    product->inst_list.emplace_back(ret = new class ShrInst(type, left, right, tempName(name)));
    return ret;
}

Instrument *
IRBuilder::BlockBuilder::OrInst(Type *type, Instrument *left, Instrument *right, std::string name)
{
    assert(!productEnded());
    Instrument *ret;
    product->inst_list.emplace_back(ret = new class OrInst(type, left, right, tempName(name)));
    return ret;
}

Instrument *
IRBuilder::BlockBuilder::AndInst(Type *type, Instrument *left, Instrument *right, std::string name)
{
    assert(!productEnded());
    Instrument *ret;
    product->inst_list.emplace_back(ret = new class AndInst(type, left, right, tempName(name)));
    return ret;
}

Instrument *
IRBuilder::BlockBuilder::NorInst(Type *type, Instrument *left, Instrument *right, std::string name)
{
    assert(!productEnded());
    Instrument *ret;
    product->inst_list.emplace_back(ret = new class NorInst(type, left, right, tempName(name)));
    return ret;
}

Instrument *
IRBuilder::BlockBuilder::XorInst(Type *type, Instrument *left, Instrument *right, std::string name)
{
    assert(!productEnded());
    Instrument *ret;
    product->inst_list.emplace_back(ret = new class XorInst(type, left, right, tempName(name)));
    return ret;
}

Instrument *
IRBuilder::BlockBuilder::SeqInst(Type *type, Instrument *left, Instrument *right, std::string name)
{
    assert(!productEnded());
    Instrument *ret;
    product->inst_list.emplace_back(ret = new class SeqInst(type, left, right, tempName(name)));
    return ret;
}

Instrument *
IRBuilder::BlockBuilder::SltInst(Type *type, Instrument *left, Instrument *right, std::string name)
{
    assert(!productEnded());
    Instrument *ret;
    product->inst_list.emplace_back(ret = new class SltInst(type, left, right, tempName(name)));
    return ret;
}

Instrument *
IRBuilder::BlockBuilder::SleInst(Type *type, Instrument *left, Instrument *right, std::string name)
{
    assert(!productEnded());
    Instrument *ret;
    product->inst_list.emplace_back(ret = new class SleInst(type, left, right, tempName(name)));
    return ret;
}

Instrument *
IRBuilder::BlockBuilder::LoadInst(Type *type, Instrument *address, std::string name)
{
    assert(!productEnded());
    Instrument *ret;
    product->inst_list.emplace_back(ret = new class LoadInst(type, address, tempName(name)));
    return ret;
}

Instrument *
IRBuilder::BlockBuilder::StoreInst(Type *type, Instrument *address, Instrument *value, std::string name)
{
    assert(!productEnded());
    Instrument *ret;
    product->inst_list.emplace_back(ret = new class StoreInst(type, address, value, tempName(name)));
    return ret;
}

Instrument *
IRBuilder::BlockBuilder::AllocaInst(Type *type, Instrument *space, std::string name)
{
    assert(!productEnded());
    Instrument *ret;
    product->inst_list.emplace_back(ret = new class AllocaInst(type, space, tempName(name)));
    return ret;
}

Instrument *
IRBuilder::BlockBuilder::RetInst(Type *type, Instrument *return_value)
{
    assert(!productEnded());
    Instrument *ret;
    product->inst_list.emplace_back(ret = new class RetInst(type, return_value));
    return ret;
}

void
IRBuilder::BlockBuilder::JumpInst(BasicBlock *block)
{
    assert(!productEnded());
    product->then_block = block;
}

void
IRBuilder::BlockBuilder::BrInst(Instrument *condition, BasicBlock *then_block, BasicBlock *else_block)
{
    assert(!productEnded());
    product->condition = condition;
    product->then_block = then_block;
    product->else_block = else_block;
}

Instrument *
IRBuilder::BlockBuilder::PhiInst(class PhiInst *inst)
{
    assert(!productEnded());
    product->inst_list.emplace_back(inst);
    return inst;
}

Instrument *
IRBuilder::BlockBuilder::PhiBuilder::commit()
{ return owner->PhiInst(builder.release()); }

Instrument *
IRBuilder::BlockBuilder::CallInst(class CallInst *inst)
{
    assert(!productEnded());
    product->inst_list.emplace_back(inst);
    return inst;
}

Instrument *
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
