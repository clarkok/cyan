//
// Created by Clarkok on 16/5/3.
//

#ifndef CYAN_INSTRUCTION_HPP
#define CYAN_INSTRUCTION_HPP

#include <vector>
#include <algorithm>
#include <type_traits>

#include "type.hpp"
#include "ir.hpp"

namespace cyan {

struct BasicBlock;
struct Function;

class CodeGen;

class Instruction
{
protected:
    Type *type;
    BasicBlock *owner_block;
    std::string name;
    size_t referenced_count;

public:
    Instruction(Type *type, BasicBlock *owner_block, std::string name)
        : type(type), owner_block(owner_block), name(name), referenced_count(0)
    { }

    virtual ~Instruction() = default;

    inline Type *
    getType() const
    { return type; }

    inline std::string
    getName() const
    { return name; }

    inline std::string
    setName(std::string name)
    { return this->name = name; }

    inline size_t
    getReferencedCount() const
    { return referenced_count; }

    inline void
    reference()
    { ++referenced_count; }

    inline void
    unreference()
    { --referenced_count; }

    inline void
    clearReferences()
    { referenced_count = 0; }

    inline BasicBlock *
    getOwnerBlock() const
    { return owner_block; }

    inline BasicBlock *
    setOwnerBlock(BasicBlock *owner_block)
    { return this->owner_block = owner_block; }

    template <typename T, typename std::enable_if<std::is_base_of<Instruction, T>::value>::type* = nullptr >
    const T* to() const
    { return dynamic_cast<const T*>(this); }

    template <typename T, typename std::enable_if<std::is_base_of<Instruction, T>::value>::type* = nullptr >
    T* to()
    { return dynamic_cast<T*>(this); }

    template <typename T>
    bool is() const
    { return this->to<T>() != nullptr; }

    virtual std::string to_string() const = 0;
    virtual void codegen(CodeGen *) = 0;
    virtual void referenceOperand() const = 0;
    virtual void unreferenceOperand() const = 0;
    virtual bool isCodeGenRoot() const = 0;
    virtual Instruction *
    clone(BasicBlock *, std::map<Instruction *, Instruction *> &, std::string name = "") const = 0;
    virtual void resolve(const std::map<Instruction *, Instruction *> &value_map) = 0;
    virtual void replaceUsage(Instruction *, Instruction *) = 0;
    virtual bool usedInstruction(Instruction *) const = 0;
};

class ImmediateInst : public Instruction
{
public:
    ImmediateInst(Type *type, BasicBlock *owner_block, std::string name)
        : Instruction(type, owner_block, name)
    { }
};

#define defineImmInst(_name, type_type, value_type)                     \
    class _name : public ImmediateInst                                  \
    {                                                                   \
    protected:                                                          \
        value_type value;                                               \
    public:                                                             \
        _name(                                                          \
            type_type *type,                                            \
            value_type value,                                           \
            BasicBlock *owner_block,                                    \
            std::string name                                            \
        )                                                               \
            : ImmediateInst(type, owner_block, name), value(value)      \
        { }                                                             \
                                                                        \
        inline value_type                                               \
        getValue() const                                                \
        { return value; }                                               \
                                                                        \
        virtual std::string to_string() const;                          \
        virtual void codegen(CodeGen *);                                \
        virtual void referenceOperand() const { }                       \
        virtual void unreferenceOperand() const { }                     \
        virtual bool isCodeGenRoot() const { return false; }            \
        virtual Instruction *                                           \
        clone(BasicBlock *block,                                        \
              std::map<Instruction *, Instruction *> &value_map,        \
              std::string name = "") const                              \
        {                                                               \
            auto ret = new _name(                                       \
                getType()->to<type_type>(),                             \
                value,                                                  \
                block,                                                  \
                name.size() ? name : getName()                          \
            );                                                          \
            value_map.emplace(                                          \
                const_cast<_name*>(this),                               \
                const_cast<_name*>(ret)                                 \
            );                                                          \
            return ret;                                                 \
        }                                                               \
        virtual void                                                    \
        resolve(const std::map<Instruction *, Instruction *> &)         \
        { }                                                             \
        virtual void                                                    \
        replaceUsage(Instruction *, Instruction *)                      \
        { }                                                             \
        virtual bool                                                    \
        usedInstruction(Instruction *) const                            \
        { return false; }                                               \
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
    BinaryInst(Type *type, Instruction *left, Instruction *right, BasicBlock *owner_block, std::string name)
        : Instruction(type, owner_block, name),
          left(left),
          right(right)
    { }

    inline Instruction *
    getLeft() const
    { return left; }

    inline Instruction *
    setLeft(Instruction *inst)
    { return left = inst; }

    inline Instruction *
    getRight() const
    { return right; }

    inline Instruction *
    setRight(Instruction *inst)
    { return right = inst; }

    virtual bool
    isCodeGenRoot() const
    { return false; }

    virtual void
    resolve(const std::map<Instruction *, Instruction *> &value_map)
    {
        if (value_map.find(left) != value_map.end()) {
            assert(value_map.at(left));
            left = value_map.at(left);
        }
        if (value_map.find(right) != value_map.end()) {
            assert(value_map.at(right));
            right = value_map.at(right);
        }
    }

    virtual bool
    usedInstruction(Instruction *inst) const
    { return left == inst || right == inst; }

    virtual void
    referenceOperand() const
    {
        getLeft()->reference();
        getRight()->reference();
    }

    virtual void
    unreferenceOperand() const
    {
        getLeft()->unreference();
        getRight()->unreference();
    }

    virtual void
    replaceUsage(Instruction *original, Instruction *replace)
    {
        if (left == original)   { left = replace; }
        if (right == original)  { right = replace; }
    }
};

#define defineBinaryInstSwappable(_name)                            \
    class _name : public BinaryInst                                 \
    {                                                               \
    public:                                                         \
        _name(                                                      \
            Type *type,                                             \
            Instruction *left,                                      \
            Instruction *right,                                     \
            BasicBlock *owner_block,                                \
            std::string name                                        \
        )                                                           \
            : BinaryInst(                                           \
                type,                                               \
                std::max(left, right),                              \
                std::min(left, right),                              \
                owner_block,                                        \
                name                                                \
            )                                                       \
        { }                                                         \
                                                                    \
        virtual std::string to_string() const;                      \
        virtual void codegen(CodeGen *);                            \
        virtual Instruction *                                       \
        clone(BasicBlock *block,                                    \
              std::map<Instruction *, Instruction *> &value_map,    \
              std::string name = "") const                          \
        {                                                           \
            auto ret = new _name(                                   \
                getType(),                                          \
                left,                                               \
                right,                                              \
                block,                                              \
                name.size() ? name : getName()                      \
            );                                                      \
            value_map.emplace(                                      \
                const_cast<_name*>(this),                           \
                const_cast<_name*>(ret)                             \
            );                                                      \
            return ret;                                             \
        }                                                           \
    }

#define defineBinaryInstUnswappable(_name)                          \
    class _name : public BinaryInst                                 \
    {                                                               \
    public:                                                         \
        _name(                                                      \
            Type *type,                                             \
            Instruction *left,                                      \
            Instruction *right,                                     \
            BasicBlock *owner_block,                                \
            std::string name                                        \
        )                                                           \
            : BinaryInst(type, left, right, owner_block, name)      \
        { }                                                         \
                                                                    \
        virtual std::string to_string() const;                      \
        virtual void codegen(CodeGen *);                            \
        virtual Instruction *                                       \
        clone(BasicBlock *block,                                    \
              std::map<Instruction *, Instruction *> &value_map,    \
              std::string name = "") const                          \
        {                                                           \
            auto ret = new _name(                                   \
                getType(),                                          \
                left,                                               \
                right,                                              \
                block,                                              \
                name.size() ? name : getName()                      \
            );                                                      \
            value_map.emplace(                                      \
                const_cast<_name*>(this),                           \
                const_cast<_name*>(ret)                             \
            );                                                      \
            return ret;                                             \
        }                                                           \
    }

defineBinaryInstSwappable   (AddInst);
defineBinaryInstUnswappable (SubInst);
defineBinaryInstSwappable   (MulInst);
defineBinaryInstUnswappable (DivInst);
defineBinaryInstUnswappable (ModInst);

defineBinaryInstUnswappable (ShlInst);
defineBinaryInstUnswappable (ShrInst);
defineBinaryInstSwappable   (OrInst);
defineBinaryInstSwappable   (AndInst);
defineBinaryInstSwappable   (NorInst);
defineBinaryInstSwappable   (XorInst);

defineBinaryInstSwappable   (SeqInst);
defineBinaryInstUnswappable (SltInst);
defineBinaryInstUnswappable (SleInst);

#undef defineBinaryInst

class MemoryInst : public Instruction
{
protected:
    Instruction *address;
public:
    MemoryInst(Type *type, Instruction *address, BasicBlock *owner_block, std::string name)
        : Instruction(type, owner_block, name), address(address)
    { }

    inline Instruction *
    getAddress() const
    { return address; }
};

class LoadInst : public MemoryInst
{
public:
    LoadInst(Type *type, Instruction *address, BasicBlock *owner_block, std::string name)
        : MemoryInst(type, address, owner_block, name)
    { }

    virtual std::string to_string() const;
    virtual void codegen(CodeGen *);

    virtual void
    referenceOperand() const
    { getAddress()->reference(); }

    virtual void
    unreferenceOperand() const
    { getAddress()->unreference(); }

    virtual bool
    isCodeGenRoot() const
    { return false; }

    virtual Instruction *
    clone(BasicBlock *block,
          std::map<Instruction *, Instruction *> &value_map,
          std::string name = "") const
    {
        auto ret = new LoadInst(
            getType(),
            address,
            block,
            name.size() ? name : getName()
        );
        value_map.emplace(
            const_cast<LoadInst*>(this),
            const_cast<LoadInst*>(ret)
        );
        return ret;
    }

    virtual void
    resolve(const std::map<Instruction *, Instruction *> &value_map)
    {
        if (value_map.find(address) != value_map.end()) {
            assert(value_map.at(address));
            address = value_map.at(address);
        }
    }

    virtual void
    replaceUsage(Instruction *original, Instruction *replace)
    { if (address == original) { address = replace; } }

    virtual bool
    usedInstruction(Instruction *inst) const
    { return address == inst; }
};

class StoreInst : public MemoryInst
{
protected:
    Instruction *value;
public:
    StoreInst(
        Type *type,
        Instruction *address,
        Instruction *value,
        BasicBlock *owner_block,
        std::string name
    )
        : MemoryInst(type, address, owner_block, name), value(value)
    { }

    inline Instruction *
    getValue() const
    { return value; }

    virtual std::string to_string() const;
    virtual void codegen(CodeGen *);

    virtual void
    referenceOperand() const
    {
        getAddress()->reference();
        getValue()->reference();
    }

    virtual void
    unreferenceOperand() const
    {
        getAddress()->unreference();
        getValue()->unreference();
    }

    virtual bool
    isCodeGenRoot() const
    { return true; }

    virtual Instruction *
    clone(BasicBlock *block,
          std::map<Instruction *, Instruction *> &value_map,
          std::string name = "") const
    {
        auto ret = new StoreInst(
            getType(),
            address,
            value,
            block,
            name.size() ? name : getName()
        );
        value_map.emplace(
            const_cast<StoreInst*>(this),
            const_cast<StoreInst*>(ret)
        );
        return ret;
    }

    virtual void
    resolve(const std::map<Instruction *, Instruction *> &value_map)
    {
        if (value_map.find(address) != value_map.end()) {
            assert(value_map.at(address));
            address = value_map.at(address);
        }
        if (value_map.find(value) != value_map.end()) {
            assert(value_map.at(value));
            value = value_map.at(value);
        }
    }

    virtual void
    replaceUsage(Instruction *original, Instruction *replace)
    {
        if (address == original)    { address = replace; }
        if (value == original)      { value = replace; }
    }

    virtual bool
    usedInstruction(Instruction *inst) const
    { return address == inst || value == inst; }
};

class AllocaInst : public Instruction
{
protected:
    Instruction *space;
public:
    AllocaInst(Type *type, Instruction *space, BasicBlock *owner_block, std::string name)
        : Instruction(type, owner_block, name), space(space)
    { }

    inline Instruction *
    getSpace() const
    { return space; }

    virtual std::string to_string() const;
    virtual void codegen(CodeGen *);

    virtual void
    referenceOperand() const
    { getSpace()->reference(); }

    virtual void
    unreferenceOperand() const
    { getSpace()->unreference(); }

    virtual bool
    isCodeGenRoot() const
    { return false; }

    virtual Instruction *
    clone(BasicBlock *block,
          std::map<Instruction *, Instruction *> &value_map,
          std::string name = "") const
    {
        auto ret = new AllocaInst(
            getType(),
            space,
            block,
            name.size() ? name : getName()
        );
        value_map.emplace(
            const_cast<AllocaInst*>(this),
            const_cast<AllocaInst*>(ret)
        );
        return ret;
    }

    virtual void
    resolve(const std::map<Instruction *, Instruction *> &value_map)
    {
        if (value_map.find(space) != value_map.end()) {
            assert(value_map.at(space));
            space = value_map.at(space);
        }
    }

    virtual void
    replaceUsage(Instruction *original, Instruction *replace)
    { if (space == original) { space = replace; } }

    virtual bool
    usedInstruction(Instruction *inst) const
    { return space == inst; }
};

class CallInst : public Instruction
{
protected:
    Instruction *function;
    std::vector<Instruction *> arguments;

    CallInst(Type *type, Instruction *function, BasicBlock *owner_block, std::string name)
        : Instruction(type, owner_block, name), function(function)
    { }

public:
    struct Builder
    {
        std::unique_ptr<CallInst> product;

        Builder(Type *type, Instruction *function, BasicBlock *owner_block, std::string name = "")
            : product(new CallInst(type, function, owner_block, name))
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
    begin() const -> decltype(arguments.begin())
    { return arguments.begin(); }

    inline auto
    end() const -> decltype(arguments.end())
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

    virtual void
    referenceOperand() const
    {
        function->reference();
        for (auto &arg : arguments) {
            arg->reference();
        }
    }

    virtual void
    unreferenceOperand() const
    {
        function->unreference();
        for (auto &arg : arguments){
            arg->unreference();
        }
    }

    virtual bool
    isCodeGenRoot() const
    { return true; }

    virtual Instruction *
    clone(BasicBlock *block,
          std::map<Instruction *, Instruction *> &value_map,
          std::string name = "") const
    {
        auto builder = Builder(
            getType(),
            getFunction(),
            block,
            name.size() ? name : getName()
        );
        for (auto &arg : *this) {
            builder.addArgument(arg);
        }
        auto ret = builder.release();
        value_map.emplace(
            const_cast<CallInst*>(this),
            const_cast<CallInst*>(ret)
        );
        return ret;
    }

    virtual void
    resolve(const std::map<Instruction *, Instruction *> &value_map)
    {
        if (value_map.find(function) != value_map.end()) {
            assert(value_map.at(function));
            function = value_map.at(function);
        }

        for (auto &arg : *this) {
            if (value_map.find(arg) != value_map.end()) {
                assert(value_map.at(arg));
                arg = value_map.at(arg);
            }
        }
    }

    virtual void
    replaceUsage(Instruction *original, Instruction *replace)
    {
        if (function == original) {
            function = replace;
        }

        for (auto &arg : *this) {
            if (arg == original) {
                arg = replace;
            }
        }
    }

    virtual bool
    usedInstruction(Instruction *inst) const
    {
        if (function == inst) { return true; }
        for (auto &arg : *this) {
            if (arg == inst) { return true; }
        }
        return false;
    }
};

class RetInst : public Instruction
{
protected:
    Instruction *return_value;
public:
    RetInst(Type *type, BasicBlock *owner_block, Instruction *return_value)
        : Instruction(type, owner_block, ""), return_value(return_value)
    { }

    inline Instruction *
    getReturnValue() const
    { return return_value; }

    virtual std::string to_string() const;
    virtual void codegen(CodeGen *);

    virtual void
    referenceOperand() const
    {
        if (getReturnValue()) {
            getReturnValue()->reference();
        }
    }

    virtual void
    unreferenceOperand() const
    {
        if (getReturnValue()) {
            getReturnValue()->unreference();
        }
    }

    virtual bool
    isCodeGenRoot() const
    { return true; }

    virtual Instruction *
    clone(BasicBlock *block,
          std::map<Instruction *, Instruction *> &value_map,
          std::string name = "") const
    {
        auto ret = new RetInst(
            getType(),
            block,
            return_value
        );
        value_map.emplace(
            const_cast<RetInst*>(this),
            const_cast<RetInst*>(ret)
        );
        return ret;
    }

    virtual void
    resolve(const std::map<Instruction *, Instruction *> &value_map)
    {
        if (value_map.find(return_value) != value_map.end()) {
            assert(value_map.at(return_value));
            return_value = value_map.at(return_value);
        }
    }

    virtual void
    replaceUsage(Instruction *original, Instruction *replace)
    { if (return_value == original) { return_value = replace; } }

    virtual bool
    usedInstruction(Instruction *inst) const
    { return return_value == inst; }
};

class NewInst : public Instruction
{
    Instruction *space;
public:
    NewInst(Type *type, BasicBlock *owner_block, Instruction *space, std::string name)
        : Instruction(type, owner_block, name), space(space)
    { }

    inline Instruction *
    getSpace() const
    { return space; }

    virtual std::string to_string() const;
    virtual void codegen(CodeGen *);

    virtual void
    referenceOperand() const
    { space->reference(); }

    virtual void
    unreferenceOperand() const
    { space->unreference(); }

    virtual bool
    isCodeGenRoot() const
    { return false; }

    virtual Instruction *
    clone(BasicBlock *block,
          std::map<Instruction *, Instruction *> &value_map,
          std::string name = "") const
    {
        auto ret = new NewInst(
            getType(),
            block,
            space,
            name
        );
        value_map.emplace(
            const_cast<NewInst*>(this),
            const_cast<NewInst*>(ret)
        );
        return ret;
    }

    virtual void
    resolve(const std::map<Instruction *, Instruction *> &value_map)
    {
        if (value_map.find(space) != value_map.end()) {
            assert(value_map.at(space));
            space = value_map.at(space);
        }
    }

    virtual void
    replaceUsage(Instruction *original, Instruction *replace)
    { if (space == original) { space = replace; } }

    virtual bool
    usedInstruction(Instruction *inst) const
    { return space == inst; }
};

class DeleteInst : public Instruction
{
    Instruction *target;
public:
    DeleteInst(Type *type, BasicBlock *owner_block, Instruction *target, std::string name)
        : Instruction(type, owner_block, name), target(target)
    { }

    inline Instruction *
    getTarget() const
    { return target; }

    virtual std::string to_string() const;
    virtual void codegen(CodeGen *);

    virtual void
    referenceOperand() const
    { target->reference(); }

    virtual void
    unreferenceOperand() const
    { target->unreference(); }

    virtual bool
    isCodeGenRoot() const
    { return true; }

    virtual Instruction *
    clone(BasicBlock *block,
          std::map<Instruction *, Instruction *> &value_map,
          std::string name = "") const
    {
        auto ret = new DeleteInst(
            getType(),
            block,
            target,
            name
        );
        value_map.emplace(
            const_cast<DeleteInst*>(this),
            const_cast<DeleteInst*>(ret)
        );
        return ret;
    }

    virtual void
    resolve(const std::map<Instruction *, Instruction *> &value_map)
    {
        if (value_map.find(target) != value_map.end()) {
            assert(value_map.at(target));
            target = value_map.at(target);
        }
    }

    virtual void
    replaceUsage(Instruction *original, Instruction *replace)
    { if (target == original) { target = replace; } }

    virtual bool
    usedInstruction(Instruction *inst) const
    { return target == inst; }
};

class PhiInst : public Instruction
{
public:
    struct Branch {
        Instruction *value;
        BasicBlock *preceder;

        Branch(Instruction *value, BasicBlock *preceder)
            : value(value), preceder(preceder)
        { }
    };

protected:
    std::vector<Branch> branches;

    PhiInst(Type *type, BasicBlock *owner_block, std::string name)
        : Instruction(type, owner_block, name)
    { }

public:
    struct Builder
    {
        std::unique_ptr<PhiInst> product;

        Builder(Type *type, BasicBlock *owner_block, std::string name)
            : product(new PhiInst(type, owner_block, name))
        { }

        Builder(Builder &&builder)
            : product(std::move(builder.product))
        { }

        ~Builder() = default;

        inline PhiInst *
        release()
        { return product.release(); }

        inline PhiInst *
        get() const
        { return product.get(); }

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

    inline void
    remove_branch(BasicBlock *preceder)
    {
        branches.erase(
            std::remove_if(
                branches.begin(),
                branches.end(),
                [preceder](const Branch &branch) {
                    return branch.preceder == preceder;
                }
            ),
            branches.end()
        );
    }

    virtual std::string to_string() const;
    virtual void codegen(CodeGen *);

    virtual void
    referenceOperand() const
    {
        for (auto &branch : branches) {
            branch.value->reference();
        }
    }

    virtual void
    unreferenceOperand() const
    {
        for (auto &branch : branches) {
            branch.value->unreference();
        }
    }

    virtual bool
    isCodeGenRoot() const
    { return false; }

    virtual Instruction *
    clone(BasicBlock *, std::map<Instruction *, Instruction *>&, std::string) const
    { assert(false); }

    Instruction *
    cloneBranch(
        std::map<BasicBlock *, BasicBlock *> &block_map,
        std::map<Instruction *, Instruction *> &value_map,
        std::string name = ""
    ) const
    {
        auto builder = Builder(
            getType(),
            block_map[owner_block],
            name
        );
        for (auto &branch : branches) {
            builder.addBranch(
                branch.value,
                block_map.at(branch.preceder)
            );
        }
        auto ret = builder.release();
        value_map.emplace(
            const_cast<PhiInst*>(this),
            const_cast<PhiInst*>(ret)
        );
        return ret;
    }

    virtual void
    resolve(const std::map<Instruction *, Instruction *> &value_map)
    {
        for (auto &branch : branches) {
            if (value_map.find(branch.value) != value_map.end()) {
                assert(value_map.at(branch.value));
                branch.value = value_map.at(branch.value);
            }
        }
    }

    virtual void
    replaceUsage(Instruction *, Instruction *)
    { assert(false); }

    void
    replaceBranch(Instruction *original, Instruction *replace, BasicBlock *block)
    {
        for (auto &branch : branches) {
            if (branch.value == original) {
                branch.value = replace;
                branch.preceder = block;
            }
        }
    }

    virtual bool
    usedInstruction(Instruction *inst) const
    {
        for (auto &branch : branches) {
            if (branch.value == inst) return true;
        }
        return false;
    }
};

#define inst_foreach(macro)         \
    macro(::cyan::Instruction)      \
    macro(::cyan::SignedImmInst)    \
    macro(::cyan::UnsignedImmInst)  \
    macro(::cyan::GlobalInst)       \
    macro(::cyan::ArgInst)          \
    macro(::cyan::AddInst)          \
    macro(::cyan::SubInst)          \
    macro(::cyan::MulInst)          \
    macro(::cyan::DivInst)          \
    macro(::cyan::ModInst)          \
    macro(::cyan::ShlInst)          \
    macro(::cyan::ShrInst)          \
    macro(::cyan::OrInst)           \
    macro(::cyan::AndInst)          \
    macro(::cyan::NorInst)          \
    macro(::cyan::XorInst)          \
    macro(::cyan::SeqInst)          \
    macro(::cyan::SltInst)          \
    macro(::cyan::SleInst)          \
    macro(::cyan::LoadInst)         \
    macro(::cyan::StoreInst)        \
    macro(::cyan::AllocaInst)       \
    macro(::cyan::CallInst)         \
    macro(::cyan::RetInst)          \
    macro(::cyan::NewInst)          \
    macro(::cyan::DeleteInst)       \
    macro(::cyan::PhiInst)

}

#endif //CYAN_INSTRUMENT_HPP
