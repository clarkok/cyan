//
// Created by Clarkok on 16/5/3.
//

#ifndef CYAN_INSTRUCTION_HPP
#define CYAN_INSTRUCTION_HPP

#include <vector>

#include "type.hpp"

namespace cyan {

struct BasicBlock;
struct Function;

class CodeGen;

class Instruction
{
protected:
    Type *type;
    std::string name;
    size_t referenced_count;

public:
    Instruction(Type *type, std::string name)
        : type(type), name(name), referenced_count(0)
    { }

    virtual ~Instruction() = default;

    inline Type *
    getType() const
    { return type; }

    inline std::string
    getName() const
    { return name; }

    inline size_t
    getReferencedCount() const
    { return referenced_count; }

    inline void
    reference()
    { ++referenced_count; }

    inline void
    unreference()
    { --referenced_count; }

    template <typename T>
    bool is() const
    { return dynamic_cast<const T*>(this) != nullptr; }

    template <typename T>
    const T* to() const
    { return dynamic_cast<const T*>(this); }

    template <typename T>
    T* to()
    { return dynamic_cast<T*>(this); }

    virtual std::string to_string() const = 0;
    virtual void codegen(CodeGen *) = 0;
    virtual void unreferenceOperand() const = 0;
    virtual bool isCodeGenRoot() const = 0;
};

class ImmediateInst : public Instruction
{
public:
    ImmediateInst(Type *type, std::string name)
        : Instruction(type, name)
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
        virtual void codegen(CodeGen *);                                \
        virtual void unreferenceOperand() const;                        \
        virtual bool isCodeGenRoot() const { return false; }            \
    }

defineImmInst(SignedImmInst, SignedIntegerType, intptr_t);
defineImmInst(UnsignedImmInst, UnsignedIntegerType, uintptr_t);
defineImmInst(GlobalInst, PointerType, std::string);
defineImmInst(ArgInst, PointerType, intptr_t);

#undef defineImmInst

class BinaryInst : public Instruction
{
protected:
    Instruction *left, *right;
public:
    BinaryInst(Type *type, Instruction *left, Instruction *right, std::string name)
        : Instruction(type, name),
          left(left),
          right(right)
    { }

    inline Instruction *
    getLeft() const
    { return left; }

    inline Instruction *
    getRight() const
    { return right; }

    virtual bool
    isCodeGenRoot() const
    { return false; }
};

#define defineBinaryInst(_name)                                                         \
    class _name : public BinaryInst                                                     \
    {                                                                                   \
    public:                                                                             \
        _name(Type *type, Instruction *left, Instruction *right, std::string name)      \
            : BinaryInst(type, left, right, name)                                       \
        { }                                                                             \
                                                                                        \
        virtual std::string to_string() const;                                          \
        virtual void codegen(CodeGen *);                                                \
        virtual void unreferenceOperand() const;                                        \
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

class MemoryInst : public Instruction
{
protected:
    Instruction *address;
public:
    MemoryInst(Type *type, Instruction *address, std::string name)
        : Instruction(type, name), address(address)
    { }

    inline Instruction *
    getAddress() const
    { return address; }
};

class LoadInst : public MemoryInst
{
public:
    LoadInst(Type *type, Instruction *address, std::string name)
        : MemoryInst(type, address, name)
    { }

    virtual std::string to_string() const;
    virtual void codegen(CodeGen *);
    virtual void unreferenceOperand() const;
    virtual bool
    isCodeGenRoot() const
    { return false; }
};

class StoreInst : public MemoryInst
{
protected:
    Instruction *value;
public:
    StoreInst(Type *type, Instruction *address, Instruction *value, std::string name)
        : MemoryInst(type, address, name), value(value)
    { }

    inline Instruction *
    getValue() const
    { return value; }

    virtual std::string to_string() const;
    virtual void codegen(CodeGen *);
    virtual void unreferenceOperand() const;
    virtual bool
    isCodeGenRoot() const
    { return true; }
};

class AllocaInst : public Instruction
{
protected:
    Instruction *space;
public:
    AllocaInst(Type *type, Instruction *space, std::string name)
        : Instruction(type, name), space(space)
    { }

    inline Instruction *
    getSpace() const
    { return space; }

    virtual std::string to_string() const;
    virtual void codegen(CodeGen *);
    virtual void unreferenceOperand() const;
    virtual bool
    isCodeGenRoot() const
    { return false; }
};

class CallInst : public Instruction
{
protected:
    Instruction *function;
    std::vector<Instruction *> arguments;

    CallInst(Type *type, Instruction *function, std::string name)
        : Instruction(type, name), function(function)
    { }

public:
    struct Builder
    {
        std::unique_ptr<CallInst> product;

        Builder(Type *type, Instruction *function, std::string name = "")
            : product(new CallInst(type, function, name))
        { }

        Builder(Builder &&builder)
            : product(std::move(builder.product))
        { }

        inline CallInst *
        release()
        { return product.release(); }

        inline Builder &
        addArgument(Instruction *argument)
        {
            product->arguments.push_back(argument);
            return *this;
        }
    };

    inline Instruction *
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

    inline Instruction *
    getArgumentByIndex(size_t index) const
    { return arguments[index]; }

    virtual std::string to_string() const;
    virtual void codegen(CodeGen *);
    virtual void unreferenceOperand() const;

    virtual bool
    isCodeGenRoot() const
    { return true; }
};

class RetInst : public Instruction
{
protected:
    Instruction *return_value;
public:
    RetInst(Type *type, Instruction *return_value)
        : Instruction(type, ""), return_value(return_value)
    { }

    inline Instruction *
    getReturnValue() const
    { return return_value; }

    virtual std::string to_string() const;
    virtual void codegen(CodeGen *);
    virtual void unreferenceOperand() const;

    virtual bool
    isCodeGenRoot() const
    { return true; }
};

class PhiInst : public Instruction
{
public:
    struct Branch
    {
        Instruction *value;
        BasicBlock *preceder;

        Branch(Instruction *value, BasicBlock *preceder)
            : value(value), preceder(preceder)
        { }
    };

protected:
    std::vector<Branch> branches;

    PhiInst(Type *type, std::string name)
        : Instruction(type, name)
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
        addBranch(Instruction *value, BasicBlock *preceder)
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
    virtual void codegen(CodeGen *);
    virtual void unreferenceOperand() const;

    virtual bool
    isCodeGenRoot() const
    { return false; }
};

#define inst_foreach(macro) \
    macro(Instruction)      \
    macro(SignedImmInst)    \
    macro(UnsignedImmInst)  \
    macro(GlobalInst)       \
    macro(ArgInst)          \
    macro(AddInst)          \
    macro(SubInst)          \
    macro(MulInst)          \
    macro(DivInst)          \
    macro(ModInst)          \
    macro(ShlInst)          \
    macro(ShrInst)          \
    macro(OrInst)           \
    macro(AndInst)          \
    macro(NorInst)          \
    macro(XorInst)          \
    macro(SeqInst)          \
    macro(SltInst)          \
    macro(SleInst)          \
    macro(LoadInst)         \
    macro(StoreInst)        \
    macro(AllocaInst)       \
    macro(CallInst)         \
    macro(RetInst)          \
    macro(PhiInst)

}

#endif //CYAN_INSTRUMENT_HPP
