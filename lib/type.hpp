//
// Created by Clarkok on 16/5/2.
//

#ifndef CYAN_TYPE_HPP
#define CYAN_TYPE_HPP

#include <cassert>
#include <cstddef>
#include <vector>
#include <string>

#include "cyan.hpp"

namespace cyan {

class Type
{
    bool isIntegeral() const;

public:
    Type() = default;
    virtual ~Type() = default;

    virtual size_t size() const = 0;
    virtual std::string to_string() const = 0;

    bool isPointer() const;
    bool isConcept() const;
    bool isStruct() const;

    inline bool
    isIncreasable() const
    { return isIntegeral(); }

    inline bool
    isNumber() const
    { return isIntegeral() && !isPointer(); }

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

class SignedIntegerType : public IntegeralType
{
public:
    SignedIntegerType(size_t bitwise_width)
        : IntegeralType(bitwise_width)
    { }

    virtual bool isSigned() const;
    virtual std::string to_string() const;
};

class UnsignedIntegerType : public IntegeralType
{
public:
    UnsignedIntegerType(size_t bitwise_width)
        : IntegeralType(bitwise_width)
    { }

    virtual bool isSigned() const;
    virtual std::string to_string() const;
};

class PointerType : public UnsignedIntegerType
{
protected:
    Type *base_type;
public:
    PointerType(Type *base_type)
        : UnsignedIntegerType(CYAN_PRODUCT_BITS), base_type(base_type)
    { }
    virtual std::string to_string() const;
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

    FunctionType(Type *return_type)
        : PointerType(this), return_type(return_type)
    { }

public:
    struct Builder
    {
    private:
        std::unique_ptr<FunctionType> product;

    public:
        Builder(Type *return_type)
            : product(new FunctionType(return_type))
        { }

        inline FunctionType *
        release()
        { return product.release(); }

        inline Builder &
        appendArgument(Type *arg_type)
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

    inline Type *
    getReturnType() const
    { return return_type; }

    virtual std::string to_string() const;
};

class MethodType : public FunctionType
{
protected:
    Type *owner;

    MethodType(Type *return_type, Type *owner)
        : FunctionType(return_type), owner(owner)
    { }

public:
    static MethodType *fromFunction(Type *owner, FunctionType *function);

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
    std::string name;
public:
    TemplateArgumentType(Type *concept, std::string name)
        : PointerType(concept), name(name)
    { assert(dynamic_cast<ConceptType*>(concept)); }

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
        TemplateArgumentType *type;

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
        addArgument(std::string name, TemplateArgumentType *argument)
        {
            product->arguments.emplace_back(name, argument);
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
public:
    TypePool()
    { }

    VoidType *getVoidType();
    SignedIntegerType *getSignedIntegerType(size_t bitwise_width);
    UnsignedIntegerType *getUnsignedIntegerType(size_t bitwise_width);
    ArrayType *getArrayType(Type *base_type, int lower_bound, int upper_bound);
};

}

#endif //CYAN_TYPE_HPP
