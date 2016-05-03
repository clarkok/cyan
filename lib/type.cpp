//
// Created by Clarkok on 16/5/2.
//

#include "type.hpp"

using namespace cyan;

bool
Type::equalTo(Type *type) const
{
    if (is<VoidType>()) {
        return type->is<VoidType>();
    }
    else if (is<SignedIntegerType>()) {
        return type->is<SignedIntegerType>() &&
                to<SignedIntegerType>()->size() ==
                type->to<SignedIntegerType>()->size();
    }
    else if (is<UnsignedIntegerType>()) {
        return type->is<UnsignedIntegerType>() &&
                to<UnsignedIntegerType>()->size() ==
                type->to<UnsignedIntegerType>()->size();
    }
    else if (is<ArrayType>()) {
        return type->is<ArrayType>() &&
               to<ArrayType>()->getBaseType()->equalTo(type->to<ArrayType>()->getBaseType());
    }
    else if (is<PointerType>()) {
        return type->is<PointerType>() &&
               to<PointerType>()->getBaseType()->equalTo(type->to<PointerType>()->getBaseType());
    }
    else if (is<MethodType>()) {
        if (!type->is<MethodType>()) {
            return false;
        }

        auto this_method = to<MethodType>();
        auto type_method = to<MethodType>();
        if (!this_method->getReturnType()->equalTo(type_method->getReturnType())) {
            return false;
        }
        if (!this_method->getOwnerType()->equalTo(type_method->getOwnerType())) {
            return false;
        }
        if (this_method->arguments_size() != type_method->arguments_size()) {
            return false;
        }

        for (
            auto this_iter = this_method->cbegin(),
                type_iter = type_method->cbegin();
            this_iter != this_method->cend();
            ++this_iter, ++type_iter
            ) {
            if (!(*this_iter)->equalTo(*type_iter)) {
                return false;
            }
        }
        return true;
    }
    else if (is<FunctionType>()) {
        if (!type->is<FunctionType>()) {
            return false;
        }

        auto this_func = to<FunctionType>();
        auto type_func = type->to<FunctionType>();
        if (!this_func->getReturnType()->equalTo(type_func->getReturnType())) {
            return false;
        }
        if (this_func->arguments_size() != type_func->arguments_size()) {
            return false;
        }

        for (
            auto this_iter = this_func->cbegin(),
                 type_iter = type_func->cbegin();
            this_iter != this_func->cend();
            ++this_iter, ++type_iter
        ) {
            if (!(*this_iter)->equalTo(*type_iter)) {
                return false;
            }
        }
        return true;
    }
    else if (is<ConceptType>()) {
        return type->is<ConceptType>() &&
               to<ConceptType>()->getName() ==
               type->to<ConceptType>()->getName();
    }
    else if (is<StructType>()) {
        return type->is<StructType>() &&
               to<StructType>()->getName() ==
               type->to<StructType>()->getName();
    }
    else {
        assert(false && "should not reach here");
    }
}

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

bool
PointerType::isSigned() const
{ return false; }

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

VoidType *
TypePool::getVoidType()
{
    if (!void_type) { void_type.reset(new VoidType()); }
    return void_type.get();
}

SignedIntegerType *
TypePool::getSignedIntegerType(size_t bitwise_width)
{
    if (signed_integer_type.find(bitwise_width) == signed_integer_type.end()) {
        signed_integer_type.emplace(
            bitwise_width,
            std::unique_ptr<SignedIntegerType>(new SignedIntegerType(bitwise_width))
        );
    }
    return signed_integer_type[bitwise_width].get();
}

UnsignedIntegerType *
TypePool::getUnsignedIntegerType(size_t bitwise_width)
{
    if (unsigned_integer_type.find(bitwise_width) == unsigned_integer_type.end()) {
        unsigned_integer_type.emplace(
            bitwise_width,
            std::unique_ptr<UnsignedIntegerType>(new UnsignedIntegerType(bitwise_width))
        );
    }
    return unsigned_integer_type[bitwise_width].get();
}
