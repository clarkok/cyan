//
// Created by Clarkok on 16/4/29.
//

#ifndef CYAN_SYMBOLS_HPP
#define CYAN_SYMBOLS_HPP

#include <map>
#include <memory>
#include <vector>
#include <cassert>

#include "location.hpp"
#include "type.hpp"

namespace cyan {

struct Symbol
{
public:
    enum Klass
    {
        K_RESERVED,
        K_VARIABLE,
        K_STRUCT,
        K_CONCEPT,
        K_FUNCTION,
        K_TEMPLATE_ARG,
    };

    Location location;
    std::string name;
    Klass klass;
    intptr_t token_value;
    bool is_template;
    Type *type;

    ~Symbol() = default;
    friend class SymbolScope;

private:
    Symbol(
        Location location,
        std::string name,
        Klass klass,
        intptr_t token_value,
        bool is_template,
        Type *type
    )
        : location(location),
          name(name),
          klass(klass),
          token_value(token_value),
          is_template(is_template),
          type(type)
    { }
};

class SymbolScope
{
    std::map<std::string, std::unique_ptr<Symbol> > table;
    std::vector<std::unique_ptr<SymbolScope> > children;
    SymbolScope *parent;

    SymbolScope(SymbolScope *parent)
        : parent(parent)
    { }

    template <typename ...T_Args> Symbol *
    definedSymbol(std::string name, T_Args ...args)
    {
        Symbol *ret;
        table.emplace(name, std::unique_ptr<Symbol>(ret = (new Symbol(args...))));
        return ret;
    }

    inline bool
    isRootScope() const
    { return parent == nullptr; }

    inline SymbolScope *
    createChildScope()
    {
        children.emplace_back(this);
        return children.back().get();
    }

    inline Symbol *
    lookup(std::string name) const
    {
        if (table.find(name) != table.end()) { return table.at(name).get(); }
        if (isRootScope()) { return nullptr; }
        return parent->lookup(name);
    }

    inline SymbolScope *
    lookupDefineScope(std::string name)
    {
        if (table.find(name) != table.end()) { return this; }
        if (isRootScope()) { return nullptr; }
        return parent->lookupDefineScope(name);
    }

public:
    ~SymbolScope() = default;
    friend class SymbolTable;
};

class SymbolTable
{
    std::unique_ptr<SymbolScope> root_scope;
    SymbolScope *current_scope;

public:
    SymbolTable()
        : root_scope(new SymbolScope(nullptr)),
          current_scope(root_scope.get())
    { }

    ~SymbolTable() = default;

    inline void
    pushScope()
    { current_scope = current_scope->createChildScope(); }

    inline void
    popScope()
    {
        assert(!current_scope->isRootScope());
        current_scope = current_scope->parent;
    }

    inline SymbolScope *
    currentScope() const
    { return current_scope; }

    template <typename ...T_Args> Symbol *
    definedSymbol(std::string name, T_Args ...args)
    { return current_scope->definedSymbol(name, args...); }

    inline Symbol *
    lookup(std::string name) const
    { return current_scope->lookup(name); }

    inline SymbolScope *
    lookupDefineScope(std::string name) const
    { return current_scope->lookupDefineScope(name); }
};

}

#endif //CYAN_SYMBOLS_HPP
