//
// Created by Clarkok on 16/5/2.
//

#ifndef CYAN_TYPE_HPP
#define CYAN_TYPE_HPP

#include <cassert>
#include <cstddef>
#include <vector>
#include <string>
#include <map>
#include <exception>
#include <limits>
#include <memory>
#include <functional>

#include "cyan.hpp"

namespace cyan {

struct Function;

class Type
{
public:
    Type() = default;
    virtual ~Type() = default;

    virtual size_t size() const = 0;
    virtual std::string to_string() const = 0;

    template <typename T>
    T* to()
    { return dynamic_cast<T*>(this); }

    template <typename T>
    const T* to() const
    { return dynamic_cast<const T*>(this); }

    template <typename T>
    bool is() const
    { return to<T>() != nullptr; }

    bool equalTo(Type *type) const;
};

class VoidType : public Type
{
public:
    VoidType() = default;

    virtual size_t size() const;
    virtual std::string to_string() const;
};

class ForwardType : public Type
{
protected:
    std::string name;
public:
    ForwardType(std::string name)
        : name(name)
    { }

    inline std::string
    getName() const
    { return name; }

    virtual size_t size() const;
    virtual std::string to_string() const;
};

class IntegeralType : public Type
{
protected:
    size_t bitwise_width;

    IntegeralType(size_t bitwise_width)
        : bitwise_width(bitwise_width)
    { assert((1u << __builtin_ctz(bitwise_width)) == bitwise_width); }

public:
    inline size_t
    getBitwiseWidth() const
    { return bitwise_width; }

    virtual size_t size() const;
    virtual bool isSigned() const = 0;
};

class NumericType : public IntegeralType
{
public:
    NumericType(size_t bitwise_width)
        : IntegeralType(bitwise_width)
    { }
};

class SignedIntegerType : public NumericType
{
public:
    SignedIntegerType(size_t bitwise_width)
        : NumericType(bitwise_width)
    { }

    virtual bool isSigned() const;
    virtual std::string to_string() const;
};

class UnsignedIntegerType : public NumericType
{
public:
    UnsignedIntegerType(size_t bitwise_width)
        : NumericType(bitwise_width)
    { }

    virtual bool isSigned() const;
    virtual std::string to_string() const;
};

class PointerType : public IntegeralType
{
protected:
    Type *base_type;
public:
    PointerType(Type *base_type)
        : IntegeralType(CYAN_PRODUCT_BITS), base_type(base_type)
    { }

    inline Type *
    getBaseType() const
    { return base_type; }

    virtual std::string to_string() const;
    virtual bool isSigned() const;
};

class ArrayType : public PointerType
{
public:
    ArrayType(Type *base_type)
        : PointerType(base_type)
    { }

    virtual size_t size() const;
    virtual std::string to_string() const;
};

class FunctionType : public PointerType
{
protected:
    std::vector<Type *> arguments;
    Type *return_type;

    FunctionType()
        : PointerType(this), return_type(nullptr)
    { }

public:
    struct Builder
    {
    private:
        std::unique_ptr<FunctionType> product;

    public:
        Builder()
            : product(new FunctionType())
        { }

        Builder(Builder &&builder)
            : product(std::move(builder.product))
        { }

        inline FunctionType *
        release(Type *return_type)
        {
            product->return_type = return_type;
            return product.release();
        }

        inline Builder &
        addArgument(Type *arg_type)
        {
            product->arguments.push_back(arg_type);
            return *this;
        }
    };

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

    inline Type *
    getReturnType() const
    { return return_type; }

    virtual std::string to_string() const;
};

class MethodType : public FunctionType
{
protected:
    Type *owner;

    MethodType(Type *owner)
        : owner(owner)
    { }

public:
    static MethodType *fromFunction(Type *owner, FunctionType *function);

    inline Type *
    getOwnerType() const
    { return owner; }

    virtual std::string to_string() const;
};

class ConceptType : public Type
{
public:
    struct Method
    {
        std::string name;
        MethodType *prototype;
        Function *impl;

        Method(std::string name, MethodType *prototype, Function *impl)
            : name(name), prototype(prototype), impl(impl)
        { }
    };

    struct RedefinedMethodException : std::exception
    {
        std::string _what;

        RedefinedMethodException(std::string name, MethodType *original_prototype)
            : _what("method `" + name + "` is already defined as " + original_prototype->to_string())
        { }

        virtual const char *
        what() const noexcept
        { return _what.c_str(); }
    };

protected:
    std::string name;
    ConceptType *base_concept;
    std::vector<Method> methods;

    ConceptType(std::string name, ConceptType *base_concept)
        : name(name), base_concept(base_concept)
    {
        if (base_concept) {
            for (auto &m : *base_concept) {
                methods.emplace_back(m.name, m.prototype, m.impl);
            }
        }
    }

public:
    struct Builder
    {
    private:
        std::unique_ptr<ConceptType> product;
    public:
        inline ConceptType *
        release()
        { return product.release(); }

        inline ConceptType *
        get() const
        { return product.get(); }

        inline Builder &
        addMethod(std::string name, MethodType *prototype, Function *impl)
        {
            for (auto &m : *product) {
                if (m.name == name) {
                    if (m.prototype->equalTo(prototype)) {
                        m.impl = impl ? impl : m.impl;
                        return *this;
                    }
                    else {
                        throw RedefinedMethodException(name, m.prototype);
                    }
                }
            }
            product->methods.emplace_back(name, prototype, impl);
            return *this;
        }

        Builder(std::string name, ConceptType *base_concept)
            : product(new ConceptType(name, base_concept))
        { }
    };

    inline auto
    begin() -> decltype(methods.begin())
    { return methods.begin(); }

    inline auto
    end() -> decltype(methods.end())
    { return methods.end(); }

    inline auto
    cbegin() const -> decltype(methods.cbegin())
    { return methods.cbegin(); }

    inline auto
    cend() const -> decltype(methods.cend())
    { return methods.cend(); }

    inline auto
    methods_size() const -> decltype(methods.size())
    { return methods.size(); }

    inline std::string
    getName() const
    { return name; }

    inline ConceptType *
    getBaseConcept() const
    { return base_concept; }

    inline int
    getMethodOffset(std::string name) const
    {
        int offset = 0;
        for (auto &m : methods) {
            if (m.name == name) {
                return offset;
            }
            ++offset;
        }
        return std::numeric_limits<int>::max();
    }

    inline const Method &
    getMethodByOffset(int offset) const
    { return methods[offset]; }

    inline bool
    isInheritedFrom(ConceptType *base) const
    {
        if (equalTo(base)) {
            return true;
        }
        if (getBaseConcept()) {
            return getBaseConcept()->isInheritedFrom(base);
        }
        return false;
    }

    virtual size_t size() const;
    virtual std::string to_string() const;
};

class StructType : public Type
{
public:
    struct Member
    {
        std::string name;
        Type *type;

        Member(std::string name, Type *type)
            : name(name), type(type)
        { }
    };

    struct RedefinedMethodException : std::exception
    {
        std::string _what;

        RedefinedMethodException(std::string name, Type *original_type)
            : _what("member `" + name + "` is already defined as " + original_type->to_string())
        { }

        virtual const char *
        what() const noexcept
        { return _what.c_str(); }
    };

protected:
    std::string name;
    std::vector<Member> members;
    std::vector<ConceptType *> concepts;

    StructType(std::string name)
        : name(name)
    { }
public:
    struct Builder
    {
    private:
        std::unique_ptr<StructType> product;

    public:
        Builder(std::string name)
            : product(new StructType(name))
        { }

        inline StructType *
        release()
        { return product.release(); }

        inline Builder &
        addMember(std::string name, Type *type)
        {
            for (auto &m : *product) {
                if (m.name == name) {
                    throw RedefinedMethodException(name, m.type);
                }
            }

            product->members.emplace_back(name, type);
            return *this;
        }
    };

    inline auto
    begin() -> decltype(members.begin())
    { return members.begin(); }

    inline auto
    end() -> decltype(members.end())
    { return members.end(); }

    inline auto
    cbegin() const -> decltype(members.cbegin())
    { return members.cbegin(); }

    inline auto
    cend() const -> decltype(members.cend())
    { return members.cend(); }

    inline auto
    members_size() const -> decltype(members.size())
    { return members.size(); }

    inline const Member &
    getMemberByOffset(int offset) const
    { return members[offset]; }

    inline ConceptType *
    getConceptByOffset(int offset) const
    { return concepts[offset - members_size()]; }

    inline void
    implementConcept(ConceptType *concept)
    { concepts.push_back(concept); }

    inline bool
    implementedConcept(ConceptType *concept)
    {
        for(auto c : concepts) {
            while (c) {
                if (c->equalTo(concept)) { return true; }
                c = c->getBaseConcept();
            }
        }
        return false;
    }

    inline std::string
    getName() const
    { return name; }

    inline int
    getMemberOffset(std::string name) const
    {
        int counter = 0;
        for (auto &m : members) {
            if (m.name == name) {
                return counter;
            }
            ++counter;
        }
        return std::numeric_limits<int>::max();
    }

    inline int
    getConceptOffset(std::string name) const
    {
        int offset = static_cast<int>(members_size());
        for (auto c : concepts) {
            while (c) {
                if (c->getName() == name) {
                    return offset;
                }
                c = c->getBaseConcept();
            }
            ++offset;
        }
        return std::numeric_limits<int>::max();
    }

    virtual size_t size() const;
    virtual std::string to_string() const;
};

class CastedStructType : public ConceptType
{
public:
    struct MethodUndefinedException : std::exception
    {
        std::string _what;

        MethodUndefinedException(std::string name)
            : _what("method `" + name + "` undefined")
        { }

        virtual const char *
        what() const noexcept
        { return _what.c_str(); }
    };

    struct MethodNotMatchedException : std::exception
    {
        std::string _what;

        MethodNotMatchedException(
            std::string method_name,
            MethodType *original_prototype,
            MethodType *prototype
        )
            : _what(
                "method `" + method_name + "` with prototype " + prototype->to_string() +
                " does not match the concept prototype " + original_prototype->to_string()
            )
        { }

        virtual const char *
        what() const noexcept
        { return _what.c_str(); }
    };

protected:
    StructType *original_struct;
    int offset_base;

public:
    CastedStructType(StructType *original_struct, ConceptType *casted_concept)
        : ConceptType(casted_concept->getName(), casted_concept),
          original_struct(original_struct),
          offset_base(original_struct->getConceptOffset(casted_concept->getName()))
    { }

    inline void
    implement(std::string name, MethodType *prototype, Function *impl)
    {
        for (auto &m : methods) {
            if (m.name == name) {
                if (!m.prototype->equalTo(prototype)) {
                    throw MethodNotMatchedException(
                        name,
                        m.prototype,
                        prototype
                    );
                }

                m.impl = impl;
                return;
            }
        }
        throw MethodUndefinedException(name);
    }

    inline int
    getMemberOffset(std::string name) const
    {
        int offset = original_struct->getMemberOffset(name);
        if (offset == std::numeric_limits<int>::max()) {
            return offset;
        }
        return offset - offset_base;
    }

    inline StructType *
    getOriginalStruct() const
    { return original_struct; }

    inline const StructType::Member &
    getMemberByOffset(int offset) const
    { return getOriginalStruct()->getMemberByOffset(offset + offset_base); }

    virtual std::string to_string() const;
};

class VTableType : public Type
{
    ConceptType *owner;
public:
    VTableType(ConceptType *owner)
        : owner(owner)
    { }

    inline ConceptType *
    getOwner() const
    { return owner; }

    virtual size_t size() const;
    virtual std::string to_string() const;
};

class TemplateArgumentType : public Type
{
    ConceptType *concept;
    class TemplateType *scope;
    std::string name;
public:
    TemplateArgumentType(ConceptType *concept, class TemplateType *scope, std::string name)
        : concept(concept), scope(scope), name(name)
    { }

    inline class TemplateType *
    getScope() const
    { return scope; }

    inline ConceptType *
    getConcept() const
    { return concept; }

    inline std::string
    getName() const
    { return name; }

    virtual size_t size() const;
    virtual std::string to_string() const;
};

class TemplateType : public Type
{
public:
    struct Argument
    {
        std::string name;
        std::unique_ptr<TemplateArgumentType> type;

        Argument(std::string name, TemplateArgumentType *type)
            : name(name), type(type)
        { }
    };

protected:
    Type *base_type;
    std::vector<Argument> arguments;

    TemplateType()
        : base_type(nullptr)
    { }

public:
    struct Builder
    {
    private:
        std::unique_ptr<TemplateType> product;

    public:
        Builder()
            : product(new TemplateType())
        { }

        Builder(Builder &&b)
            : product(std::move(b.product))
        { }

        inline TemplateType *
        release(Type *base_type)
        {
            product->base_type = base_type;
            return product.release();
        }

        inline TemplateType *
        get() const
        { return product.get(); }

        inline Builder &
        addArgument(std::string name, ConceptType *concept)
        {
            product->arguments.emplace_back(
                name,
                new TemplateArgumentType(concept, product.get(), name)
            );
            return *this;
        }
    };

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

    inline size_t
    arguments_size() const
    { return arguments.size(); }

    inline Type *
    getBaseType()
    { return base_type; }

    inline TemplateArgumentType *
    findTemplateArgument(std::string name) const
    {
        for (auto &arg : arguments) {
            if (arg.name == name) {
                return arg.type.get();
            }
        }
        return nullptr;
    }

    virtual size_t size() const;
    virtual std::string to_string() const;
};

class TemplateExpandingType : public ConceptType
{
protected:
    std::vector<Type *> arguments;

public:
    TemplateExpandingType(ConceptType *concept, const std::vector<Type *> arguments)
        : ConceptType(concept->getName(), concept), arguments(arguments)
    { }

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

    inline size_t
    arguments_size() const
    { return arguments.size(); }
};

class TypePool
{
    std::unique_ptr<VoidType> void_type;
    std::map<std::string, std::unique_ptr<ForwardType> > forward_type;
    std::map<size_t, std::unique_ptr<SignedIntegerType> > signed_integer_type;
    std::map<size_t, std::unique_ptr<UnsignedIntegerType> > unsigned_integer_type;
    std::map<Type *, std::unique_ptr<PointerType> > pointer_type;
    std::map<Type *, std::unique_ptr<ArrayType> > array_type;
    std::vector<std::unique_ptr<FunctionType> > function_type;
    std::vector<std::unique_ptr<MethodType> > method_type;
    std::map<std::string, std::unique_ptr<ConceptType> > concept_type;
    std::map<std::string, std::unique_ptr<StructType> > struct_type;
    std::map<std::pair<StructType *, ConceptType *>, std::unique_ptr<CastedStructType> > casted_struct_type;
    std::vector<std::unique_ptr<TemplateType> > template_type;
    std::map<ConceptType *, std::unique_ptr<VTableType> > vtable_type;
public:
    class FunctionTypeBuilder
    {
        TypePool *owner;
        FunctionType::Builder builder;

        FunctionTypeBuilder(TypePool *owner)
            : owner(owner), builder()
        { }

    public:
        FunctionTypeBuilder(FunctionTypeBuilder &&b)
            : owner(b.owner), builder(std::move(b.builder))
        { }

        inline FunctionType *
        commit(Type *return_type)
        {
            owner->function_type.emplace_back(builder.release(return_type));
            return owner->function_type.back().get();
        }

        inline FunctionTypeBuilder &
        addArgument(Type *type)
        {
            builder.addArgument(type);
            return *this;
        }

        friend class TypePool;
    };

    class ConceptTypeBuilder
    {
        TypePool *owner;
        ConceptType::Builder builder;
        ConceptTypeBuilder(TypePool *owner, std::string name, ConceptType *base_concept)
            : owner(owner), builder(name, base_concept)
        { }

    public:
        ConceptTypeBuilder(ConceptTypeBuilder &&b)
            : owner(b.owner), builder(std::move(b.builder))
        { }

        inline ConceptType *
        get() const
        { return builder.get(); }

        inline ConceptType *
        commit()
        {
            auto product = builder.release();
            owner->concept_type.emplace(product->getName(), std::unique_ptr<ConceptType>(product));
            return product;
        }

        inline ConceptTypeBuilder &
        addMethod(std::string name, MethodType *prototype, Function *impl)
        {
            builder.addMethod(name, prototype, impl);
            return *this;
        }

        friend class TypePool;
    };

    class StructTypeBuilder
    {
        TypePool *owner;
        StructType::Builder builder;
        StructTypeBuilder(TypePool *owner, std::string name)
            : owner(owner), builder(name)
        { }

    public:
        StructTypeBuilder(StructTypeBuilder &&b)
            : owner(b.owner), builder(std::move(b.builder))
        { }

        inline StructType *
        commit()
        {
            auto product = builder.release();
            owner->struct_type.emplace(product->getName(), std::unique_ptr<StructType>(product));
            return product;
        }

        inline StructTypeBuilder &
        addMember(std::string name, Type *type)
        {
            builder.addMember(name, type);
            return *this;
        }

        friend class TypePool;
    };

    class TemplateTypeBuilder
    {
        TypePool *owner;
        TemplateType::Builder builder;

        TemplateTypeBuilder(TypePool *owner)
            : owner(owner), builder()
        { }

    public:
        TemplateTypeBuilder(TemplateTypeBuilder &&b)
            : owner(b.owner), builder(std::move(b.builder))
        { }

        inline TemplateType *
        commit(Type *base_type)
        {
            TemplateType *ret = builder.release(base_type);
            owner->template_type.emplace_back(ret);
            return ret;
        }

        inline TemplateType *
        get() const
        { return builder.get(); }

        inline TemplateTypeBuilder &
        addArgument(std::string name, ConceptType *concept)
        {
            builder.addArgument(name, concept);
            return *this;
        }

        friend class TypePool;
    };

    TypePool() = default;
    ~TypePool() = default;

    VoidType *getVoidType();
    ForwardType *getForwardType(std::string name);
    SignedIntegerType *getSignedIntegerType(size_t bitwise_width);
    UnsignedIntegerType *getUnsignedIntegerType(size_t bitwise_width);
    PointerType *getPointerType(Type *base_type);
    ArrayType *getArrayType(Type *base_type);
    MethodType *getMethodType(ConceptType *owner, FunctionType *function);
    CastedStructType *getCastedStructType(StructType *original_struct, ConceptType *concept);
    VTableType *getVTableType(ConceptType *owner);

    inline FunctionTypeBuilder
    getFunctionTypeBuilder()
    { return FunctionTypeBuilder(this); };

    inline ConceptTypeBuilder
    getConceptTypeBuilder(std::string name, ConceptType *base_concept)
    { return ConceptTypeBuilder(this, name, base_concept); }

    inline StructTypeBuilder
    getStructTypeBuilder(std::string name)
    { return StructTypeBuilder(this, name); }

    inline TemplateTypeBuilder
    getTemplateTypeBuilder()
    { return TemplateTypeBuilder(this); }

    inline void
    foreachCastedStructType(std::function<void(CastedStructType *)> callback) const
    {
        for (auto &casted_struct_pair : casted_struct_type) {
            callback(casted_struct_pair.second.get());
        }
    }
};

}

#endif //CYAN_TYPE_HPP
