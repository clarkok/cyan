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
                : owner(owner), builder(type, name)
            { }

        public:
            PhiBuilder(PhiBuilder &&builder)
                : owner(builder.owner), builder(std::move(builder.builder))
            { }

            ~PhiBuilder() = default;

            Instrument *commit();

            inline PhiBuilder &
            addBranch(Instrument *value, BasicBlock *preceder)
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

            CallBuilder(BlockBuilder *owner, Type *type, Instrument *function, std::string name)
                : owner(owner), builder(type, function, name)
            { }
        public:
            CallBuilder(CallBuilder &&builder)
                : owner(builder.owner), builder(std::move(builder.builder))
            { }

            ~CallBuilder() = default;

            Instrument *commit();

            inline CallBuilder &
            addArgument(Instrument *value)
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
        newCallBuilder(Type *type, Instrument *function, std::string name = "")
        { return CallBuilder(this, type, function, tempName(name)); }

        virtual Instrument *SignedImmInst(SignedIntegerType *type, intptr_t value, std::string name = "");
        virtual Instrument *UnsignedImmInst(UnsignedIntegerType *type, uintptr_t value, std::string name = "");
        virtual Instrument *GlobalInst(PointerType *type, std::string value, std::string name = "");
        virtual Instrument *ArgInst(PointerType *type, intptr_t value, std::string name = "");

        virtual Instrument *AddInst(Type *type, Instrument *left, Instrument *right, std::string name = "");
        virtual Instrument *SubInst(Type *type, Instrument *left, Instrument *right, std::string name = "");
        virtual Instrument *MulInst(Type *type, Instrument *left, Instrument *right, std::string name = "");
        virtual Instrument *DivInst(Type *type, Instrument *left, Instrument *right, std::string name = "");
        virtual Instrument *ModInst(Type *type, Instrument *left, Instrument *right, std::string name = "");

        virtual Instrument *ShlInst(Type *type, Instrument *left, Instrument *right, std::string name = "");
        virtual Instrument *ShrInst(Type *type, Instrument *left, Instrument *right, std::string name = "");
        virtual Instrument *OrInst(Type *type, Instrument *left, Instrument *right, std::string name = "");
        virtual Instrument *AndInst(Type *type, Instrument *left, Instrument *right, std::string name = "");
        virtual Instrument *NorInst(Type *type, Instrument *left, Instrument *right, std::string name = "");
        virtual Instrument *XorInst(Type *type, Instrument *left, Instrument *right, std::string name = "");

        virtual Instrument *SeqInst(Type *type, Instrument *left, Instrument *right, std::string name = "");
        virtual Instrument *SltInst(Type *type, Instrument *left, Instrument *right, std::string name = "");
        virtual Instrument *SleInst(Type *type, Instrument *left, Instrument *right, std::string name = "");

        virtual Instrument *LoadInst(Type *type, Instrument *address, std::string name = "");
        virtual Instrument *StoreInst(Type *type, Instrument *address, Instrument *value, std::string name = "");
        virtual Instrument *AllocaInst(Type *type, Instrument *space, std::string name = "");

        virtual Instrument *RetInst(Type *type, Instrument *return_value);

        virtual void JumpInst(BasicBlock *block);
        virtual void BrInst(Instrument *condition, BasicBlock *then_block, BasicBlock *else_block);

        friend class FunctionBuilder;
    protected:
        virtual Instrument *CallInst(class CallInst *inst);
        virtual Instrument *PhiInst(class PhiInst *inst);
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

        virtual std::unique_ptr<BlockBuilder> newBasicBlock(std::string name = "");

        friend class IRBuilder;
    };

    IRBuilder()
        : product(new IR())
    { }

    virtual std::unique_ptr<FunctionBuilder> newFunction(std::string name, FunctionType *prototype);
    virtual std::unique_ptr<FunctionBuilder> findFunction(std::string name);

    inline IR *
    release()
    { return product.release(); }
};

}

#endif //CYAN_IR_BUILDER_HPP
