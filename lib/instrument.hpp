//
// Created by Clarkok on 16/5/3.
//

#ifndef CYAN_INSTRUMENT_HPP
#define CYAN_INSTRUMENT_HPP

#include <vector>

#include "type.hpp"

namespace cyan {

class BasicBlock;
class Function;

class Instrument
{
protected:
    Type *type;
    std::string name;
public:
    Instrument(Type *type, std::string name)
        : type(type), name(name)
    { }

    virtual ~Instrument() = default;

    inline Type *
    getType() const
    { return type; }

    inline std::string
    getName() const
    { return name; }

    virtual std::string to_string() const = 0;
};

class ImmediateInst : public Instrument
{
public:
    ImmediateInst(Type *type, std::string name)
        : Instrument(type, name)
    { }
};

#define defineImmInst(name, type_type, value_type)                      \
    class name : public ImmediateInst                                   \
    {                                                                   \
    protected:                                                          \
        value_type value;                                               \
    public:                                                             \
        name(type_type *type, value_type value, std::string name)       \
            : ImmediateInst(type, name), value(value)                   \
        { }                                                             \
                                                                        \
        inline value_type                                               \
        getValue() const                                                \
        { return value; }                                               \
                                                                        \
        virtual std::string to_string() const;                          \
    }

defineImmInst(SignedImmInst, SignedIntegerType, intptr_t);
defineImmInst(UnsignedImmInst, UnsignedIntegerType, uintptr_t);
defineImmInst(GlobalInst, PointerType, std::string);
defineImmInst(ArgInst, PointerType, intptr_t);

#undef defineImmInst

class BinaryInst : public Instrument
{
protected:
    Instrument *left, *right;
public:
    BinaryInst(Type *type, Instrument *left, Instrument *right, std::string name)
        : Instrument(type, name),
          left(left),
          right(right)
    { }

    inline Instrument *
    getLeft() const
    { return left; }

    inline Instrument *
    getRight() const
    { return right; }
};

#define defineBinaryInst(_name)                                                         \
    class _name : public BinaryInst                                                     \
    {                                                                                   \
    public:                                                                             \
        _name(Type *type, Instrument *left, Instrument *right, std::string name)        \
            : BinaryInst(type, left, right, name)                                       \
        { }                                                                             \
                                                                                        \
        virtual std::string to_string() const;                                          \
    }

defineBinaryInst(AddInst);
defineBinaryInst(SubInst);
defineBinaryInst(MulInst);
defineBinaryInst(DivInst);
defineBinaryInst(ModInst);

defineBinaryInst(ShlInst);
defineBinaryInst(ShrInst);
defineBinaryInst(OrInst);
defineBinaryInst(AndInst);
defineBinaryInst(NorInst);
defineBinaryInst(XorInst);

defineBinaryInst(SeqInst);
defineBinaryInst(SltInst);
defineBinaryInst(SleInst);

#undef defineBinaryInst

class MemoryInst : public Instrument
{
protected:
    Instrument *address;
public:
    MemoryInst(Type *type, Instrument *address, std::string name)
        : Instrument(type, name), address(address)
    { }

    inline Instrument *
    getAddress() const
    { return address; }
};

class LoadInst : public MemoryInst
{
public:
    LoadInst(Type *type, Instrument *address, std::string name)
        : MemoryInst(type, address, name)
    { }

    virtual std::string to_string() const;
};

class StoreInst : public MemoryInst
{
protected:
    Instrument *value;
public:
    StoreInst(Type *type, Instrument *address, Instrument *value, std::string name)
        : MemoryInst(type, address, name), value(value)
    { }

    virtual std::string to_string() const;
};

class AllocaInst : public Instrument
{
protected:
    Instrument *space;
public:
    AllocaInst(Type *type, Instrument *space, std::string name)
        : Instrument(type, name), space(space)
    { }

    virtual std::string to_string() const;
};

class CallInst : public Instrument
{
protected:
    Instrument *function;
    std::vector<Instrument *> arguments;

    CallInst(Type *type, Instrument *function, std::string name)
        : Instrument(type, name), function(function)
    { }

public:
    struct Builder
    {
        std::unique_ptr<CallInst> product;

        Builder(Type *type, Instrument *function, std::string name = "")
            : product(new CallInst(type, function, name))
        { }

        Builder(Builder &&builder)
            : product(std::move(builder.product))
        { }

        inline CallInst *
        release()
        { return product.release(); }

        inline Builder &
        addArgument(Instrument *argument)
        {
            product->arguments.push_back(argument);
            return *this;
        }
    };

    inline Instrument *
    getFunction() const
    { return function; }

    inline auto
    begin() -> decltype(arguments.begin())
    { return arguments.begin(); }

    inline auto
    end() -> decltype(arguments.end())
    { return arguments.end(); }

    inline auto
    cbegin() const -> decltype(arguments.cbegin())
    { return arguments.cbegin(); }

    inline auto
    cend() const -> decltype(arguments.cend())
    { return arguments.cend(); }

    inline auto
    arguments_size() const -> decltype(arguments.size())
    { return arguments.size(); }

    virtual std::string to_string() const;
};

class RetInst : public Instrument
{
protected:
    Instrument *return_value;
public:
    RetInst(Type *type, Instrument *return_value)
        : Instrument(type, ""), return_value(return_value)
    { }

    inline Instrument *
    getReturnValue() const
    { return return_value; }

    virtual std::string to_string() const;
};

class PhiInst : public Instrument
{
public:
    struct Branch
    {
        Instrument *value;
        BasicBlock *preceder;

        Branch(Instrument *value, BasicBlock *preceder)
            : value(value), preceder(preceder)
        { }
    };

protected:
    std::vector<Branch> branches;

    PhiInst(Type *type, std::string name)
        : Instrument(type, name)
    { }

public:
    struct Builder
    {
        std::unique_ptr<PhiInst> product;

        Builder(Type *type, std::string name)
            : product(new PhiInst(type, name))
        { }

        Builder(Builder &&builder)
            : product(std::move(builder.product))
        { }

        inline PhiInst *
        release()
        { return product.release(); }

        inline Builder &
        addBranch(Instrument *value, BasicBlock *preceder)
        {
            product->branches.emplace_back(value, preceder);
            return *this;
        }
    };

    inline auto
    begin() -> decltype(branches.begin())
    { return branches.begin(); }

    inline auto
    end() -> decltype(branches.end())
    { return branches.end(); }

    inline auto
    cbegin() const -> decltype(branches.cbegin())
    { return branches.cbegin(); }

    inline auto
    cend() const -> decltype(branches.cend())
    { return branches.cend(); }

    inline auto
    branches_size() const -> decltype(branches.size())
    { return branches.size(); }

    virtual std::string to_string() const;
};

}

#endif //CYAN_INSTRUMENT_HPP
