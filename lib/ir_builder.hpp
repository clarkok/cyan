//
// Created by Clarkok on 16/5/3.
//

#ifndef CYAN_IR_BUILDER_HPP
#define CYAN_IR_BUILDER_HPP

#include "ir.hpp"

namespace cyan {

class IRBuilder
{
    std::unique_ptr<IR> product;

public:
    class FunctionBuilder;

    class BlockBuilder
    {
        IRBuilder *owner;
        FunctionBuilder *function;
        BasicBlock *product;

        BlockBuilder(IRBuilder *owner, FunctionBuilder *function, BasicBlock *basic_block)
            : owner(owner), function(function), product(basic_block)
        { }
    public:
        class PhiBuilder
        {
            BlockBuilder *owner;
            PhiInst::Builder builder;

            PhiBuilder(BlockBuilder *owner, Type *type, std::string name)
                : owner(owner), builder(type, owner->product, name)
            { }

        public:
            PhiBuilder(PhiBuilder &&builder)
                : owner(builder.owner), builder(std::move(builder.builder))
            { }

            ~PhiBuilder() = default;

            Instruction *commit();

            inline PhiBuilder &
            addBranch(Instruction *value, BasicBlock *preceder)
            {
                builder.addBranch(value, preceder);
                return *this;
            }

            friend class BlockBuilder;
        };

        class CallBuilder
        {
            BlockBuilder *owner;
            CallInst::Builder builder;

            CallBuilder(BlockBuilder *owner, Type *type, Instruction *function, std::string name)
                : owner(owner), builder(type, function, owner->product, name)
            { }
        public:
            CallBuilder(CallBuilder &&builder)
                : owner(builder.owner), builder(std::move(builder.builder))
            { }

            ~CallBuilder() = default;

            Instruction *commit();

            inline CallBuilder &
            addArgument(Instruction *value)
            {
                builder.addArgument(value);
                return *this;
            }

            friend class BlockBuilder;
        };

        BlockBuilder(BlockBuilder &&builder)
            : owner(builder.owner), product(builder.product)
        { }

        virtual ~BlockBuilder() = default;

        inline std::string
        tempName(std::string name)
        {
            return name.length()
                   ? function->product->makeName(name)
                   : "_" + std::to_string(function->product->countLocalTemp());
        }

        inline BasicBlock *
        get() const
        { return product; }

        inline bool
        productEnded() const
        { return product->then_block || product->else_block; }

        inline PhiBuilder
        newPhiBuilder(Type *type, std::string name = "")
        { return PhiBuilder(this, type, tempName(name)); }

        inline CallBuilder
        newCallBuilder(Type *type, Instruction *function, std::string name = "")
        { return CallBuilder(this, type, function, tempName(name)); }

        virtual Instruction *SignedImmInst(SignedIntegerType *type, intptr_t value, std::string name = "");
        virtual Instruction *UnsignedImmInst(UnsignedIntegerType *type, uintptr_t value, std::string name = "");
        virtual Instruction *GlobalInst(PointerType *type, std::string value, std::string name = "");
        virtual Instruction *ArgInst(PointerType *type, intptr_t value, std::string name = "");

        virtual Instruction *AddInst(Type *type, Instruction *left, Instruction *right, std::string name = "");
        virtual Instruction *SubInst(Type *type, Instruction *left, Instruction *right, std::string name = "");
        virtual Instruction *MulInst(Type *type, Instruction *left, Instruction *right, std::string name = "");
        virtual Instruction *DivInst(Type *type, Instruction *left, Instruction *right, std::string name = "");
        virtual Instruction *ModInst(Type *type, Instruction *left, Instruction *right, std::string name = "");

        virtual Instruction *ShlInst(Type *type, Instruction *left, Instruction *right, std::string name = "");
        virtual Instruction *ShrInst(Type *type, Instruction *left, Instruction *right, std::string name = "");
        virtual Instruction *OrInst(Type *type, Instruction *left, Instruction *right, std::string name = "");
        virtual Instruction *AndInst(Type *type, Instruction *left, Instruction *right, std::string name = "");
        virtual Instruction *NorInst(Type *type, Instruction *left, Instruction *right, std::string name = "");
        virtual Instruction *XorInst(Type *type, Instruction *left, Instruction *right, std::string name = "");

        virtual Instruction *SeqInst(Type *type, Instruction *left, Instruction *right, std::string name = "");
        virtual Instruction *SltInst(Type *type, Instruction *left, Instruction *right, std::string name = "");
        virtual Instruction *SleInst(Type *type, Instruction *left, Instruction *right, std::string name = "");

        virtual Instruction *LoadInst(Type *type, Instruction *address, std::string name = "");
        virtual Instruction *StoreInst(Type *type, Instruction *address, Instruction *value, std::string name = "");
        virtual Instruction *AllocaInst(Type *type, Instruction *space, std::string name = "");

        virtual Instruction *RetInst(Type *type, Instruction *return_value);

        virtual void JumpInst(BasicBlock *block);
        virtual void BrInst(Instruction *condition, BasicBlock *then_block, BasicBlock *else_block);

        friend class FunctionBuilder;
    protected:
        virtual Instruction *CallInst(class CallInst *inst);
        virtual Instruction *PhiInst(class PhiInst *inst);
    };

    class FunctionBuilder
    {
        IRBuilder *owner;
        Function *product;

        FunctionBuilder(IRBuilder *owner, Function *function)
            : owner(owner), product(function)
        { }
    public:
        FunctionBuilder(FunctionBuilder &&builder)
            : owner(builder.owner), product(builder.product)
        { }

        virtual ~FunctionBuilder() = default;

        inline Function *
        get() const
        { return product; }

        inline std::string
        tempName(std::string name)
        { return name.length() ? name : "BB_" + std::to_string(product->countLocalTemp()); }

        virtual std::unique_ptr<BlockBuilder> newBasicBlock(size_t depth, std::string name = "");

        friend class IRBuilder;
    };

    IRBuilder()
        : product(new IR())
    { }

    virtual std::unique_ptr<FunctionBuilder> newFunction(std::string name, FunctionType *prototype);
    virtual std::unique_ptr<FunctionBuilder> findFunction(std::string name);

    inline IR *
    release(TypePool *type_pool)
    {
        product->type_pool.reset(type_pool);
        return product.release();
    }

    inline void
    defineGlobal(std::string name, Type *type)
    { product->global_defines.emplace(name, type); }
};

}

#endif //CYAN_IR_BUILDER_HPP
