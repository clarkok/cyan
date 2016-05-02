//
// Created by Clarkok on 16/5/2.
//

#include "type.hpp"

using namespace cyan;

bool
Type::isIntegeral() const
{ return dynamic_cast<const IntegeralType*>(this) != nullptr; }

bool
Type::isPointer() const
{ return dynamic_cast<const PointerType*>(this) != nullptr; }

size_t
VoidType::size() const
{ return 0; }

std::string
VoidType::to_string() const
{ return "void"; }

size_t
IntegeralType::size() const
{ return bitwise_width / 8; }

bool
SignedIntegerType::isSigned() const
{ return true; }

std::string
SignedIntegerType::to_string() const
{ return "i" + std::to_string(bitwise_width); }

bool
UnsignedIntegerType::isSigned() const
{ return false; }

std::string
UnsignedIntegerType::to_string() const
{ return "u" + std::to_string(bitwise_width); }

std::string
PointerType::to_string() const
{ return "*" + base_type->to_string(); }

size_t
ArrayType::size() const
{ return (upper_bound - lower_bound) * base_type->size(); }

std::string
ArrayType::to_string() const
{
    return base_type->to_string() + "[" +
        (lower_bound ? std::to_string(lower_bound) + ":" : "") + std::to_string(upper_bound) + "]";
}

std::string
FunctionType::to_string() const
{
    std::string ret("(");
    for (auto &arg : arguments) {
        ret += arg->to_string() + ",";
    }
    ret.pop_back();
    ret += "):" + return_type->to_string();
    return ret;
}

MethodType *
MethodType::fromFunction(Type *owner, FunctionType *function)
{
    auto ret = new MethodType(function->getReturnType(), owner);
    std::copy(function->cbegin(), function->cend(), ret->arguments.begin());
    return ret;
}

std::string
MethodType::to_string() const
{ return owner->to_string() + ".(" + dynamic_cast<const FunctionType*>(this)->to_string() + ")"; }

size_t
ConceptType::size() const
{ return methods.size() * CYAN_PRODUCT_BITS; }

std::string
ConceptType::to_string() const
{ return "concept " + getName(); }

size_t
StructType::size() const
{ return members.size() ? members.back().offset + members.back().type->size() : 0; }

std::string
StructType::to_string() const
{ return "struct " + getName(); }

std::string
TemplateArgumentType::to_string() const
{ return "(" + getName() + ":" + base_type->to_string() + ")"; }

size_t
TemplateType::size() const
{ return base_type->size(); }

std::string
TemplateType::to_string() const
{
    std::string ret("template<");

    for (auto &arg : arguments) {
        ret += arg.type->to_string() + ",";
    }
    ret.pop_back();
    ret += "> " + base_type->to_string();

    return ret;
}


