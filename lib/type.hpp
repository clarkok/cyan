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

#include "cyan.hpp"

namespace cyan {

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

class IntegeralType : public Type
{
protected:
    size_t bitwise_width;

    IntegeralType(size_t bitwise_width)
        : bitwise_width(bitwise_width)
    { assert((1 << __builtin_ctz(bitwise_width)) == bitwise_width); }

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
protected:
    int lower_bound;
    int upper_bound;

public:
    ArrayType(Type *base_type, int lower_bound, int upper_bound)
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
protected:
    std::string name;
    std::vector<MethodType *> methods;

    ConceptType(std::string name)
        : name(name)
    { }
public:
    struct Builder
    {
    private:
        std::unique_ptr<ConceptType> product;

    public:
        Builder(std::string name)
            : product(new ConceptType(name))
        { }

        inline ConceptType *
        release()
        { return product.release(); }

        inline Builder &
        addMethod(MethodType *method)
        {
            product->methods.push_back(method);
            return *this;
        }
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
        int offset;

        Member(std::string name, Type *type, int offset)
            : name(name), type(type), offset(offset)
        { }
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
        int offset;

    public:
        Builder(std::string name)
            : product(new StructType(name)), offset(0)
        { }

        inline StructType *
        release()
        { return product.release(); }

        inline Builder &
        addMember(std::string name, Type *type)
        {
            offset = (int)((offset + CYAN_PRODUCT_ALIGN - 1) % CYAN_PRODUCT_ALIGN);
            product->members.emplace_back(name, type, offset);
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

    inline void
    implementConcept(ConceptType *concept)
    { concepts.push_back(concept); }

    inline bool
    implementedConcept(ConceptType *concept)
    {
        for(auto c : concepts) { if (c == concept) { return true; } }
        return false;
    }

    inline std::string
    getName() const
    { return name; }

    virtual size_t size() const;
    virtual std::string to_string() const;
};

class TemplateArgumentType : public PointerType
{
    class TemplateType *scope;
    std::string name;
public:
    TemplateArgumentType(ConceptType *concept, class TemplateType *scope, std::string name)
        : PointerType(concept), scope(scope), name(name)
    { }

    inline class TemplateType *
    getScope() const
    { return scope; }

    inline std::string
    getName() const
    { return name; }

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

    TemplateType(Type *base_type)
        : base_type(base_type)
    { }

public:
    struct Builder
    {
    private:
        std::unique_ptr<TemplateType> product;

    public:
        Builder(Type *base_type)
            : product(new TemplateType(base_type))
        { }

        inline TemplateType *
        release()
        { return product.release(); }

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

    inline Type *
    getBaseType()
    { return base_type; }

    virtual size_t size() const;
    virtual std::string to_string() const;
};

class TypePool
{
    std::unique_ptr<VoidType> void_type;
    std::map<size_t, std::unique_ptr<SignedIntegerType> > signed_integer_type;
    std::map<size_t, std::unique_ptr<UnsignedIntegerType> > unsigned_integer_type;
    std::map<Type *, std::unique_ptr<PointerType> > pointer_type;
    std::vector<std::unique_ptr<FunctionType> > function_type;
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

    TypePool() = default;
    ~TypePool() = default;

    VoidType *getVoidType();
    SignedIntegerType *getSignedIntegerType(size_t bitwise_width);
    UnsignedIntegerType *getUnsignedIntegerType(size_t bitwise_width);
    PointerType *getPointerType(Type *base_type);

    inline FunctionTypeBuilder
    getFunctionTypeBuilder()
    { return FunctionTypeBuilder(this); };
};

}

#endif //CYAN_TYPE_HPP
